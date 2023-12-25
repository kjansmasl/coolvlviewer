/**
 * @file llviewermessage.cpp
 * @brief Dumping ground for viewer-side message system callbacks.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include <deque>

#include "llviewermessage.h"

#include "llanimationstates.h"			// For ANIM_AGENT_PUPPET_MOTION
#include "llaudioengine.h"
#include "llassetstorage.h"
#include "llcachename.h"
#include "llcallbacklist.h"
#include "llconsole.h"
#include "lldbstrings.h"
#include "lldir.h"
#include "lldispatcher.h"
#include "lleconomy.h"
#include "llevents.h"
#include "lleventtimer.h"
#include "llexperiencecache.h"
#include "llfasttimer.h"
#include "llfilesystem.h"
#include "llmaterialtable.h"
#include "llmd5.h"
#include "llmenugl.h"
#include "llregionflags.h"
#include "llregionhandle.h"
#include "llscriptpermissions.h"
#include "llsdserialize.h"
#include "llteleportflags.h"
#include "lltracker.h"
#include "lltrans.h"
#include "lltransactionflags.h"
#include "lluploaddialog.h"
#include "llxfermanager.h"
#include "llmessage.h"
#include "sound_ids.h"

#include "llagent.h"
#include "llagentpilot.h"
#include "llappearancemgr.h"
#include "llappviewer.h"
#include "llavatartracker.h"
#include "lldrawpool.h"
#include "llexperiencelog.h"			// For PUMP_EXPERIENCE
#include "llfirstuse.h"
#include "llfloateractivespeakers.h"
#include "hbfloaterareasearch.h"
#include "hbfloaterbump.h"
#include "llfloaterbuycurrency.h"
#include "llfloaterbuyland.h"
#include "llfloaterchat.h"
#include "llfloatergroupinfo.h"
#include "llfloaterim.h"
#include "llfloaterinventory.h"
#include "llfloaterland.h"
#include "llfloaterlandholdings.h"
#include "llfloatermute.h"
#include "llfloaterpostcard.h"
#include "llfloaterpreference.h"
#include "llfloaterregioninfo.h"
#include "hbfloatersearch.h"
#include "hbfloatersoundslist.h"
#include "hbfloaterteleporthistory.h"
#include "llfloaterworldmap.h"
#include "llfolderview.h"
#include "llfollowcam.h"
#include "llgltfmateriallist.h"
#include "llgridmanager.h"
#include "llgroupnotify.h"
#include "llhudeffect.h"
#include "llhudeffectspiral.h"
#include "llimmgr.h"
#include "llinventoryactions.h"
#include "llinventorymodel.h"
#include "llmarketplacefunctions.h"
#include "llmutelist.h"
#include "llnotify.h"
#include "llpanelgrouplandmoney.h"
#include "llpipeline.h"
#include "llpuppetmodule.h"
#include "llpuppetmotion.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llslurl.h"
#include "llstartup.h"
#include "llsky.h"
#include "llstatusbar.h"
#include "lltool.h"
#include "lltoolbar.h"
#include "lltoolmgr.h"
#include "llurldispatcher.h"
#include "llvieweraudio.h"
#include "hbviewerautomation.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewerinventory.h"
#include "llviewerjoystick.h"
#include "llviewermenu.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerpartsource.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvlmanager.h"
#include "llvoavatarpuppet.h"
#include "llvoavatarself.h"
#include "llweb.h"
#include "llwlskyparammgr.h"
#include "llwlwaterparammgr.h"
#include "llworld.h"
#include "llworldmap.h"

// Set to 1 to automatically accept and open offered scripts: since those are
// most often closed sources (no-mod), and a "giver" script also often offers
// itself (wrongly) when opening scripted boxes, I find it more annoying than
// anything else, so it is disabled in the Cool VL Viewer. *TODO, maybe: add
// two debug settings to also auto-accepting and/or auto-opening scripts ? HB
#define HB_AUTO_ACCEPT_OPEN_SCRIPTS 0

//
// Constants
//

// Determine how quickly residents' scripts can issue question dialogs. Allow
// bursts of up to 5 dialogs in 10 seconds. 10*2=20 seconds recovery if
// throttle kicks in
constexpr U32 LLREQUEST_PERMISSION_THROTTLE_LIMIT = 5;			// Requests
constexpr F32 LLREQUEST_PERMISSION_THROTTLE_INTERVAL = 10.f;	// Seconds

// Generic message dispatcher
LLDispatcher gGenericDispatcher;

// Inventory offer throttle globals
LLFrameTimer gThrottleTimer;
constexpr U32 OFFER_THROTTLE_MAX_COUNT = 5;	// Number of items per time period
constexpr F32 OFFER_THROTTLE_TIME = 10.f;	// Time period in seconds

// Agent Update Flags (U8)
constexpr U8 AU_FLAGS_NONE				= 0x00;
constexpr U8 AU_FLAGS_HIDETITLE			= 0x01;
constexpr U8 AU_FLAGS_CLIENT_AUTOPILOT	= 0x02;

// Script permissions
static const std::string SCRIPT_QUESTIONS[SCRIPT_PERMISSION_EOF] = {
	"ScriptTakeMoney",
	"ActOnControlInputs",
	"RemapControlInputs",
	"AnimateYourAvatar",
	"AttachToYourAvatar",
	"ReleaseOwnership",
	"LinkAndDelink",
	"AddAndRemoveJoints",
	"ChangePermissions",
	"TrackYourCamera",
	"ControlYourCamera",
	"TeleportYourAgent",
	"JoinAnExperience",
	"SilentEstateManagement",
	"OverrideAgentAnimations",
	"ScriptReturnObjects",
	"ForceSitAvatar",
	"ChangeEnvSettings"
};

static const bool SCRIPT_QUESTION_IS_CAUTION[SCRIPT_PERMISSION_EOF] = {
	true,   // ScriptTakeMoney
	false,  // ActOnControlInputs
	false,  // RemapControlInputs
	false,  // AnimateYourAvatar
	false,  // AttachToYourAvatar
	false,  // ReleaseOwnership
	false,  // LinkAndDelink
	false,  // AddAndRemoveJoints
	false,  // ChangePermissions
	false,  // TrackYourCamera
	false,  // ControlYourCamera
	false,  // TeleportYourAgent
	false,  // JoinAnExperience
	false,  // SilentEstateManagement
	false,  // OverrideAgentAnimations
	false,  // ScriptReturnObjects
	false,  // ForceSitAvatar
	false	// ChangeEnvSettings
};

///////////////////////////////////////////////////////////////////////////////
// LLKeyThrottle template. It used to be in llcommon/llkeythrottle.h, but since
// it is only used here, there is no point in keeping it in a separate file...
///////////////////////////////////////////////////////////////////////////////

// LLKeyThrottle keeps track of the number of action occurences with a key
// value for a type over a given time period. If the rate set in the
// constructor is exceeed, the key is considered blocked. The transition from
// unblocked to blocked is noted so the responsible agent can be informed. This
// transition takes twice the look back window to clear.

// Forward declaration so LLKeyThrottleImpl can befriend it
template <class T> class LLKeyThrottle;

// Implementation utility class - use LLKeyThrottle, not this
template <class T>
class LLKeyThrottleImpl
{
	friend class LLKeyThrottle<T>;

protected:
	LLKeyThrottleImpl()
	:	mPrevMap(NULL),
		mCurrMap(NULL),
		mCountLimit(0),
		mIntervalLength(1),
		mStartTime(0)
	{
	}

	LL_INLINE static U64 getTime()
	{
		return LLFrameTimer::getTotalTime();
	}

	LL_INLINE static U64 getFrame()		// Return the current frame number
	{
		return (U64)LLFrameTimer::getFrameCount();
	}

protected:
	struct Entry
	{
		Entry()
		:	mCount(0),
			mBlocked(false)
		{
		}

		U32			mCount;
		bool		mBlocked;
	};
	typedef std::map<T, Entry> entry_map_t;

	entry_map_t*	mPrevMap;
	entry_map_t*	mCurrMap;

	// Each map covers this time period (usec or frame number)
	U64				mIntervalLength;

	// Start of the time period (usec or frame number); mCurrMap started
	// counting at this time while mPrevMap covers the previous interval.
	U64				mStartTime;

	// Maximum number of keys allowed per interval
	U32				mCountLimit;
};

template<class T>
class LLKeyThrottle
{
public:
	// realtime = false for frame-based throttle, true for usec real-time
	// throttle
	LLKeyThrottle(U32 limit, F32 interval, bool realtime = true)
	:	mImpl(*new LLKeyThrottleImpl<T>)
	{
		setParameters(limit, interval, realtime);
	}

	~LLKeyThrottle()
	{
		delete mImpl.mPrevMap;
		delete mImpl.mCurrMap;
		delete &mImpl;
	}

	enum State
	{
		THROTTLE_OK,			// Rate not exceeded, let pass
		THROTTLE_NEWLY_BLOCKED,	// Rate exceed for the first time
		THROTTLE_BLOCKED,		// Rate exceed, block key
	};

	F64 getActionCount(const T& id)
	{
		U64 now = 0;
		if (mIsRealtime)
		{
			now = LLKeyThrottleImpl<T>::getTime();
		}
		else
		{
			now = LLKeyThrottleImpl<T>::getFrame();
		}

		if (now >= mImpl.mStartTime + mImpl.mIntervalLength)
		{
			if (now < mImpl.mStartTime + 2 * mImpl.mIntervalLength)
			{
				// Prune old data
				delete mImpl.mPrevMap;
				mImpl.mPrevMap = mImpl.mCurrMap;
				mImpl.mCurrMap =
					new typename LLKeyThrottleImpl<T>::entry_map_t;

				mImpl.mStartTime += mImpl.mIntervalLength;
			}
			else
			{
				// Lots of time has passed, all data is stale
				delete mImpl.mPrevMap;
				delete mImpl.mCurrMap;
				mImpl.mPrevMap =
					new typename LLKeyThrottleImpl<T>::entry_map_t;
				mImpl.mCurrMap =
					new typename LLKeyThrottleImpl<T>::entry_map_t;

				mImpl.mStartTime = now;
			}
		}

		U32 prev_cnt = 0;
		typename LLKeyThrottleImpl<T>::entry_map_t::const_iterator prev =
			mImpl.mPrevMap->find(id);
		if (prev != mImpl.mPrevMap->end())
		{
			prev_cnt = prev->second.mCount;
		}

		typename LLKeyThrottleImpl<T>::Entry& curr = (*mImpl.mCurrMap)[id];

		// Compute current, windowed rate
		F64 time_in_current = (F64)(now - mImpl.mStartTime) /
							  mImpl.mIntervalLength;
		return curr.mCount + prev_cnt * (1.0 - time_in_current);
	}

	// Call each time the key wants use
	State noteAction(const T& id, S32 weight = 1)
	{
		U64 now;
		if (mIsRealtime)
		{
			now = LLKeyThrottleImpl<T>::getTime();
		}
		else
		{
			now = LLKeyThrottleImpl<T>::getFrame();
		}

		if (now >= mImpl.mStartTime + mImpl.mIntervalLength)
		{
			if (now < mImpl.mStartTime + 2 * mImpl.mIntervalLength)
			{
				// Prune old data
				delete mImpl.mPrevMap;
				mImpl.mPrevMap = mImpl.mCurrMap;
				mImpl.mCurrMap =
					new typename LLKeyThrottleImpl<T>::entry_map_t;

				mImpl.mStartTime += mImpl.mIntervalLength;
			}
			else
			{
				// Lots of time has passed, all data is stale
				delete mImpl.mPrevMap;
				delete mImpl.mCurrMap;
				mImpl.mPrevMap =
					new typename LLKeyThrottleImpl<T>::entry_map_t;
				mImpl.mCurrMap =
					new typename LLKeyThrottleImpl<T>::entry_map_t;

				mImpl.mStartTime = now;
			}
		}

		U32 prev_cnt = 0;
		bool prev_blocked = false;
		typename LLKeyThrottleImpl<T>::entry_map_t::const_iterator prev =
			mImpl.mPrevMap->find(id);
		if (prev != mImpl.mPrevMap->end())
		{
			prev_cnt = prev->second.mCount;
			prev_blocked = prev->second.mBlocked;
		}

		typename LLKeyThrottleImpl<T>::Entry& curr = (*mImpl.mCurrMap)[id];

		// curr.mCount is the number of keys in this current 'time slice' from
		// the beginning of it until now prev_cnt is the number of keys in the
		// previous time slice scaled to be one full time slice back from the
		// current (now) time.
		curr.mCount += weight;

		// Compute current, windowed rate
		F64 time_in_current = ((F64)(now - mImpl.mStartTime) /
							  mImpl.mIntervalLength);
		F64 average_cnt = curr.mCount + prev_cnt * (1.0 - time_in_current);

		bool was_blocked = curr.mBlocked;
		curr.mBlocked |= average_cnt > mImpl.mCountLimit;

		if (!prev_blocked && !curr.mBlocked)
		{
			return THROTTLE_OK;
		}
		if (!prev_blocked && !was_blocked)
		{
			return THROTTLE_NEWLY_BLOCKED;
		}
		return THROTTLE_BLOCKED;
	}

	// Call to force throttle conditions for id
	void throttleAction(const T& id)
	{
		noteAction(id);
		typename LLKeyThrottleImpl<T>::Entry& curr = (*mImpl.mCurrMap)[id];
		curr.mCount = llmax(mImpl.mCountLimit, curr.mCount);
		curr.mBlocked = true;
	}

	// Returns true if key is blocked
	bool isThrottled(const T& id) const
	{
		if (mImpl.mCurrMap->empty() && mImpl.mPrevMap->empty())
		{
			// Most of the time we will fall in here
			return false;
		}

		// NOTE, we ignore the case where id is in the map but the map is
		// stale. You might think that we'd stop throttling things in such a
		// case, however it may be that a "god" has disabled scripts in the
		// region or estate and we probably want to report the state of the Id
		// when the scripting engine was paused.
		typename LLKeyThrottleImpl<T>::entry_map_t::const_iterator entry =
			mImpl.mCurrMap->find(id);
		if (entry != mImpl.mCurrMap->end())
		{
			return entry->second.mBlocked;
		}

		entry = mImpl.mPrevMap->find(id);
		return entry != mImpl.mPrevMap->end() && entry->second.mBlocked;
	}

	// Gets the throttling parameters
	void getParameters(U32& out_limit, F32& out_interval, bool& out_realtime)
	{
		out_limit = mImpl.mCountLimit;
		out_interval = mImpl.mIntervalLength;
		out_realtime = mIsRealtime;
	}

	// Sets the throttling behavior
	void setParameters(U32 limit, F32 interval, bool realtime = true)
	{
		// 'limit' is the maximum number of keys allowed per interval (in
		// seconds or frames)
		mIsRealtime = realtime;
		mImpl.mCountLimit = limit;
		if (mIsRealtime)
		{
			mImpl.mIntervalLength = (U64)(interval * USEC_PER_SEC);
			mImpl.mStartTime = LLKeyThrottleImpl<T>::getTime();
		}
		else
		{
			mImpl.mIntervalLength = (U64)interval;
			mImpl.mStartTime = LLKeyThrottleImpl<T>::getFrame();
		}

		if (mImpl.mIntervalLength == 0)
		{
			// Do not allow zero intervals
			mImpl.mIntervalLength = 1;
		}

		delete mImpl.mPrevMap;
		mImpl.mPrevMap = new typename LLKeyThrottleImpl<T>::entry_map_t;
		delete mImpl.mCurrMap;
		mImpl.mCurrMap = new typename LLKeyThrottleImpl<T>::entry_map_t;
	}

protected:
	LLKeyThrottleImpl<T>&	mImpl;

	// true to be time based (default), FALSE for frame based:
	bool					mIsRealtime;
};

///////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////

void give_money(const LLUUID& uuid, LLViewerRegion* region, S32 amount,
				bool is_group, S32 trx_type, const std::string& desc)
{
	if (amount <= 0 || !region) return;

	if (uuid.isNull())
	{
		llwarns << "Cannot give money to to null UUID target !" << llendl;
		return;
	}

	llinfos << "give_money(" << uuid << "," << amount << ")"<< llendl;
	if (can_afford_transaction(amount))
	{
#if 0
		gStatusBarp->debitBalance(amount);
#endif
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_MoneyTransferRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_MoneyData);
		msg->addUUIDFast(_PREHASH_SourceID, gAgentID);
		msg->addUUIDFast(_PREHASH_DestID, uuid);
		msg->addU8Fast(_PREHASH_Flags,
					   pack_transaction_flags(false, is_group));
		msg->addS32Fast(_PREHASH_Amount, amount);
		msg->addU8Fast(_PREHASH_AggregatePermNextOwner,
					   (U8)LLAggregatePermissions::AP_EMPTY);
		msg->addU8Fast(_PREHASH_AggregatePermInventory,
					   (U8)LLAggregatePermissions::AP_EMPTY);
		msg->addS32Fast(_PREHASH_TransactionType, trx_type);
		msg->addStringFast(_PREHASH_Description, desc);
		msg->sendReliable(region->getHost());
	}
	else
	{
		LLFloaterBuyCurrency::buyCurrency("Giving", amount);
	}
}

void send_complete_agent_movement(const LLHost& sim_host)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_CompleteAgentMovement);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addU32Fast(_PREHASH_CircuitCode, msg->mOurCircuitCode);
	msg->sendReliable(sim_host);

	// Close any old notification about a restarting sim
	LLNotifyBox::closeLastNotifyRestart();

	// Inform interested floaters that we arrived in a new region (no need to
	// use boost signals or any other complex mechanism since these are static
	// methods used to clear static caches not depending on an actually open
	// and live floater).
	HBFloaterAreaSearch::newRegion();
	HBFloaterSoundsList::newRegion();
}

void process_logout_reply(LLMessageSystem* msg, void**)
{
	// The server has told us it is ok to quit.
	LL_DEBUGS("Messaging") << "Logout reply" << LL_ENDL;

	LLUUID agent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID session_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	if (agent_id != gAgentID || session_id != gAgentSessionID)
	{
		llwarns << "Bogus Logout Reply" << llendl;
	}

	LLInventoryModel::update_map_t parents;
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_InventoryData);
	for (S32 i = 0; i < count; ++i)
	{
		LLUUID item_id;
		msg->getUUIDFast(_PREHASH_InventoryData, _PREHASH_ItemID, item_id, i);

		if (count == 1 && item_id.isNull())
		{
			// Detect dummy item.  Indicates an empty list.
			break;
		}

		// We do not need to track the asset ids, just account for an updated
		// inventory version.
		llinfos << "Processing item: " << item_id << llendl;
		LLInventoryItem* item = gInventory.getItem(item_id);
		if (item)
		{
			parents[item->getParentUUID()] = 0;
			gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
		}
		else
		{
			llinfos << "Item not found: " << item_id << llendl;
		}
	}
	gAppViewerp->forceQuit();
}

void process_layer_data(LLMessageSystem* msg, void**)
{
	LLViewerRegion* regionp;
	regionp = gWorld.getRegion(msg->getSender());
	if (!regionp)
	{
		return;
	}

	S8 type;
	msg->getS8Fast(_PREHASH_LayerID, _PREHASH_Type, type);
	S32 size = msg->getSizeFast(_PREHASH_LayerData, _PREHASH_Data);
	if (size == 0)
	{
		llwarns << "Layer data has zero size." << llendl;
		return;
	}
	if (size < 0)
	{
		// getSizeFast() is probably trying to tell us about an error
		llwarns << "getSizeFast() returned negative result: " << size
				<< llendl;
		return;
	}

	if (type == CLOUD_LAYER_CODE)
	{
		static LLCachedControl<bool> sparse_clouds(gSavedSettings,
												   "SparseClassicClouds");
		if (!LLCloudLayer::needClassicClouds())
		{
			// The user does not want classic clouds, or the clouds are past
			// the draw distance.
			regionp->mCloudLayer.resetDensity();
			return;
		}
		else if (sparse_clouds && !regionp->mCloudLayer.shouldUpdateDensity())
		{
			// We already updated this region's layer during the past second
			// and the user wishes to sparse update messages.
			return;
		}
	}

	U8* datap = new U8[size];
	msg->getBinaryDataFast(_PREHASH_LayerData, _PREHASH_Data, datap, size);
	LLVLData* vl_datap = new LLVLData(regionp, type, datap, size);
	if (msg->getReceiveCompressedSize())
	{
		gVLManager.addLayerData(vl_datap, msg->getReceiveCompressedSize());
	}
	else
	{
		gVLManager.addLayerData(vl_datap, msg->getReceiveSize());
	}

	if (!regionp->mGotClouds)
	{
		if (type == CLOUD_LAYER_CODE)
		{
			// The server is providing us with cloud data for this region.
			regionp->mGotClouds = true;
		}
		else if (type == WIND_LAYER_CODE)
		{
			if (!LLCloudLayer::needClassicClouds())
			{
				// The user does not want classic clouds or the clouds are past
				// the draw distance: remove them.
				regionp->mCloudLayer.resetDensity();
			}
			else if (regionp->mFirstWindLayerReceivedTime == 0.f)
			{
				// Remember the time when we first received a wind layer data
				// packet
				regionp->mFirstWindLayerReceivedTime = gFrameTimeSeconds;
			}
			else if (gFrameTimeSeconds -
					 regionp->mFirstWindLayerReceivedTime >= 3.f)
			{
				// Over three seconds elapsed since the fist wind data layer
				// was received and we still did not get any cloud layer data;
				// the server is obviously not sending classic clouds data...
				// Generate or update the random cloud cover probability matrix
				// at each new wind layer data.
				regionp->mCloudLayer.generateDensity();
			}
		}
	}
}

void process_derez_ack(LLMessageSystem*, void**)
{
	if (gWindowp)
	{
		gWindowp->decBusyCount();
	}
}

void process_places_reply(LLMessageSystem* msg, void** data)
{
	LLUUID query_id;

	msg->getUUID(_PREHASH_AgentData, _PREHASH_QueryID, query_id);
	if (query_id.isNull())
	{
		LLFloaterLandHoldings::processPlacesReply(msg, data);
	}
	else if (gAgent.isInGroup(query_id))
	{
		LLPanelGroupLandMoney::processPlacesReply(msg, data);
	}
	else
	{
		llwarns << "Got invalid PlacesReply message" << llendl;
	}
}

void send_sound_trigger(const LLUUID& sound_id, F32 gain)
{
	if (sound_id.isNull() || !gAgent.getRegion())
	{
		// Disconnected agent or zero guids do not get sent (no sound)
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_SoundTrigger);
	msg->nextBlockFast(_PREHASH_SoundData);
	msg->addUUIDFast(_PREHASH_SoundID, sound_id);
	// Client untrusted, ids set on sim
	msg->addUUIDFast(_PREHASH_OwnerID, LLUUID::null);
	msg->addUUIDFast(_PREHASH_ObjectID, LLUUID::null);
	msg->addUUIDFast(_PREHASH_ParentID, LLUUID::null);

	msg->addU64Fast(_PREHASH_Handle, gAgent.getRegionHandle());

	LLVector3 position = gAgent.getPositionAgent();
	msg->addVector3Fast(_PREHASH_Position, position);
	msg->addF32Fast(_PREHASH_Gain, gain);

	gAgent.sendMessage();
}

//-----------------------------------------------------------------------------
// Instant Message
//-----------------------------------------------------------------------------
class LLOpenAgentOffer final : public LLInventoryFetchObserver
{
protected:
	LOG_CLASS(LLOpenAgentOffer);

public:
	LLOpenAgentOffer(const std::string& from_name)
	:	mFromName(from_name),
		mRetried(false)
	{
	}

	void fetchItems(const uuid_vec_t& ids) override
	{
		mFetchedItems = ids;
		LLInventoryFetchObserver::fetchItems(ids);
	}

	void done() override
	{
		size_t incomplete = mIncomplete.size();
		if (incomplete)
		{
			llwarns << "Incomplete fetch for " << incomplete << " items."
					<< llendl;
		}
		uuid_vec_t* completed_vecp = &mComplete;
		if (mComplete.empty() && incomplete != mFetchedItems.size())
		{
			llwarns << "Observer for " << mFromName
					<< "'s offer done with empty completed items list."
					<< llendl;
			if (!mRetried)
			{
				llinfos << "Retrying offered items fetch for "
						<< mFromName << llendl;
				mRetried = true;
				LLInventoryFetchObserver::fetchItems(mFetchedItems);
				return;
			}
			llinfos << "Trying to open items nonetheless for " << mFromName
					<< llendl;
			completed_vecp = &mFetchedItems;
		}
		open_inventory_offer(*completed_vecp, mFromName);
		gInventory.removeObserver(this);
		delete this;
	}

private:
	uuid_vec_t	mFetchedItems;
	std::string mFromName;
	bool		mRetried;
};

// Unlike the FetchObserver for AgentOffer, we only make one instance of the
// AddedObserver for TaskOffers and it never dies. We do this because we do not
// know the UUID of task offers until they are accepted, we do not know what to
// watch for, so instead we just watch for all additions.
class LLOpenTaskOffer : public LLInventoryAddedObserver
{
protected:
	void done() override
	{
		open_inventory_offer(mAdded, "added inventory observer");
		mAdded.clear();
	}
};

// One global task offer observer instance to bind them
LLOpenTaskOffer* gNewInventoryObserverp = NULL;

void start_new_inventory_observer()
{
	if (!gNewInventoryObserverp)
	{
		// Observer is deleted by gInventory
		gNewInventoryObserverp = new LLOpenTaskOffer;
		gInventory.addObserver(gNewInventoryObserverp);
	}
}

void stop_new_inventory_observer()
{
	if (gNewInventoryObserverp)
	{
		gInventory.removeObserver(gNewInventoryObserverp);
		delete gNewInventoryObserverp;
		gNewInventoryObserverp = NULL;
	}
}

class LLDiscardAgentOffer final : public LLInventoryFetchComboObserver
{
protected:
	LOG_CLASS(LLDiscardAgentOffer);

public:
	LLDiscardAgentOffer(const LLUUID& folder_id, const LLUUID& object_id)
	:	mFolderID(folder_id),
		mObjectID(object_id)
	{
	}

	void done() override
	{
		LL_DEBUGS("InventoryOffer") << "Discard done, Scheduling removal of item: "
									<< mObjectID << LL_ENDL;
		// We are invoked from LLInventoryModel::notifyObservers(); should we
		// try to remove the inventory item now, it would cause a nested call
		// to notifyObservers() call, which would not work. So defer moving the
		// item to trash until viewer gets idle (in a moment).
		// Note: I migrated this code from the now removed LLDeferredTaskList
		// mechanism (that was only used here) to standard idle callbacks.
		doOnIdleOneTime(boost::bind(&LLDiscardAgentOffer::oneShotIdleCallback,
									this));
		gInventory.removeObserver(this);
	}

private:
	void oneShotIdleCallback()
	{
		LL_DEBUGS("InventoryOffer") << "Removing item: " << mObjectID
									<< LL_ENDL;
		gInventory.removeItem(mObjectID);
		// Commit suicide.
		delete this;
	}

private:
	LLUUID mFolderID;
	LLUUID mObjectID;
};

// Returns true if we are OK, false if we are throttled. Set check_only to true
// if you want to know the throttle status without registering a hit
bool check_offer_throttle(const std::string& from_name, bool check_only)
{
	static U32 throttle_count;
	static bool throttle_logged;
	LLChat chat;
	std::string log_message;

	if (!gSavedSettings.getBool("ShowNewInventory"))
	{
		return false;
	}

	if (check_only)
	{
		return gThrottleTimer.hasExpired();
	}

	if (gThrottleTimer.checkExpirationAndReset(OFFER_THROTTLE_TIME))
	{
		LL_DEBUGS("InventoryOffer") << "Throttle expired." << LL_ENDL;
		throttle_count = 1;
		throttle_logged = false;
		return true;
	}
	else	// Has not yet expired
	{
		LL_DEBUGS("InventoryOffer") << "Throttle not expired, count: "
									<< throttle_count << LL_ENDL;
		// When downloading the initial inventory we get a lot of new items
		// coming in and cannot tell that from spam.
		if (LLStartUp::isLoggedIn() &&
			throttle_count >= OFFER_THROTTLE_MAX_COUNT)
		{
			if (!throttle_logged)
			{
				// Use the name of the last item giver, who is probably the
				// person spamming you.
				std::ostringstream message;
				message << gSecondLife;
				if (!from_name.empty())
				{
					message << ": Items coming in too fast from " << from_name;
				}
				else
				{
					message << ": Items coming in too fast";
				}
				message << ", automatic preview disabled for "
						<< OFFER_THROTTLE_TIME << " seconds.";
				chat.mText = message.str();
				// This is relatively important, so actually put it on screen
				LLFloaterChat::addChat(chat, false, false);
				throttle_logged = true;
			}
			return false;
		}
		else
		{
			++throttle_count;
			return true;
		}
	}
}

void open_inventory_offer(const uuid_vec_t& items,
						  const std::string& from_name)
{
	LL_DEBUGS("InventoryOffer") << "Offer from: " << from_name
								<< " - Number of items to process: "
								<< items.size() << LL_ENDL;
	if (items.empty())
	{
		return;
	}
	const LLUUID& trash_id = gInventory.getTrashID();
	const LLUUID& laf_id = gInventory.getLostAndFoundID();
	bool user_is_away = gAwayTimer.getStarted();
	bool throttled = false;
	bool show_new_inventory = gSavedSettings.getBool("ShowInInventory");
	LLUUID show_item;
	for (U32 i = 0, count = items.size(); i < count; ++i)
	{
		const LLUUID& item_id = items[i];
		// NOTE: this *must* be LLViewerInventoryItem and NOT LLInventoryItem,
		// because we must call the proper virtual method for the tests using
		// getInventoryType() below. HB 
		LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
		if (!itemp)
		{
			// This could be a folder, which we do not care about. HB
			if (!gInventory.getCategory(item_id))
			{
				llinfos << "Received item " << item_id
						<< " not found in inventory... Temporary attachment ?"
						<< llendl;
			}
			continue;
		}
		if (itemp->getIsLinkType() ||	// Ignore created links. HB
			gInventory.isObjectDescendentOf(item_id, trash_id))
		{
			continue;
		}

		LLAssetType::EType asset_type = itemp->getType();
		LL_DEBUGS("InventoryOffer") << "Checking auto-open condition for item: "
									<< item_id << " - Asset type: "
									<< asset_type << LL_ENDL;
		if (asset_type == LLAssetType::AT_NOTECARD ||
#if HB_AUTO_ACCEPT_OPEN_SCRIPTS
			asset_type == LLAssetType::AT_LSL_TEXT ||
#endif
			asset_type == LLAssetType::AT_LANDMARK ||
			asset_type == LLAssetType::AT_TEXTURE ||
#if 0		// *TODO: after PBR editor is implemented. HB
			asset_type == LLAssetType::AT_MATERIAL ||
#endif
			asset_type == LLAssetType::AT_SOUND ||
			asset_type == LLAssetType::AT_ANIMATION)
		{
			llinfos << "Auto-opening item: " << item_id << llendl;
			if (check_offer_throttle(from_name, throttled))
			{
				switch (asset_type)
				{
					case LLAssetType::AT_NOTECARD:
						open_notecard(itemp, "Note: " + itemp->getName(), true,
									  LLUUID::null, false);
						break;

#if HB_AUTO_ACCEPT_OPEN_SCRIPTS
					case LLAssetType::AT_LSL_TEXT:
						open_script(item_id, "Script: " + itemp->getName(),
									false);
						break;
#endif

					case LLAssetType::AT_LANDMARK:
						open_landmark(itemp, "Landmark: " + itemp->getName(),
									  true, false);
						break;

					case LLAssetType::AT_TEXTURE:
						open_texture(item_id, "Texture: " + itemp->getName(),
									 true, LLUUID::null, false);
						break;

#if 0		// *TODO: after PBR editor is implemented. HB
					case LLAssetType::AT_MATERIAL:
						// Implement open_material() and call it here.
						break;
#endif

					case LLAssetType::AT_SOUND:
						open_sound(item_id, "Sound: " + itemp->getName(),
								   LLUUID::null, false);
						break;

					case LLAssetType::AT_ANIMATION:
						open_animation(item_id,
									   "Animation: " + itemp->getName(), 0,
									   LLUUID::null, false);
						break;

					default:
						break;
				}
			}
			else	// If we are throttled, do not display them
			{
				// Only do a simple check for next time, without spamming in
				// chat about the throttling...
				throttled = true;
			}
		}

		// Do not show item if not asked, or if the originator name is empty,
		// or when the item is a calling card.
		if (!show_new_inventory || from_name.empty() ||
			asset_type == LLAssetType::AT_CALLINGCARD)
		{
			continue;
		}
		// Do not show when the item is a newly attached object, or newly worn
		// wearable, or newly activated gesture since *existing* inventory
		// items are reported as "new" when attached/worn/activated. HB
		LLInventoryType::EType type = itemp->getInventoryType();
		if (type == LLInventoryType::IT_ATTACHMENT ||
			type == LLInventoryType::IT_WEARABLE ||
			type == LLInventoryType::IT_GESTURE)
		{
			continue;
		}
		// Do not select lost and found items if the user is active
		if (!user_is_away && gInventory.isObjectDescendentOf(item_id, laf_id))
		{
			continue;
		}
		// Store the item UUID for later.
		show_item = item_id;

		LL_DEBUGS("InventoryOffer") << "Auto-show registered for item: "
									<< item_id << LL_ENDL;
	}

	if (show_item.isNull())
	{
		return;
	}

	LLFloaterInventory::showAgentInventory();
	LLFloaterInventory* floaterp = LLFloaterInventory::getActiveFloater();
	if (floaterp)
	{
		// Highlight item
		LL_DEBUGS("InventoryOffer") << "Showing item: " << show_item
									<< LL_ENDL;
		LLFocusableElement* focus_ctrl = gFocusMgr.getKeyboardFocus();
		floaterp->getPanel()->setSelection(show_item, TAKE_FOCUS_NO);
		gFocusMgr.setKeyboardFocus(focus_ctrl);
	}
}

// Purge the message queue of any previously queued inventory offers from the
// same source.
class OfferMatcher : public LLNotifyBoxView::Matcher
{
public:
	OfferMatcher(const LLUUID& to_block)
	:	mBlockedId(to_block)
	{
	}

	bool matches(const LLNotificationPtr notif) const
	{
		const std::string& name = notif->getName();
		if (name == "ObjectGiveItem" || name == "ObjectGiveItemOurs" ||
			name == "ObjectGiveItemUnknownUser" || name == "UserGiveItem")
		{
			return notif->getPayload()["from_id"].asUUID() == mBlockedId;
		}
		return false;
	}

private:
	LLUUID mBlockedId;
};

void inventory_offer_mute_callback(const LLUUID& blocked_id,
								   const std::string& full_name, bool is_group)
{
	std::string from_name = full_name;
	LLMute::EType type;

	if (is_group)
	{
		type = LLMute::GROUP;
	}
	else
	{
		type = LLMute::AGENT;
	}

	LLMute mute(blocked_id, from_name, type);
	if (LLMuteList::add(mute))
	{
		LLFloaterMute::selectMute(mute.mID);
	}

	gNotifyBoxViewp->purgeMessagesMatching(OfferMatcher(blocked_id));
}

LLOfferInfo::LLOfferInfo(const LLSD& sd)
{
	mIM = (EInstantMessage)sd["im_type"].asInteger();
	mFromID = sd["from_id"].asUUID();
	mLogInChat = !sd.has("log_in_chat") || sd["log_in_chat"].asBoolean();
	mFromGroup = sd["from_group"].asBoolean();
	mFromObject = sd["from_object"].asBoolean();
	mTransactionID = sd["transaction_id"].asUUID();
	mFolderID = sd["folder_id"].asUUID();
	mObjectID = sd["object_id"].asUUID();
	mType = LLAssetType::lookup(sd["type"].asString().c_str());
	mFromName = sd["from_name"].asString();
	mDesc = sd["description"].asString();
	if (sd.has("slurl"))
	{
		mSLURL = sd["slurl"].asString();
	}
	else
	{
		extractSLURL();
	}
	mHost = LLHost(sd["sender"].asString());
}

LLOfferInfo::LLOfferInfo(const LLOfferInfo& other)
{
	mIM = other.mIM;
	mFromID = other.mFromID;
	mLogInChat = other.mLogInChat;
	mFromGroup = other.mFromGroup;
	mFromObject = other.mFromObject;
	mTransactionID = other.mTransactionID;
	mFolderID = other.mFolderID;
	mObjectID = other.mObjectID;
	mType = other.mType;
	mFromName = other.mFromName;
	mDesc = other.mDesc;
	mHost = other.mHost;
}

LLSD LLOfferInfo::asLLSD()
{
	LLSD sd;
	sd["im_type"] = mIM;
	sd["from_id"] = mFromID;
	sd["log_in_chat"] = mLogInChat;
	sd["from_group"] = mFromGroup;
	sd["from_object"] = mFromObject;
	sd["transaction_id"] = mTransactionID;
	sd["folder_id"] = mFolderID;
	sd["object_id"] = mObjectID;
	sd["type"] = LLAssetType::lookup(mType);
	sd["from_name"] = mFromName;
	sd["description"] = mDesc;
	sd["slurl"] = mSLURL;
	sd["sender"] = mHost.getIPandPort();
	return sd;
}

void LLOfferInfo::extractSLURL()
{
	std::string msg = mDesc;
	size_t i = msg.find("http://");
	if (i != std::string::npos)
	{
		// Remove the SLURL from mDesc
		LLStringUtil::truncate(mDesc, i);
		// Remember the SLURL
		mSLURL = msg.substr(i);
		// Also strip the opening parenthesis from mDesc. Note that the message
		// used to be "... (slurl)" in old servers, and now is "... ( slurl )":
		// make it so both cases are covered, just in the event things would
		// change again...
		i = mDesc.rfind('(');
		if (i != std::string::npos)
		{
			LLStringUtil::truncate(mDesc, i);
			LLStringUtil::trimTail(mDesc);
		}
		// Strip the closing parenthesis and possible trailing space from mSLURL
		i = mSLURL.rfind(')');
		if (i != std::string::npos)
		{
			LLStringUtil::truncate(mSLURL, i);
			LLStringUtil::trimTail(mSLURL);
		}
	}
}

void LLOfferInfo::sendReceiveResponse(bool accept)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg)
	{
		return;
	}

	msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_MessageBlock);
	msg->addBoolFast(_PREHASH_FromGroup, false);
	msg->addUUIDFast(_PREHASH_ToAgentID, mFromID);
	msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
	msg->addUUIDFast(_PREHASH_ID, mTransactionID);
	msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP); // No timestamp needed
	std::string name;
	gAgent.buildFullname(name);
	msg->addStringFast(_PREHASH_FromAgentName, name);
	msg->addStringFast(_PREHASH_Message, "");
	msg->addU32Fast(_PREHASH_ParentEstateID, 0);
	msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
	msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());

	if (accept)
	{
		// ACCEPT. The math for the dialog works, because the accept for
		// inventory_offered, task_inventory_offer or group_notice_inventory is
		// 1 greater than the offer integer value.
		// Generates IM_INVENTORY_ACCEPTED, IM_TASK_INVENTORY_ACCEPTED, or
		// IM_GROUP_NOTICE_INVENTORY_ACCEPTED.
		msg->addU8Fast(_PREHASH_Dialog, (U8)(mIM + 1));
		msg->addBinaryDataFast(_PREHASH_BinaryBucket, &(mFolderID.mData),
							   sizeof(mFolderID.mData));
	}
	else
	{
		// Decline for inventory_offered, task_inventory_offer or
		// group_notice_inventory is 2 greater than the offer integer value.
		msg->addU8Fast(_PREHASH_Dialog, (U8)(mIM + 2));
		msg->addBinaryDataFast(_PREHASH_BinaryBucket, EMPTY_BINARY_BUCKET,
							   EMPTY_BINARY_BUCKET_SIZE);
	}
	// Send the message
	msg->sendReliable(mHost);
}

bool LLOfferInfo::inventoryOfferCallback(const LLSD& notification,
										 const LLSD& response)
{
	LLChat chat;
	std::string log_message;
	S32 button = LLNotification::getSelectedOption(notification, response);

	// For muting, we need to add the mute, then decline the offer.
	// This must be done here because:
	// * callback may be called immediately,
	// * adding the mute sends a message,
	// * we cannot build two messages at once.
	if (button == IOR_MUTE && gCacheNamep)
	{
		gCacheNamep->get(mFromID, mFromGroup, inventory_offer_mute_callback);
	}

	LLInventoryObserver* opener = NULL;
	LLViewerInventoryCategory* catp =
		(LLViewerInventoryCategory*)gInventory.getCategory(mObjectID);
	LLViewerInventoryItem* itemp = NULL;
	if (!catp)
	{
		itemp = (LLViewerInventoryItem*)gInventory.getItem(mObjectID);
	}

	// *TODO:translate
	std::string from_string;	// Used in the pop-up.
	std::string chat_history;	// Used in chat history.
	if (mFromObject)
	{
		from_string = "An object named '" + mFromName + "'";
		chat_history = mFromName;
		if (!mSLURL.empty()
//MK
			&& !(gRLenabled && gRLInterface.mContainsShowloc))
//mk
		{
			chat_history += " (" + mSLURL + ")";
		}

		std::string owner_info;
		if (mFromGroup)
		{
			std::string group_name;
			if (gCacheNamep && gCacheNamep->getGroupName(mFromID, group_name))
			{
				owner_info = " owned by the group '" + group_name + "'";
			}
			else
			{
				owner_info = " owned by an unknown group";
			}
		}
		else
		{
			std::string first_name, last_name;
			if (gCacheNamep &&
				gCacheNamep->getName(mFromID, first_name, last_name))
			{
				owner_info = " owned by " + first_name + " " + last_name;
			}
			else
			{
				owner_info = " owned by an unknown user";
			}
		}
		from_string += owner_info;
		chat_history += owner_info;
	}
	else
	{
		from_string = chat_history = mFromName;
	}

	bool busy = false;

//MK
	std::string folder_name = mDesc;
	if (gRLenabled)
	{
		// mDesc looks like '#RLV/~foldername'
		// => we need to parse in order to find the folder name
		size_t i1 = folder_name.find("'");
		size_t i2 = folder_name.rfind("'");
		if (i1 != std::string::npos && i2 > i1 + 1)
		{
			folder_name = folder_name.substr(i1 + 1, i2 - 1);
		}

		if (gRLInterface.mContainsShownames ||
			gRLInterface.mContainsShownametags)
		{
			chat_history = gRLInterface.getDummyName(chat_history);
		}
#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
		// Remember received folder name
		if (folder_name.find(RL_RLV_REDIR_FOLDER_PREFIX) != 0)
		{
			gRLInterface.mReceivedInventoryFolders.emplace(folder_name);
		}
#endif
	}
//mk

	switch (button)
	{
		case IOR_ACCEPT:
			sendReceiveResponse(true);

			// Do not spam them if they are getting flooded
			if (check_offer_throttle(mFromName, true))
			{
				log_message = chat_history + " gave you " + mDesc + ".";
 				chat.mText = log_message;
 				LLFloaterChat::addChatHistory(chat);
			}

			// We will want to open this item when it comes back.
			LL_DEBUGS("InventoryOffer") << "Initializing an opener for tid: "
										<< mTransactionID << LL_ENDL;
			switch (mIM)
			{
				case IM_INVENTORY_OFFERED:
				{
					LL_DEBUGS("InventoryOffer") << "Offer accepted."
												<< LL_ENDL;
					// This is an offer from an agent. In this case, the
					// backend has already copied the items into your
					// inventory, so we can fetch it out of our inventory.
					uuid_vec_t items;
					items.emplace_back(mObjectID);
					if (catp || (itemp && itemp->isFinished()))
					{
						open_inventory_offer(items, from_string);
					}
					else
					{
						LLOpenAgentOffer* open_agent_offer =
							new LLOpenAgentOffer(from_string);
						open_agent_offer->fetchItems(items);
						opener = open_agent_offer;
					}
					break;
				}

				case IM_TASK_INVENTORY_OFFERED:
				case IM_GROUP_NOTICE:
				case IM_GROUP_NOTICE_REQUESTED:
					// This is an offer from a task or group. We do not use a
					// new instance of an opener. We instead use the singular
					// observer LLOpenTaskOffer. Since it already exists, we do
					// not need to actually do anything
					LL_DEBUGS("InventoryOffer") << "Routed via LLOpenTaskOffer"
												<< LL_ENDL;
					break;

				default:
					llwarns << "Unknown offer type: " << mIM << llendl;
			}
//MK
			if (gRLenabled)
			{
				std::string report;
				if (gRLInterface.getRlvShare() &&
					mFolderID == gRLInterface.getRlvShare()->getUUID())
				{
					report = "accepted_in_rlv inv_offer ";
				}
				else
				{
					report = "accepted_in_inv inv_offer ";
				}
				gRLInterface.notify(report + folder_name);

#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
				// Remember received folder name
				gRLInterface.mReceivedInventoryFolders.emplace(folder_name);
#endif
			}
//mk
			break;

		case IOR_BUSY:
			// Busy falls through to decline. Says to make busy message.
			busy = true;
		case IOR_MUTE:
		case IOR_MUTED:
			// MUTE falls through to decline
		case IOR_DECLINE:
		default:
			LL_DEBUGS("InventoryOffer") << "Offer declined." << LL_ENDL;
			// Close button probably (or any of the fall-throughs from above)
			sendReceiveResponse(false);

			if (mLogInChat && button != IOR_MUTED)
			{
				log_message = "You decline " + mDesc + " from " + mFromName;
				if (!mSLURL.empty()
//MK
					&& !(gRLenabled && gRLInterface.mContainsShowloc))
//mk
				{
					log_message += " (" + mSLURL + ")";
				}
				chat.mText = log_message + ".";
				LLFloaterChat::addChatHistory(chat);
			}

			// If it is from an agent, we have to fetch the item to throw it
			// away. If it is from a task or group, just denying the request
			// will suffice to discard the item.
			if (mIM == IM_INVENTORY_OFFERED)
			{
				uuid_vec_t folders, items;
				items.emplace_back(mObjectID);
				LLDiscardAgentOffer* discard_agent_offer =
					new LLDiscardAgentOffer(mFolderID, mObjectID);
				discard_agent_offer->fetch(folders, items);
				if ((catp && gInventory.isCategoryComplete(mObjectID)) ||
					(itemp && itemp->isFinished()))
				{
					discard_agent_offer->done();
				}
				else
				{
					opener = discard_agent_offer;
				}
			}
			if (busy && !mFromGroup && !mFromObject)
			{
				busy_message(mFromID);
			}
//MK
			if (gRLenabled)
			{
				gRLInterface.notify("declined inv_offer " + folder_name);
			}
//mk
	}

	if (opener)
	{
		gInventory.addObserver(opener);
	}

	// Allow these to stack up, but once you deal with one, reset the position.
	if (gFloaterViewp)
	{
		gFloaterViewp->resetStartingFloaterPosition();
	}

	delete this;
	return false;
}

void LLOfferInfo::inventoryOfferHandler()
{
	bool muted = false;
	std::string name;
	bool name_found = false;
	if (mFromObject)
	{
		// Name cache callbacks do not store userdata, so cannot save off the
		// LLOfferInfo. Argh.
		if (mFromGroup)
		{
			if (gCacheNamep && gCacheNamep->getGroupName(mFromID, name))
			{
				name_found = true;
			}
		}
		else if (gCacheNamep && gCacheNamep->getFullName(mFromID, name))
		{
			name_found = true;
		}

		// Search for mutes by object name (the object UUID is alas unknown)
		muted = LLMuteList::isMuted(LLUUID::null, mFromName, 0,
									LLMute::OBJECT);
		if (!muted)
		{
			if (name_found)
			{
				// Search for mutes by owner's group or agent UUID and name
				muted = LLMuteList::isMuted(mFromID, name);
			}
			else
			{
				// Search for mutes by owner's group or agent UUID
				muted = LLMuteList::isMuted(mFromID);
			}
		}
	}
	else
	{
		name = mFromName;
		if (LLAvatarName::sOmitResidentAsLastName)
		{
			name =  LLCacheName::cleanFullName(name);
		}

		// Search for mutes by group or agent id or name
		muted = LLMuteList::isMuted(mFromID, name);
	}

	// If muted, do not even go through the messaging stuff. Just curtail the
	// offer here.
	if (muted)
	{
		static F32 last_notification = 0.f;
		// Do not spam with such messages...
		llinfos_once << "Declining inventory offer from muted object/agent: "
					 << mFromName << llendl;
		if (gFrameTimeSeconds - last_notification > 30.f)
		{
			LLSD args;
			args["NAME"] = mFromName;
			gNotifications.add("MutedObjectOfferDeclined", args);
			last_notification = gFrameTimeSeconds;
		}
		// Not IOR_MUTE, since this would auto-mute agents owning an object we
		// muted...
		forceResponse(IOR_MUTED);
		return;
	}

	// Avoid the Accept/Discard dialog if the user so desires. JC
	if (gSavedSettings.getBool("AutoAcceptNewInventory") &&
		(mType == LLAssetType::AT_NOTECARD ||
#if HB_AUTO_ACCEPT_OPEN_SCRIPTS
		 mType == LLAssetType::AT_LSL_TEXT ||
#endif
		 mType == LLAssetType::AT_LANDMARK ||
		 mType == LLAssetType::AT_TEXTURE ||
		 mType == LLAssetType::AT_SOUND ||
		 mType == LLAssetType::AT_ANIMATION))
	{
		LL_DEBUGS("InventoryOffer") << "Auto accepting offer." << LL_ENDL;
		// For certain types, just accept the items into the inventory and
		// possibly open them on receipt depending upon "ShowNewInventory".
		forceResponse(IOR_ACCEPT);
		return;
	}

	LLSD args;
	args["OBJECTNAME"] = mDesc;

	LLSD payload;

	// Must protect against a NULL return from lookupHumanReadable()
	std::string typestr = ll_safe_string(LLAssetType::lookupHumanReadable(mType));
	if (typestr.empty())
	{
		llwarns << "Bad/unknown asset type: " << mType << llendl;
		args["OBJECTTYPE"] = "";

		// This seems safest, rather than propagating bogosity
		llwarns << "Forcing an inventory-decline for probably-bad asset type."
				<< llendl;
		forceResponse(IOR_DECLINE);
		return;
	}

	args["OBJECTTYPE"] = typestr;

	payload["from_id"] = mFromID;
	args["OBJECTFROMNAME"] = mFromName;
	args["NAME"] = name;

	LLNotification::Params p("ObjectGiveItem");
	p.substitutions(args).payload(payload).functor(boost::bind(&LLOfferInfo::inventoryOfferCallback,
															   this, _1, _2));

	if (mFromObject)
	{
		if (mFromID == gAgentID)
		{
			p.name = "ObjectGiveItemOurs";
		}
		else if (name_found)
		{
			p.name = "ObjectGiveItem";
		}
		else
		{
			p.name = "ObjectGiveItemUnknownUser";
		}
	}
	else
	{
		p.name = "UserGiveItem";
	}

	gNotifications.add(p);
}

bool lure_callback(const LLSD& notification, const LLSD& response)
{
	LLUUID from_id = notification["payload"]["from_id"].asUUID();
	LLUUID lure_id = notification["payload"]["lure_id"].asUUID();

	S32 option = 0;
	if (response.isInteger())
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotification::getSelectedOption(notification, response);
	}
	if (option == 0)	// Accept
	{
		bool godlike = notification["payload"]["godlike"].asBoolean();
		gAgent.teleportViaLure(lure_id, godlike);
	}
	else				// Decline
	{
		send_simple_im(from_id, LLStringUtil::null, IM_LURE_DECLINED, lure_id);
	}
	return false;
}
static LLNotificationFunctorRegistration lure_callback_reg("TeleportOffered",
														   lure_callback);

void send_lures(const LLSD& notification, const LLSD& response,
				bool censor_message)
{
	std::string text = response["message"].asString();

//MK
	if (censor_message && gRLenabled)
	{
		if (gRLInterface.containsWithoutException("sendim"))
		{
			text = "(Hidden)";
		}
		else
		{
			for (LLSD::array_const_iterator
				 it = notification["payload"]["ids"].beginArray();
				 it != notification["payload"]["ids"].endArray(); ++it)
			{
				if (gRLInterface.containsSubstr("sendimto:" +
												it->asUUID().asString()))
				{
					text = "(Hidden)";
					break;
				}
			}
		}
	}
//mk
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_StartLure);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_Info);
	msg->addU8Fast(_PREHASH_LureType, (U8)0); // sim will fill this in.
	msg->addStringFast(_PREHASH_Message, text);
	for (LLSD::array_const_iterator
		 it = notification["payload"]["ids"].beginArray();
		 it != notification["payload"]["ids"].endArray(); ++it)
	{
		msg->nextBlockFast(_PREHASH_TargetData);
		msg->addUUIDFast(_PREHASH_TargetID, it->asUUID());
	}
	gAgent.sendReliableMessage();
}

bool teleport_request_callback(const LLSD& notification, const LLSD& response)
{
	LLUUID from_id = notification["payload"]["from_id"].asUUID();
	if (from_id.isNull())
	{
		llwarns << "from_id is NULL" << llendl;
		return false;
	}

	std::string from_name;
	if (!gCacheNamep || !gCacheNamep->getFullName(from_id, from_name))
	{
		return false;
	}
	if (LLMuteList::isMuted(from_id, from_name) &&
		!LLMuteList::isLinden(from_name))
	{
		return false;
	}

	S32 option = 0;
	if (response.isInteger())
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotification::getSelectedOption(notification, response);
	}
	if (option == 0)	// Accepted
	{
		LLSD dummy_notification;
		dummy_notification["payload"]["ids"][0] = from_id;

		LLSD dummy_response;
		dummy_response["message"] = response["message"];

		send_lures(dummy_notification, dummy_response, false);
	}

	return false;
}
static LLNotificationFunctorRegistration teleport_request_callback_reg("TeleportRequest",
																	   teleport_request_callback);

bool goto_url_callback(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 1)
	{
		LLWeb::loadURL(notification["payload"]["url"].asString());
	}
	return false;
}
static LLNotificationFunctorRegistration goto_url_callback_reg("GotoURL",
															   goto_url_callback);

void process_improved_im(LLMessageSystem* msg, void**)
{
	if (!gIMMgrp)
	{
		return;
	}

	// *TODO:translate - need to fix the full name to first/last (maybe)
	LLUUID from_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, from_id);

	bool from_group;
	msg->getBoolFast(_PREHASH_MessageBlock, _PREHASH_FromGroup, from_group);

	LLUUID to_id;
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_ToAgentID, to_id);

	U8 offline;
	msg->getU8Fast(_PREHASH_MessageBlock, _PREHASH_Offline, offline);

	U8 d = 0;
	msg->getU8Fast(_PREHASH_MessageBlock, _PREHASH_Dialog, d);
	EInstantMessage dialog = (EInstantMessage)d;

	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_ID, session_id);

	U32 timestamp;
	msg->getU32Fast(_PREHASH_MessageBlock, _PREHASH_Timestamp, timestamp);

	std::string name;
	msg->getStringFast(_PREHASH_MessageBlock, _PREHASH_FromAgentName, name);

	std::string message;
	msg->getStringFast(_PREHASH_MessageBlock, _PREHASH_Message, message);

	U32 estate_id = 0;
	msg->getU32Fast(_PREHASH_MessageBlock, _PREHASH_ParentEstateID, estate_id);

	LLUUID region_id;
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_RegionID, region_id);

	LLVector3 position;
	msg->getVector3Fast(_PREHASH_MessageBlock, _PREHASH_Position, position);

	U8 binary_bucket[MTUBYTES];
	msg->getBinaryDataFast(_PREHASH_MessageBlock, _PREHASH_BinaryBucket,
						   binary_bucket, 0, 0, MTUBYTES);
	S32 bucket_size = msg->getSizeFast(_PREHASH_MessageBlock,
									   _PREHASH_BinaryBucket);

	LLHost sender = msg->getSender();

	gIMMgrp->processNewMessage(from_id, from_group, to_id, offline, dialog,
							   session_id, timestamp, name, message, estate_id,
							   region_id, position, binary_bucket, bucket_size,
							   sender);
}

void busy_message(const LLUUID& from_id)
{
	if (gAgent.getBusy())
	{
		std::string my_name;
		gAgent.buildFullname(my_name);
		std::string response = "Busy mode auto-response: ";
		response += gSavedPerAccountSettings.getText("BusyModeResponse");
		pack_instant_message(gAgentID, false, gAgentSessionID, from_id,
							 my_name, response, IM_ONLINE,
							 IM_BUSY_AUTO_RESPONSE);
		gAgent.sendReliableMessage();
	}
}

bool callingcard_offer_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option != 0 && option != 1)
	{
		// Close button probably, possibly timed out
		return false;
	}

	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return false;	// Paranoia

	LLUUID tid = notification["payload"]["transaction_id"].asUUID();
	LLHost sender(notification["payload"]["sender"].asString());

	if (option == 0)	// Accept
	{
		msg->newMessageFast(_PREHASH_AcceptCallingCard);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_TransactionBlock);
		msg->addUUIDFast(_PREHASH_TransactionID, tid);
		const LLUUID& fid =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
		msg->nextBlockFast(_PREHASH_FolderData);
		msg->addUUIDFast(_PREHASH_FolderID, fid);
		msg->sendReliable(sender);
	}
	else				// Decline
	{
		msg->newMessageFast(_PREHASH_DeclineCallingCard);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_TransactionBlock);
		msg->addUUIDFast(_PREHASH_TransactionID, tid);
		msg->sendReliable(sender);
		busy_message(notification["payload"]["source_id"].asUUID());
	}

	return false;
}
static LLNotificationFunctorRegistration callingcard_offer_cb_reg("OfferCallingCard",
																  callingcard_offer_callback);

void process_offer_callingcard(LLMessageSystem* msg, void**)
{
	LLUUID source_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, source_id);
	LLUUID tid;
	msg->getUUIDFast(_PREHASH_AgentBlock, _PREHASH_TransactionID, tid);

	// Someone has offered to form a friendship
	LL_DEBUGS("InventoryOffer") << "Callingcard offer from source: "
								<< source_id << LL_ENDL;

	LLSD payload;
	payload["transaction_id"] = tid;
	payload["source_id"] = source_id;
	payload["sender"] = msg->getSender().getIPandPort();

	LLViewerObject* source = gObjectList.findObject(source_id);
	LLSD args;
	std::string source_name;
	if (source && source->isAvatar())
	{
		LLNameValue* nvfirst = source->getNVPair("FirstName");
		LLNameValue* nvlast  = source->getNVPair("LastName");
		if (nvfirst && nvlast)
		{
			source_name = LLCacheName::buildFullName(nvfirst->getString(),
													 nvlast->getString());
			args["NAME"] = source_name;
		}
	}

	if (source_name.empty())
	{
		llwarns << "Calling card offer from an unknown source. Ignored."
				<< llendl;
	}
	else if (gAgent.getBusy() ||
			 LLMuteList::isMuted(source_id, source_name, LLMute::flagTextChat))
	{
		// Automatically decline offer
		gNotifications.forceResponse(LLNotification::Params("OfferCallingCard").payload(payload),
									 1);
	}
	else
	{
		gNotifications.add("OfferCallingCard", args, payload);
	}
}

void process_accept_callingcard(LLMessageSystem*, void**)
{
	gNotifications.add("CallingCardAccepted");
}

void process_decline_callingcard(LLMessageSystem*, void**)
{
	gNotifications.add("CallingCardDeclined");
}

void add_floater_chat(LLChat& chat, bool history)
{
	if (history)
	{
		// Just add to history
		LLFloaterChat::addChatHistory(chat);
	}
	else
	{
		// Show on screen and add to history
		LLFloaterChat::addChat(chat, false, false);
	}
}

void process_chat_from_simulator(LLMessageSystem* msg, void**)
{
	LLChat chat;

	std::string from_name;
	msg->getString(_PREHASH_ChatData, _PREHASH_FromName, from_name);
	chat.mFromName = from_name;

	LLUUID from_id;
	msg->getUUID(_PREHASH_ChatData, _PREHASH_SourceID, from_id);
	chat.mFromID = from_id;

	// Object owner for objects
	LLUUID owner_id;
	msg->getUUID(_PREHASH_ChatData, _PREHASH_OwnerID, owner_id);
	chat.mOwnerID = owner_id;

	U8 chat_source;
	msg->getU8Fast(_PREHASH_ChatData, _PREHASH_SourceType, chat_source);
	chat.mSourceType = (EChatSourceType)chat_source;

	U8 chat_type;
	msg->getU8(_PREHASH_ChatData, _PREHASH_ChatType, chat_type);
	chat.mChatType = (EChatType)chat_type;

	U8 chat_audible;
	msg->getU8Fast(_PREHASH_ChatData, _PREHASH_Audible, chat_audible);
	chat.mAudible = (EChatAudible)chat_audible;

	chat.mTime = LLFrameTimer::getElapsedSeconds();

	bool is_busy = gAgent.getBusy();

	bool is_muted = LLMuteList::isMuted(from_id, from_name,
										LLMute::flagTextChat) ||
					LLMuteList::isMuted(owner_id, LLMute::flagTextChat);

	bool is_linden = chat.mSourceType != CHAT_SOURCE_OBJECT &&
					 LLMuteList::isLinden(from_name);

	bool is_audible = CHAT_AUDIBLE_FULLY == chat.mAudible;

	bool is_owned_by_me = false;

	bool twirly = false;
	LLViewerObject*	chatter = gObjectList.findObject(from_id);
	if (chatter)
	{
		chat.mPosAgent = chatter->getPositionAgent();

		// Make swirly things only for talking objects (not for script debug
		// messages, though)
		if (chat.mSourceType == CHAT_SOURCE_OBJECT &&
			chat.mChatType != CHAT_TYPE_DEBUG_MSG &&
			gSavedSettings.getBool("EffectScriptChatParticles"))
		{
			twirly = true;
		}

		// Record last audible utterance
		if (is_audible && (is_linden || (!is_muted && !is_busy)))
		{
			if (chat.mChatType != CHAT_TYPE_START &&
				chat.mChatType != CHAT_TYPE_STOP)
			{
				gAgent.heardChat(chat.mFromID);
			}
		}

		is_owned_by_me = chatter->permYouOwner();

		// Keep track of the owner's Id for the chatter object.
		if (chatter->mOwnerID.isNull() && owner_id.notNull())
		{
			chatter->mOwnerID = owner_id;
		}
	}

	U32 links_for_chatting_objects =
		gSavedSettings.getU32("LinksForChattingObjects");
	if (links_for_chatting_objects != 0 &&
		chat.mSourceType == CHAT_SOURCE_OBJECT &&
//MK
		(!gRLenabled || !gRLInterface.mContainsShownames) &&
//mk
		(!is_owned_by_me || links_for_chatting_objects == 2))
	{
		LLSD query_string;
		query_string["name"]  = from_name;
		query_string["owner"] = owner_id;
//MK
		if (!gRLenabled || !gRLInterface.mContainsShowloc)
		{
//mk
			LLViewerObject* obj;
			// Compute the object SLURL.
			if (chatter)
			{
				obj = chatter;
			}
			else
			{
				// It is a HUD: use the object owner instead.
				obj = gObjectList.findObject(owner_id);
			}
			if (obj)
			{
				LLVector3 pos = obj->getPositionRegion();
				S32 x = ll_round((F32)fmod((F64)pos.mV[VX],
										   (F64)REGION_WIDTH_METERS));
				S32 y = ll_round((F32)fmod((F64)pos.mV[VY],
										   (F64)REGION_WIDTH_METERS));
				S32 z = ll_round((F32)pos.mV[VZ]);
				std::ostringstream location;
				location << obj->getRegion()->getName() << "/" << x << "/" << y
						 << "/" << z;
				query_string["slurl"] = location.str();
			}
//MK
		}
//mk
		std::ostringstream link;
		link << "secondlife:///app/objectim/" << from_id
			 << LLURI::mapToQueryString(query_string);
		chat.mURL = link.str();
	}

	if (is_audible)
	{
		if (chatter && chatter->isAvatar())
		{
//MK
			if (!gRLenabled || !gRLInterface.mContainsShownames)
			{
//mk
				if (LLAvatarName::sOmitResidentAsLastName)
				{
					from_name = LLCacheName::cleanFullName(from_name);
				}
				if (LLAvatarNameCache::useDisplayNames())
				{
					LLAvatarName avatar_name;
					if (LLAvatarNameCache::get(from_id, &avatar_name))
					{
						if (LLAvatarNameCache::useDisplayNames() == 2)
						{
							from_name = avatar_name.mDisplayName;
						}
						else
						{
							from_name = avatar_name.getNames();
						}
					}
					chat.mFromName = from_name;
				}
//MK
			}
//mk
		}

		bool visible_in_chat_bubble = false;
		std::string verb;

		std::string	mesg;
		msg->getStringFast(_PREHASH_ChatData, _PREHASH_Message, mesg);

		bool ircstyle = false;
//MK
		if (gRLenabled && chat.mChatType != CHAT_TYPE_OWNER &&
			chat.mChatType != CHAT_TYPE_DIRECT)
		{
			if ((chatter &&
				 // Avatar, object or attachment that does not belong to me...
				 (chatter->isAvatar() || !chatter->isAttachment() ||
				  !chatter->permYouOwner())) ||
				// or this may be a HUD (visible only to the other party) or an
				// unrezzed avatar or object...
				!chatter)
			{
				if (gRLInterface.containsWithoutException("recvchat",
														  from_id.asString()) ||
					gRLInterface.contains("recvchatfrom:" +
										  from_id.asString()) ||
					gRLInterface.contains("recvchatfrom:" +
										  owner_id.asString()))
				{
					chat.mFromName = from_name;
					chat.mText = gRLInterface.crunchEmote(mesg, 20);
					if (!gSavedSettings.getBool("RestrainedLoveShowEllipsis") &&
						chat.mText == "...")
					{
						return;
					}
					mesg = chat.mText;
				}

				if (gRLInterface.containsWithoutException("recvemote",
														  from_id.asString()) ||
					gRLInterface.contains("recvemotefrom:" +
										  from_id.asString()) ||
					gRLInterface.contains("recvemotefrom:" +
										  owner_id.asString()))
				{
					std::string prefix = mesg.substr(0, 4);
					if (prefix == "/me " || prefix == "/me'")
					{
						chat.mFromName = from_name;
						if (gSavedSettings.getBool("RestrainedLoveShowEllipsis"))
						{
							chat.mText = "/me ...";
						}
						else
						{
							return;
						}
						mesg = chat.mText;
					}
				}

				if (from_id != gAgentID && gRLInterface.mContainsShownames)
				{
					// Also scramble the name of the chatter (replace with a
					// dummy name)
					if (chatter && chatter->isAvatar())
					{
						std::string uuid_str = chatter->getID().asString();
						if (gRLInterface.containsWithoutException("shownames",
																  uuid_str))
						{
							from_name = gRLInterface.getDummyName(from_name,
																  chat.mAudible);
						}
					}
					else
					{
						from_name = gRLInterface.getCensoredMessage(from_name);
					}
					chat.mFromName = from_name;
				}
			}
			else if (gRLInterface.mContainsShownames)
			{
				// This is an object, but it could fake an avatar name
				from_name = gRLInterface.getCensoredMessage(from_name);
				chat.mFromName = from_name;
			}
		}
//mk
		// Look for IRC-style emotes here so chatbubbles work
		std::string prefix = mesg.substr(0, 4);
		if (prefix == "/me " || prefix == "/me'")
		{
			chat.mText = from_name;
			mesg = mesg.substr(3);
			ircstyle = true;
		}
		chat.mText += mesg;

		// Look for the start of typing so we can put "..." in the bubbles.
		if (chat.mChatType == CHAT_TYPE_START)
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, true);

			// Might not have the avatar constructed yet, eg on login.
			if (chatter && chatter->isAvatar())
			{
				((LLVOAvatar*)chatter)->startTyping();
			}
			return;
		}
		else if (chat.mChatType == CHAT_TYPE_STOP)
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, false);

			// Might not have the avatar constructed yet, eg on login.
			if (chatter && chatter->isAvatar())
			{
				((LLVOAvatar*)chatter)->stopTyping();
			}
			return;
		}

		// We have a real utterance now, so can stop showing "..." and proceed.
		if (chatter && chatter->isAvatar())
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, false);
			((LLVOAvatar*)chatter)->stopTyping();

			if (!is_muted && !is_busy)
			{
				visible_in_chat_bubble = gSavedSettings.getBool("UseChatBubbles");
				((LLVOAvatar*)chatter)->addChat(chat);
			}
		}

		// Look for IRC-style emotes
		if (ircstyle)
		{
			// Do nothing, ircstyle is fixed above for chat bubbles
		}
		else
		{
			switch (chat.mChatType)
			{
			case CHAT_TYPE_WHISPER:
				verb = " " + LLTrans::getString("whisper") + " ";
				break;
			case CHAT_TYPE_OWNER:
//MK
			// This is the actual handling of the commands sent by owned
			// objects
			{
				if (gRLenabled && mesg.length() > 2 &&
					mesg[0] == RL_PREFIX && mesg[1] != ' ')
				{
					std::string command = mesg.substr(1);
					LLStringUtil::toLower(command);
					gRLInterface.queueCommands(from_id, chat.mFromName,
											   command);
					return;
				}
				else
				{
					if (HBViewerAutomation::checkLuaCommand(mesg, from_id,
															chat.mFromName))
					{
						return;
					}

					if (gRLenabled)
					{
						if (gRLInterface.mContainsShowloc)
						{
							// Hide every occurrence of the Region and Parcel
							// names if the location restriction is active
							mesg = gRLInterface.getCensoredLocation(mesg);
						}
						if (gRLInterface.mContainsShownames)
						{
							mesg = gRLInterface.getCensoredMessage(mesg);
							from_name =
								gRLInterface.getCensoredMessage(from_name);
							chat.mFromName = from_name;
						}
					}

					verb = ": ";
				}
				break;
			}
//mk
			case CHAT_TYPE_DEBUG_MSG:
			case CHAT_TYPE_NORMAL:
			case CHAT_TYPE_DIRECT:
				verb = ": ";
				break;

			case CHAT_TYPE_SHOUT:
				verb = " " + LLTrans::getString("shout") + " ";
				break;

			case CHAT_TYPE_START:
			case CHAT_TYPE_STOP:
				llwarns << "Got chat type start/stop in main chat processing."
						<< llendl;
				break;

			default:
				llwarns << "Unknown type " << chat.mChatType << " in chat !"
						<< llendl;
				verb = " say, ";
			}
//MK
			if (gRLenabled && gRLInterface.mContainsShownames &&
				(!chatter || (chatter && !chatter->isAvatar())))
			{
				// Censor object chat but not avatar chat
				mesg = gRLInterface.getCensoredMessage(mesg);
			}
//mk
			chat.mText = from_name + verb + mesg;
		}

		if (twirly)
		{
			LLPointer<LLViewerPartSourceChat> psc =
				new LLViewerPartSourceChat(chatter->getPositionAgent());
			psc->setSourceObject(chatter);
			psc->setColor(LLColor4::white);
			// We set the particles to be owned by the object's owner, just in
			// case they should be muted by the mute list
			psc->setOwnerUUID(owner_id);
			gViewerPartSim.addPartSource(psc);
		}

		if (chatter)
		{
			chat.mPosAgent = chatter->getPositionAgent();
		}

		// truth table:
		// LINDEN  BUSY  MUTED  OWNED_BY_YOU  TASK  DISPLAY  STORE IN HISTORY
		// F	   F	 F	  F			 *	 Yes	  Yes
		// F	   F	 F	  T			 *	 Yes	  Yes
		// F	   F	 T	  F			 *	 No	   No
		// F	   F	 T	  T			 *	 No	   No
		// F	   T	 F	  F			 *	 No	   Yes
		// F	   T	 F	  T			 *	 Yes	  Yes
		// F	   T	 T	  F			 *	 No	   No
		// F	   T	 T	  T			 *	 No	   No
		// T	   *	 *	  *			 F	 Yes	  Yes

		chat.mMuted = is_muted && !is_linden;

		if (!visible_in_chat_bubble &&
			(is_linden || !is_busy || is_owned_by_me))
		{
			// Show on screen and add to history
			add_floater_chat(chat, false);
		}
		else
		{
			// Just add to the chat history
			add_floater_chat(chat, true);
		}

		if (gAutomationp && !chat.mMuted && from_id != gAgentID &&
			chat.mChatType != CHAT_TYPE_DEBUG_MSG &&
			chat.mChatType != CHAT_TYPE_START &&
			chat.mChatType != CHAT_TYPE_STOP)
		{
			gAutomationp->onReceivedChat(chat.mChatType, from_id,
										 chat.mFromName, chat.mText);
		}
	}
}

// The simulator we are on is informing the viewer that the agent is starting
// to teleport (perhaps to another sim, perhaps to the same sim). If we
// initiated the teleport process by sending TeleportRequest, then this info is
// redundant, but if the sim initiated the teleport (via a script call, being
// killed, etc) then this info is news to us.
void process_teleport_start(LLMessageSystem* msg, void**)
{
	U32 teleport_flags = 0x0;
	msg->getU32(_PREHASH_Info, _PREHASH_TeleportFlags, teleport_flags);

	LL_DEBUGS("Teleport") << "Got TeleportStart with TeleportFlags="
						  << teleport_flags << ". gTeleportDisplay: "
						  << gTeleportDisplay << ", gAgent.mTeleportState: "
						  << gAgent.getTeleportState() << LL_ENDL;

	if (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL)
	{
		gViewerWindowp->setProgressCancelButtonVisible(false);
	}
	else
	{
		// *TODO: Translate
		gViewerWindowp->setProgressCancelButtonVisible(true, "Cancel");
	}

//MK
	if (gRLenabled && !gRLInterface.getAllowCancelTp())
	{
		gViewerWindowp->setProgressCancelButtonVisible(false);
	}
//mk

	// Note: could add data here to differentiate between normal teleport and
	// death.
	if (!gAgent.teleportInProgress())
	{
		gAgent.setTeleportState(LLAgent::TELEPORT_START);
		make_ui_sound("UISndTeleportOut");

		llinfos << "Teleport initiated by remote TeleportStart message with TeleportFlags: "
				<<  teleport_flags << llendl;
	}
}

void process_teleport_progress(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (gAgentID != agent_id || !gAgent.teleportInProgress())
	{
		llwarns << "Unexpected teleport progress message." << llendl;
		return;
	}
	U32 teleport_flags = 0x0;
	msg->getU32(_PREHASH_Info, _PREHASH_TeleportFlags, teleport_flags);
	if (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL
//MK
		|| (gRLenabled && !gRLInterface.getAllowCancelTp()))
//mk
	{
		gViewerWindowp->setProgressCancelButtonVisible(false);
	}
	else
	{
		// *TODO: Translate
		gViewerWindowp->setProgressCancelButtonVisible(true, "Cancel");
	}
	std::string buffer;
	msg->getString(_PREHASH_Info, _PREHASH_Message, buffer);
	LL_DEBUGS("Teleport") << "Teleport progress: " << buffer << LL_ENDL;

	// Sorta hacky... Default to using simulator raw messages if we do not find
	// the coresponding mapping in our progress mappings.
	std::string message = buffer;
	if (LLAgent::sTeleportProgressMessages.find(buffer) !=
		LLAgent::sTeleportProgressMessages.end())
	{
		message = LLAgent::sTeleportProgressMessages[buffer];
	}
	gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages[message]);
}

class LLFetchInWelcomeArea : public LLInventoryFetchDescendentsObserver
{
public:
	LLFetchInWelcomeArea()
	{
	}

	void done() override
	{
		LLIsType is_landmark(LLAssetType::AT_LANDMARK);
		LLIsType is_card(LLAssetType::AT_CALLINGCARD);

		LLInventoryModel::cat_array_t	card_cats;
		LLInventoryModel::item_array_t	card_items;
		LLInventoryModel::cat_array_t	land_cats;
		LLInventoryModel::item_array_t	land_items;

		for (U32 i = 0, count = mCompleteFolders.size(); i < count; ++i)
		{
			const LLUUID& id = mCompleteFolders[i];
			gInventory.collectDescendentsIf(id, land_cats, land_items,
											LLInventoryModel::EXCLUDE_TRASH,
											is_landmark);
			gInventory.collectDescendentsIf(id, card_cats, card_items,
											LLInventoryModel::EXCLUDE_TRASH,
											is_card);
		}

		if (land_items.size() > 0)
		{
			// Show notification that they can now teleport to landmarks. Use a
			// random landmark from the inventory
			S32 random_land = ll_rand(land_items.size() - 1);
			LLSD args;
			args["NAME"] = land_items[random_land]->getName();
			gNotifications.add("TeleportToLandmark", args);
		}
		if (card_items.size() > 0)
		{
			// Show notification that they can now contact people. Use a random
			// calling card from the inventory
			S32 random_card = ll_rand(card_items.size() - 1);
			LLSD args;
			args["NAME"] = card_items[random_card]->getName();
			gNotifications.add("TeleportToPerson", args);
		}

		gInventory.removeObserver(this);
		delete this;
	}
};

class LLPostTeleportNotifiers final : public LLEventTimer
{
public:
	LLPostTeleportNotifiers()
	:	LLEventTimer(2.f)
	{
	}

	~LLPostTeleportNotifiers() override
	{
	}

	// Method to be called at the supplied frequency
	bool tick() override;
};

bool LLPostTeleportNotifiers::tick()
{
	if (gAgent.teleportInProgress())
	{
		return false;
	}

	// Get calling cards and land marks available to the user arriving.
	uuid_vec_t folders;
	LLUUID folder_id =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
	if (folder_id.notNull())
	{
		folders.emplace_back(folder_id);
	}

	folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LANDMARK);
	if (folder_id.notNull())
	{
		folders.emplace_back(folder_id);
	}

	if (!folders.empty())
	{
		LLFetchInWelcomeArea* fetcher = new LLFetchInWelcomeArea;
		fetcher->fetchDescendents(folders);
		if (fetcher->isFinished())
		{
			fetcher->done();
		}
		else
		{
			gInventory.addObserver(fetcher);
		}
	}

	return true;
}

// Teleport notification from the simulator. We are going to pretend to be a
// new agent
void process_teleport_finish(LLMessageSystem* msg, void**)
{
	if (gAgent.getTeleportState() >= LLAgent::TELEPORT_MOVING)
	{
		llwarns << "Received redundant TeleportFinish message." << llendl;
		if (gSavedSettings.getBool("HardenedMessaging"))
		{
			return;
		}
	}
//MK
	if (gRLenabled && !gRLInterface.getAllowCancelTp())
	{
		// Cancel button was forcibly hidden by the RLV code ("@tpto") => allow
		// it to show again for next time
		gRLInterface.setAllowCancelTp(true);
	}
//mk
	LL_DEBUGS("Teleport") << "Got teleport location message" << LL_ENDL;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_Info, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got teleport notification for wrong agent !" << llendl;
		return;
	}

	// Teleport is finished; it cannot be cancelled now.
	gViewerWindowp->setProgressCancelButtonVisible(false);

	// Force a vertex buffer reset
	gPipeline.doResetVertexBuffers(true);

	// Do teleport effect for where you are leaving.
	LLHUDEffectSpiral::swirlAtPosition(gAgent.getPositionGlobal(), -1.f, true);

	U32 location_id;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_LocationID, location_id);
	U32 sim_ip;
	msg->getIPAddrFast(_PREHASH_Info, _PREHASH_SimIP, sim_ip);
	U16 sim_port;
	msg->getIPPortFast(_PREHASH_Info, _PREHASH_SimPort, sim_port);
#if 0
	LLVector3 pos, look_at;
	msg->getVector3Fast(_PREHASH_Info, _PREHASH_Position, pos);
	msg->getVector3Fast(_PREHASH_Info, _PREHASH_LookAt, look_at);
#endif
	U64 region_handle;
	msg->getU64Fast(_PREHASH_Info, _PREHASH_RegionHandle, region_handle);
	U32 teleport_flags;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_TeleportFlags, teleport_flags);

	std::string seed_cap;
	msg->getStringFast(_PREHASH_Info, _PREHASH_SeedCapability, seed_cap);

	// Update home location if we are teleporting out of prelude - specific to
	// teleporting to welcome area
	if ((teleport_flags & TELEPORT_FLAGS_SET_HOME_TO_TARGET) &&
		!gAgent.isGodlike())
	{
		LLVector3 pos;
		gAgent.setHomePosRegion(region_handle, pos);

		// Create a timer that will send notices when teleporting is all
		// finished. Since this is based on the LLEventTimer class, it will be
		// managed by that class and not orphaned or leaked.
		new LLPostTeleportNotifiers();
	}

	LLHost sim_host(sim_ip, sim_port);

	// Viewer trusts the simulator.
	gMessageSystemp->enableCircuit(sim_host, true);

	// Variable region size support
	U32 region_size_x = REGION_WIDTH_METERS;
	U32 region_size_y = REGION_WIDTH_METERS;
	if (!gIsInSecondLife)
	{
		msg->getU32Fast(_PREHASH_Info, _PREHASH_RegionSizeX, region_size_x);
		if (region_size_x == 0)
		{
			region_size_x = REGION_WIDTH_METERS;
		}
		msg->getU32Fast(_PREHASH_Info, _PREHASH_RegionSizeY, region_size_y);
		if (region_size_y == 0)
		{
			region_size_y = region_size_x;
		}
		if (region_size_x > REGION_WIDTH_METERS ||
			region_size_y > REGION_WIDTH_METERS)
		{
			llinfos << "Arriving in a VARREGION... Cross your fingers !"
					<< llendl;
		}
	}
	if (region_size_x != region_size_y)
	{
		llwarns << "RECTANGULAR REGIONS NOT SUPPORTED: expect a crash !"
				<< llendl;
		region_size_x = llmax(region_size_x, region_size_y);
	}

	LLViewerRegion* regionp =
		gWorld.addRegion(region_handle, sim_host, region_size_x);

	gWLSkyParamMgr.processLightshareReset();

	gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["contacting"]);
	llinfos << "Enabling: " << sim_host << " - With code: "
			<< msg->mOurCircuitCode << llendl;
	// Now, use the circuit info to tell simulator about us !
	msg->newMessageFast(_PREHASH_UseCircuitCode);
	msg->nextBlockFast(_PREHASH_CircuitCode);
	msg->addU32Fast(_PREHASH_Code, msg->getOurCircuitCode());
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_ID, gAgentID);
	msg->sendReliable(sim_host);

	send_complete_agent_movement(sim_host);

	gAgent.setTeleportState(LLAgent::TELEPORT_MOVING);

	regionp->setSeedCapability(seed_cap);

	// Now do teleport effect (TeleportEnd) for where you are going.
	LLHUDEffectSpiral::swirlAtPosition(gAgent.getPositionGlobal(), -1.f, true);
}

void process_agent_movement_complete(LLMessageSystem* msg, void**)
{
	gShiftFrame = true;
	gAgentMovementCompleted = true;

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	if (gAgentID != agent_id || gAgentSessionID != session_id)
	{
		llwarns << "Incorrect agent or session Id; ignored." << llendl;
		return;
	}

	// Check timestamp to make sure the movement completion makes sense.
	static U32 last_move_time = 0;
	U32 timestamp;
	msg->getU32(_PREHASH_Data, _PREHASH_Timestamp, timestamp);
	if (timestamp < last_move_time)
	{
		llwarns << "Timestamp for move is in the past." << llendl;
		if (gSavedSettings.getBool("HardenedMessaging"))
		{
			return;
		}
	}
	last_move_time = timestamp;

	LLVector3 agent_pos;
	msg->getVector3Fast(_PREHASH_Data, _PREHASH_Position, agent_pos);
	LLVector3 look_at;
	msg->getVector3Fast(_PREHASH_Data, _PREHASH_LookAt, look_at);
	U64 region_handle;
	msg->getU64Fast(_PREHASH_Data, _PREHASH_RegionHandle, region_handle);

	msg->getString(_PREHASH_SimData, _PREHASH_ChannelVersion,
				   gLastVersionChannel);

	// Could happen if you were immediately god-teleported away on login, maybe
	// other cases. Continue, but warn, excepted if encountered at normal login
	// time (since it *always* happens at this time).
	if (!isAgentAvatarValid() &&
		LLStartUp::getStartupState() >= STATE_INVENTORY_SEND)
	{
		llwarns << "NULL avatar !" << llendl;
	}

	F32 x, y;
	from_region_handle(region_handle, &x, &y);
	LLViewerRegion* regionp = gWorld.getRegionFromHandle(region_handle);
	if (!regionp || !gAgent.getRegion())
	{
		if (gAgent.getRegion())
		{
			llwarns << "Current region: "
					<< gAgent.getRegion()->getOriginGlobal() << llendl;
		}

		llwarns << "Agent being sent to invalid home region: " << x << ":" << y
				<< " - current pos " << gAgent.getPositionGlobal() << llendl;
		gAppViewerp->forceDisconnect("You were sent to an invalid region.");
		return;

	}

	llinfos << "Changing home region to " << x << ":" << y << llendl;

	// Set our upstream host the new simulator and shuffle things as
	// appropriate.
	LLVector3 shift_vector =
		regionp->getPosRegionFromGlobal(gAgent.getRegion()->getOriginGlobal());
	// *HACK: prevent octree insertion failures when TPing far, far away...
	constexpr F32 EXTRA_LONG_TP = 2048.f * REGION_WIDTH_METERS;
	if (shift_vector.length() > EXTRA_LONG_TP)
	{
		regionp->deletePartitions();
		regionp->initPartitions();
		gAgent.setRegion(regionp);
		gObjectList.shiftObjects(shift_vector);
		// Kill objects in the regions we left behind
		for (LLWorld::region_list_t::const_iterator
				it = gWorld.getRegionList().begin(),
				end = gWorld.getRegionList().end();
			 it != end; ++it)
		{
			LLViewerRegion* regp = *it;
			if (regp != regionp)
			{
				gObjectList.killObjects(regp);
			}
		}
	}
	else
	{
		gAgent.setRegion(regionp);
		gObjectList.shiftObjects(shift_vector);
	}


	if (gAssetStoragep)
	{
		gAssetStoragep->setUpstream(msg->getSender());
	}
	if (gCacheNamep)
	{
		gCacheNamep->setUpstream(msg->getSender());
	}
	gViewerThrottle.sendToSim();
	if (gViewerWindowp)
	{
		gViewerWindowp->sendShapeToSim();
	}

	// If this is an AgentMovementComplete message that happened as the result
	// of a teleport, then we need to do things like chat the URL and reset the
	// camera.
	bool is_teleport = gAgent.getTeleportState() == LLAgent::TELEPORT_MOVING;
	if (is_teleport)
	{
		if (gAgent.getTeleportKeepsLookAt())
		{
			// *NOTE: the LookAt data we get from the sim here does not
			// seem to be useful, so get it from the camera instead
			look_at = gViewerCamera.getAtAxis();
		}
		// Force the camera back onto the agent, do not animate.
		gAgent.setFocusOnAvatar(true, false);
		gAgent.slamLookAt(look_at);
		gAgent.updateCamera();

		// IMPORTANT: setRegion() must be called before changing to TP state
		// TELEPORT_START_ARRIVAL, so that the appropriate actions are taken.
		gAgent.setTeleportState(LLAgent::TELEPORT_START_ARRIVAL);

		// Set the appearance on teleport since the new sim does not know what
		// you look like.
		gAgent.sendAgentSetAppearance();

		if (isAgentAvatarValid())
		{
			if (gSavedSettings.getBool("TeleportHistoryInChat")
//MK
				&& (!gRLenabled || !gRLInterface.mContainsShowloc))
//mk
			{
				// Chat the "back" SLURL. (DEV-4907)
				LLChat chat("Teleport completed from " +
							gAgent.getTeleportSourceSLURL());
				chat.mSourceType = CHAT_SOURCE_SYSTEM;
 				LLFloaterChat::addChatHistory(chat);
			}
			if (gFloaterTeleportHistoryp &&
				gSavedSettings.getBool("TeleportHistoryDeparture"))
			{
				// Add the departure location, using the "current" parcel name
				// (which is in fact still the old parcel name since the new
				// parcel properties message was not yet received at this
				// point).
				gFloaterTeleportHistoryp->addSourceEntry(gAgent.getTeleportSourceSLURL(),
														 gViewerParcelMgr.getAgentParcelName());
			}

			// Set the new position
			gAgentAvatarp->setPositionAgent(agent_pos);
			gAgentAvatarp->clearChat();
			gAgentAvatarp->slamPosition();
		}

		// Add teleport destination to the list of visited places
		if (gFloaterTeleportHistoryp)
		{
			gFloaterTeleportHistoryp->addPendingEntry(regionp->getName(),
													  agent_pos);
		}
	}
	else
	{
		// This is likely just the initial logging in phase.
		LL_DEBUGS("Teleport") << "Resetting to TELEPORT_NONE" << LL_ENDL;
		gAgent.setTeleportState(LLAgent::TELEPORT_NONE);

		if (!LLStartUp::isLoggedIn())
		{
			// This is initial log-in, not a region crossing: set the camera
			// looking ahead of the AV so send_agent_update() below will report
			// the correct location to the server.
			LLVector3 look_at_point = agent_pos +
									  look_at.rotVec(gAgent.getQuat());
			gViewerCamera.lookAt(agent_pos, look_at_point, LLVector3::z_axis);
		}
	}

	if (gTracker.isTracking())
	{
		// Check distance to beacon, if < 5m, remove beacon
		LLVector3d beacon_pos = gTracker.getTrackedPositionGlobal();
		LLVector3 beacon_dir(agent_pos.mV[VX] -
							 (F32)fmod(beacon_pos.mdV[VX], 256.0),
							 agent_pos.mV[VY] -
							 (F32)fmod(beacon_pos.mdV[VY], 256.0), 0.f);
		if (beacon_dir.lengthSquared() < 25.f)
		{
			// Do not stop tracking landmarks here, so they can properly be
			// marked as visited in LLTracker()
			if (gTracker.getTrackingStatus() != LLTracker::TRACKING_LANDMARK)
			{
				gTracker.stopTracking();
			}
		}
		else if (is_teleport && !gAgent.getTeleportKeepsLookAt())
		{
			// Look at the beacon
			LLVector3 global_agent_pos = agent_pos;
			global_agent_pos[0] += x;
			global_agent_pos[1] += y;
			look_at = (LLVector3)beacon_pos - global_agent_pos;
			look_at.normalize();
			gAgent.slamLookAt(look_at);
		}
	}

#if 0	// *TODO: put back a check for flying status ! DK 12/19/05
	// Sim tells us whether the new position is off the ground
	if (teleport_flags & TELEPORT_FLAGS_IS_FLYING)
	{
		gAgent.setFlying(true);
	}
	else
	{
		gAgent.setFlying(false);
	}
#endif

	send_agent_update(true, true);

	if (gAgent.getRegion()->getBlockFly())
	{
		gAgent.setFlying(gAgent.canFly());
	}

	// Force simulator to recognize busy state
	if (gAgent.getBusy())
	{
		gAgent.setBusy();
	}
	else
	{
		gAgent.clearBusy();
	}

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->mFootPlane.clear();
	}

	// Send walk-vs-run status
	gAgent.sendWalkRun(gAgent.getRunning() || gAgent.getAlwaysRun());
}

void process_crossed_region(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	if (agent_id != gAgentID || gAgentSessionID != session_id)
	{
		llwarns << "Incorrect agent or session Id. Ignoring message." << llendl;
		return;
	}

	U64 region_handle;
	msg->getU64Fast(_PREHASH_RegionData, _PREHASH_RegionHandle, region_handle);
	if (region_handle && region_handle == gAgent.getRegionHandle())
	{
		llwarns << "Received redundant CrossedRegion message (already there)."
				<< llendl;
		if (gSavedSettings.getBool("HardenedMessaging"))
		{
			return;
		}
	}

	llinfos << "Crossing region boundary" << llendl;
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->resetRegionCrossingTimer();
	}

	U32 sim_ip;
	msg->getIPAddrFast(_PREHASH_RegionData, _PREHASH_SimIP, sim_ip);
	U16 sim_port;
	msg->getIPPortFast(_PREHASH_RegionData, _PREHASH_SimPort, sim_port);
	LLHost sim_host(sim_ip, sim_port);

	std::string seed_cap;
	msg->getStringFast(_PREHASH_RegionData, _PREHASH_SeedCapability, seed_cap);

	send_complete_agent_movement(sim_host);

	// Variable region size support
	U32 region_size_x = REGION_WIDTH_METERS;
	U32 region_size_y = REGION_WIDTH_METERS;
	if (!gIsInSecondLife)
	{
		msg->getU32Fast(_PREHASH_Info, _PREHASH_RegionSizeX, region_size_x);
		if (region_size_x == 0)
		{
			region_size_x = REGION_WIDTH_METERS;
		}
		msg->getU32Fast(_PREHASH_Info, _PREHASH_RegionSizeY, region_size_y);
		if (region_size_y == 0)
		{
			region_size_y = region_size_x;
		}
		if (region_size_x > REGION_WIDTH_METERS ||
			region_size_y > REGION_WIDTH_METERS)
		{
			llinfos << "Arriving in a VARREGION... Cross your fingers !"
					<< llendl;
		}
	}
	if (region_size_x != region_size_y)
	{
		llwarns << "RECTANGULAR REGIONS NOT SUPPORTED: expect a crash !"
				<< llendl;
		region_size_x = llmax(region_size_x, region_size_y);
	}

	LLViewerRegion* regionp = gWorld.addRegion(region_handle, sim_host,
											   region_size_x);
	regionp->setSeedCapability(seed_cap);
}

// Sends avatar and camera information to simulator. Sent roughly once per
// frame, or 20 times per second, whichever is less often.

// ~2.5 degrees -- if its less than this we need to update head_rot:
constexpr F32 THRESHOLD_HEAD_ROT_QDOT = 0.9997f;
// ~0.5 degrees -- if its greater than this then no need to update head_rot
// between these values we delay the updates (but no more than one second):
constexpr F32 MAX_HEAD_ROT_QDOT = 0.99999f;

void send_agent_update(bool force_send, bool send_reliable)
{
	if (gAgent.teleportInProgress() || !gAgent.getRegion())
	{
		// We do not care if they want to send an agent update, they are not
		// allowed to until the target simulator is ready to receive them.
		return;
	}

	// We have already requested to log out. Do not send agent updates.
	if (gAppViewerp->logoutRequestSent())
	{
		return;
	}

	constexpr F32 TRANSLATE_THRESHOLD = 0.01f;

	// Rotation threshold: 0.2 deg.
	// Note: this is (intentionally ?) using the small angle sine approximation
	// to test for rotation. Plus, there is an extra 0.5 in the mix since the
	// perpendicular between last_camera_at and getAtAxis() bisects
	// cam_rot_change. Thus, we are actually testing against 0.2 degrees.
	constexpr F32 ROTATION_THRESHOLD = 0.1f * 2.f * F_PI / 360.f;

	// *HACK: number of times to repeat data on motionless agent:
	constexpr U8 DUP_MSGS = 1;

	// Store data on last sent update so that if no changes, no send
	static LLVector3 last_camera_pos_agent, last_camera_at,
					 last_camera_left, last_camera_up, cam_center_chg,
					 cam_rot_chg;

	static LLQuaternion last_head_rot;
	static U32 last_control_flags = 0;
	static U8 last_render_state;
	static U8 duplicate_count = 0;
	static F32 head_rot_chg = 1.0;
	static U8 last_flags;

	LLMessageSystem* msg = gMessageSystemp;
	LLVector3 camera_pos_agent;		// local to avatar's region
	LLVector3 camera_at_axis, camera_left_axis, camera_up_axis;

	LLQuaternion body_rotation = gAgent.getFrameAgent().getQuaternion();
	LLQuaternion head_rotation = gAgent.getHeadRotation();

	static LLCachedControl<bool> spoof_mouse_look(gSavedSettings,
												  "SpoofMouseLook");
	if (spoof_mouse_look)
	{
		// In mouse look the camera is at the agent's position and follows the
		// agent's head movements... Let's spoof that too.
		camera_pos_agent = gAgent.getPositionAgent();
		camera_at_axis = gAgent.getAtAxis();
		camera_left_axis = gAgent.getLeftAxis();
		camera_up_axis = gAgent.getUpAxis();
	}
	else
	{
		camera_pos_agent = gAgent.getCameraPositionAgent();
		camera_at_axis = gViewerCamera.getAtAxis();
		camera_left_axis = gViewerCamera.getLeftAxis();
		camera_up_axis = gViewerCamera.getUpAxis();
	}

	U8 render_state = gAgent.getRenderState();

	U32 control_flag_change = 0;
	U8 flag_change = 0;

	cam_center_chg = last_camera_pos_agent - camera_pos_agent;
	cam_rot_chg = last_camera_at - camera_at_axis;

	// If a modifier key is held down, turn off LBUTTON and ML_LBUTTON so that
	// using the camera (alt-key) does not trigger a control event.
	U32 control_flags = gAgent.getControlFlags();
	if (spoof_mouse_look)
	{
		// Let's the scripts believe we are in mouse-look even when not
		control_flags |= AGENT_CONTROL_MOUSELOOK;
	}
	MASK key_mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
	if (key_mask & MASK_ALT || key_mask & MASK_CONTROL)
	{
		control_flags &= ~(AGENT_CONTROL_LBUTTON_DOWN |
						   AGENT_CONTROL_ML_LBUTTON_DOWN);
		control_flags |= AGENT_CONTROL_LBUTTON_UP |
						 AGENT_CONTROL_ML_LBUTTON_UP;
	}

	control_flag_change = last_control_flags ^ control_flags;

	U8 flags = AU_FLAGS_NONE;
	if (gAgent.isGroupTitleHidden())
	{
		flags |= AU_FLAGS_HIDETITLE;
	}
	if (gAgentPilot.isActive())
	{
		flags |= AU_FLAGS_CLIENT_AUTOPILOT;
	}

	flag_change = last_flags ^ flags;

	head_rot_chg = dot(last_head_rot, head_rotation);

	if (force_send || control_flag_change != 0 || flag_change != 0 ||
		last_render_state != render_state ||
		head_rot_chg < THRESHOLD_HEAD_ROT_QDOT ||
		cam_center_chg.length() > TRANSLATE_THRESHOLD ||
		cam_rot_chg.length() > ROTATION_THRESHOLD)
	{
		duplicate_count = 0;
	}
	else
	{
		++duplicate_count;

		if (head_rot_chg < MAX_HEAD_ROT_QDOT &&
			duplicate_count < AGENT_UPDATES_PER_SECOND)
		{
			// The head_rotation is sent for updating things like attached
			// guns. We only trigger a new update when head_rotation deviates
			// beyond some threshold from the last update, however this can
			// break fine adjustments when trying to aim an attached gun, so
			// what we do here (where we would normally skip sending an update
			// when nothing has changed) is gradually reduce the threshold to
			// allow a better update to eventually get sent... should update to
			// within 0.5 degrees in less than a second.
			if (head_rot_chg < THRESHOLD_HEAD_ROT_QDOT +
							  (MAX_HEAD_ROT_QDOT - THRESHOLD_HEAD_ROT_QDOT) *
							  duplicate_count / AGENT_UPDATES_PER_SECOND)
			{
				duplicate_count = 0;
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	if (duplicate_count < DUP_MSGS && !gDisconnected)
	{
		// Build the message
		msg->newMessageFast(_PREHASH_AgentUpdate);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addQuatFast(_PREHASH_BodyRotation, body_rotation);
		msg->addQuatFast(_PREHASH_HeadRotation, head_rotation);
		msg->addU8Fast(_PREHASH_State, render_state);
		msg->addU8Fast(_PREHASH_Flags, flags);
		msg->addVector3Fast(_PREHASH_CameraCenter, camera_pos_agent);
		msg->addVector3Fast(_PREHASH_CameraAtAxis, camera_at_axis);
		msg->addVector3Fast(_PREHASH_CameraLeftAxis, camera_left_axis);
		msg->addVector3Fast(_PREHASH_CameraUpAxis, camera_up_axis);
		msg->addF32Fast(_PREHASH_Far, gAgent.mDrawDistance);
		msg->addU32Fast(_PREHASH_ControlFlags, control_flags);

		if (gDebugClicks)
		{
			if (control_flags & AGENT_CONTROL_LBUTTON_DOWN)
			{
				llinfos << "AgentUpdate left button down" << llendl;
			}

			if (control_flags & AGENT_CONTROL_LBUTTON_UP)
			{
				llinfos << "AgentUpdate left button up" << llendl;
			}
		}

		gAgent.enableControlFlagReset();

		if (!send_reliable)
		{
			gAgent.sendMessage();
		}
		else
		{
			gAgent.sendReliableMessage();
		}

		// Copy the old data
		last_head_rot = head_rotation;
		last_render_state = render_state;
		last_camera_pos_agent = camera_pos_agent;
		last_camera_at = camera_at_axis;
		last_camera_left = camera_left_axis;
		last_camera_up = camera_up_axis;
		last_control_flags = control_flags;
		last_flags = flags;
	}
}

// Kept for OpenSim compatibility. HB
void process_time_synch(LLMessageSystem* msg, void**)
{
	if (gAgent.hasExtendedEnvironment())
	{
		return;
	}

	msg->getF32Fast(_PREHASH_TimeInfo, _PREHASH_SunPhase,
					LLWLAnimator::sSunPhase);

	LLVector3 sun_direction;
	msg->getVector3Fast(_PREHASH_TimeInfo, _PREHASH_SunDirection,
						sun_direction);
	LLVector3 sun_ang_velocity;
	msg->getVector3Fast(_PREHASH_TimeInfo, _PREHASH_SunAngVelocity,
						sun_ang_velocity);
	if (!gSky.getOverrideSun())
	{
		gSky.setSunTargetDirection(sun_direction, sun_ang_velocity);
		gSky.setSunDirection(sun_direction, sun_ang_velocity);
	}
	// Propagate to current environment. HB
	gWLSkyParamMgr.propagateParameters();
	gWLWaterParamMgr.propagateParameters();
}

void process_sound_trigger(LLMessageSystem* msg, void**)
{
	if (!gAudiop) return;

	LLUUID sound_id;
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_SoundID, sound_id);
	LLUUID owner_id;
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_OwnerID, owner_id);
	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_ObjectID, object_id);
	LLUUID parent_id;
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_ParentID, parent_id);
	U64 region_handle = 0;
	msg->getU64Fast(_PREHASH_SoundData, _PREHASH_Handle, region_handle);
	LLVector3 pos_local;
	msg->getVector3Fast(_PREHASH_SoundData, _PREHASH_Position, pos_local);
	F32 gain = 0;
	msg->getF32Fast(_PREHASH_SoundData, _PREHASH_Gain, gain);

	// Adjust sound location to true global coords
	LLVector3d pos_global = from_region_handle(region_handle);
	pos_global.mdV[VX] += pos_local.mV[VX];
	pos_global.mdV[VY] += pos_local.mV[VY];
	pos_global.mdV[VZ] += pos_local.mV[VZ];

	// Do not play a trigger sound if you cannot hear it due to parcel "local
	// audio only" setting or to maturity rating
	if (!gViewerParcelMgr.canHearSound(pos_global) ||
		!gAgent.canAccessMaturityInRegion(region_handle))
	{
		return;
	}

	// Do not play sounds from others' gestures if they are not enabled.
	// NOTE: we always play *our* sounds, since send_sound_trigger() is used
	// in the viewer for such purposes as sound preview in inventory.
	static LLCachedControl<bool> gesture_sounds(gSavedSettings,
												"EnableGestureSounds");
	if (!gesture_sounds && object_id == owner_id && owner_id != gAgentID)
	{
		return;
	}

	static LLCachedControl<bool> collision_sounds(gSavedSettings,
												  "EnableCollisionSounds");
	if (!collision_sounds && gMaterialTable.isCollisionSound(sound_id))
	{
		return;
	}

	// Check for mutes
	if (LLMuteList::isMuted(owner_id, LLMute::flagObjectSounds) ||
		LLMuteList::isMuted(object_id) ||
		(parent_id.notNull() && LLMuteList::isMuted(parent_id)))
	{
		// Muted resident, object or parent (the latter check should be
		// unnecessary now that the mutes act on root prims, but we still
		// check this in case we got an old mute list with child objects
		// in it instead of the corresponding root objects).
		return;
	}

	gAudiop->triggerSound(sound_id, owner_id, gain,
						  LLAudioEngine::AUDIO_TYPE_SFX, pos_global);
}

void process_preload_sound(LLMessageSystem* msg, void**)
{
	if (!gAudiop) return;

	LLUUID sound_id;
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_SoundID, sound_id);
	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_id);
	LLUUID owner_id;
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_OwnerID, owner_id);

	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (!objectp || LLMuteList::isMuted(object_id) ||
		LLMuteList::isMuted(owner_id, LLMute::flagObjectSounds))
	{
		// Unknown object or muted object/object owner.
		return;
	}

	LLAudioSource* sourcep = objectp->getAudioSource(owner_id);
	if (!sourcep) return;

	// Only play sounds from regions matching current agent maturity
	LLVector3d pos_global = objectp->getPositionGlobal();
	if (gAgent.canAccessMaturityAtGlobal(pos_global))
	{
		// Add audioData starts a transfer internally. Note that we do not
		// actually do any loading of the audio data into a buffer at this
		// point, as it would not actually help us out.
		LLAudioData* datap = gAudiop->getAudioData(sound_id);
		sourcep->addAudioData(datap, false);
	}
}

// Returns false when the object is not yet in the viewer objects list
bool set_attached_sound(const LLUUID& object_id, const LLUUID& sound_id,
						const LLUUID& owner_id, F32 gain, U8 flags)
{
	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (!objectp)
	{
		return false;
	}

	if (objectp->isDead())
	{
		// Do not bother setting sounds on dead objects
		return true;
	}

	if (LLMuteList::isMuted(object_id) ||
		LLMuteList::isMuted(owner_id, LLMute::flagObjectSounds))
	{
		// Muted object/object owner.
		return true;
	}

	// Only play sounds from regions matching current agent maturity
	LLVector3d pos = objectp->getPositionGlobal();
	if (gAgent.canAccessMaturityAtGlobal(pos))
	{
		objectp->setAttachedSound(sound_id, owner_id, gain, flags);
	}

	return true;
}

constexpr F32 POSTPONED_SOUND_MAX_DELAY = 15.f;

class LLPostponedSoundData
{
protected:
	LOG_CLASS(LLPostponedSoundData);

public:
	// Default constructor for unordered map: DO NOT USE otherwise
	LL_INLINE LLPostponedSoundData()
	{
	}

	static void addPostponedSound(const LLUUID& object_id,
								  const LLUUID& sound_id,
								  const LLUUID& owner_id,
								  F32 gain, U8 flags);

	// Global, fast expiration check based on last postponed sound time stamp
	LL_INLINE static void expirationCheck()
	{
		if (sLastExpiration > 0.f && gFrameTimeSeconds >= sLastExpiration)
		{
			sNewlyCreatedOjects.clear();
			sPostponedSounds.clear();
			sLastExpiration = 0.f;
		}
	}

	LL_INLINE static void addNewlyCreatedOject(const LLUUID& object_id)
	{
		sNewlyCreatedOjects.emplace(object_id);
	}

	static void updateAttachedSounds();

private:
	// Private because addPostponedSound() must be used to add new data
	LL_INLINE LLPostponedSoundData(const LLUUID& sound_id,
								   const LLUUID& owner_id, F32 gain, U8 flags)
	:	mSoundId(sound_id),
		mOwnerId(owner_id),
		mGain(gain),
		mFlags(flags),
		mExpirationTime(gFrameTimeSeconds + POSTPONED_SOUND_MAX_DELAY)
	{
		sLastExpiration = mExpirationTime;
	}

private:
	LLUUID				mSoundId;
	LLUUID				mOwnerId;
	F32					mExpirationTime;
	F32					mGain;
	U8					mFlags;

	static F32			sLastExpiration;

	typedef fast_hmap<LLUUID, LLPostponedSoundData> data_map_t;
	static data_map_t	sPostponedSounds;

	// Maintaining a list of newly created objects prevents having to scan
	// (99% of the time fruitlessly) the viewer objects list for *each* object
	// registered in sPostponedSounds: if a new object with the right UUID is
	// not in sNewlyCreatedOjects, then it is not yet either in the viewer
	// objects list. sNewlyCreatedOjects is cleared on each call to
	// updateAttachedSounds() and is therefore a very short list of UUIDs,
	// unlike the viewer objects list which contains thousands of entries...
	static uuid_list_t	sNewlyCreatedOjects;
};

// Static members
F32 LLPostponedSoundData::sLastExpiration = 0.f;
LLPostponedSoundData::data_map_t LLPostponedSoundData::sPostponedSounds;
uuid_list_t LLPostponedSoundData::sNewlyCreatedOjects;

//static
void  LLPostponedSoundData::addPostponedSound(const LLUUID& object_id,
											  const LLUUID& sound_id,
											  const LLUUID& owner_id,
											  F32 gain, U8 flags)
{
	data_map_t::iterator it = sPostponedSounds.find(object_id);
	if (it == sPostponedSounds.end())
	{
		LL_DEBUGS("Messaging") << "Postponing sound " << sound_id
							   << " for not yet rezzed object " << object_id
							   << LL_ENDL;
		sPostponedSounds[object_id] = LLPostponedSoundData(sound_id, owner_id,
														   gain, flags);
		return;
	}

	LLPostponedSoundData& data = it->second;
	if (data.mSoundId != sound_id)
	{
		LL_DEBUGS("Messaging") << "Updating data to postponed sound "
							   << sound_id << " for not yet rezzed object "
							   << object_id << LL_ENDL;
		data.mSoundId = sound_id;
		data.mOwnerId = owner_id;
		data.mGain = gain;
		data.mFlags = flags;
	}
	sLastExpiration = data.mExpirationTime = gFrameTimeSeconds +
											 POSTPONED_SOUND_MAX_DELAY;
}

//static
void LLPostponedSoundData::updateAttachedSounds()
{
	if (sNewlyCreatedOjects.empty())
	{
		return;
	}

	expirationCheck();

	if (sPostponedSounds.empty())
	{
		return;
	}

	data_map_t::iterator iter = sPostponedSounds.begin();
	data_map_t::iterator end = sPostponedSounds.end();
	while (iter != end)
	{
		data_map_t::iterator curiter = iter++;

		LLPostponedSoundData& data = curiter->second;
		if (gFrameTimeSeconds >= data.mExpirationTime)
		{
			sPostponedSounds.erase(curiter);
			continue;
		}

		const LLUUID& object_id = curiter->first;
		if (!sNewlyCreatedOjects.count(object_id))
		{
			continue;
		}

		if (set_attached_sound(object_id, data.mSoundId, data.mOwnerId,
							   data.mGain, data.mFlags))
		{
			llinfos << "Postponed sound " << data.mSoundId
					<< " attached to object " << object_id << llendl;
			sPostponedSounds.erase(curiter);
		}
	}

	sNewlyCreatedOjects.clear();
}

// Called by LLViewerObjectList::processObjectUpdate(), when a new object is
// created.
void add_newly_created_object(const LLUUID& object_id)
{
	LLPostponedSoundData::addNewlyCreatedOject(object_id);
}

void process_attached_sound(LLMessageSystem* msg, void**)
{
	LLUUID sound_id;
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_SoundID, sound_id);
	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_id);
	LLUUID owner_id;
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_OwnerID, owner_id);
	F32 gain = 0;
	msg->getF32Fast(_PREHASH_DataBlock, _PREHASH_Gain, gain);
	U8 flags;
	msg->getU8Fast(_PREHASH_DataBlock, _PREHASH_Flags, flags);

	LLPostponedSoundData::expirationCheck();

	if (!set_attached_sound(object_id, sound_id, owner_id, gain, flags) &&
		sound_id.notNull())
	{
		LLPostponedSoundData::addPostponedSound(object_id, sound_id, owner_id,
												gain, flags);
	}
}

void process_attached_sound_gain_change(LLMessageSystem* msg, void**)
{
	LLUUID object_guid;
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_guid);

	LLViewerObject* objectp = gObjectList.findObject(object_guid);
	if (objectp)
	{
		F32 gain = 0;
 		msg->getF32Fast(_PREHASH_DataBlock, _PREHASH_Gain, gain);
		objectp->adjustAudioGain(gain);
	}
}

void process_object_update(LLMessageSystem* msg, void** data)
{
	// Update the data counters
	if (msg->getReceiveCompressedSize())
	{
		gObjectBits += msg->getReceiveCompressedSize() * 8;
	}
	else
	{
		gObjectBits += msg->getReceiveSize() * 8;
	}

	// Update the object...
	gObjectList.processObjectUpdate(msg, data, OUT_FULL);
	LLPostponedSoundData::updateAttachedSounds();
}

void process_compressed_object_update(LLMessageSystem* msg, void** data)
{
	// Update the data counters
	if (msg->getReceiveCompressedSize())
	{
		gObjectBits += msg->getReceiveCompressedSize() * 8;
	}
	else
	{
		gObjectBits += msg->getReceiveSize() * 8;
	}

	// Update the object...
	gObjectList.processCompressedObjectUpdate(msg, data, OUT_FULL_COMPRESSED);
	LLPostponedSoundData::updateAttachedSounds();
}

void process_cached_object_update(LLMessageSystem* msg, void** data)
{
	// Update the data counters
	if (msg->getReceiveCompressedSize())
	{
		gObjectBits += msg->getReceiveCompressedSize() * 8;
	}
	else
	{
		gObjectBits += msg->getReceiveSize() * 8;
	}

	// Update the object...
	gObjectList.processCachedObjectUpdate(msg, data, OUT_FULL_CACHED);
}

void process_terse_object_update_improved(LLMessageSystem* msg, void** data)
{
	if (msg->getReceiveCompressedSize())
	{
		gObjectBits += msg->getReceiveCompressedSize() * 8;
	}
	else
	{
		gObjectBits += msg->getReceiveSize() * 8;
	}

	gObjectList.processCompressedObjectUpdate(msg, data, OUT_TERSE_IMPROVED);
	LLPostponedSoundData::updateAttachedSounds();
}

void process_object_properties_family(LLMessageSystem* msg, void**)
{
	// Send the result to the corresponding requesters.
	LLSelectMgr::processObjectPropertiesFamily(msg, NULL);
	HBFloaterAreaSearch::processObjectPropertiesFamily(msg);
	HBFloaterSoundsList::processObjectPropertiesFamily(msg);
	HBViewerAutomation::processObjectPropertiesFamily(msg);
}

void process_kill_object(LLMessageSystem* msg, void**)
{
	LL_FAST_TIMER(FTM_PROCESS_OBJECTS);

	U32 ip = msg->getSenderIP();
	U32 port = msg->getSenderPort();
	LLHost host(ip, port);
	LLViewerRegion* regionp = gWorld.getRegion(host);
	if (!regionp) return;

	LLViewerRegion* agent_region = gAgent.getRegion();
	bool non_agent_region = agent_region && regionp != agent_region;
	bool need_cof_resync = false;

	LLUUID id;
	bool delete_object = LLViewerRegion::sVOCacheCullingEnabled;
	S32 num_objects = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);
	for (S32 i = 0; i < num_objects; ++i)
	{
		U32 local_id;
		msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);

		LLViewerObjectList::getUUIDFromLocal(id, local_id, ip, port);
		if (id.isNull())
		{
			continue;
		}

		if (id == gAgentID)	// Never kill our own avatar !
		{
			LL_DEBUGS("Messaging") << "Received kill-object message from "
								   << (non_agent_region ? "non-agent"
														: "agent")
								   << " region for our agent Id. Ignoring."
								   << LL_ENDL;
			continue;
		}

		LLViewerObject* objectp = gObjectList.findObject(id);
		if (!objectp)
		{
			continue;
		}

		static LLCachedControl<bool> filter_kill(gSavedSettings,
												"IgnoreOuterRegionAttachKill");
		if (objectp->isAttachment() &&
			LLVOAvatar::findAvatarFromAttachment(objectp) == gAgentAvatarp)
		{
			if (filter_kill && non_agent_region)
			{
				LL_DEBUGS("Attachment") << "Received kill-object message from non-agent region for agent attachment: "
										<< objectp->getID() << ". Ignoring."
									    << LL_ENDL;
				need_cof_resync = gIsInSecondLife;
				continue;
			}
			LLViewerObjectList::registerKilledAttachment(id);
			LL_DEBUGS("Attachment") << "Received kill object order for agent attachment: "
									<< objectp->getID()
									<< " - Delete object from cache = "
									<< (delete_object ? "true" : "false")
									<< LL_ENDL;
		}

		// Display green bubble on kill
		if (gShowObjectUpdates)
		{
			gPipeline.addDebugBlip(objectp->getPositionAgent(),
								   LLColor4::green);
		}

		// Do the kill
		gSelectMgr.removeObjectFromSelections(id);
		gObjectList.killObject(objectp);
		if (delete_object)
		{
			regionp->killCacheEntry(local_id);
		}
	}

	if (need_cof_resync)
	{
		gAppearanceMgr.incrementCofVersion();
		gAppearanceMgr.resetCOFUpdateTimer();
	}
}

void process_health_message(LLMessageSystem* msg, void**)
{
	F32 health;
	msg->getF32Fast(_PREHASH_HealthData, _PREHASH_Health, health);

	if (gStatusBarp)
	{
		gStatusBarp->setHealth((S32)health);
	}
}

void process_sim_stats(LLMessageSystem* msg, void**)
{
	S32 count = msg->getNumberOfBlocks(_PREHASH_Stat);
	U32 stat_id;
	F32 stat_value;
	for (S32 i = 0; i < count; ++i)
	{
		msg->getU32(_PREHASH_Stat, _PREHASH_StatID, stat_id, i);
		msg->getF32(_PREHASH_Stat, _PREHASH_StatValue, stat_value, i);
		gViewerStats.addSample(stat_id, stat_value);
	}

	// Various hacks that are not statistics, but are being handled here.

	U32 max_tasks;
	msg->getU32(_PREHASH_Region, _PREHASH_ObjectCapacity, max_tasks);

	U64 region_flags = 0;
	if (msg->has(_PREHASH_RegionInfo))
	{
		msg->getU64(_PREHASH_RegionInfo, _PREHASH_RegionFlagsExtended,
					region_flags);
	}
	else
	{
		U32 flags = 0;
		msg->getU32(_PREHASH_Region, _PREHASH_RegionFlags, flags);
		region_flags = flags;
	}

	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		bool was_flying = gAgent.getFlying();
		regionp->setRegionFlags(region_flags);
		regionp->setMaxTasks(max_tasks);
		// *HACK: this makes agent drop from the sky if flying and the region
		// is set to no fly.
		if (was_flying && regionp->getBlockFly())
		{
			gAgent.setFlying(gAgent.canFly());
		}
	}
}

static void handle_puppetry_data(LLMessageSystem* msg, LLVOAvatar* avatarp,
								 S32 num_physav_blocks)
{
	LLPuppetMotion* motionp = avatarp->getPuppetMotion();
	if (!motionp)
	{
		return;
	}

	for (S32 i = 0; i < num_physav_blocks; ++i)
	{
		S32 data_size = msg->getSizeFast(_PREHASH_PhysicalAvatarEventList,
										 i, _PREHASH_TypeData);
		if (data_size > 0)
		{
			motionp->unpackEvents(msg, i);
		}
	}

	if (!motionp->isActive() && motionp->needsUpdate())
	{
		avatarp->startMotion(ANIM_AGENT_PUPPET_MOTION);
	}
}

void process_avatar_animation(LLMessageSystem* msg, void**)
{
	LLUUID uuid;
	msg->getUUIDFast(_PREHASH_Sender, _PREHASH_ID, uuid);
	LLVOAvatar* avatarp = gObjectList.findAvatar(uuid);
	if (!avatarp)
	{
		// No agent by this Id...
		llwarns_once << "Received animation state for unknown avatar " << uuid
					 << llendl;
		return;
	}

	// Clear animation flags
	avatarp->mSignaledAnimations.clear();

	S32 num_blocks = msg->getNumberOfBlocksFast(_PREHASH_AnimationList);
	if (num_blocks <= 0) return;

	S32 num_source_blocks =
		msg->getNumberOfBlocksFast(_PREHASH_AnimationSourceList);

	S32 num_physav_blocks =
		msg->getNumberOfBlocksFast(_PREHASH_PhysicalAvatarEventList);

	LLUUID animation_id;
	S32 anim_sequence_id;
	if (avatarp->isSelf())
	{
		LLUUID object_id;

		for (S32 i = 0; i < num_blocks; ++i)
		{
			msg->getUUIDFast(_PREHASH_AnimationList, _PREHASH_AnimID,
							 animation_id, i);
			msg->getS32Fast(_PREHASH_AnimationList, _PREHASH_AnimSequenceID,
							anim_sequence_id, i);

			LL_DEBUGS("Messaging") << "Anim sequence ID: " << anim_sequence_id
								   << LL_ENDL;

			avatarp->mSignaledAnimations[animation_id] = anim_sequence_id;

			if (i < num_source_blocks)
			{
				msg->getUUIDFast(_PREHASH_AnimationSourceList,
								 _PREHASH_ObjectID, object_id, i);

				LLViewerObject* object = gObjectList.findObject(object_id);
				if (object)
				{
					object->setFlagsWithoutUpdate(FLAGS_ANIM_SOURCE, true);

					bool anim_found = false;
					for (LLVOAvatar::anim_src_map_it_t
							it = avatarp->mAnimationSources.find(object_id),
							end = avatarp->mAnimationSources.end();
						 it != end; ++it)
					{
						if (it->first != object_id)
						{
							// Elements with the same key are always
							// contiguous, bail if we went past the end of this
							// object's animations.
							break;
						}
						if (it->second == animation_id)
						{
							anim_found = true;
							break;
						}
					}

					if (!anim_found)
					{
						avatarp->mAnimationSources.emplace(object_id,
														   animation_id);
					}
				}
			}
		}

		if (LLPuppetMotion::enabled() &&
			LLPuppetModule::getInstance()->getEcho())
		{
			handle_puppetry_data(msg, avatarp, num_physav_blocks);
		}
	}
	else
	{
		for (S32 i = 0; i < num_blocks; ++i)
		{
			msg->getUUIDFast(_PREHASH_AnimationList, _PREHASH_AnimID,
							 animation_id, i);
			msg->getS32Fast(_PREHASH_AnimationList, _PREHASH_AnimSequenceID,
							anim_sequence_id, i);
			avatarp->mSignaledAnimations[animation_id] = anim_sequence_id;
		}

		if (LLPuppetMotion::enabled())
		{
	        // Extract and process puppetry data from message
    	    handle_puppetry_data(msg, avatarp, num_physav_blocks);
		}
	}

	avatarp->processAnimationStateChanges();
}

void process_object_animation(LLMessageSystem* msg, void**)
{
	LLUUID uuid;
	msg->getUUIDFast(_PREHASH_Sender, _PREHASH_ID, uuid);

	S32 num_blocks = msg->getNumberOfBlocksFast(_PREHASH_AnimationList);
	LL_DEBUGS("Messaging") << "Handling object animation requests for object: "
						   << uuid << " - num_blocks = " << num_blocks
						   << LL_ENDL;
	LLVOAvatar::anim_map_t signaled_anims;
	LLUUID animation_id;
	S32 anim_sequence_id;
	for (S32 i = 0; i < num_blocks; ++i)
	{
		msg->getUUIDFast(_PREHASH_AnimationList, _PREHASH_AnimID, animation_id,
						 i);
		msg->getS32Fast(_PREHASH_AnimationList, _PREHASH_AnimSequenceID,
						anim_sequence_id, i);
		signaled_anims[animation_id] = anim_sequence_id;
		LL_DEBUGS("Messaging") << " - got request for animation: "
							   << animation_id << LL_ENDL;
	}
	// Note: do *NOT* use emplace(uuid, std::move(signaled_anims)). For some
	// reason, it does not work as expected (failure to start some animesh
	// anims)... HB
	LLVOAvatarPuppet::getSignaledAnimMap()[uuid] = signaled_anims;
	LL_DEBUGS("Puppets") << "Object animation requests handlded." << LL_ENDL;

	LLViewerObject* objp = gObjectList.findObject(uuid);
	if (!objp || objp->isDead())
	{
		// This case is fairly common (on login and TPs, i.e. when not all
		// objects data has been received) and not critical at all. Changed to
		// a debug message to avoid log spam. HB
		LL_DEBUGS("Messaging") << "Received animation state for unknown object: "
							   << uuid << LL_ENDL;
		return;
	}

	LLVOVolume* volp = objp->asVolume();
	if (!volp)
	{
		llwarns_once << "Received animation state for non-volume object: "
					 << uuid << llendl;
		return;
	}

	if (!volp->isAnimatedObject())
	{
		llwarns_once << "Received animation state for non-animated object: "
					 << uuid << llendl;
		return;
	}

	volp->updatePuppetAvatar();

	LLVOAvatarPuppet* avatarp = volp->getPuppetAvatar();
	if (!avatarp)
	{
		llinfos_once << "No puppet avatar for object: " << uuid
					 << ". Ignoring." << llendl;
		return;
	}

	if (!avatarp->mPlaying)
	{
		avatarp->mPlaying = true;
		if (avatarp->mRootVolp)
		{
#if 0
			if (!avatarp->mRootVolp->isAnySelected())
#endif
			{
				avatarp->updateVolumeGeom();
				avatarp->mRootVolp->recursiveMarkForUpdate();
			}
		}
	}

	avatarp->updateAnimations();
}

void process_avatar_appearance(LLMessageSystem* msg, void**)
{
	LLUUID uuid;
	msg->getUUIDFast(_PREHASH_Sender, _PREHASH_ID, uuid);

	LLVOAvatar* avatarp = gObjectList.findAvatar(uuid);
	if (avatarp)
	{
		avatarp->processAvatarAppearance(msg);
	}
	else
	{
		llwarns << "Avatar appearance message received for unknown avatar "
				<< uuid << llendl;
	}
}

void process_camera_constraint(LLMessageSystem* msg, void**)
{
	LLVector4 plane;
	msg->getVector4Fast(_PREHASH_CameraCollidePlane, _PREHASH_Plane, plane);
	gAgent.setCameraCollidePlane(plane);
}

void near_sit_object(bool success, void*)
{
	if (success)
	{
		// Send message to sit on object
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_AgentSit);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		gAgent.sendReliableMessage();
	}
}

void process_avatar_sit_response(LLMessageSystem* msg, void**)
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	LLUUID seat_id;
	msg->getUUIDFast(_PREHASH_SitObject, _PREHASH_ID, seat_id);
	bool use_autopilot;
	msg->getBoolFast(_PREHASH_SitTransform, _PREHASH_AutoPilot, use_autopilot);
	LLVector3 sit_pos;
	msg->getVector3Fast(_PREHASH_SitTransform, _PREHASH_SitPosition, sit_pos);
	LLQuaternion sit_rot;
	msg->getQuatFast(_PREHASH_SitTransform, _PREHASH_SitRotation, sit_rot);
	LLVector3 camera_eye;
	msg->getVector3Fast(_PREHASH_SitTransform, _PREHASH_CameraEyeOffset,
						camera_eye);
	LLVector3 camera_at;
	msg->getVector3Fast(_PREHASH_SitTransform, _PREHASH_CameraAtOffset,
						camera_at);
	bool force_mouselook;
	msg->getBoolFast(_PREHASH_SitTransform, _PREHASH_ForceMouselook,
					 force_mouselook);

	if (dist_vec_squared(camera_eye, camera_at) > 0.0001f)
	{
		gAgent.setSitCamera(seat_id, camera_eye, camera_at);
	}

	gAgent.mForceMouselook = force_mouselook;

	LLViewerObject* object = gObjectList.findObject(seat_id);
	if (!object)
	{
		llwarns << "Received sit approval for unknown object " << seat_id
				<< llendl;
		return;
	}

	// If not allowed to use the auto-pilot, bail now.
	if (!use_autopilot)
	{
		return;
	}

	// If we are not already sitting on this object, we may autopilot
	if (!gAgentAvatarp->mIsSitting ||
		gAgentAvatarp->getRoot() != object->getRoot())
	{
		LLVector3 sit_spot = object->getPositionAgent() +
							 sit_pos * object->getRotation();
		gAgentPilot.startAutoPilotGlobal(gAgent.getPosGlobalFromAgent(sit_spot),
										 "Sit", &sit_rot, near_sit_object,
										 NULL, 0.5f, gAgent.getFlying());
	}
}

void process_clear_follow_cam_properties(LLMessageSystem* msg, void**)
{
	LLUUID source_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, source_id);
	LLFollowCamMgr::removeFollowCamParams(source_id);
}

void process_set_follow_cam_properties(LLMessageSystem* msg, void**)
{
	S32 type;
	F32 value;
	bool setting_pos = false;
	bool setting_focus = false;
	bool setting_focus_offset = false;
	LLVector3 position, focus, focus_offset;

	LLUUID source_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, source_id);
	LLViewerObject* objectp = gObjectList.findObject(source_id);
	if (objectp)
	{
		objectp->setFlagsWithoutUpdate(FLAGS_CAMERA_SOURCE, true);
	}

	S32 num_objects = msg->getNumberOfBlocks(_PREHASH_CameraProperty);
	for (S32 block_index = 0; block_index < num_objects; ++block_index)
	{
		msg->getS32(_PREHASH_CameraProperty, _PREHASH_Type, type, block_index);
		msg->getF32(_PREHASH_CameraProperty, _PREHASH_Value, value,
					block_index);
		switch (type)
		{
			case FOLLOWCAM_PITCH:
				LLFollowCamMgr::setPitch(source_id, value);
				break;

			case FOLLOWCAM_FOCUS_OFFSET_X:
				focus_offset.mV[VX] = value;
				setting_focus_offset = true;
				break;

			case FOLLOWCAM_FOCUS_OFFSET_Y:
				focus_offset.mV[VY] = value;
				setting_focus_offset = true;
				break;

			case FOLLOWCAM_FOCUS_OFFSET_Z:
				focus_offset.mV[VZ] = value;
				setting_focus_offset = true;
				break;

			case FOLLOWCAM_POSITION_LAG:
				LLFollowCamMgr::setPositionLag(source_id, value);
				break;

			case FOLLOWCAM_FOCUS_LAG:
				LLFollowCamMgr::setFocusLag(source_id, value);
				break;

			case FOLLOWCAM_DISTANCE:
				LLFollowCamMgr::setDistance(source_id, value);
				break;

			case FOLLOWCAM_BEHINDNESS_ANGLE:
				LLFollowCamMgr::setBehindnessAngle(source_id, value);
				break;

			case FOLLOWCAM_BEHINDNESS_LAG:
				LLFollowCamMgr::setBehindnessLag(source_id, value);
				break;

			case FOLLOWCAM_POSITION_THRESHOLD:
				LLFollowCamMgr::setPositionThreshold(source_id, value);
				break;

			case FOLLOWCAM_FOCUS_THRESHOLD:
				LLFollowCamMgr::setFocusThreshold(source_id, value);
				break;

			case FOLLOWCAM_ACTIVE:
				LLFollowCamMgr::setCameraActive(source_id, value != 0.f);
				break;

			case FOLLOWCAM_POSITION_X:
				setting_pos = true;
				position.mV[0] = value;
				break;

			case FOLLOWCAM_POSITION_Y:
				setting_pos = true;
				position.mV[1] = value;
				break;

			case FOLLOWCAM_POSITION_Z:
				setting_pos = true;
				position.mV[2] = value;
				break;

			case FOLLOWCAM_FOCUS_X:
				setting_focus = true;
				focus.mV[0] = value;
				break;

			case FOLLOWCAM_FOCUS_Y:
				setting_focus = true;
				focus.mV[1] = value;
				break;

			case FOLLOWCAM_FOCUS_Z:
				setting_focus = true;
				focus.mV[2] = value;
				break;

			case FOLLOWCAM_POSITION_LOCKED:
				LLFollowCamMgr::setPositionLocked(source_id, value != 0.f);
				break;

			case FOLLOWCAM_FOCUS_LOCKED:
				LLFollowCamMgr::setFocusLocked(source_id, value != 0.f);
				break;

			default:
				break;
		}
	}

	if (setting_pos)
	{
		LLFollowCamMgr::setPosition(source_id, position);
	}
	if (setting_focus)
	{
		LLFollowCamMgr::setFocus(source_id, focus);
	}
	if (setting_focus_offset)
	{
		LLFollowCamMgr::setFocusOffset(source_id, focus_offset);
	}
}

void process_name_value(LLMessageSystem* msg, void**)
{
	LLUUID id;
	msg->getUUIDFast(_PREHASH_TaskData, _PREHASH_ID, id);
	LLViewerObject* object = gObjectList.findObject(id);
	if (object)
	{
		std::string temp_str;
		S32 num_blocks;
		num_blocks = msg->getNumberOfBlocksFast(_PREHASH_NameValueData);
		for (S32 i = 0; i < num_blocks; ++i)
		{
			msg->getStringFast(_PREHASH_NameValueData, _PREHASH_NVPair,
							   temp_str, i);
			llinfos << "Added to object Name Value: " << temp_str << llendl;
			object->addNVPair(temp_str);
		}
	}
	else
	{
		llinfos << "Cannot find object " << id << " to add name value pair"
				<< llendl;
	}
}

void process_remove_name_value(LLMessageSystem* msg, void**)
{
	LLUUID id;
	msg->getUUIDFast(_PREHASH_TaskData, _PREHASH_ID, id);
	LLViewerObject* object = gObjectList.findObject(id);
	if (object)
	{
		std::string	temp_str;
		S32 num_blocks;
		num_blocks = msg->getNumberOfBlocksFast(_PREHASH_NameValueData);
		for (S32 i = 0; i < num_blocks; ++i)
		{
			msg->getStringFast(_PREHASH_NameValueData, _PREHASH_NVPair,
							   temp_str, i);
			llinfos << "Removed from object Name Value: " << temp_str
					<< llendl;
			object->removeNVPair(temp_str);
		}
	}
	else
	{
		llinfos << "Cannot find object " << id << " to remove name value pair"
				<< llendl;
	}
}

void process_kick_user(LLMessageSystem* msg, void**)
{
	std::string message;
	msg->getStringFast(_PREHASH_UserInfo, _PREHASH_Reason, message);
	gAppViewerp->forceDisconnect(message);
}

void set_god_level(U8 god_level)
{
	U8 old_god_level = gAgent.getGodLevel();
	gAgent.setGodLevel(god_level);

	if (gIMMgrp)
	{
		gIMMgrp->refresh();
	}

	gViewerParcelMgr.notifySelectionObservers();

	// Some classifieds change visibility on god mode
	HBFloaterSearch::requestClassifieds();

	// God mode changes region visibility
	gWorldMap.reset();
	gWorldMap.setCurrentLayer(0);

	// Inventory in items may change in god mode
	gObjectList.dirtyAllObjectInventory();

	if (gViewerWindowp)
	{
		gViewerWindowp->setMenuBackgroundColor();
	}

	LLSD args;
	if (god_level > GOD_NOT)
	{
		args["LEVEL"] = llformat("%d", (S32)god_level);
		gNotifications.add("EnteringGodMode", args);
	}
	else
	{
		args["LEVEL"] = llformat("%d", (S32)old_god_level);
		gNotifications.add("LeavingGodMode", args);
	}

	// Changing god-level can affect which menus we see
	show_debug_menus();
}

void process_grant_godlike_powers(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	if (agent_id == gAgentID && session_id == gAgentSessionID)
	{
		U8 god_level;
		msg->getU8Fast(_PREHASH_GrantData, _PREHASH_GodLevel, god_level);
		set_god_level(god_level);
	}
	else
	{
		llwarns << "Grant godlike received for wrong agent: " << agent_id
				<< ". Ignored." << llendl;
	}
}

static std::string reason_from_transaction_type(S32 transaction_type,
												const std::string& item_desc)
{
	// *NOTE: The keys for the reason strings are unusual because an earlier
	// version of the code used English language strings extracted from hard-
	// coded server English descriptions. Keeping them so we do not have to
	// re-localize them.
	switch (transaction_type)
	{
		case TRANS_OBJECT_SALE:
		{
			LLStringUtil::format_map_t arg;
			arg["ITEM"] = item_desc;
			return LLTrans::getString("for item", arg);
		}

		case TRANS_LAND_SALE:
			return LLTrans::getString("for a parcel of land");

		case TRANS_LAND_PASS_SALE:
			return LLTrans::getString("for a land access pass");

		case TRANS_GROUP_LAND_DEED:
			return LLTrans::getString("for deeding land");

		case TRANS_GROUP_CREATE:
			return LLTrans::getString("to create a group");

		case TRANS_GROUP_JOIN:
			return LLTrans::getString("to join a group");

		case TRANS_UPLOAD_CHARGE:
			return LLTrans::getString("to upload");

		case TRANS_CLASSIFIED_CHARGE:
			return LLTrans::getString("to publish a classified ad");

		case TRANS_GIFT:
			return item_desc == "Payment" ? std::string() : item_desc;

		// These have no reason to display, but are expected and should not
		// generate warnings
		case TRANS_PAY_OBJECT:
		case TRANS_OBJECT_PAYS:
			return "";

		default:
			llwarns << "Unknown transaction type " << transaction_type
					<< llendl;
			return "";
	}
}

static void money_balance_group_notify(const LLUUID& group_id,
									   const std::string& name,
									   bool is_group,
									   LLSD args,
									   std::string message)
{
	args["NAME"] = name;
	message = LLTrans::getString(message);
	LLStringUtil::format(message, args);
	args.clear();
	args["MESSAGE"] = message;
	gNotifications.add("SystemMessage", args);
}

static void money_balance_avatar_notify(const LLUUID& agent_id,
										const LLAvatarName& av_name,
										LLSD args, std::string message)
{
	if (LLAvatarNameCache::useDisplayNames())
	{
		args["NAME"] = av_name.getNames();
	}
	else
	{
		args["NAME"] = av_name.getLegacyName();
	}
	message = LLTrans::getString(message);
	LLStringUtil::format(message, args);
	args.clear();
	args["MESSAGE"] = message;
	gNotifications.add("SystemMessage", args);
}

// Added in server 1.40 and viewer 2.1, support for localization and agent Ids
// for name lookup.
static void process_money_balance_reply_extended(LLMessageSystem* msg,
												 std::string desc)
{
	S32 transaction_type = 0;
	msg->getS32(_PREHASH_TransactionInfo, _PREHASH_TransactionType,
				transaction_type);
	LLUUID source_id;
	msg->getUUID(_PREHASH_TransactionInfo, _PREHASH_SourceID, source_id);
	bool is_source_group = false;
	msg->getBool(_PREHASH_TransactionInfo, _PREHASH_IsSourceGroup,
				 is_source_group);
	LLUUID dest_id;
	msg->getUUID(_PREHASH_TransactionInfo, _PREHASH_DestID, dest_id);
	bool is_dest_group = false;
	msg->getBool(_PREHASH_TransactionInfo, _PREHASH_IsDestGroup,
				 is_dest_group);
	S32 amount = 0;
	msg->getS32(_PREHASH_TransactionInfo, _PREHASH_Amount, amount);
	std::string item_description;
	msg->getString(_PREHASH_TransactionInfo, _PREHASH_ItemDescription,
				   item_description);
	bool success = false;
	msg->getBool(_PREHASH_MoneyData, _PREHASH_TransactionSuccess, success);

	llinfos << "MoneyBalanceReply source " << source_id << " dest " << dest_id
			<< " type " << transaction_type << " item " << item_description
			<< llendl;

	LLSD args;
	if (source_id.isNull() && dest_id.isNull())
	{
		// this is a pure balance update, use the already built message
		args["MESSAGE"] = desc;
		gNotifications.add("SystemMessage", args);
		return;
	}

	std::string reason = reason_from_transaction_type(transaction_type,
													  item_description);

	args["REASON"] = reason; // could be empty
	args["AMOUNT"] = llformat("%d", amount);

	// Need to delay until name looked up, so need to know whether it is a
	// group or not
	bool is_name_group = false;
	LLUUID name_id;
	std::string message;
	if (source_id == gAgentID)
	{
		// You paid someone...
		is_name_group = is_dest_group;
		name_id = dest_id;
		if (!reason.empty())
		{
			if (dest_id.notNull())
			{
				message = success ? "you_paid_ldollars"
								  : "you_paid_failure_ldollars";
				if (transaction_type == TRANS_GIFT)
				{
					message += "_gift";
				}
			}
			else
			{
				// Transaction fee to the system, eg, to create a group
				message = success ? "you_paid_ldollars_no_name"
								  : "you_paid_failure_ldollars_no_name";
			}
		}
		else if (dest_id.notNull())
		{
			message = success ? "you_paid_ldollars_no_reason"
							  : "you_paid_failure_ldollars_no_reason";
		}
		else
		{
			// No target, no reason, you just paid money
			message = success ? "you_paid_ldollars_no_info"
							  : "you_paid_failure_ldollars_no_info";
		}
	}
	else
	{
		// ...someone paid you
		is_name_group = is_source_group;
		name_id = source_id;
		if (!reason.empty() && !LLMuteList::isMuted(source_id))
		{
			message ="paid_you_ldollars";
			if (transaction_type == TRANS_GIFT)
			{
				message += "_gift";
			}
		}
		else
		{
			message = "paid_you_ldollars_no_reason";
		}
	}

	// Wait until the name is available before showing the notification
	if (!is_name_group)
	{
		LLAvatarNameCache::get(name_id,
							   boost::bind(&money_balance_avatar_notify, _1,
										   _2, args, message));
	}
	else if (gCacheNamep)
	{
		gCacheNamep->get(name_id, true,
						 boost::bind(&money_balance_group_notify, _1, _2, _3,
									 args, message));
	}
}

void process_money_balance_reply(LLMessageSystem* msg, void**)
{
	S32 balance = 0;
	msg->getS32(_PREHASH_MoneyData, _PREHASH_MoneyBalance, balance);
	S32 credit = 0;
	msg->getS32(_PREHASH_MoneyData, _PREHASH_SquareMetersCredit, credit);
	S32 committed = 0;
	msg->getS32(_PREHASH_MoneyData, _PREHASH_SquareMetersCommitted, committed);
	std::string desc;
	msg->getStringFast(_PREHASH_MoneyData, _PREHASH_Description, desc);
	llinfos << "L$, credit, committed: " << balance << " " << credit << " "
			<< committed << llendl;

	S32 old_balance = -1;
	if (gStatusBarp)
	{
		old_balance = gStatusBarp->getBalance();

		// This is an update, not the first transmission of balance
		if (old_balance != 0)
		{
			// This is actually an update
			if (balance > old_balance)
			{
				LLFirstUse::useBalanceIncrease(balance - old_balance);
			}
			else if (balance < old_balance)
			{
				LLFirstUse::useBalanceDecrease(balance - old_balance);
			}
		}

		gStatusBarp->setBalance(balance);
		gStatusBarp->setLandCredit(credit);
		gStatusBarp->setLandCommitted(committed);
	}

	LLUUID tid;
	msg->getUUID(_PREHASH_MoneyData, _PREHASH_TransactionID, tid);
	static std::deque<LLUUID> recent;
	if (gSavedSettings.getBool("NotifyMoneyChange") &&
	 	std::find(recent.rbegin(), recent.rend(), tid) == recent.rend())
	{
		// Confirm the transaction to the user, since they might have missed
		// something during an event, or this may be an out-world transaction.
		if (desc.empty())
		{
			// Out-world transaction.
			if (balance == old_balance || old_balance <= 0)
			{
				return;
			}
			if (balance > old_balance)
			{
				desc = LLTrans::getString("money_balance_increased");
			}
			else
			{
				desc = LLTrans::getString("money_balance_decreased");
			}
			LLSD args;
			args["AMOUNT"] = abs(balance - old_balance);
			LLStringUtil::format(desc, args);
		}

		// Once the 'recent' container gets large enough, chop some off the
		// beginning.
		constexpr U32 MAX_LOOKBACK = 30;
		constexpr S32 POP_FRONT_SIZE = 12;
		if (recent.size() > MAX_LOOKBACK)
		{
			LL_DEBUGS("Messaging") << "Removing oldest transaction records"
								   << LL_ENDL;
			recent.erase(recent.begin(), recent.begin() + POP_FRONT_SIZE);
		}
		LL_DEBUGS("Messaging") << "Pushing back transaction " << tid
							   << LL_ENDL;
		recent.emplace_back(tid);

		if (msg->has(_PREHASH_TransactionInfo))
		{
			// ...message has extended info for localization
			process_money_balance_reply_extended(msg, desc);
		}
		else
		{
			// Old grids will not supply the TransactionInfo block, so we can
			// just use the hard-coded English string.
			LLSD args;
			args["MESSAGE"] = desc;
			gNotifications.add("SystemMessage", args);
		}
	}
}

bool handle_special_notification_callback(const LLSD& notification,
										  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		// Set the preference to the maturity of the region we are calling
		U32 maturity = notification["payload"]["_region_access"].asInteger();
		gSavedSettings.setU32("PreferredMaturity", maturity);
		gAgent.sendMaturityPreferenceToServer(maturity);
	}
	return false;
}

// Some of the server notifications need special handling. This is where we do
// that.
bool handle_special_notification(std::string notif_name, LLSD& data)
{
	if (data.has("_region_access"))
	{
		U8 region_access = (U8)data["_region_access"].asInteger();
		std::string maturity = LLViewerRegion::accessToString(region_access);
		LLStringUtil::toLower(maturity);
		data["REGIONMATURITY"] = maturity;

		// We are going to throw the LLSD in there in case anyone ever wants to
		// use it
		gNotifications.add(notif_name + "_Notify", data);

		if (region_access == SIM_ACCESS_MATURE)
		{
			if (gAgent.isTeen())
			{
				gNotifications.add(notif_name + "_KB", data);
				return true;
			}
			else if (gAgent.prefersPG())
			{
				gNotifications.add(notif_name + "_Change", data, data,
								   handle_special_notification_callback);
				return true;
			}
		}
		else if (region_access == SIM_ACCESS_ADULT)
		{
			if (!gAgent.isAdult())
			{
				gNotifications.add(notif_name + "_KB", data);
				return true;
			}
			else if (gAgent.prefersPG() || gAgent.prefersMature())
			{
				gNotifications.add(notif_name + "_Change", data, data,
								   handle_special_notification_callback);
				return true;
			}
		}
	}
	return false;
}

bool handle_trusted_experiences_notification(const LLSD& data)
{
	if (data.has("trusted_experiences"))
	{
		std::ostringstream str;
		const LLSD& experiences = data["trusted_experiences"];
		for (LLSD::array_const_iterator it = experiences.beginArray(),
										end = experiences.endArray();
			 it != end; ++it)
		{
			str << LLSLURL("experience", it->asUUID(),
						   "profile").getSLURLString() << "\n";
		}
		std::string str_list = str.str();
		if (!str_list.empty())
		{
			LLSD args;
			args["EXPERIENCE_LIST"] = (LLSD)str_list;
			gNotifications.add("TrustedExperiencesAvailable", args);
			return true;
		}
	}
	return false;
}

bool attempt_standard_notification(LLMessageSystem* msgsystem)
{
	// If we have additional alert data
	if (msgsystem->has(_PREHASH_AlertInfo) &&
		msgsystem->getNumberOfBlocksFast(_PREHASH_AlertInfo) > 0)
	{
		// Notification was specified using the new mechanism, so we can just
		// handle it here
		std::string notif_name;
		msgsystem->getStringFast(_PREHASH_AlertInfo, _PREHASH_Message,
								 notif_name);
		if (!gNotifications.templateExists(notif_name))
		{
			return false;
		}

		std::string raw_data;
		LLSD data;
		msgsystem->getStringFast(_PREHASH_AlertInfo, _PREHASH_ExtraParams,
								 raw_data);
		if (raw_data.length())
		{
			std::istringstream streamed(raw_data);
			if (!LLSDSerialize::deserialize(data, streamed, raw_data.length()))
			{
				llwarns << "Attempted to read notification parameter data into LLSD but failed: "
						<< raw_data << llendl;
			}
		}

		if (notif_name == "RegionEntryAccessBlocked" ||
			notif_name == "LandClaimAccessBlocked" ||
			notif_name == "LandBuyAccessBlocked")
		{
			/*-----------------------------------------------------------------
			 (Commented so a grep will find the notification strings, since
			 we construct them on the fly; if you add additional notifications,
			 please update the comment.)

			 Could throw any of the following notifications:

				RegionEntryAccessBlocked
				RegionEntryAccessBlocked_Notify
				RegionEntryAccessBlocked_Change
				RegionEntryAccessBlocked_KB
				LandClaimAccessBlocked
				LandClaimAccessBlocked_Notify
				LandClaimAccessBlocked_Change
				LandClaimAccessBlocked_KB
				LandBuyAccessBlocked
				LandBuyAccessBlocked_Notify
				LandBuyAccessBlocked_Change
				LandBuyAccessBlocked_KB
			-----------------------------------------------------------------*/
			if (handle_special_notification(notif_name, data))
			{
				return true;
			}
		}

		// Special Marketplace update notification
		if (notif_name == "SLM_UPDATE_FOLDER")
		{
			return LLMarketplace::processUpdateNotification(data);
		}

		gNotifications.add(notif_name, data);
		return true;
	}
	return false;
}

void process_agent_alert_message(LLMessageSystem* msgsystem, void**)
{
	// Make sure the cursor is back to the usual default since the alert is
	// probably due to some kind of error.
	gWindowp->resetBusyCount();

	if (!attempt_standard_notification(msgsystem))
	{
		bool modal = false;
		msgsystem->getBool(_PREHASH_AlertData, _PREHASH_Modal, modal);
		std::string buffer;
		msgsystem->getStringFast(_PREHASH_AlertData, _PREHASH_Message, buffer);
		process_alert_core(buffer, modal);
	}
}

// The only difference between this routine and the previous is the fact that
// for this routine, the modal parameter is always false. Sadly, for the
// message handled by this routine, there is no _PREHASH_Modal parameter on the
// message, and there is no API to tell if a message has the given parameter or
// not. So we cannot handle the messages with the same handler.
void process_alert_message(LLMessageSystem* msgsystem, void**)
{
	// Make sure the cursor is back to the usual default since the alert is
	// probably due to some kind of error.
	gWindowp->resetBusyCount();

	if (!attempt_standard_notification(msgsystem))
	{
		constexpr bool modal = false;
		std::string buffer;
		msgsystem->getStringFast(_PREHASH_AlertData, _PREHASH_Message, buffer);
		process_alert_core(buffer, modal);
	}
}

void process_alert_core(const std::string& message, bool modal)
{
	// *HACK: handle callbacks for specific alerts
	if (message == "You died and have been teleported to your home location")
	{
		gViewerStats.incStat(LLViewerStats::ST_KILLED_COUNT);
	}
	else if (message == "Home position set.")
	{
		// Save the home location image to disk
		std::string snap_filename = gDirUtilp->getLindenUserDir();
		snap_filename += LL_DIR_DELIM_STR + SCREEN_HOME_FILENAME;
		gViewerWindowp->saveSnapshot(snap_filename,
									 gViewerWindowp->getWindowDisplayWidth(),
									 gViewerWindowp->getWindowDisplayHeight(),
									 false, false);
	}

	static const std::string ALERT_PREFIX("ALERT: ");
	static const std::string NOTIFY_PREFIX("NOTIFY: ");
	if (message.find(ALERT_PREFIX) == 0)
	{
		// Allow the server to spawn a named alert so that server alerts can be
		// translated out of English.
		gNotifications.add(message.substr(ALERT_PREFIX.length()));
	}
	else if (message.find(NOTIFY_PREFIX) == 0)
	{
		// Allow the server to spawn a named notification so that server
		// notifications can be translated out of English.
		gNotifications.add(message.substr(NOTIFY_PREFIX.length()));
	}
	else if (message[0] == '/')		// System message
	{
		std::string text(message.substr(1));
		std::string prefix;
		if (text.length() > 17)
		{
			prefix = text.substr(0, 17);
		}
		LLSD args;
		if (prefix == "RESTART_X_MINUTES")
		{
			S32 mins = 0;
			LLStringUtil::convertToS32(text.substr(18), mins);
			args["MINUTES"] = llformat("%d", mins);
			gNotifications.add("RegionRestartMinutes", args);
		}
		else if (prefix == "RESTART_X_SECONDS")
		{
			S32 secs = 0;
			LLStringUtil::convertToS32(text.substr(18), secs);
			args["SECONDS"] = llformat("%d", secs);
			gNotifications.add("RegionRestartSeconds", args);
		}
		else
		{
			// *TODO:translate
			args["MESSAGE"] = text;
			gNotifications.add("SystemMessage", args);
		}
	}
	else if (modal)
	{
		// *TODO:translate
		LLSD args;
		args["ERROR_MESSAGE"] = message;
		gNotifications.add("ErrorMessage", args);
	}
	else if (message != "Autopilot canceled")	// Do not spam us with that !
	{
		// *TODO:translate
		LLSD args;
		args["MESSAGE"] = message;
		gNotifications.add("SystemMessageTip", args);
	}
}

void process_mean_collision_alert_message(LLMessageSystem* msgsystem, void**)
{
	if (gAgent.inPrelude())
	{
		// In prelude, bumping is OK. This dialog is rather confusing to
		// newbies, so we do not show it. Drop the packet on the floor.
		return;
	}

	LLUUID id;
	U32 time;
	U8 type;
	F32 mag;
	S32 count = msgsystem->getNumberOfBlocks(_PREHASH_MeanCollision);
	for (S32 i = 0; i < count; ++i)
	{
		msgsystem->getUUIDFast(_PREHASH_MeanCollision, _PREHASH_Perp, id);
		msgsystem->getU32Fast(_PREHASH_MeanCollision, _PREHASH_Time, time);
		msgsystem->getF32Fast(_PREHASH_MeanCollision, _PREHASH_Mag, mag);
		msgsystem->getU8Fast(_PREHASH_MeanCollision, _PREHASH_Type, type);

		HBFloaterBump::addMeanCollision(id, time, (EMeanCollisionType)type,
										mag);
		if (gAutomationp)
		{
			gAutomationp->onAgentPush(id, type, mag);
		}
	}
}

void process_frozen_message(LLMessageSystem* msgsystem, void**)
{
	// Make sure the cursor is back to the usual default since the alert is
	// probably due to some kind of error.
	gWindowp->resetBusyCount();

	bool b_frozen;
	msgsystem->getBool(_PREHASH_FrozenData, _PREHASH_Data, b_frozen);
	if (b_frozen)
	{
		// *TODO: make being frozen change view
		llwarns << "You have been frozen !" << llendl;
	}
	else
	{
		llinfos << "You have been un-frozen." << llendl;
	}
}

void process_economy_data(LLMessageSystem* msg, void**)
{
	LLEconomy::getInstance()->processEconomyData(msg);
	update_upload_costs_in_menus();
}

void notify_cautioned_script_question(const LLSD& notification,
									  const LLSD& response,
									  S32 orig_questions, bool granted)
{
	// Only continue if at least some permissions were requested
	if (orig_questions)
	{
		// Check to see if the person we are asking
		// "'[OBJECTNAME]', an object owned by '[OWNERNAME]',
		// located in [REGIONNAME] at [REGIONPOS],
		// has been <granted|denied> permission to: [PERMISSIONS]."

		LLUIString notice(LLTrans::getString(granted ? "ScriptQuestionCautionChatGranted"
													 : "ScriptQuestionCautionChatDenied"));

		std::string object_name =
			notification["payload"]["object_name"].asString();
		std::string owner_name =
			notification["payload"]["owner_name"].asString();
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			object_name = gRLInterface.getCensoredMessage(object_name);
			owner_name = gRLInterface.getDummyName(owner_name);
		}
//mk
		// Always include the object name and owner name
		notice.setArg("[OBJECTNAME]", object_name);
		notice.setArg("[OWNERNAME]", owner_name);

		// Try to lookup viewerobject that corresponds to the object that
		// requested permissions (here, taskid->requesting object id)
		bool foundpos = false;
		LLViewerObject* vobj =
			gObjectList.findObject(notification["payload"]["task_id"].asUUID());
		if (vobj)
		{
			// Found the viewerobject, get its position in its region
			LLVector3 objpos(vobj->getPosition());

			// Try to lookup the name of the region the object is in
			LLViewerRegion* regionp = vobj->getRegion();
			if (regionp)
			{
				// Got the region, so include the region and 3d coordinates of
				// the object
//MK
				if (gRLenabled && gRLInterface.mContainsShowloc)
				{
					notice.setArg("[REGIONNAME]", "(Region hidden)");
				}
				else
				{
//mk
					notice.setArg("[REGIONNAME]", regionp->getName());
//MK
				}
//mk
				std::string formatpos = llformat("%.1f, %.1f,%.1f",
												 objpos[VX], objpos[VY],
												 objpos[VZ]);
				notice.setArg("[REGIONPOS]", formatpos);

				foundpos = true;
			}
		}

		if (!foundpos)
		{
			// Unable to determine location of the object
			notice.setArg("[REGIONNAME]", "(unknown region)");
			notice.setArg("[REGIONPOS]", "(unknown position)");
		}

		// Check each permission that was requested, and list each permission
		// that has been flagged as a caution permission
		bool caution = false;
		S32 count = 0;
		std::string perms;
		for (S32 i = 0; i < SCRIPT_PERMISSION_EOF; ++i)
		{
			if ((orig_questions & LSCRIPTRunTimePermissionBits[i]) &&
				SCRIPT_QUESTION_IS_CAUTION[i])
			{
				++count;
				caution = true;

				// Add a comma before the permission description if it is not
				// the first permission added to the list or the last
				// permission to check
				if (count > 1 && i < SCRIPT_PERMISSION_EOF)
				{
					perms.append(", ");
				}

				perms.append(LLTrans::getString(SCRIPT_QUESTIONS[i]));
			}
		}

		notice.setArg("[PERMISSIONS]", perms);

		// Log a chat message as long as at least one requested permission
		// is a caution permission
		if (caution)
		{
			LLChat chat(notice.getString());
			LLFloaterChat::addChat(chat, false, false);
		}
	}
}

// Purge the message queue of any previously queued requests from the same
// source. DEV-4879
class QuestionMatcher : public LLNotifyBoxView::Matcher
{
public:
	QuestionMatcher(const LLUUID& to_block)
	:	mBlockedId(to_block)
	{
	}

	bool matches(const LLNotificationPtr notif) const
	{
		// We do not test for ScriptQuestionOurs neither for
		// ScriptQuestionCaution because these come from our objects which are
		// not mutable (if we got a Mute, it can only come from someone else's
		// object via ScriptQuestion)
		const std::string& name = notif->getName();
		if (name == "ScriptQuestion")
		{
			return notif->getPayload()["task_id"].asUUID() == mBlockedId;
		}
		return false;
	}

private:
	LLUUID mBlockedId;
};

void script_question_mute(const LLUUID& task_id,
						  const std::string& object_name)
{
	LLMute mute(task_id, object_name, LLMute::OBJECT);
	LLMuteList::add(mute);
	LLFloaterMute::selectMute(mute.mID);

	// Should do this via the channel
	gNotifyBoxViewp->purgeMessagesMatching(QuestionMatcher(task_id));
}

void block_experience(const LLUUID& exp_id, const LLSD& result)
{
	LLSD permission;
	permission["permission"] = "Block";
	LLSD data;
	data[exp_id.asString()] = permission;
	data["experience"] = exp_id;
	gEventPumps.obtain(PUMP_EXPERIENCE).post(data);
}

bool script_question_cb(const LLSD& notification, const LLSD& response)
{
	S32 orig = notification["payload"]["questions"].asInteger();
	S32 new_questions = orig;

	if (response["Details"])
	{
		// Respawn notification...
		gNotifications.add(notification["name"], notification["substitutions"],
						   notification["payload"]);

		// ... with description on top
		gNotifications.add("DebitPermissionDetails");
		return false;
	}

	LLUUID exp_id;
	if (notification["payload"].has("experience"))
	{
		exp_id = notification["payload"]["experience"].asUUID();
	}

	// Check whether permissions were granted or denied
	bool allowed = true;
	if (LLNotification::getSelectedOption(notification, response) != 0)
	{
		// The "yes/accept" button is the first button in the template, making
		// it button 0; if any other button was clicked, the permissions were
		// denied.
		new_questions = 0;
		allowed = false;
	}
	else if (exp_id.notNull())
	{
		LLSD permission;
		permission["permission"] = "Allow";
		LLSD data;
		data[exp_id.asString()] = permission;
		data["experience"] = exp_id;
		gEventPumps.obtain(PUMP_EXPERIENCE).post(data);
	}

	LLUUID task_id = notification["payload"]["task_id"].asUUID();
	LLUUID item_id = notification["payload"]["item_id"].asUUID();

	// Reply with the permissions granted or denied
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ScriptAnswerYes);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_Data);
	msg->addUUIDFast(_PREHASH_TaskID, task_id);
	msg->addUUIDFast(_PREHASH_ItemID, item_id);
	msg->addS32Fast(_PREHASH_Questions, new_questions);
	msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));

	// Only log a chat message if caution prompts are enabled
	if (gSavedSettings.getBool("PermissionsCautionEnabled"))
	{
		// Log a chat message, if appropriate
		notify_cautioned_script_question(notification, response, orig,
										 allowed);
	}

	if (response["client_side_mute"])	// Mute from ScriptQuestion
	{
		std::string name = notification["payload"]["object_name"].asString();
		script_question_mute(task_id, name);
	}
	if (response["BlockExperience"] && exp_id.notNull())
	{
		LLExperienceCache* cachep = LLExperienceCache::getInstance();
		cachep->setExperiencePermission(exp_id, "Block",
										boost::bind(&block_experience,
													exp_id, _1));
	}

	return false;
}
static LLNotificationFunctorRegistration script_question_cb_reg_1("ScriptQuestion",
																  script_question_cb);
static LLNotificationFunctorRegistration script_question_cb_reg_2("ScriptQuestionOurs",
																  script_question_cb);
static LLNotificationFunctorRegistration script_question_cb_reg_3("ScriptQuestionCaution",
																  script_question_cb);
static LLNotificationFunctorRegistration script_question_cb_reg_4("ScriptQuestionExperience",
																  script_question_cb);

void process_script_experience_details(const LLSD& experience_details,
									   LLSD args, LLSD payload)
{
	if (experience_details[LLExperienceCache::PROPERTIES].asInteger() &
		LLExperienceCache::PROPERTY_GRID)
	{
		args["GRID_WIDE"] = LLTrans::getString("Grid-Scope");
	}
	else
	{
		args["GRID_WIDE"] = LLTrans::getString("Land-Scope");
	}

	std::string experience =
		LLSLURL("experience",
				experience_details[LLExperienceCache::EXPERIENCE_ID].asUUID(),
				"profile").getSLURLString();
	args["EXPERIENCE"] = experience;

	gNotifications.add("ScriptQuestionExperience", args, payload);
}

void process_script_question(LLMessageSystem* msg, void**)
{
	// *TODO:translate owner name -> [FIRST] [LAST]

	// taskid -> object key of object requesting permissions
	LLUUID taskid;
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_TaskID, taskid);
	// itemid -> script asset key of script requesting permissions
	LLUUID itemid;
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_ItemID, itemid);
	std::string object_name;
	msg->getStringFast(_PREHASH_Data, _PREHASH_ObjectName, object_name);
	std::string owner_name;
	msg->getStringFast(_PREHASH_Data, _PREHASH_ObjectOwner, owner_name);
	S32 questions;
	msg->getS32Fast(_PREHASH_Data, _PREHASH_Questions, questions);
	LLUUID experienceid;
	if (msg->has(_PREHASH_Experience))
	{
		msg->getUUIDFast(_PREHASH_Experience, _PREHASH_ExperienceID,
						 experienceid);
	}

	// Special case. If the objects are owned by this agent, throttle per-
	// object instead of per-owner. It is common for residents to reset a ton
	// of scripts that re-request permissions, as with tier boxes. UUIDs cannot
	// be valid agent names and vice-versa, so we will reuse the same namespace
	// for both throttle types.
	std::string throttle_name = owner_name;
	std::string self_name;
	gAgent.getName(self_name);
	bool is_ours = owner_name == self_name ||
				   (gIsInSecondLife && owner_name == self_name + " Resident");
	if (is_ours)
	{
		throttle_name = taskid.asString();
	}

	// Do not display permission requests if this object is muted by Id, by
	// name, or by owner name (agent or group)
	if (LLMuteList::isMuted(taskid, object_name) ||
		LLMuteList::isMuted(LLUUID::null, owner_name, 0, LLMute::AGENT) ||
		LLMuteList::isMuted(LLUUID::null, owner_name, 0, LLMute::GROUP))
	{
		return;
	}

//MK
	bool auto_acceptable_permission = false;
	if (gRLenabled && gRLInterface.contains("acceptpermission"))
	{
		U32 perms =
			LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_TAKE_CONTROLS] |
			LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_TRIGGER_ANIMATION] |
			LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_ATTACH];

		auto_acceptable_permission = (questions	& perms);

		// Security check: if there is any other permission contained in this
		// package, we cannot automatically grant anything
		if (auto_acceptable_permission)
		{
			U32 other_perms = questions & ~perms;
			if (other_perms)
			{
				auto_acceptable_permission = false;
			}
			// Cannot accept animation permission if not sitting
			if (isAgentAvatarValid() && !gAgentAvatarp->mIsSitting &&
				(questions &
				 LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_TRIGGER_ANIMATION]))
			{
				auto_acceptable_permission = false;
			}
			// Never auto-accept temp-attach requests from others' objects,
			// unless RestrainedLoveAutoTempAttach is TRUE.
			if (!is_ours &&
				(questions &
				 LSCRIPTRunTimePermissionBits[SCRIPT_PERMISSION_ATTACH]) &&
				 !gSavedSettings.getBool("RestrainedLoveRelaxedTempAttach"))
			{
				auto_acceptable_permission = false;
			}
		}
	}
//mk

	// Throttle excessive requests from any specific user's scripts
	typedef LLKeyThrottle<std::string> LLStringThrottle;
	static LLStringThrottle throttling(LLREQUEST_PERMISSION_THROTTLE_LIMIT,
									   LLREQUEST_PERMISSION_THROTTLE_INTERVAL);
//MK
	// Do not throttle automatically accepted permissions
	if (!auto_acceptable_permission)
//mk
	{
		switch (throttling.noteAction(throttle_name))
		{
			case LLStringThrottle::THROTTLE_NEWLY_BLOCKED:
				llinfos << "Throttled script question for script pertaining to: "
						<< owner_name << llendl;
				return;

			case LLStringThrottle::THROTTLE_BLOCKED:
				// Escape altogether until we recover
				return;

			case LLStringThrottle::THROTTLE_OK:
				break;
		}
	}

	std::string script_question;
	if (questions)
	{
		bool is_owner_linden = LLMuteList::isLinden(owner_name);
		bool caution = false;
		LLSD args;
		args["OBJECTNAME"] = object_name;
		args["NAME"] = owner_name;
		// Check the received permission flags against each permission
		S32 known_questions = 0;
		for (S32 i = 0; i < SCRIPT_PERMISSION_EOF; ++i)
		{
			if (questions & LSCRIPTRunTimePermissionBits[i])
			{
				known_questions |= LSCRIPTRunTimePermissionBits[i];
				// Check whether permission question should cause special
				// caution dialog
				if (!is_owner_linden)
				{
					caution |= SCRIPT_QUESTION_IS_CAUTION[i];
				}
				script_question += "	" +
								   LLTrans::getString(SCRIPT_QUESTIONS[i]) +
								   "\n";
			}
		}
		args["QUESTIONS"] = script_question;

		LLSD payload;
		payload["task_id"] = taskid;
		payload["item_id"] = itemid;
		payload["sender"] = msg->getSender().getIPandPort();
		payload["object_name"] = object_name;
		payload["owner_name"] = owner_name;

		if (known_questions != questions)
		{
			llwarns << "Object \"" << object_name << "\" (" << taskid
					<< ") owned by " << owner_name
					<< " requested an unknown permission, that therefore cannot be granted."
					<< llendl;
			if (!known_questions)
			{
				// No known question so give up now.
				return;
			}
		}

		payload["questions"] = known_questions;
//MK
		if (auto_acceptable_permission &&
			!(caution && gSavedSettings.getBool("PermissionsCautionEnabled")))
		{
			// Reply with the permissions granted
			gNotifications.forceResponse(LLNotification::Params("ScriptQuestion").payload(payload), 0/*YES*/);
			return;
		}
//mk
		std::string dialog_name = is_ours ? "ScriptQuestionOurs"
										  : "ScriptQuestion";
		// Check whether cautions are even enabled or not
		if (caution && gSavedSettings.getBool("PermissionsCautionEnabled"))
		{
			// Display the caution permissions prompt
			dialog_name = "ScriptQuestionCaution";
		}
		else if (experienceid.notNull())
		{
			payload["experience"] = experienceid;
			LLExperienceCache* exp = LLExperienceCache::getInstance();
			exp->get(experienceid,
					 boost::bind(process_script_experience_details, _1, args,
								 payload));
			return;
		}
		gNotifications.add(dialog_name, args, payload);
	}
}

void process_derez_container(LLMessageSystem* msg, void**)
{
	llwarns << "Deprecated message callback. Ignored." << llendl;
}

// Helper function to format the time.
std::string formatted_time(time_t the_time)
{
	static LLCachedControl<std::string> fmt(gSavedSettings, "TimestampFormat");
	std::string timestr;
	timeToFormattedString(the_time, std::string(fmt).c_str(), timestr);
#if 0	// The original code used to truncate to 24 characters... Seems useless
		// and too restrictive to me... HB
	if (timestr.size() > 24)
	{
		timestr.erase(23);
	}
#endif
	return timestr;
}

void process_teleport_failed(LLMessageSystem* msg, void**)
{
	std::string reason;
	std::string big_reason;
	LLSD args;

	// Let the interested parties know that teleport failed.
	gViewerParcelMgr.onTeleportFailed();

	// If we have additional alert data
	if (msg->has(_PREHASH_AlertInfo) &&
		msg->getSizeFast(_PREHASH_AlertInfo, _PREHASH_Message) > 0)
	{
		// Get the message ID
		msg->getStringFast(_PREHASH_AlertInfo, _PREHASH_Message, reason);
		big_reason = LLAgent::sTeleportErrorMessages[reason];
		if (big_reason.size() > 0)
		{
			// Substitute verbose reason from the local map
			args["REASON"] = big_reason;
		}
		else
		{
			// Nothing found in the map - use what the server returned in the
			// original message block
			msg->getStringFast(_PREHASH_Info, _PREHASH_Reason, reason);
			args["REASON"] = reason;
		}

		LLSD llsd_block;
		std::string llsd_raw;
		msg->getStringFast(_PREHASH_AlertInfo, _PREHASH_ExtraParams, llsd_raw);
		if (llsd_raw.length())
		{
			std::istringstream llsd_data(llsd_raw);
			if (!LLSDSerialize::deserialize(llsd_block, llsd_data,
											llsd_raw.length()))
			{
				llwarns << "Attempted to read alert parameter data into LLSD but failed: "
						<< llsd_raw << llendl;
			}
			// Change notification name in this special case
			else if (handle_trusted_experiences_notification(llsd_block) ||
					 handle_special_notification("RegionEntryAccessBlocked",
												 llsd_block))
			{
				if (gAgent.teleportInProgress())
				{
					LL_DEBUGS("Teleport") << "Resetting to TELEPORT_NONE"
										  << LL_ENDL;
					gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
				}
				return;
			}
		}
	}
	else
	{
		msg->getStringFast(_PREHASH_Info, _PREHASH_Reason, reason);

		big_reason = LLAgent::sTeleportErrorMessages[reason];
		if (big_reason.size() > 0)
		{
			// Substitute verbose reason from the local map
			args["REASON"] = big_reason;
		}
		else
		{
			// Nothing found in the map - use what the server returned
			args["REASON"] = reason;
		}
	}

	if (gAgent.teleportInProgress())
	{
		LL_DEBUGS("Teleport") << "Resetting to TELEPORT_NONE" << LL_ENDL;
		gAgent.setTeleportState(LLAgent::TELEPORT_NONE, reason);
	}

	gNotifications.add("CouldNotTeleportReason", args);
}

void process_teleport_local(LLMessageSystem* msg, void**)
{
	LL_DEBUGS("Teleport") << "Processing local teleport message" << LL_ENDL;
	
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_Info, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Got teleport notification for wrong agent !" << llendl;
		return;
	}

	U32 location_id;
	LLVector3 pos, look_at;
	U32 teleport_flags;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_LocationID, location_id);
	msg->getVector3Fast(_PREHASH_Info, _PREHASH_Position, pos);
	msg->getVector3Fast(_PREHASH_Info, _PREHASH_LookAt, look_at);
	msg->getU32Fast(_PREHASH_Info, _PREHASH_TeleportFlags, teleport_flags);

	// Sim tells us whether the new position is off the ground
	if (teleport_flags & TELEPORT_FLAGS_IS_FLYING)
	{
		gAgent.setFlying(true);
	}
	else
	{
		gAgent.setFlying(false);
	}

	gAgent.setPositionAgent(pos);
	gAgent.slamLookAt(look_at);

	if (!(gAgent.getTeleportKeepsLookAt() &&
		  LLViewerJoystick::getInstance()->getOverrideCamera()))
	{
		gAgent.resetView(true, true);
	}

	// Send camera update to new region
	gAgent.updateCamera();

	// Do this *after* the agent position is set and camera update is done
	// (see above), so that the setTeleportState() method can use the new
	// position... HB.
	if (gAgent.teleportInProgress())
	{
		if (gAgent.getTeleportState() == LLAgent::TELEPORT_LOCAL)
		{
			// To prevent TeleportStart messages re-activating the progress
			// screen right after tp, keep the teleport state and let progress
			// screen clear it after a short delay (progress screen is active
			// but not visible)  *TODO: remove when SVC-5290 is fixed
			gTeleportDisplayTimer.reset();
			gTeleportDisplay = true;
		}
		else
		{
			LL_DEBUGS("Teleport") << "Resetting to TELEPORT_NONE" << LL_ENDL;
			gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
		}
	}

	send_agent_update(true, true);

	// Let the interested parties know we've teleported.
	// *HACK: Agent position seems to get reset (to render position ?) on each
	// frame, so we have to pass the new position manually. - Vadim
	gViewerParcelMgr.onTeleportFinished(true,
										gAgent.getPosGlobalFromAgent(pos));
}

void send_simple_im(const LLUUID& to_id,
					const std::string& message,
					EInstantMessage dialog,
					const LLUUID& id)
{
	std::string my_name;
	gAgent.buildFullname(my_name);
	send_improved_im(to_id, my_name, message, IM_ONLINE, dialog, id,
					 NO_TIMESTAMP, (U8*)EMPTY_BINARY_BUCKET,
					 EMPTY_BINARY_BUCKET_SIZE);
}

void send_group_notice(const LLUUID& group_id,
					   const std::string& subject,
					   const std::string& message,
					   const LLInventoryItem* item)
{
	// Put this notice into an instant message form. This will mean converting
	// the item to a binary bucket and the subject/message into a single field.
	std::string my_name;
	gAgent.buildFullname(my_name);

	// Combine subject + message into a single string.
	std::ostringstream subject_and_message;
	// *TODO: turn all existing |'s into ||'s in subject and message.
	subject_and_message << subject << "|" << message;

	// Create an empty binary bucket.
	U8 bin_bucket[MAX_INVENTORY_BUFFER_SIZE];
	U8* bucket_to_send = bin_bucket;
	bin_bucket[0] = '\0';
	S32 bin_bucket_size = EMPTY_BINARY_BUCKET_SIZE;
	// If there is an item being sent, pack it into the binary bucket.
	if (item)
	{
		LLSD item_def;
		item_def["item_id"] = item->getUUID();
		item_def["owner_id"] = item->getPermissions().getOwner();
		std::ostringstream ostr;
		LLSDSerialize::serialize(item_def, ostr, LLSDSerialize::LLSD_XML);
		bin_bucket_size = ostr.str().copy((char*)bin_bucket,
										  ostr.str().size());
		bin_bucket[bin_bucket_size] = '\0';
	}
	else
	{
		bucket_to_send = (U8*) EMPTY_BINARY_BUCKET;
	}

	send_improved_im(group_id, my_name, subject_and_message.str(), IM_ONLINE,
					 IM_GROUP_NOTICE, LLUUID::null, NO_TIMESTAMP,
					 bucket_to_send, bin_bucket_size);
}

bool handle_lure_callback(const LLSD& notification, const LLSD& response)
{
	constexpr S32 OFFER_RECIPIENT_LIMIT = 250;
	if (notification["payload"]["ids"].size() > OFFER_RECIPIENT_LIMIT)
	{
		// More than OFFER_RECIPIENT_LIMIT targets will overload the message
		// producing an llerror.
		LLSD args;
		args["OFFERS"] = notification["payload"]["ids"].size();
		args["LIMIT"] = OFFER_RECIPIENT_LIMIT;
		gNotifications.add("TooManyTeleportOffers", args);
		return false;
	}
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		send_lures(notification, response);
	}

	return false;
}

// Prompt for a message to the invited user.
void handle_lure(const uuid_vec_t& ids)
{
	if (ids.size() == 0)
	{
		return;
	}

	LLSD edit_args;
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		edit_args["REGION"] = "(Hidden)";
	}
	else
//mk
	if (gAgent.getRegion())
	{
		edit_args["REGION"] = gAgent.getRegion()->getName();
	}
	else
	{
		edit_args["REGION"] = "Unknown region";
	}

	LLSD payload;
	for (uuid_vec_t::const_iterator it = ids.begin(), end = ids.end();
		 it != end; ++it)
	{
		payload["ids"].append(*it);
	}
	if (gAgent.isGodlike())
	{
		gNotifications.add("OfferTeleportFromGod", edit_args, payload,
						   handle_lure_callback);
	}
	else
	{
		gNotifications.add("OfferTeleport", edit_args, payload,
						   handle_lure_callback);
	}
}

void send_improved_im(const LLUUID& to_id, const std::string& name,
					  const std::string& message, U8 offline,
					  EInstantMessage dialog, const LLUUID& id, U32 timestamp,
					  const U8* binary_bucket, S32 binary_bucket_size)
{
	pack_instant_message(gAgentID, false, gAgentSessionID, to_id, name,
						 message, offline, dialog, id, 0, LLUUID::null,
						 gAgent.getPositionAgent(), timestamp, binary_bucket,
						 binary_bucket_size);
	gAgent.sendReliableMessage();
}

void send_places_query(const LLUUID& query_id, const LLUUID& trans_id,
					   const std::string& query_text, U32 query_flags,
					   S32 category, const std::string& sim_name)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("PlacesQuery");
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUID(_PREHASH_QueryID, query_id);
	msg->nextBlock(_PREHASH_TransactionData);
	msg->addUUID(_PREHASH_TransactionID, trans_id);
	msg->nextBlock(_PREHASH_QueryData);
	msg->addString(_PREHASH_QueryText, query_text);
	msg->addU32(_PREHASH_QueryFlags, query_flags);
	msg->addS8(_PREHASH_Category, (S8)category);
	msg->addString(_PREHASH_SimName, sim_name);
	gAgent.sendReliableMessage();
}

void process_user_info_reply(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "process_user_info_reply -  wrong agent id." << llendl;
	}

	bool im_via_email;
	msg->getBoolFast(_PREHASH_UserData, _PREHASH_IMViaEMail, im_via_email);
	std::string email;
	msg->getStringFast(_PREHASH_UserData, _PREHASH_EMail, email);
	std::string dir_visibility;
	msg->getString(_PREHASH_UserData, _PREHASH_DirectoryVisibility,
				   dir_visibility);

	LLFloaterPreference::updateUserInfo(dir_visibility, im_via_email, email);
	LLFloaterPostcard::updateUserInfo(email);
}

//---------------------------------------------------------------------------
// Script Dialog
//---------------------------------------------------------------------------

constexpr S32 SCRIPT_DIALOG_MAX_BUTTONS = 12;

bool callback_script_dialog(const LLSD& notification, const LLSD& response)
{
	LLNotificationForm form(notification["form"]);
	std::string button = LLNotification::getSelectedOptionName(response);
	S32 button_idx = LLNotification::getSelectedOption(notification, response);
	LLUUID object_id = notification["payload"]["object_id"].asUUID();
	if (button_idx == -2)		// Clicked "Mute"
	{
		std::string object_name =
			notification["payload"]["object_name"].asString();
		LLMute mute(object_id, object_name, LLMute::OBJECT);
		if (LLMuteList::add(mute))
		{
			LLFloaterMute::selectMute(mute.mID);
		}
	}
	else if (button_idx != -1)	// Did not click "Ignore"
	{
		if (notification["payload"].has("textbox"))
		{
			button = response["message"].asString();
		}
		S32 channel = notification["payload"]["chat_channel"].asInteger();
//MK
		if (!channel && gRLenabled &&
			(gRLInterface.containsSubstr("redirchat:") ||
			 gRLInterface.containsSubstr("sendchat")))
		{
			return false;
		}
//mk
		LL_DEBUGS("Messaging") << "Sending dialog reply to object "
							   << object_id << " on channel " << channel
							   << " with button index " << button_idx
							   << " and message: " << button << LL_ENDL;
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(_PREHASH_ScriptDialogReply);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlock(_PREHASH_Data);
		msg->addUUID(_PREHASH_ObjectID, object_id);
		msg->addS32(_PREHASH_ChatChannel, channel);
		msg->addS32(_PREHASH_ButtonIndex, button_idx);
		msg->addString(_PREHASH_ButtonLabel, button);
		msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));
	}

	return false;
}
static LLNotificationFunctorRegistration callback_script_dialog_reg_1("ScriptDialog",
																	  callback_script_dialog);
static LLNotificationFunctorRegistration callback_script_dialog_reg_2("ScriptDialogOurs",
																	  callback_script_dialog);
static LLNotificationFunctorRegistration callback_script_dialog_reg_3("ScriptTextBox",
																	  callback_script_dialog);
static LLNotificationFunctorRegistration callback_script_dialog_reg_4("ScriptTextBoxOurs",
																	  callback_script_dialog);

// Purge the message queue from any previously queued script dialog from the
// same source, with the same channel.
class ScriptDialogMatcher : public LLNotifyBoxView::Matcher
{
public:
	ScriptDialogMatcher(const std::string& dialog, const LLUUID& object_id,
						S32 channel)
	:	mName(dialog),
		mObjectId(object_id),
		mChannel(channel)
	{
	}

	bool matches(const LLNotificationPtr notif) const
	{
		static LLCachedControl<bool> ignore_channel(gSavedSettings,
													"ScriptDialogUnique");
		return notif->getName() == mName &&
			   notif->getPayload()["object_id"].asUUID() == mObjectId &&
			   (ignore_channel ||
				notif->getPayload()["chat_channel"].asInteger() == mChannel);
	}

private:
	S32			mChannel;
	LLUUID		mObjectId;
	std::string	mName;
};

void process_script_dialog(LLMessageSystem* msg, void**)
{
	LLUUID object_id;
	msg->getUUID(_PREHASH_Data, _PREHASH_ObjectID, object_id);

	std::string first_name;
	msg->getString(_PREHASH_Data, _PREHASH_FirstName, first_name);
	if (first_name == "(??\?)")
	{
		first_name.clear();
	}
	std::string  last_name;
	msg->getString(_PREHASH_Data, _PREHASH_LastName, last_name);
	if (last_name == "(??\?)")
	{
		last_name.clear();
	}

	std::string object_name;
	msg->getString(_PREHASH_Data, _PREHASH_ObjectName, object_name);

	std::string message;
	msg->getString(_PREHASH_Data, _PREHASH_Message, message);

	S32 chat_channel;
	msg->getS32(_PREHASH_Data, _PREHASH_ChatChannel, chat_channel);

#if 0	// Unused for now
	LLUUID image_id;
	msg->getUUID(_PREHASH_Data, _PREHASH_ImageID, image_id);
#endif

	LLUUID owner_id;
	// Get the owner Id if it is part of the message (new ScriptDialog message)
	if (gMessageSystemp->getNumberOfBlocks(_PREHASH_OwnerData) > 0)
	{
		msg->getUUID(_PREHASH_OwnerData, _PREHASH_OwnerID, owner_id);
	}

	LLViewerObject* vobj = gObjectList.findObject(object_id);
	// Keep track of the owner's Id for that object.
	if (vobj && vobj->mOwnerID.isNull() && owner_id.notNull())
	{
		vobj->mOwnerID = owner_id;
	}

	// Ignore dialogs coming from muted objects or pertaining to muted
	// residents.
	bool is_ours = vobj && vobj->permYouOwner();
	if (!is_ours) // Do not apply to objects we own
	{
		// Check for mutes by object id and by name
		bool muted = LLMuteList::isMuted(object_id, object_name);

		// Check for mutes by owner
		if (!muted)
		{
			if (owner_id.notNull())
			{
				// Check for mutes by owner id
				muted = LLMuteList::isMuted(owner_id);
			}
			else if (!last_name.empty())
			{
				// Check for mutes by group or owner name (id is unknown to us)
				if (first_name.empty())
				{
					muted = LLMuteList::isMuted(LLUUID::null, last_name, 0,
												LLMute::GROUP);
				}
				else
				{
					muted = LLMuteList::isMuted(LLUUID::null,
												first_name + " " + last_name,
												0, LLMute::AGENT);
				}
			}
		}

		if (muted)
		{
			// Do not spam the log with such messages...
			llinfos_once << "Muting scripted object dialog(s) from: "
						 << first_name << " " << last_name << "'s "
						 << object_name << llendl;
			return;
		}
	}

	LLSD payload;
	payload["sender"] = msg->getSender().getIPandPort();
	payload["object_id"] = object_id;
	payload["chat_channel"] = chat_channel;
	payload["object_name"] = object_name;

	// Build up custom form
	S32 button_count = msg->getNumberOfBlocks(_PREHASH_Buttons);
	if (button_count > SCRIPT_DIALOG_MAX_BUTTONS)
	{
		llwarns << "Too many script dialog buttons - omitting some" << llendl;
		button_count = SCRIPT_DIALOG_MAX_BUTTONS;
	}

	LLNotificationForm form;	// Used only for llDialog()
	bool is_text_box = false;
	if (button_count)
	{
		for (S32 i = 0; i < button_count; ++i)
		{
			std::string label;
			msg->getString(_PREHASH_Buttons, _PREHASH_ButtonLabel, label, i);
			if (label == "!!llTextBox!!")
			{
				is_text_box = true;
				// Do not bother with the rest of the buttons in 'form': it is
				// not used for llTextBox()...
				break;
			}
			form.addElement("button", label);
		}
	}
	else	// This should not happen...
	{
		form.addElement("button", LLStringUtil::null);
	}

	LLSD args;
	args["TITLE"] = object_name;
	args["MESSAGE"] = message;

	std::string name;
	if (first_name.empty())
	{
		name = last_name;
	}
	else
	{
		name = first_name;
		if (!last_name.empty())
		{
			name += " " + last_name;
		}
		if (LLAvatarName::sOmitResidentAsLastName)
		{
			name = LLCacheName::cleanFullName(name);
		}
	}
	if (name.empty())
	{
		name = "Unknown owner";
	}
	args["NAME"] = name;

	bool anti_spam = gSavedSettings.getBool("ScriptDialogAntiSpam");
	std::string dialog;
	if (is_text_box)
	{
		payload["textbox"] = "true";
		dialog = is_ours ? "ScriptTextBoxOurs" : "ScriptTextBox";
		if (anti_spam)
		{
			gNotifyBoxViewp->purgeMessagesMatching(ScriptDialogMatcher(dialog,
																	   object_id,
																	   chat_channel));
		}
		gNotifications.add(dialog, args, payload);
	}
	else
	{
		dialog = is_ours ? "ScriptDialogOurs" : "ScriptDialog";
		if (anti_spam)
		{
			gNotifyBoxViewp->purgeMessagesMatching(ScriptDialogMatcher(dialog,
																	   object_id,
																	   chat_channel));
		}
		gNotifications.add(LLNotification::Params(dialog).substitutions(args).payload(payload).form_elements(form.asLLSD()));
	}
}

//---------------------------------------------------------------------------

std::vector<LLSD> gLoadUrlList;

bool callback_load_url(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)	// Goto page
	{
		LLWeb::loadURL(notification["payload"]["url"].asString());
	}
	else if (option == 2)	// Mute
	{
		LLUUID id = notification["payload"]["object_id"].asUUID();
		std::string name = notification["payload"]["object_name"].asString();
		LLMute mute(id, name, LLMute::OBJECT);
		if (LLMuteList::add(mute))
		{
			LLFloaterMute::selectMute(mute.mID);
		}
	}

	return false;
}
static LLNotificationFunctorRegistration callback_load_url_reg1("LoadWebPage",
																callback_load_url);
static LLNotificationFunctorRegistration callback_load_url_reg2("LoadWebPageOurs",
																callback_load_url);

// We have got the name of the person who owns the object hurling the url.
// Display confirmation dialog.
void callback_load_url_name(const LLUUID& id, const std::string& full_name,
							bool is_group)
{
	std::vector<LLSD>::iterator it;
	for (it = gLoadUrlList.begin(); it != gLoadUrlList.end(); )
	{
		LLSD load_url_info = *it;
		if (load_url_info["owner_id"].asUUID() == id)
		{
			it = gLoadUrlList.erase(it);

			// Check for mutes.
			if (LLMuteList::isMuted(id, full_name, 0,
									is_group ? LLMute::GROUP : LLMute::AGENT))
			{
				// Do not spam the log with such messages...
				llinfos_once << "Ignoring load_url from muted owner "
							 << full_name << llendl;
				continue;
			}

			std::string owner_name;
			if (is_group)
			{
				owner_name = full_name + " (group)";
			}
			else
			{
				owner_name = full_name;
			}

			LLSD args;
			args["URL"] = load_url_info["url"].asString();
			args["MESSAGE"] = load_url_info["message"].asString();;
			args["OBJECTNAME"] = load_url_info["object_name"].asString();
			args["NAME"] = owner_name;

			std::string dialog = id == gAgentID ? "LoadWebPageOurs"
												: "LoadWebPage";
			gNotifications.add(dialog, args, load_url_info);
		}
		else
		{
			++it;
		}
	}
}

void process_load_url(LLMessageSystem* msg, void**)
{
	char object_name[256];
	msg->getString(_PREHASH_Data, _PREHASH_ObjectName, 256, object_name);
	LLUUID object_id;
	msg->getUUID(_PREHASH_Data, _PREHASH_ObjectID, object_id);
	LLUUID owner_id;
	msg->getUUID(_PREHASH_Data, _PREHASH_OwnerID, owner_id);
	bool owner_is_group;
	msg->getBool(_PREHASH_Data, _PREHASH_OwnerIsGroup, owner_is_group);
	char message[256];
	msg->getString(_PREHASH_Data, _PREHASH_Message, 256, message);
	char url[256];
	msg->getString(_PREHASH_Data, _PREHASH_URL, 256, url);

	LLSD payload;
	payload["object_id"] = object_id;
	payload["owner_id"] = owner_id;
	payload["owner_is_group"] = owner_is_group;
	payload["object_name"] = object_name;
	payload["message"] = message;
	payload["url"] = url;

	// URL is safety checked in load_url above

	// Check if object or owner is muted
	if (LLMuteList::isMuted(owner_id))
	{
		llinfos_once << "Ignoring load_url from muted object owner: "
					 << owner_id << llendl;
		return;
	}
	if (LLMuteList::isMuted(object_id, object_name))
	{
		llinfos_once << "Ignoring load_url from muted object: " << object_name
					 << llendl;
		return;
	}

	// Add to list of pending name lookups
	gLoadUrlList.emplace_back(payload);

	if (gCacheNamep)
	{
		gCacheNamep->get(owner_id, owner_is_group, callback_load_url_name);
	}
}

void callback_download_complete(void** data, S32 result, LLExtStat)
{
	std::string* filepath = (std::string*)data;
	LLSD args;
	args["DOWNLOAD_PATH"] = *filepath;
	gNotifications.add("FinishedRawDownload", args);
	delete filepath;
}

void process_initiate_download(LLMessageSystem* msg, void**)
{
	if (!gXferManagerp)
	{
		llwarns << "Transfer manager gone. Aborted." << llendl;
		return;
	}

	LLUUID agent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "Initiate download for wrong agent" << llendl;
		return;
	}

	std::string sim_filename;
	std::string viewer_filename;
	msg->getString("FileData", "SimFilename", sim_filename);
	msg->getString("FileData", "ViewerFilename", viewer_filename);

	if (!gXferManagerp->validateFileForRequest(viewer_filename))
	{
		llwarns << "SECURITY: Unauthorized download to local file '"
				<< viewer_filename << "'. Aborted !" << llendl;
		return;
	}
	gXferManagerp->requestFile(viewer_filename, sim_filename, LL_PATH_NONE,
							   msg->getSender(), false, // do not delete remote
							   callback_download_complete,
							   (void**)new std::string(viewer_filename));
}

void process_script_teleport_request(LLMessageSystem* msg, void**)
{
	std::string object_name;
	msg->getString(_PREHASH_Data, _PREHASH_ObjectName, object_name);
	std::string sim_name;
	msg->getString(_PREHASH_Data, _PREHASH_SimName, sim_name);
	LLVector3 pos;
	msg->getVector3(_PREHASH_Data, _PREHASH_SimPosition, pos);
	LLVector3 look_at;
	msg->getVector3(_PREHASH_Data, _PREHASH_LookAt, look_at);

#if 1	// 0 to re-enable parcel browser for llMapDestination()
	if (gFloaterWorldMapp)
	{
		llinfos << "Object '" << object_name << "' is offering TP to region '"
				<< sim_name << "' at position " << pos << llendl;
		gFloaterWorldMapp->trackURL(sim_name, (S32)pos.mV[VX], (S32)pos.mV[VY],
								   (S32)pos.mV[VZ]);
		LLFloaterWorldMap::show(NULL, true);
	}
#else
	LLURLDispatcher::dispatch(LLURLDispatcher::buildSLURL(sim_name,
														  (S32)pos.mV[VX],
														  (S32)pos.mV[VY],
														  (S32)pos.mV[VZ]),
														  false);
#endif
}

void callbackCacheEstateOwnerName(const LLUUID& id,
								  const std::string& fullname,
								  bool is_group)
{
	std::string name;
	if (id.isNull())
	{
		name = "(none)";
	}
	else
	{
		name = fullname;
	}
	LLPanelEstateInfo::updateEstateOwnerName(name);
	LLPanelEstateCovenant::updateEstateOwnerName(name);
	LLPanelLandCovenant::updateEstateOwnerName(name);
	LLFloaterBuyLand::updateEstateOwnerName(name);
}

void onCovenantLoadComplete(const LLUUID& asset_uuid, LLAssetType::EType type,
							void*, S32 status, LLExtStat)
{
	LL_DEBUGS("Messaging") << "Covenant loaded" << LL_ENDL;
	std::string covenant_text;
	if (status == 0)
	{
		LLFileSystem file(asset_uuid);

		S32 file_length = file.getSize();

		char* buffer = new char[file_length + 1];
		if (buffer == NULL)
		{
			llerrs << "Memory Allocation failed" << llendl;
			return;
		}

		file.read((U8*)buffer, file_length);

		// Put a EOS at the end
		buffer[file_length] = 0;

		if (file_length > 19 && !strncmp(buffer, "Linden text version", 19))
		{
			LLViewerTextEditor* editor;
			editor = new LLViewerTextEditor(std::string("temp"),
											LLRect(0, 0, 0, 0),
											file_length + 1);
			if (!editor->importBuffer(buffer, file_length + 1))
			{
				llwarns << "Problem importing estate covenant." << llendl;
				covenant_text = "Problem importing estate covenant.";
			}
			else
			{
				// Version 0 (just text, does not include version number)
				covenant_text = editor->getText();
			}
			delete[] buffer;
			delete editor;
		}
		else
		{
			covenant_text = "Problem importing estate covenant: covenant file format error.";
			llwarns << covenant_text << llendl;
		}
	}
	else
	{
		gViewerStats.incStat(LLViewerStats::ST_DOWNLOAD_FAILED);

		if (LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
			LL_ERR_FILE_EMPTY == status)
		{
			covenant_text = "Estate covenant notecard is missing from database.";
		}
		else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
		{
			covenant_text = "Insufficient permissions to view estate covenant.";
		}
		else
		{
			covenant_text = "Unable to load estate covenant at this time.";
		}

		llwarns << "Problem loading notecard: " << covenant_text << llendl;
	}
	LLPanelEstateCovenant::updateCovenantText(covenant_text, asset_uuid);
	LLPanelLandCovenant::updateCovenantText(covenant_text);
	LLFloaterBuyLand::updateCovenantText(covenant_text, asset_uuid);
}

void process_covenant_reply(LLMessageSystem* msg, void**)
{
	LLUUID covenant_id, estate_owner_id;
	std::string estate_name;
	U32 covenant_timestamp;
	msg->getUUID(_PREHASH_Data, _PREHASH_CovenantID, covenant_id);
	msg->getU32(_PREHASH_Data, _PREHASH_CovenantTimestamp, covenant_timestamp);
	msg->getString(_PREHASH_Data, _PREHASH_EstateName, estate_name);
	msg->getUUID(_PREHASH_Data, _PREHASH_EstateOwnerID, estate_owner_id);

	LLPanelEstateInfo::updateEstateName(estate_name);
	LLPanelEstateCovenant::updateEstateName(estate_name);
	LLPanelLandCovenant::updateEstateName(estate_name);
	LLFloaterBuyLand::updateEstateName(estate_name);

	// Standard message, not from system
	std::string last_modified;
	if (covenant_timestamp == 0)
	{
		last_modified = LLTrans::getString("covenant_never_modified");
	}
	else
	{
		last_modified = LLTrans::getString("covenant_modified") + " " +
						formatted_time((time_t)covenant_timestamp);
	}

	LLPanelEstateCovenant::updateLastModified(last_modified);
	LLPanelLandCovenant::updateLastModified(last_modified);
	LLFloaterBuyLand::updateLastModified(last_modified);

	if (gCacheNamep)
	{
		gCacheNamep->get(estate_owner_id, false, callbackCacheEstateOwnerName);
	}

	// Load the actual covenant asset data
	if (covenant_id.notNull())
	{
		constexpr bool high_priority = true;
		gAssetStoragep->getEstateAsset(gAgent.getRegionHost(), gAgentID,
									   gAgentSessionID, covenant_id,
									   LLAssetType::AT_NOTECARD, ET_Covenant,
									   onCovenantLoadComplete, NULL,
									   high_priority);
	}
	else
	{
		std::string covenant_text;
		if (estate_owner_id.isNull())	// Mainland
		{
			covenant_text = LLTrans::getString("no_covenant_for_mainland");
		}
		else							// Privately owned estate
		{
			covenant_text = LLTrans::getString("no_covenant_for_estate");
		}
		LLPanelEstateCovenant::updateCovenantText(covenant_text, covenant_id);
		LLPanelLandCovenant::updateCovenantText(covenant_text);
		LLFloaterBuyLand::updateCovenantText(covenant_text, covenant_id);
	}
}

// Handles black-listed feature simulator response.
void process_feature_disabled_message(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_FailureInfo, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		return;	// Not for us !...
	}

	std::string	message;
	msg->getStringFast(_PREHASH_FailureInfo, _PREHASH_ErrorMessage,
					   message, 0);
	LLUUID trans_id;
	msg->getUUIDFast(_PREHASH_FailureInfo, _PREHASH_TransactionID,
					 trans_id);

	llwarns << "Blacklisted feature response:" << message
			<< " - Transaction id: " << trans_id << llendl;
}

// ------------------------------------------------------------
// Message system exception callbacks
// ------------------------------------------------------------

void invalid_message_callback(LLMessageSystem*, void*, EMessageException)
{
	gAppViewerp->badNetworkHandler();
}

void LLOfferInfo::forceResponse(InventoryOfferResponse response)
{
	LLNotification::Params params("UserGiveItem");
	params.functor(boost::bind(&LLOfferInfo::inventoryOfferCallback, this,
							   _1, _2));
	// NOTE: keep UserGiveItem options in sync !
	// 0 = accept = IOR_ACCEPT, 1 = decline = IOR_DECLINE, 2 = mute = IOR_MUTE.
	// For IOR_BUSY and IOR_MUTED, we pass "decline" to the UserGiveItem
	// notification.
	S32 option = response <= IOR_MUTE ? response : 1;
	LL_DEBUGS("InventoryOffer") << "Forcing response: " << option << LL_ENDL;
	gNotifications.forceResponse(params, option);
}

// Generic message (formerly in llviewergenericmessage.cpp)

void send_generic_message(const char* method,
						  const std::vector<std::string>& strings,
						  const LLUUID& invoice)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_GenericMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); // Not used
	msg->nextBlock(_PREHASH_MethodData);
	msg->addString(_PREHASH_Method, method);
	msg->addUUID(_PREHASH_Invoice, invoice);
	if (strings.empty())
	{
		msg->nextBlock(_PREHASH_ParamList);
		msg->addString(_PREHASH_Parameter, NULL);
	}
	else
	{
		for (std::vector<std::string>::const_iterator it = strings.begin(),
													  end = strings.end();
			 it != end; ++it)
		{
			msg->nextBlock(_PREHASH_ParamList);
			msg->addString(_PREHASH_Parameter, *it);
		}
	}
	gAgent.sendReliableMessage();
}

void process_generic_message(LLMessageSystem* msg, void**)
{
	std::string method;
	msg->getStringFast(_PREHASH_MethodData, _PREHASH_Method, method);
	if (method == "Windlight")
	{
		gWLSkyParamMgr.processLightshareMessage(msg);
		return;
	}
	else if (method == "WindlightReset")
	{
		gWLSkyParamMgr.processLightshareReset();
		return;
	}

	LLUUID agent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "GenericMessage for wrong agent" << llendl;
		return;
	}

	std::string request;
	LLUUID invoice;
	LLDispatcher::sparam_t strings;
	LLDispatcher::unpackMessage(msg, request, invoice, strings);
	if (!gGenericDispatcher.dispatch(request, invoice, strings))
	{
		llwarns << "GenericMessage " << request << " failed to dispatch"
				<< llendl;
	}
}

void process_generic_streaming_message(LLMessageSystem* msg, void**)
{
	LLGenericStreamingMessage data;
	data.unpack(msg);
	if (data.mMethod ==
			LLGenericStreamingMessage::METHOD_GLTF_MATERIAL_OVERRIDE)
	{
		gGLTFMaterialList.applyOverrideMessage(msg, data.mData);
	}
	else
	{
		llwarns_once << "Unknown generic streaming message method: "
					 << (S32)data.mMethod << llendl;
	}
}

void process_large_generic_message(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgentID)
	{
		llwarns << "LargeGenericMessage for wrong agent" << llendl;
		return;
	}

	std::string request;
	LLUUID invoice;
	LLDispatcher::sparam_t strings;
	LLDispatcher::unpackLargeMessage(msg, request, invoice, strings);
	if (!gGenericDispatcher.dispatch(request, invoice, strings))
	{
		llwarns << "LargeGenericMessage " << request << " failed to dispatch"
				<< llendl;
	}
}
