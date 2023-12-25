/**
 * @file llviewerstats.cpp
 * @brief LLViewerStats class implementation
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

#if LL_LINUX
# include <stdlib.h>					// For getenv()
#endif

#include "llviewerstats.h"

#include "llavatarnamecache.h"
#include "llcorehttputil.h"
#include "llfasttimer.h"
#include "llgl.h"
#include "llsdutil.h"
#include "llsys.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldebugview.h"
#include "llfasttimerview.h"
#include "llfeaturemanager.h"
#include "llflexibleobject.h"
#include "llfloatertools.h"
#include "llgridmanager.h"
#include "llmeshrepository.h"
#include "llpipeline.h"
#include "llsurface.h"
#include "lltexturefetch.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewertexlayer.h"
#include "llviewertextureanim.h"
#include "llviewertexturelist.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvlmanager.h"
#include "llvoavatarself.h"
#include "llworld.h"

constexpr F32 SEND_STATS_PERIOD = 300.f;
constexpr F32 SEND_AVATAR_STATS_PERIOD = 60.f;

static const std::string KEY_AGENTS = "agents";								// map
static const std::string KEY_WEIGHT = "weight";								// integer
static const std::string KEY_TOO_COMPLEX  = "tooComplex";					// boolean
static const std::string KEY_OVER_COMPLEXITY_LIMIT = "overlimit";			// integer
static const std::string KEY_REPORTING_COMPLEXITY_LIMIT = "reportinglimit";	// integer

static const std::string KEY_IDENTIFIER = "identifier";
static const std::string KEY_MESSAGE = "message";
static const std::string KEY_ERROR = "error";

// Globals

LLViewerStats gViewerStats;

U32 gTotalLandIn = 0;
U32 gTotalLandOut = 0;
U32 gTotalWaterIn = 0;
U32 gTotalWaterOut = 0;

F32 gAveLandCompression = 0.f;
F32 gAveWaterCompression = 0.f;
F32 gBestLandCompression = 1.f;
F32 gBestWaterCompression = 1.f;
F32 gWorstLandCompression = 0.f;
F32 gWorstWaterCompression = 0.f;

U32 gTotalWorldBytes = 0;
U32 gTotalObjectBytes = 0;
U32 gTotalTextureBytes = 0;
U32 gSimPingCount = 0;
U32 gObjectBits = 0;
F32 gAvgSimPing = 0.f;

extern U32 gVisCompared;
extern U32 gVisTested;

LLFrameTimer gTextureTimer;

class StatAttributes
{
public:
	StatAttributes(const char* name, bool enabled, bool is_timer)
	:	mName(name),
		mEnabled(enabled),
		mIsTimer(is_timer)
	{
	}

	std::string mName;
	bool mEnabled;
	bool mIsTimer;
};

const StatAttributes STAT_INFO[LLViewerStats::ST_COUNT] =
{
	// ST_VERSION
	StatAttributes("Version", true, false),
	// ST_AVATAR_EDIT_SECONDS
	StatAttributes("Seconds in Edit Appearence", false, true),
	// ST_TOOLBOX_SECONDS
	StatAttributes("Seconds using Toolbox", false, true),
	// ST_CHAT_COUNT
	StatAttributes("Chat messages sent", false, false),
	// ST_IM_COUNT
	StatAttributes("IMs sent", false, false),
	// ST_FULLSCREEN_BOOL
	StatAttributes("Fullscreen mode", false, false),
	// ST_RELEASE_COUNT
	StatAttributes("Object release count", false, false),
	// ST_CREATE_COUNT
	StatAttributes("Object create count", false, false),
	// ST_REZ_COUNT
	StatAttributes("Object rez count", false, false),
	// ST_FPS_10_SECONDS
	StatAttributes("Seconds below 10 FPS", false, true),
	// ST_FPS_2_SECONDS
	StatAttributes("Seconds below 2 FPS", false, true),
	// ST_MOUSELOOK_SECONDS
	StatAttributes("Seconds in Mouselook", false, true),
	// ST_FLY_COUNT
	StatAttributes("Fly count", false, false),
	// ST_TELEPORT_COUNT
	StatAttributes("Teleport count", false, false),
	// ST_OBJECT_DELETE_COUNT
	StatAttributes("Objects deleted", false, false),
	// ST_SNAPSHOT_COUNT
	StatAttributes("Snapshots taken", false, false),
	// ST_UPLOAD_SOUND_COUNT
	StatAttributes("Sounds uploaded", false, false),
	// ST_UPLOAD_TEXTURE_COUNT
	StatAttributes("Textures uploaded", false, false),
	// ST_EDIT_TEXTURE_COUNT
	StatAttributes("Changes to textures on objects", false, false),
	// ST_KILLED_COUNT
	StatAttributes("Number of times killed", false, false),
	// ST_FRAMETIME_JITTER
	StatAttributes("Average delta between sucessive frame times", false,
				   false),
	// ST_FRAMETIME_SLEW
	StatAttributes("Average delta between frame time and mean", false, false),
	// ST_INVENTORY_TOO_LONG
	StatAttributes("Inventory took too long to load", false, false),
	// ST_WEARABLES_TOO_LONG
	StatAttributes("Wearables took too long to load", false, false),
	// ST_LOGIN_SECONDS
	StatAttributes("Time between LoginRequest and LoginReply", false, false),
	// ST_LOGIN_TIMEOUT_COUNT
	StatAttributes("Number of login attempts that timed out", false, false),
	// ST_HAS_BAD_TIMER
	StatAttributes("Known bad timer if != 0.0", false, false),
	// ST_DOWNLOAD_FAILED
	StatAttributes("Number of times LLAssetStorage::getAssetData() has failed",
				   false, false),
	// ST_LSL_SAVE_COUNT
	StatAttributes("Number of times user has saved a script", false, false),
	// ST_UPLOAD_ANIM_COUNT
	StatAttributes("Animations uploaded", false, false),
	// ST_FPS_8_SECONDS
	StatAttributes("Seconds below 8 FPS", false, true),
	// ST_SIM_FPS_20_SECONDS
	StatAttributes("Seconds with sim FPS below 20", false, true),
	// ST_PHYS_FPS_20_SECONDS
	StatAttributes("Seconds with physics FPS below 20", false, true),
	// ST_LOSS_05_SECONDS
	StatAttributes("Seconds with packet loss > 5%", false, true),
	// ST_FPS_DROP_50_RATIO
	StatAttributes("Ratio of frames 2x longer than previous", false, false),
	// ST_DELTA_BANDWIDTH
	StatAttributes("Increase/Decrease in bandwidth based on packet loss",
				   false, false),
	// ST_MAX_BANDWIDTH
	StatAttributes("Max bandwidth setting", false, false),
	// ST_VISIBLE_AVATARS
	StatAttributes("Visible Avatars", false, false),
	// ST_SHADER_OJECTS
	StatAttributes("Object Shaders", false, false),
	// ST_SHADER_ENVIRONMENT
	StatAttributes("Environment Shaders", false, false),
	// ST_VISIBLE_DRAW_DIST
	StatAttributes("Draw Distance", false, false),
	// ST_VISIBLE_CHAT_BUBBLES
	StatAttributes("Chat Bubbles Enabled", false, false),
	// ST_SHADER_AVATAR
	StatAttributes("Avatar Shaders", false, false),
	// ST_FRAME_SECS
	StatAttributes("FRAME_SECS", false, false),
	// ST_UPDATE_SECS
	StatAttributes("UPDATE_SECS", false, false),
	// ST_NETWORK_SECS
	StatAttributes("NETWORK_SECS", false, false),
	// ST_IMAGE_SECS
	StatAttributes("IMAGE_SECS", false, false),
	// ST_REBUILD_SECS
	StatAttributes("REBUILD_SECS", false, false),
	// ST_RENDER_SECS
	StatAttributes("RENDER_SECS", false, false),
	// ST_CROSSING_AVG
	StatAttributes("CROSSING_AVG", false, false),
	// ST_CROSSING_MAX
	StatAttributes("CROSSING_MAX", false, false),
	// ST_WINDOW_WIDTH
	StatAttributes("Window width", false, false),
	// ST_WINDOW_HEIGHT
	StatAttributes("Window height", false, false),
	// ST_TEX_BAKES
	StatAttributes("Texture Bakes", false, false),
	// ST_TEX_REBAKES
	StatAttributes("Texture Rebakes", false, false)
};

LLViewerStats::LLViewerStats()
:	mPacketsLostPercentStat(64),
	mLastTimeDiff(0.0),
	// Initialize mNextStatsSendingTime with 0 so that the initial stats report
	// will be sent immediately.
	mNextStatsSendingTime(0),
	mNextAvStatsSendingTime(SEND_AVATAR_STATS_PERIOD),
	mComplexityReports(0),
	mTooComplexReports(0)
{
	for (S32 i = 0; i < ST_COUNT; ++i)
	{
		mStats[i] = 0.0;
	}
}

void LLViewerStats::resetStats()
{
	mKBitStat.reset();
	mLayersKBitStat.reset();
	mObjectKBitStat.reset();
	mTextureKBitStat.reset();
	mAssetKBitStat.reset();
	mPacketsInStat.reset();
	mPacketsLostStat.reset();
	mPacketsOutStat.reset();
	mFPSStat.reset();
	mTexturePacketsStat.reset();
	mNextStatsSendingTime = 0;
	mNextAvStatsSendingTime = gFrameTimeSeconds + SEND_AVATAR_STATS_PERIOD;
}

void LLViewerStats::idleUpdate()
{
	if (LLSurface::sTextureUpdateTime)
	{
		LLSurface::sTexelsUpdatedPerSecStat.addValue(0.001f *
													 (LLSurface::sTexelsUpdated /
													  LLSurface::sTextureUpdateTime));
		LLSurface::sTexelsUpdated = 0;
		LLSurface::sTextureUpdateTime = 0.f;
	}
	if (gFrameTimeSeconds >= mNextStatsSendingTime)
	{
		sendStats();
	}
	if (gFrameTimeSeconds >= mNextAvStatsSendingTime)
	{
		avatarRenderingStats();
	}
}

void LLViewerStats::addSample(U32 stat_id, F32 stat_value)
{
	if (llisnan(stat_value))
	{
		llwarns_once << "NaN value received for stat: " << stat_id << llendl;
		return;
	}

	switch (stat_id)
	{
		case LL_SIM_STAT_TIME_DILATION:
			mSimTimeDilation.addValue(stat_value);
			break;

		case LL_SIM_STAT_FPS:
			mSimFPS.addValue(stat_value);
			break;

		case LL_SIM_STAT_PHYSFPS:
			mSimPhysicsFPS.addValue(stat_value);
			break;

		case LL_SIM_STAT_AGENTUPS:
			mSimAgentUPS.addValue(stat_value);
			break;

		case LL_SIM_STAT_FRAMEMS:
			mSimFrameMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_NETMS:
			mSimNetMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_SIMOTHERMS:
			mSimSimOtherMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_SIMPHYSICSMS:
			mSimSimPhysicsMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_AGENTMS:
			mSimAgentMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_IMAGESMS:
			mSimImagesMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_SCRIPTMS:
			mSimScriptMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_NUMTASKS:
			mSimObjects.addValue(stat_value);
			break;

		case LL_SIM_STAT_NUMTASKSACTIVE:
			mSimActiveObjects.addValue(stat_value);
			break;

		case LL_SIM_STAT_NUMAGENTMAIN:
			mSimMainAgents.addValue(stat_value);
			break;

		case LL_SIM_STAT_NUMAGENTCHILD:
			mSimChildAgents.addValue(stat_value);
			break;

		case LL_SIM_STAT_NUMSCRIPTSACTIVE:
			mSimActiveScripts.addValue(stat_value);
			break;

		case LL_SIM_STAT_SCRIPT_EPS:
			mSimScriptEPS.addValue(stat_value);
			break;

		case LL_SIM_STAT_INPPS:
			mSimInPPS.addValue(stat_value);
			break;

		case LL_SIM_STAT_OUTPPS:
			mSimOutPPS.addValue(stat_value);
			break;

		case LL_SIM_STAT_PENDING_DOWNLOADS:
			mSimPendingDownloads.addValue(stat_value);
			break;

		case LL_SIM_STAT_PENDING_UPLOADS:
			mSimPendingUploads.addValue(stat_value);
			break;

		case LL_SIM_STAT_PENDING_LOCAL_UPLOADS:
			mSimPendingLocalUploads.addValue(stat_value);
			break;

		case LL_SIM_STAT_TOTAL_UNACKED_BYTES:
		{
			constexpr F32 scaler = 1.f / 1024.f;
			mSimTotalUnackedBytes.addValue(stat_value * scaler);
			break;
		}

		case LL_SIM_STAT_PHYSICS_PINNED_TASKS:
			mPhysicsPinnedTasks.addValue(stat_value);
			break;

		case LL_SIM_STAT_PHYSICS_LOD_TASKS:
			mPhysicsLODTasks.addValue(stat_value);
			break;

		case LL_SIM_STAT_SIMPHYSICSSTEPMS:
			mSimSimPhysicsStepMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_SIMPHYSICSSHAPEMS:
			mSimSimPhysicsShapeUpdateMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_SIMPHYSICSOTHERMS:
			mSimSimPhysicsOtherMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_SIMPHYSICSMEMORY:
			mPhysicsMemoryAllocated.addValue(stat_value);
			break;

		case LL_SIM_STAT_SIMSPARETIME:
			mSimSpareMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_SIMSLEEPTIME:
			mSimSleepMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_IOPUMPTIME:
			mSimPumpIOMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_PCTSCRIPTSRUN:
			mSimPctScriptsRun.addValue(stat_value);
			break;

		case LL_SIM_STAT_SIMAISTEPTIMEMS:
			mSimSimAIStepMsec.addValue(stat_value);
			break;

		case LL_SIM_STAT_SKIPPEDAISILSTEPS_PS:
			mSimSimSkippedSilhouetteSteps.addValue(stat_value);
			break;

		case LL_SIM_STAT_PCTSTEPPEDCHARACTERS:
			mSimSimPctSteppedCharacters.addValue(stat_value);
			break;

		default:
 			LL_DEBUGS("Messaging") << "Unknown stat id " << stat_id << LL_ENDL;
	}
}

void LLViewerStats::updateFrameStats(F64 time_diff)
{
	if (mPacketsLostPercentStat.getCurrent() > 5.0)
	{
		incStat(ST_LOSS_05_SECONDS, time_diff);
	}

	if (mSimFPS.getCurrent() < 20.f && mSimFPS.getCurrent() > 0.f)
	{
		incStat(ST_SIM_FPS_20_SECONDS, time_diff);
	}

	if (mSimPhysicsFPS.getCurrent() < 20.f &&
		mSimPhysicsFPS.getCurrent() > 0.f)
	{
		incStat(ST_PHYS_FPS_20_SECONDS, time_diff);
	}

	if (time_diff >= 0.5)
	{
		incStat(ST_FPS_2_SECONDS, time_diff);
	}
	if (time_diff >= 0.125)
	{
		incStat(ST_FPS_8_SECONDS, time_diff);
	}
	if (time_diff >= 0.1)
	{
		incStat(ST_FPS_10_SECONDS, time_diff);
	}

	if (gFrameCount && mLastTimeDiff > 0.0)
	{
		// New "stutter" meter
		setStat(ST_FPS_DROP_50_RATIO,
				(getStat(ST_FPS_DROP_50_RATIO) * (F64)(gFrameCount - 1) +
				 (time_diff >= 2.0 * mLastTimeDiff ? 1.0 : 0.0)) / gFrameCount);

		// Old stats that were never really used
		setStat(ST_FRAMETIME_JITTER,
				(getStat(ST_FRAMETIME_JITTER) * (gFrameCount - 1) +
				 fabs(mLastTimeDiff - time_diff) / mLastTimeDiff) / gFrameCount);

		F32 average_frametime = gRenderStartTime.getElapsedTimeF32() / (F32)gFrameCount;
		setStat(ST_FRAMETIME_SLEW,
				(getStat(ST_FRAMETIME_SLEW) * (gFrameCount - 1) +
				 fabs(average_frametime - time_diff) / average_frametime) / gFrameCount);

		F32 max_bandwidth = gViewerThrottle.getMaxBandwidth();
		F32 delta_bandwidth = gViewerThrottle.getCurrentBandwidth() - max_bandwidth;
		setStat(ST_DELTA_BANDWIDTH, delta_bandwidth / 1024.f);

		setStat(ST_MAX_BANDWIDTH, max_bandwidth / 1024.f);
	}

	mLastTimeDiff = time_diff;
}

void LLViewerStats::addToMessage(LLSD& body) const
{
	LLSD& misc = body["misc"];

	for (S32 i = 0; i < ST_COUNT; ++i)
	{
		if (STAT_INFO[i].mEnabled)
		{
			// *TODO: send timer value so dataserver can normalize
			misc[STAT_INFO[i].mName] = mStats[i];
			llinfos << "STAT: " << STAT_INFO[i].mName << ": " << mStats[i]
					<< llendl;
		}
	}
}

void LLViewerStats::updateStatistics(U32 frame_count)
{
	gTotalWorldBytes += gVLManager.getTotalBytes();
	gTotalObjectBytes += gObjectBits / 8;

	// Make sure we have a valid time delta for this frame
	if (gFrameIntervalSeconds > 0.f)
	{
		if (gAgent.getCameraMode() == CAMERA_MODE_MOUSELOOK)
		{
			incStat(ST_MOUSELOOK_SECONDS, gFrameIntervalSeconds);
		}
		else if (gAgent.getCameraMode() == CAMERA_MODE_CUSTOMIZE_AVATAR)
		{
			incStat(ST_AVATAR_EDIT_SECONDS, gFrameIntervalSeconds);
		}
		else if (LLFloaterTools::isVisible())
		{
			incStat(ST_TOOLBOX_SECONDS, gFrameIntervalSeconds);
		}
	}

	static LLCachedControl<F32> render_far_clip(gSavedSettings,
												"RenderFarClip");
	setStat(ST_DRAW_DIST, (F64)render_far_clip);

	static LLCachedControl<bool> use_chat_bubbles(gSavedSettings,
												  "UseChatBubbles");
	setStat(ST_CHAT_BUBBLES, (F64)use_chat_bubbles);

#if LL_FAST_TIMERS_ENABLED
	if (gEnableFastTimers && gFastTimerViewp)
	{
		setStat(ST_FRAME_SECS,
				gFastTimerViewp->getTime(LLFastTimer::FTM_FRAME));

		F64 idle_secs = gFastTimerViewp->getTime(LLFastTimer::FTM_IDLE);
		F64 network_secs = gFastTimerViewp->getTime(LLFastTimer::FTM_NETWORK);
		setStat(ST_UPDATE_SECS, idle_secs - network_secs);

		setStat(ST_NETWORK_SECS, network_secs);

		setStat(ST_IMAGE_SECS,
				gFastTimerViewp->getTime(LLFastTimer::FTM_IMAGE_UPDATE));

		setStat(ST_REBUILD_SECS,
				gFastTimerViewp->getTime(LLFastTimer::FTM_STATESORT));

		setStat(ST_RENDER_SECS,
				gFastTimerViewp->getTime(LLFastTimer::FTM_RENDER_GEOMETRY));
	}
#endif

	LLCircuitData* cdp = NULL;
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		cdp = gMessageSystemp->mCircuitInfo.findCircuit(regionp->getHost());
	}
	if (cdp)
	{
		mSimPingStat.addValue(cdp->getPingDelay());
		gAvgSimPing = (gAvgSimPing * (F32)gSimPingCount + (F32)cdp->getPingDelay()) /
					  ((F32)gSimPingCount + 1);
		++gSimPingCount;
	}
	else
	{
		mSimPingStat.addValue(10000);
	}

	mFPSStat.addValue(1);
	F32 layer_bits = (F32)(gVLManager.getLandBits() +
						   gVLManager.getWindBits() +
						   gVLManager.getCloudBits());
	mLayersKBitStat.addValue(layer_bits / 1024.f);
	mObjectKBitStat.addValue(gObjectBits / 1024.f);
	mAssetKBitStat.addValue(gTransferManager.getTransferBitsIn(LLTCT_ASSET) / 1024.f);
	gTransferManager.resetTransferBitsIn(LLTCT_ASSET);

	static LLCachedControl<U32> low_water(gSavedSettings,
										  "TexFetchingTimerThreshold");
	if (gTextureFetchp &&
		gTextureFetchp->getApproxNumRequests() <= (U32)low_water)
	{
		gTextureTimer.pause();
	}
	else
	{
		gTextureTimer.unpause();
	}

	{
		static F32 visible_avatar_frames = 0.f;
		static F32 avg_visible_avatars = 0;
		F32 visible_avatars = (F32)LLVOAvatar::sNumVisibleAvatars;
		if (visible_avatars > 0.f)
		{
			visible_avatar_frames = 1.f;
			avg_visible_avatars = (avg_visible_avatars *
								   (F32)(visible_avatar_frames - 1.f) +
								   visible_avatars) / visible_avatar_frames;
		}
		setStat(ST_VISIBLE_AVATARS, (F64)avg_visible_avatars);
	}
	gWorld.updateNetStats();
	gWorld.requestCacheMisses();

	// Reset all of these values.
	gVLManager.resetBitCounts();
	gObjectBits = 0;

	// Only update texture stats periodically so that they are less noisy
	{
		constexpr F32 texture_stats_freq = 1.f;
		static LLFrameTimer texture_stats_timer;
		if (texture_stats_timer.getElapsedTimeF32() >= texture_stats_freq)
		{
			mTextureKBitStat.addValue(LLViewerTextureList::sTextureBits / 1024.f);
			mTexturePacketsStat.addValue(LLViewerTextureList::sTexturePackets);
			gTotalTextureBytes += LLViewerTextureList::sTextureBits / 8;
			LLViewerTextureList::sTextureBits = 0;
			LLViewerTextureList::sTexturePackets = 0;
			texture_stats_timer.reset();
		}
	}
}

void LLViewerStats::sendStats()
{
	const std::string& url = gAgent.getRegionCapability("ViewerStats");
	if (url.empty())
	{
		if (gIsInSecondLife)
		{
			// Capabilities still in flight ?... Retry a bit later.
			mNextStatsSendingTime = gFrameTimeSeconds +
									SEND_STATS_PERIOD / 5;
			llwarns << "Could not get ViewerStats capability" << llendl;
		}
		else
		{
			// Retry later, just in case, but OpenSim normally do not
			// provide this capability.
			mNextStatsSendingTime = gFrameTimeSeconds + SEND_STATS_PERIOD;
		}
		return;
	}

	llinfos << "Transmitting sessions stats" << llendl;

	LLSD body;
	body["session_id"] = gAgentSessionID;
	LLSD& agent = body["agent"];

	time_t ltime;
	time(&ltime);
	F32 run_time = (F32)LLFrameTimer::getElapsedSeconds();

	agent["start_time"] = S32(ltime - (time_t)run_time);

	// The first stat set must have a 0 run time if it doesn't actually contain
	// useful data in terms of FPS, etc. We use half the SEND_STATS_PERIOD
	// seconds as the point at which these statistics become valid. Data
	// warehouse uses a 0 value here to easily discard these records with
	// non-useful FPS values etc.
	if (run_time < SEND_STATS_PERIOD / 2)
	{
		agent["run_time"] = 0.f;
	}
	else
	{
		agent["run_time"] = run_time;
	}

	// Send FPS only for the time spent as a foreground application.
	F32 foreground_time = gForegroundTime.getElapsedTimeF32();
	if (foreground_time > 0.f)
	{
		F32 fps = F32(gForegroundFrameCount) / foreground_time;
		agent["fps"] = fps;
		// Let's also inform the server about any FPS limiting in force. This
		// is for now an unofficial stats and specific to the Cool VL Viewer,
		// but maybe LL will be interested in using it and generalizing it. HB
		static LLCachedControl<U32> max_fps(gSavedSettings, "FrameRateLimit");
		static LLCachedControl<bool> no_vsync(gSavedSettings,
											  "DisableVerticalSync");
		std::string fps_limit;
		if (max_fps >= 20)
		{
			fps_limit = llformat("%d fps", S32(max_fps));
		}
		if (!no_vsync)
		{
			if (!fps_limit.empty())
			{
				fps_limit += ", ";
			}
			fps_limit += "vsync";
		}
		if (fps_limit.empty())
		{
			fps_limit = "none";
		}
		agent["fps_limiting"] = fps_limit;
		llinfos << "Average FPS rate since session start with window in foreground: "
				<< fps << " - Current FPS rate limiting: " << fps_limit
				<< llendl;
	}

	agent["version"] = gCurrentVersion;
	std::string language = LLUI::getLanguage();
	agent["language"] = language;

	agent["sim_fps"] = ((F32)gFrameCount - gSimFrames) /
						(F32)(gRenderStartTime.getElapsedTimeF32() - gSimLastTime);

	gSimLastTime = gRenderStartTime.getElapsedTimeF32();
	gSimFrames = (F32)gFrameCount;

	agent["agents_in_view"] = LLVOAvatar::sNumVisibleAvatars;
	agent["ping"] = gAvgSimPing;
	agent["meters_traveled"] = gAgent.getDistanceTraveled();
	agent["regions_visited"] = gAgent.getRegionsVisited();
	agent["mem_use"] = LLMemory::getCurrentRSS() / 1024.0;

	// Let's cache this non-varying data... HB
	static LLSD system;
	if (system.isUndefined())
	{
		system["ram"] = (S32)LLMemory::getPhysicalMemoryKB();
		system["os"] = LLOSInfo::getInstance()->getOSStringSimple();
		LLCPUInfo* cpuinfo = LLCPUInfo::getInstance();
		system["cpu"] = cpuinfo->getCPUString();
		system["cpu_sse"] = cpuinfo->getSSEVersions();
		// This is now *always* 64 bits for the Cool VL Viewer
		system["address_size"] = 64;
		system["os_bitness"] = 64;
		system["hardware_concurrency"] = (S32)cpuinfo->getPhysicalCores();
		std::string mac_string = llformat("%02x-%02x-%02x-%02x-%02x-%02x",
										  gMACAddress[0], gMACAddress[1],
										  gMACAddress[2], gMACAddress[3],
										  gMACAddress[4], gMACAddress[5]);
		system["mac_address"] = mac_string;
		system["serial_number"] = gAppViewerp->getSerialNumber();

		std::string gpu_desc =
			llformat("%-6s Class %d ",
					 gGLManager.mGLVendorShort.substr(0,6).c_str(),
					 (S32)gFeatureManager.getGPUClass()) +
			gFeatureManager.getGPUString();
		system["gpu"] = gpu_desc;
		system["gpu_class"] = (S32)gFeatureManager.getGPUClass();
		F32 bw = gFeatureManager.getGPUMemoryBandwidth();
		if (bw > 0.f)
		{
			system["gpu_memory_bandwidth"] = bw;
		}
		system["gpu_vendor"] = gGLManager.mGLVendorShort;
		system["gpu_version"] = gGLManager.mDriverVersionVendorString;
		system["opengl_version"] = gGLManager.mGLVersionString;
		gGLManager.asLLSD(system["gl"]);
	}
	body["system"] = system;

	S32 shader_level = 0;
	if (LLPipeline::sRenderDeferred)
	{
		if (LLPipeline::RenderShadowDetail)
		{
			shader_level = 5;
		}
		else if (LLPipeline::RenderDeferredSSAO)
		{
			shader_level = 4;
		}
		else
		{
			shader_level = 3;
		}
	}
	else if (gPipeline.canUseWindLightShaders())
	{
		shader_level = 2;
	}
	else if (gPipeline.shadersLoaded())
	{
		shader_level = 1;
	}
	system["shader_level"] = shader_level;

	LLSD& download = body["downloads"];
	download["world_kbytes"] = gTotalWorldBytes / 1024.0;
	download["object_kbytes"] = gTotalObjectBytes / 1024.0;
	download["texture_kbytes"] = gTotalTextureBytes / 1024.0;
	download["mesh_kbytes"] = LLMeshRepository::sBytesReceived / 1024.0;

	LLMessageSystem* msg = gMessageSystemp;
	LLSD& in = body["stats"]["net"]["in"];
	in["kbytes"] = msg->mTotalBytesIn / 1024.0;
	in["packets"] = (S32)msg->mPacketsIn;
	in["compressed_packets"] = (S32)msg->mCompressedPacketsIn;
	in["savings"] = (msg->mUncompressedBytesIn -
					 msg->mCompressedBytesIn) / 1024.0;

	LLSD& out = body["stats"]["net"]["out"];
	out["kbytes"] = msg->mTotalBytesOut / 1024.0;
	out["packets"] = (S32)msg->mPacketsOut;
	out["compressed_packets"] = (S32)msg->mCompressedPacketsOut;
	out["savings"] = (msg->mUncompressedBytesOut -
					  msg->mCompressedBytesOut) / 1024.0;

	LLSD& fail = body["stats"]["failures"];
	fail["send_packet"] = (S32)msg->mSendPacketFailureCount;
	fail["dropped"] = (S32)msg->mDroppedPackets;
	fail["resent"] = (S32)msg->mResentPackets;
	fail["failed_resends"] = (S32)msg->mFailedResendPackets;
	fail["off_circuit"] = (S32)msg->mOffCircuitPackets;
	fail["invalid"] = (S32)msg->mInvalidOnCircuitPackets;

	// Misc stats, two strings and two ints. These are not expected to persist
	// across multiple releases. Comment any changes with your name and the
	// expected release revision. If the current revision is recent, ping the
	// previous author before overriding.
	LLSD& misc = body["stats"]["misc"];

#if LL_DARWIN
	// No Vulkan driver detection code for macOS... *TODO: detect MoltenVK ?
	misc["string_1"] = "";
#else
	std::string version;
	if (gAppViewerp->probeVulkan(version))
	{
		misc["string_1"] = "Vulkan driver is detected";
		misc["VulkanMaxApiVersion"] = version;
	}
	else
	{
		misc["string_1"] = "No Vulkan driver detected";
	}
#endif

	if (gFrameTimeSeconds > 0)
	{
		misc["string_2"] = llformat("Texture Time: %.2f, Total Time: %.2f",
									gTextureTimer.getElapsedTimeF32(),
									gFrameTimeSeconds);
	}
	else
	{
		misc["string_2"] = "Unused";
	}

	F32 unbaked_time = LLVOAvatar::sUnbakedTime * 1000.f / gFrameTimeSeconds;
	misc["int_1"] = LLSD::Integer(unbaked_time); // Steve: 1.22
	F32 grey_time = LLVOAvatar::sGreyTime * 1000.f / gFrameTimeSeconds;
	misc["int_2"] = LLSD::Integer(grey_time); // Steve: 1.22

	llinfos << "Misc stats: int_1: " << misc["int_1"].asInteger()
			<< " - int_2: " << misc["int_2"].asInteger() << llendl;
	llinfos << "Misc stats: string_1: " << misc["string_1"].asString()
			<< " - string_2: " << misc["string_2"].asString() << llendl;

	U32 display_names_usage = LLAvatarNameCache::useDisplayNames();
	body["DisplayNamesEnabled"] = display_names_usage != 0;
	// The Cool VL Viewer actually shows legacy names, never user names...
	body["DisplayNamesShowUsername"] = display_names_usage != 2;

	addToMessage(body);

	LL_DEBUGS("ViewerStats") << "Sending stats:\n" << ll_pretty_print_sd(body)
							 << LL_ENDL;
	LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(url, body,
														  "Statistics posted to sim",
														  "Failed to post statistics to sim");
	mNextStatsSendingTime = gFrameTimeSeconds + SEND_STATS_PERIOD;
}

void LLViewerStats::resetAvatarStats()
{
	mNextAvStatsSendingTime = gFrameTimeSeconds + SEND_AVATAR_STATS_PERIOD;
	mComplexityReports = mTooComplexReports = 0;
}

void LLViewerStats::avatarRenderingStats()
{
	mComplexityReports = mTooComplexReports = 0;

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp) return;

	// Unlike LL's lame code we limit ourselves to our region and neighbouring
	// regions (since anyway avatars beyond these are not fully rendered, when
	// at all). HB
	std::vector<LLViewerRegion*> regions;
	regions.push_back(regionp);
	regionp->getNeighboringRegions(regions);

	// Construct a map of <regionp, cap_url> with live regions actually having
	// the necessary capability.
	fast_hmap<LLViewerRegion*, std::string> regions_cap;
	for (S32 i = 0, count = regions.size(); i < count; ++i)
	{
		regionp = regions[i];
		if (regionp && regionp->isAlive() && regionp->capabilitiesReceived())
		{
			const std::string& url =
				regionp->getCapability("AvatarRenderInfo");
			if (!url.empty())
			{
				regions_cap.emplace(regionp, url);
			}
		}
	}

	if (regions_cap.empty())
	{
		// Do not bother...
		mNextAvStatsSendingTime = gFrameTimeSeconds + SEND_AVATAR_STATS_PERIOD;
		return;
	}

	// Construct a LLSD with all avatars we got a complexity for, in our own
	// and all neighbouring regions that got the necessary capability. Unlike
	// LL's lame code, we scan the characters list only once for all regions
	// (instead of once per region !). HB
	LLSD data = LLSD::emptyMap();
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatar* avatarp = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (avatarp && !avatarp->isDead() && !avatarp->mIsDummy &&
			!avatarp->isOrphaned() && avatarp->isFullyLoaded(true))
		{
			regionp = avatarp->getRegion();
			if (!regionp || !regions_cap.count(regionp)) continue;

			U32 complexity = avatarp->getVisualComplexity();
			if (complexity)
			{
				LLSD info = LLSD::emptyMap();
				if (complexity > (U32)S32_MAX) // very unlikely...
				{
					complexity = S32_MAX;
				}
				info[KEY_WEIGHT] = LLSD::Integer(complexity);
				info[KEY_TOO_COMPLEX] = LLSD::Boolean(avatarp->isTooComplex());

				const LLUUID& rg_id = regionp->getRegionID();
				const LLUUID& av_id = avatarp->getID();
				data[rg_id.asString()][KEY_AGENTS][av_id.asString()] = info;
			}
		}
	}

	// Send the stats request and report (if any for the latter) for the
	// concerned regions. HB
	for (fast_hmap<LLViewerRegion*, std::string>::const_iterator
			it = regions_cap.begin(), end = regions_cap.end();
		 it != end; ++it)
	{
		regionp = it->first;
		const std::string& url = it->second;

		gCoros.launch("LLViewerStats::getAvatarRenderStatsCoro",
					  boost::bind(&LLViewerStats::getAvatarRenderStatsCoro, url,
								  regionp->getName()));

		std::string region_id_str = regionp->getRegionID().asString();
		if (!data.has(region_id_str)) continue;

		gCoros.launch("LLViewerStats::sendAvatarRenderStatsCoro",
					  boost::bind(&LLViewerStats::sendAvatarRenderStatsCoro,
								  url, regionp->getName(),
								  data[region_id_str]));
	}

	mNextAvStatsSendingTime = gFrameTimeSeconds + SEND_AVATAR_STATS_PERIOD;
}

//static
void LLViewerStats::getAvatarRenderStatsCoro(const std::string& url,
											 std::string region_name)
{
	if (url.empty()) return;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("getAvatarRenderStats");
	LLSD result = adapter.getAndSuspend(url);
	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "HTTP error getting avatar rendering stats for region '"
				<< region_name << "'. Status: " << status.toString() << llendl;
		return;
	}
#if 0	// Not really useful...
	else if (result.isMap() && result.has(KEY_AGENTS))
	{
		const LLSD& agents = result[KEY_AGENTS];
		if (agents.isMap())
		{
			for (LLSD::map_const_iterator it = agents.beginMap(),
										  end = agents.endMap();
				 it != end; ++it)
			{
				LLUUID agent_id(it->first);
				LLVOAvatar* avatarp = gObjectList.findAvatar(agent_id);
				if (avatarp)
				{
					const LLSD& agent_info_map = it->second;
					if (agent_info_map.isMap())
					{
						LL_DEBUGS("ViewerStats") << "Agent " << target_agent_id
												 << ": " << agent_info_map
												 << LL_ENDL;
					}
				}
			}
		}
	}
#endif

	if (result.has(KEY_REPORTING_COMPLEXITY_LIMIT) &&
		result.has(KEY_OVER_COMPLEXITY_LIMIT))
	{
		U32 reporting = result[KEY_REPORTING_COMPLEXITY_LIMIT].asInteger();
		gViewerStats.mComplexityReports += reporting;
		U32 overlimit = result[KEY_OVER_COMPLEXITY_LIMIT].asInteger();
		gViewerStats.mTooComplexReports += overlimit;
		if (reporting != 0 || overlimit != 0)
		{
			llinfos << "Complexity reports: "
					<< gViewerStats.mComplexityReports
					<< " - Too complex reports: "
					<< gViewerStats.mTooComplexReports
					<< llendl;
		}
	}
	else
	{
		llwarns << "Malformed response to the last avatar rendering stats query in region: "
				<< region_name << ":\n" << result.asString() << llendl;
	}
}

//static
void LLViewerStats::sendAvatarRenderStatsCoro(const std::string& url,
											  std::string region_name,
											  const LLSD& data)
{
	if (url.empty()) return;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("sendAvatarRenderStats");
	LLSD result = adapter.postAndSuspend(url, data);
	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "HTTP error sending avatar rendering stats for region '"
				<< region_name << "'. Status: " << status.toString() << llendl;
	}
	else if (result.isMap() && result.has(KEY_ERROR))
	{
		const LLSD& error = result[KEY_ERROR];
		llwarns << "Error sending avatar rendering stats for region '"
				<< region_name << "': " << error[KEY_MESSAGE] << llendl;
	}
	else
	{
		LL_DEBUGS("ViewerStats") << "result for avatar rendering stats sending to region: "
								 << region_name << ":\n" << result.asString()
								 << LL_ENDL;
	}
}

void output_statistics(void*)
{
	llinfos << "--------------------------------" << llendl;
	llinfos << "Objects:" << llendl;
	llinfos << "--------------------------------" << llendl;
	S32 num_objects = LLViewerObject::getNumObjects();
	llinfos << "Number of Viewer Objects in memory: " << num_objects << llendl;
	S32 listed_objects = gObjectList.getNumObjects();
	llinfos << "Number of objects in list: " << listed_objects << llendl;
	llinfos << "Zombie Viewer Objects: " << num_objects - listed_objects
			<< llendl;
	llinfos << "Number of dead objects: " << gObjectList.getNumDeadObjects()
			<< llendl;
	llinfos << "Number of orphans: " << gObjectList.getOrphanCount() << llendl;
	LLVolumeImplFlexible::dumpStats();

	llinfos << "--------------------------------" << llendl;
	llinfos << "Images:" << llendl;
	llinfos << "--------------------------------" << llendl;
	llinfos << "Num images: " << gTextureList.getNumImages() << llendl;
	llinfos << "Texture usage: " << LLImageGL::sGlobalTexMemBytes << llendl;
	llinfos << "Bound textures usage: " << LLImageGL::sBoundTexMemBytes
			<< llendl;
	LLImage::dumpStats();
	LLViewerTextureAnim::dumpStats();

	llinfos << "--------------------------------" << llendl;
	llinfos << "Lights:" << llendl;
	llinfos << "--------------------------------" << llendl;
	llinfos << "Number of lights: " << gPipeline.getLightCount() << llendl;

	llinfos << "--------------------------------" << llendl;
	llinfos << "Memory Usage:" << llendl;
	llinfos << "--------------------------------" << llendl;
	LLMemory::logMemoryInfo();

	llinfos << "--------------------------------" << llendl;
	llinfos << "Pipeline:" << llendl;
	llinfos << "--------------------------------" << llendl;
	gPipeline.dumpStats();

	llinfos << "--------------------------------" << llendl;
	llinfos << "Characters and motion controller:" << llendl;
	llinfos << "--------------------------------" << llendl;
	LLCharacter::dumpStats();
	LLMotionController::dumpStats();

	llinfos << "--------------------------------" << llendl;
	llinfos << "Avatar Memory (partly overlaps with above stats):" << llendl;
	llinfos << "--------------------------------" << llendl;
	gTexLayerStaticImageList.dumpByteCount();
	LLViewerTexLayerSetBuffer::dumpTotalByteCount();
	LLVOAvatarSelf::dumpTotalLocalTextureByteCount();
	LLTexLayerParamAlpha::dumpCacheByteCount();
	LLVOAvatar::dumpBakedStatus();

	llinfos << llendl;

	llinfos << "Object counts:" << llendl;
	S32 obj_counts[256];
	for (S32 i = 0; i < 256; ++i)
	{
		obj_counts[i] = 0;
	}
	for (S32 i = 0, count = gObjectList.getNumObjects(); i < count; ++i)
	{
		LLViewerObject* objectp = gObjectList.getObject(i);
		if (objectp)
		{
			++obj_counts[objectp->getPCode()];
		}
	}
	for (S32 i = 0; i < 256; ++i)
	{
		if (obj_counts[i])
		{
			llinfos << LLPrimitive::pCodeToString(i) << ":" << obj_counts[i]
					<< llendl;
		}
	}
	llinfos << "--------------------------------" << llendl;
}
