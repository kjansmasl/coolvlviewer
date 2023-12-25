/**
 * @file llfloaterstats.cpp
 * @brief Container for statistics view
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterstats.h"

#include "llcontainerview.h"
#include "llfloater.h"
#include "llscrollcontainer.h"
#include "llstatview.h"
#include "lluictrlfactory.h"

#include "llappviewer.h"			// gFrameTimeSeconds
#include "llpipeline.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"		// gLastFPSAverage
#include "llviewerobjectlist.h"
#include "llviewerstats.h"
#include "llviewertexturelist.h"

constexpr S32 LL_SCROLL_BORDER = 1;

LLFloaterStats::LLFloaterStats(const LLSD& val)
:   LLFloater("stats"),
	mStatsContainer(NULL),
	mScrollContainer(NULL),
	mFPSStatBar(NULL),
	mLastFPSAverageCount(0),
	mStatBarMaxFPS(100.f),
	mStatBarLastMaxBW(0),
	mCurrentMaxFPS(0.f),
	mCurrentMaxBW(0.f),
	mLastStatRangeChange(0.f)
{
	gPipeline.mNeedsDrawStats = true;

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_statistics.xml",
												 NULL, false);

	LLRect stats_rect(0, getRect().getHeight() - LLFLOATER_HEADER_SIZE,
					  getRect().getWidth() - LLFLOATER_CLOSE_BOX_SIZE, 0);
	mStatsContainer = new LLContainerView("statistics_view", stats_rect);
	mStatsContainer->showLabel(false);

	LLRect scroll_rect(LL_SCROLL_BORDER,
					   getRect().getHeight() - LLFLOATER_HEADER_SIZE - LL_SCROLL_BORDER,
					   getRect().getWidth() - LL_SCROLL_BORDER,
					   LL_SCROLL_BORDER);
	mScrollContainer = new LLScrollableContainer("statistics_scroll",
												 scroll_rect, mStatsContainer);
	mScrollContainer->setFollowsAll();
	mScrollContainer->setReserveScrollCorner(true);

	mStatsContainer->setScrollContainer(mScrollContainer);

	addChild(mScrollContainer);

	buildStats();
}

//virual
LLFloaterStats::~LLFloaterStats()
{
	if (!gSavedSettings.getBool("DebugShowRenderInfo"))
	{
		gPipeline.mNeedsDrawStats = false;
	}
}

void LLFloaterStats::setFPSStatBarRange(U32 average)
{
	mStatBarMaxFPS = (F32)((average / 50U + 1U) * 50U);
	mFPSStatBar->mMaxBar = mStatBarMaxFPS;
	mFPSStatBar->mTickSpacing = mStatBarMaxFPS / 10.f;
	mFPSStatBar->mLabelSpacing = mStatBarMaxFPS / 5.f;
	mCurrentMaxFPS = 0.f;
	mLastStatRangeChange = gFrameTimeSeconds;
}

void LLFloaterStats::setBWStatBarRange(U32 max)
{
	mStatBarLastMaxBW = llmin(((max / 1000U + 1U) * 1000U), 1000000U);
	mBWStatBar->mMaxBar = mStatBarLastMaxBW;
	mBWStatBar->mTickSpacing = mStatBarLastMaxBW / 10.f;
	mBWStatBar->mLabelSpacing = mStatBarLastMaxBW / 2.f;
	mCurrentMaxBW = 0.f;
	mLastStatRangeChange = gFrameTimeSeconds;
}

// NOTE: this cannot be implemented as a postBuild() (would cause a crash).
void LLFloaterStats::buildStats()
{
	LLRect rect;

	// Basic stats
	LLStatView* stat_viewp = new LLStatView("basic stat view", "Basic",
											"OpenDebugStatBasic", rect);
	addStatView(stat_viewp);

	mFPSStatBar = stat_viewp->addStat("FPS",
									  &gViewerStats.mFPSStat,
									  "DebugStatModeFPS", true, true);
	mFPSStatBar->setUnitLabel(" fps");
	mFPSStatBar->mMinBar = 0.f;
	mFPSStatBar->mPrecision = 1;
	setFPSStatBarRange(100);
	mLastStatRangeChange = 0.f;

	mBWStatBar = stat_viewp->addStat("Bandwidth",
									 &gViewerStats.mKBitStat,
									 "DebugStatModeBandwidth", true, false);
	mBWStatBar->setUnitLabel(" Kbps");
	mBWStatBar->mMinBar = 0.f;
	mBWStatBar->mMaxBar = 2048.f;
	mBWStatBar->mTickSpacing = 256.f;
	mBWStatBar->mLabelSpacing = 512.f;

	LLStatBar* stat_barp =
		stat_viewp->addStat("Packet loss",
							&gViewerStats.mPacketsLostPercentStat,
							"DebugStatModePacketLoss");
	stat_barp->setUnitLabel(" %");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 5.f;
	stat_barp->mTickSpacing = 1.f;
	stat_barp->mLabelSpacing = 1.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = true;
	stat_barp->mPrecision = 1;

	stat_barp = stat_viewp->addStat("Ping sim",
									&gViewerStats.mSimPingStat,
									"DebugStatMode");
	stat_barp->setUnitLabel(" ms");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1000.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	// Advanced stats
	stat_viewp = new LLStatView("advanced stat view", "Advanced",
								"OpenDebugStatAdvanced", rect);
	addStatView(stat_viewp);

	LLStatView* render_statviewp =
		stat_viewp->addStatView("render stat view", "Render",
								"OpenDebugStatRender", rect);

	stat_barp = render_statviewp->addStat("KTris drawn",
										  &gPipeline.mTrianglesDrawnStat,
										  "DebugStatModeKTrisDrawnFr");
	stat_barp->setUnitLabel("/fr");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1000.f;
	stat_barp->mTickSpacing = 250.f;
	stat_barp->mLabelSpacing = 500.f;
	stat_barp->mPerSec = false;

	stat_barp = render_statviewp->addStat("KTris drawn",
										  &gPipeline.mTrianglesDrawnStat,
										  "DebugStatModeKTrisDrawnSec");
	stat_barp->setUnitLabel("/s");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 50000.f;
	stat_barp->mTickSpacing = 12500.f;
	stat_barp->mLabelSpacing = 25000.f;

	stat_barp = render_statviewp->addStat("Total objects",
										  &gObjectList.mNumObjectsStat,
										  "DebugStatModeTotalObjs");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 15000.f;
	stat_barp->mTickSpacing = 2500.f;
	stat_barp->mLabelSpacing = 5000.f;
	stat_barp->mPerSec = false;

	stat_barp = render_statviewp->addStat("New objects",
										  &gObjectList.mNumNewObjectsStat,
										  "DebugStatModeNewObjs");
	stat_barp->setLabel("New Objs");
	stat_barp->setUnitLabel("/s");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1000.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 500.f;

	// Texture statistics
	LLStatView* texture_statviewp =
		render_statviewp->addStatView("texture stat view", "Texture",
									  "OpenDebugStatTexture", rect);

	stat_barp = texture_statviewp->addStat("Count",
										   &gTextureList.sNumImagesStat,
										   "DebugStatModeTextureCount");
	stat_barp->setUnitLabel("");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 8000.f;
	stat_barp->mTickSpacing = 2000.f;
	stat_barp->mLabelSpacing = 4000.f;
	stat_barp->mPerSec = false;

	stat_barp = texture_statviewp->addStat("Raw count",
										   &gTextureList.sNumRawImagesStat,
										   "DebugStatModeRawCount");
	stat_barp->setUnitLabel("");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 8000.f;
	stat_barp->mTickSpacing = 2000.f;
	stat_barp->mLabelSpacing = 4000.f;
	stat_barp->mPerSec = false;

	stat_barp = texture_statviewp->addStat("GL memory",
										   &gTextureList.sGLTexMemStat,
										   "DebugStatModeGLMem");
	stat_barp->setUnitLabel("");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 512.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;
	stat_barp->mPrecision = 1;
	stat_barp->mPerSec = false;

	stat_barp = texture_statviewp->addStat("Bound memory",
										   &gTextureList.sGLBoundMemStat,
										   "DebugStatModeBoundMem");
	stat_barp->setUnitLabel("");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 512.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;
	stat_barp->mPrecision = 1;
	stat_barp->mPerSec = false;


	// Network statistics
	LLStatView* net_statviewp = stat_viewp->addStatView("network stat view",
														"Network",
														"OpenDebugStatNet",
														rect);

	stat_barp = net_statviewp->addStat("Packets in",
									   &gViewerStats.mPacketsInStat,
									   "DebugStatModePacketsIn");
	stat_barp->setUnitLabel("/s");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1000.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 250.f;

	stat_barp = net_statviewp->addStat("Packets out",
									   &gViewerStats.mPacketsOutStat,
									   "DebugStatModePacketsOut");
	stat_barp->setUnitLabel("/s");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;

	stat_barp = net_statviewp->addStat("Objects",
									   &gViewerStats.mObjectKBitStat,
									   "DebugStatModeObjects");
	stat_barp->setUnitLabel(" Kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;

	stat_barp = net_statviewp->addStat("Texture",
									   &gViewerStats.mTextureKBitStat,
									   "DebugStatModeTexture");
	stat_barp->setUnitLabel(" Kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;

	stat_barp = net_statviewp->addStat("Asset",
									   &gViewerStats.mAssetKBitStat,
									   "DebugStatModeAsset");
	stat_barp->setUnitLabel(" Kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;

	stat_barp = net_statviewp->addStat("Layers",
									   &gViewerStats.mLayersKBitStat,
									   "DebugStatModeLayers");
	stat_barp->setUnitLabel(" Kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;

	stat_barp = net_statviewp->addStat("Actual in",
									   &gViewerStats.mActualInKBitStat,
									   "DebugStatModeActualIn", true, false);
	stat_barp->setUnitLabel(" Kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 2048.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;

	stat_barp = net_statviewp->addStat("Actual out",
									   &gViewerStats.mActualOutKBitStat,
									   "DebugStatModeActualOut", true, false);
	stat_barp->setUnitLabel(" Kbps");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 512.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;

	// Simulator stats
	LLStatView* sim_statviewp = new LLStatView("sim stat view", "Simulator",
											   "OpenDebugStatSim", rect);
	addStatView(sim_statviewp);

	stat_barp = sim_statviewp->addStat("Time dilation",
									   &gViewerStats.mSimTimeDilation,
									   "DebugStatModeTimeDialation");
	stat_barp->mPrecision = 2;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1.f;
	stat_barp->mTickSpacing = 0.25f;
	stat_barp->mLabelSpacing = 0.5f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Sim FPS",
									   &gViewerStats.mSimFPS,
									   "DebugStatModeSimFPS");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 60.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Physics FPS",
									   &gViewerStats.mSimPhysicsFPS,
									   "DebugStatModePhysicsFPS");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 60.f;
	stat_barp->mTickSpacing = 10.0f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	LLStatView* phys_details_viewp =
		sim_statviewp->addStatView("phys detail view", "Physics details",
								   "OpenDebugStatPhysicsDetails", rect);

	stat_barp = phys_details_viewp->addStat("Pinned objects",
											&gViewerStats.mPhysicsPinnedTasks,
											"DebugStatModePinnedObjects");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 500.f;
	stat_barp->mTickSpacing = 50.f;
	stat_barp->mLabelSpacing = 100.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = phys_details_viewp->addStat("Low LOD objects",
											&gViewerStats.mPhysicsLODTasks,
											"DebugStatModeLowLODObjects");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 500.f;
	stat_barp->mTickSpacing = 50.f;
	stat_barp->mLabelSpacing = 100.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp =
		phys_details_viewp->addStat("Memory allocated",
									&gViewerStats.mPhysicsMemoryAllocated,
									"DebugStatModeMemoryAllocated");
	stat_barp->setUnitLabel(" MB");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 1024.f;
	stat_barp->mTickSpacing = 128.f;
	stat_barp->mLabelSpacing = 256.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Agent updates/s",
									   &gViewerStats.mSimAgentUPS,
									   "DebugStatModeAgentUpdatesSec");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 60.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Main agents",
									   &gViewerStats.mSimMainAgents,
									   "DebugStatModeMainAgents");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 80.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 40.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Child agents",
									   &gViewerStats.mSimChildAgents,
									   "DebugStatModeChildAgents");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 5.f;
	stat_barp->mLabelSpacing = 10.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Objects",
									   &gViewerStats.mSimObjects,
									   "DebugStatModeSimObjects");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 30000.f;
	stat_barp->mTickSpacing = 5000.f;
	stat_barp->mLabelSpacing = 10000.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Active objects",
									   &gViewerStats.mSimActiveObjects,
									   "DebugStatModeSimActiveObjects");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 2000.f;
	stat_barp->mTickSpacing = 250.f;
	stat_barp->mLabelSpacing = 500.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Active scripts",
									   &gViewerStats.mSimActiveScripts,
									   "DebugStatModeSimActiveScripts");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 20000.f;
	stat_barp->mTickSpacing = 2500.f;
	stat_barp->mLabelSpacing = 5000.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Scripts run",
									   &gViewerStats.mSimPctScriptsRun,
									   "DebugStatModeSimPctScriptsRun");
	stat_barp->setUnitLabel(" %");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = true;
	stat_barp->mPrecision = 1;

	stat_barp = sim_statviewp->addStat("Script events",
									   &gViewerStats.mSimScriptEPS,
									   "DebugStatModeSimScriptEvents");
	stat_barp->setUnitLabel(" e/s");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 10000.f;
	stat_barp->mTickSpacing = 1000.f;
	stat_barp->mLabelSpacing = 2000.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	// Pathfinding stats in simulator stats
	LLStatView* path_finding_viewp =
		sim_statviewp->addStatView("pathfinding view", "Pathfinding",
								   "OpenDebugStatSimPathFindingDetails", rect);

	stat_barp = path_finding_viewp->addStat("AI step time",
											&gViewerStats.mSimSimAIStepMsec,
											"DebugStatModeSimSimAIStepMsec");
	stat_barp->setUnitLabel(" ms");
	stat_barp->mPrecision = 3;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 45.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp =
		path_finding_viewp->addStat("Skipped silhouette steps",
									&gViewerStats.mSimSimSkippedSilhouetteSteps,
									"DebugStatModeSimSimSkippedSilhouetteSteps");
	stat_barp->setUnitLabel("/s");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 45.f;
	stat_barp->mTickSpacing = 4.f;
	stat_barp->mLabelSpacing = 8.f;

	stat_barp =
		path_finding_viewp->addStat("Characters updated",
									&gViewerStats.mSimSimPctSteppedCharacters,
									"DebugStatModemSimSimPctSteppedCharacters");
	stat_barp->setUnitLabel(" %");
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = true;
	stat_barp->mPrecision = 1;


	// Simulator stats (continued)
	stat_barp = sim_statviewp->addStat("Packets in",
									   &gViewerStats.mSimInPPS,
									   "DebugStatModeSimInPPS");
	stat_barp->setUnitLabel(" p/s");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 20000.f;
	stat_barp->mTickSpacing = 2500.f;
	stat_barp->mLabelSpacing = 10000.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Packets out",
									   &gViewerStats.mSimOutPPS,
									   "DebugStatModeSimOutPPS");
	stat_barp->setUnitLabel(" p/s");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 20000.f;
	stat_barp->mTickSpacing = 2500.f;
	stat_barp->mLabelSpacing = 10000.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Pending downloads",
									   &gViewerStats.mSimPendingDownloads,
									   "DebugStatModeSimPendingDownloads");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 800.f;
	stat_barp->mTickSpacing = 100.f;
	stat_barp->mLabelSpacing = 200.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Pending uploads",
									   &gViewerStats.mSimPendingUploads,
									   "SimPendingUploads");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 100.f;
	stat_barp->mTickSpacing = 25.f;
	stat_barp->mLabelSpacing = 50.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_statviewp->addStat("Total unacked bytes",
									   &gViewerStats.mSimTotalUnackedBytes,
									   "DebugStatModeSimTotalUnackedBytes");
	stat_barp->setUnitLabel(" Kb");
	stat_barp->mPrecision = 0;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 10000.f;
	stat_barp->mTickSpacing = 2500.f;
	stat_barp->mLabelSpacing = 5000.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	LLStatView* sim_time_viewp =
		sim_statviewp->addStatView("sim perf view", "Time (ms)",
								   "OpenDebugStatSimTime", rect);

	stat_barp = sim_time_viewp->addStat("Total frame time",
										&gViewerStats.mSimFrameMsec,
										"DebugStatModeSimFrameMsec");
	stat_barp->setUnitLabel(" ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 50.f;
	stat_barp->mTickSpacing = 12.5f;
	stat_barp->mLabelSpacing = 25.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_time_viewp->addStat("Net time",
										&gViewerStats.mSimNetMsec,
										"DebugStatModeSimNetMsec");
	stat_barp->setUnitLabel(" ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 50.f;
	stat_barp->mTickSpacing = 12.5f;
	stat_barp->mLabelSpacing = 25.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_time_viewp->addStat("Physics time",
										&gViewerStats.mSimSimPhysicsMsec,
										"DebugStatModeSimSimPhysicsMsec");
	stat_barp->setUnitLabel(" ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 20.f;
	stat_barp->mTickSpacing = 5.f;
	stat_barp->mLabelSpacing = 10.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_time_viewp->addStat("Simulation time",
										&gViewerStats.mSimSimOtherMsec,
										"DebugStatModeSimSimOtherMsec");
	stat_barp->setUnitLabel(" ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 20.f;
	stat_barp->mTickSpacing = 5.f;
	stat_barp->mLabelSpacing = 10.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_time_viewp->addStat("Agent time",
										&gViewerStats.mSimAgentMsec,
										"DebugStatModeSimAgentMsec");
	stat_barp->setUnitLabel(" ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 20.f;
	stat_barp->mTickSpacing = 5.f;
	stat_barp->mLabelSpacing = 10.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_time_viewp->addStat("Images time",
										&gViewerStats.mSimImagesMsec,
										"DebugStatModeSimImagesMsec");
	stat_barp->setUnitLabel(" ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 20.f;
	stat_barp->mTickSpacing = 5.f;
	stat_barp->mLabelSpacing = 10.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_time_viewp->addStat("Script time",
										&gViewerStats.mSimScriptMsec,
										"DebugStatModeSimScriptMsec");
	stat_barp->setUnitLabel(" ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 40.f;
	stat_barp->mTickSpacing = 10.f;
	stat_barp->mLabelSpacing = 20.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	stat_barp = sim_time_viewp->addStat("Spare time",
										&gViewerStats.mSimSpareMsec,
										"DebugStatModeSimSpareMsec");
	stat_barp->setUnitLabel(" ms");
	stat_barp->mPrecision = 1;
	stat_barp->mMinBar = 0.f;
	stat_barp->mMaxBar = 20.f;
	stat_barp->mTickSpacing = 5.f;
	stat_barp->mLabelSpacing = 10.f;
	stat_barp->mPerSec = false;
	stat_barp->mDisplayMean = false;

	// 2nd level time blocks under 'Details' second
	LLStatView* detailed_time_viewp =
		sim_time_viewp->addStatView("sim perf view", "Time details (ms)",
									"OpenDebugStatSimTimeDetails", rect);
	{
		stat_barp =
			detailed_time_viewp->addStat("Physics step",
										 &gViewerStats.mSimSimPhysicsStepMsec,
										 "DebugStatModeSimSimPhysicsStepMsec");
		stat_barp->setUnitLabel(" ms");
		stat_barp->mPrecision = 1;
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 20.f;
		stat_barp->mTickSpacing = 5.f;
		stat_barp->mLabelSpacing = 10.f;
		stat_barp->mPerSec = false;
		stat_barp->mDisplayMean = false;

		stat_barp =
			detailed_time_viewp->addStat("Update phys. shapes",
										 &gViewerStats.mSimSimPhysicsShapeUpdateMsec,
										 "DebugStatModeSimSimPhysicsShapeUpdateMsec");
		stat_barp->setUnitLabel(" ms");
		stat_barp->mPrecision = 1;
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 20.f;
		stat_barp->mTickSpacing = 5.f;
		stat_barp->mLabelSpacing = 10.f;
		stat_barp->mPerSec = false;
		stat_barp->mDisplayMean = false;

		stat_barp = detailed_time_viewp->addStat("Physics other",
												 &gViewerStats.mSimSimPhysicsOtherMsec,
												 "DebugStatModeSimSimPhysicsOtherMsec");
		stat_barp->setUnitLabel(" ms");
		stat_barp->mPrecision = 1;
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 20.f;
		stat_barp->mTickSpacing = 5.f;
		stat_barp->mLabelSpacing = 10.f;
		stat_barp->mPerSec = false;
		stat_barp->mDisplayMean = false;

		stat_barp = detailed_time_viewp->addStat("Sleep time",
												 &gViewerStats.mSimSleepMsec,
												 "DebugStatModeSimSleepMsec");
		stat_barp->setUnitLabel(" ms");
		stat_barp->mPrecision = 1;
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 20.f;
		stat_barp->mTickSpacing = 5.f;
		stat_barp->mLabelSpacing = 10.f;
		stat_barp->mPerSec = false;
		stat_barp->mDisplayMean = false;

		stat_barp = detailed_time_viewp->addStat("Pump IO",
												 &gViewerStats.mSimPumpIOMsec,
												 "DebugStatModeSimPumpIOMsec");
		stat_barp->setUnitLabel(" ms");
		stat_barp->mPrecision = 1;
		stat_barp->mMinBar = 0.f;
		stat_barp->mMaxBar = 20.f;
		stat_barp->mTickSpacing = 5.f;
		stat_barp->mLabelSpacing = 10.f;
		stat_barp->mPerSec = false;
		stat_barp->mDisplayMean = false;
	}

	LLRect r = getRect();

	// Reshape based on the parameters we set.
	reshape(r.getWidth(), r.getHeight());
}

//virtual
void LLFloaterStats::reshape(S32 width, S32 height, bool called_from_parent)
{
	if (mStatsContainer)
	{
		LLRect rect = mStatsContainer->getRect();
		mStatsContainer->reshape(rect.getWidth() - 2, rect.getHeight());
	}

	LLFloater::reshape(width, height, called_from_parent);
}

void LLFloaterStats::addStatView(LLStatView* stat)
{
	mStatsContainer->addChildAtEnd(stat);
}

//virtual
void LLFloaterStats::onOpen()
{
	LLFloater::onOpen();
	gSavedSettings.setBool("ShowDebugStats", true);
	reshape(getRect().getWidth(), getRect().getHeight());
}

//virtual
void LLFloaterStats::onClose(bool app_quitting)
{
	setVisible(false);
	if (!app_quitting)
	{
		gSavedSettings.setBool("ShowDebugStats", false);
	}
}

//virtual
void LLFloaterStats::draw()
{
	F32 fps = gViewerStats.mFPSStat.getCurrentPerSec();
	if (fps > mCurrentMaxFPS)
	{
		mCurrentMaxFPS = fps;
	}
	F32 bw = gViewerStats.mKBitStat.getMeanPerSec() * 1.5f;
	if (bw > mCurrentMaxBW)
	{
		mCurrentMaxBW = bw;
		mLastStatRangeChange = 0.f;	// Change immediately
	}

	if (gFrameTimeSeconds - mLastStatRangeChange > 2.f)
	{
		if (gLastFPSAverage && gLastFPSAverage != mLastFPSAverageCount)
		{
			mLastFPSAverageCount = gLastFPSAverage;
			setFPSStatBarRange(mLastFPSAverageCount);
		}
		if (mCurrentMaxFPS > mStatBarMaxFPS && mCurrentMaxFPS > 50.f)
		{
			setFPSStatBarRange((U32)mCurrentMaxFPS - 49);
		}

		if (mCurrentMaxBW > mStatBarLastMaxBW)
		{
			setBWStatBarRange(mCurrentMaxBW);
		}
	}

	LLFloater::draw();
}
