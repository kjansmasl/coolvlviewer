/**
* @file llstatusbar.cpp
* @brief LLStatusBar class implementation
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

#include "llstatusbar.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llmenugl.h"				// For gMenuBarHeight
#include "llparcel.h"
#include "lllineeditor.h"
#include "lllocale.h"
#include "llnotifications.h"
#include "llsys.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappearancemgr.h"
#include "llappviewer.h"			// For gFrameTimeSeconds
#include "llcommandhandler.h"
#include "llfloaterbuycurrency.h"
#include "llfloaterlagmeter.h"
#include "llfloaterland.h"
#include "llfloaterregioninfo.h"
#include "llfloaterscriptdebug.h"
#include "hbfloatersearch.h"		// To spawn search
#include "llfloaterstats.h"
#include "llgridmanager.h"
#include "llgroupnotify.h"
#include "llnotify.h"
#include "lloverlaybar.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llstatgraph.h"
#include "hbviewerautomation.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"			// gMenuBarViewp, handle_rebake_textures()
#include "llviewerparceloverlay.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llvoavatarself.h"
#include "llworld.h"

//
// Globals
//

// Instance created in LLViewerWindow::initWorldUI()
LLStatusBar* gStatusBarp = NULL;

S32 gStatusBarHeight = 26;	// Loaded from settings.xml in llappviewer.cpp

// *TODO: the there following values ought to be in the XML too
constexpr S32 TEXT_HEIGHT = 18;
constexpr S32 SIM_STAT_WIDTH = 8;
// Distance from right of menu item to parcel information:
constexpr S32 MENU_PARCEL_SPACING = 1;

// Memory stats graph and health icon flash duration and rate:
constexpr F32 FLASH_TIMER_EXPIRY	= 5.f;	// Flash duration in seconds
constexpr F32 FLASH_FREQUENCY		= 2.f;	// Flash rate in Hertz

// Parcel status refresh rate (necessary so that parcel permissions changes
// get updated without needing to exit and re-enter the parcel...).
constexpr F32 PARCEL_TIMER_EXPIRY	= 60.f;

// Do not refresh the status bar layout and icons visibility more than 5 times
// per second to avoid wasting precious CPU cycles for nothing...).
constexpr F32 STATUS_REFRESH_INTERVAL = 0.2f;

// Ellapsed delay after the first 0 UDP bandwith detection, beyond which the
// network will be considered down.
constexpr F32 NET_TIMEOUT = 4.f * STATUS_REFRESH_INTERVAL;

///////////////////////////////////////////////////////////////////////////////
// Implements secondlife:///app/balance/request to request a L$ balance update
// via UDP message system. JC
class LLBalanceHandler final : public LLCommandHandler
{
public:
	// Requires "trusted" browser/URL source
	LLBalanceHandler()
	:	LLCommandHandler("balance", UNTRUSTED_BLOCK)
	{
	}

	bool handle(const LLSD& tokens, const LLSD&, LLMediaCtrl*) override
	{
		if (tokens.size() == 1 && tokens[0].asString() == "request")
		{
			LLStatusBar::sendMoneyBalanceRequest();
			return true;
		}
		return false;
	}
};
// register with command dispatch system
LLBalanceHandler gBalanceHandler;

///////////////////////////////////////////////////////////////////////////////
// LLStatusBar class

LLStatusBar::LLStatusBar(const LLRect& rect)
:	LLPanel("status bar", LLRect(), false),	// Not mouse opaque
	mUseOldIcons(true),
	mParcelTextColor(LLColor4(gColors.getColor("ParcelTextColor"))),
	mVisibility(true),
	mDirty(true),
	mBalance(0),
	mHealth(100),
	mLastNotifications(0),
	mSquareMetersCredit(0),
	mSquareMetersCommitted(0),
	mAgentRegionFailedEventPolls(0),
	mLastZeroBandwidthTime(0.f),
	mNetworkDown(false),
	mFrameRateLimited(false)
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_status_bar.xml");

	mTimeMode = llmin(gSavedSettings.getU32("StatusBarTimeMode"),
					  U32(TIME_MODE_END - 1));

	mAbsoluteMaxBandwidth = gSavedSettings.getU32("ThrottleBandwidthKbps");

	mTextParcelName = getChild<LLTextBox>("ParcelNameText");
	mTextParcelName->setClickedCallback(onClickParcelInfo);

	mBtnScriptError = getChild<LLButton>("script_error");
	mBtnScriptError->setClickedCallback(onClickScriptDebug, this);

	mBtnLuaFunction = getChild<LLButton>("lua");
	mBtnLuaFunction->setClickedCallback(onClickLuaFunction, this);

	mBtnRebaking = getChild<LLButton>("not_rezzed");
	mBtnRebaking->setClickedCallback(handle_rebake_textures, NULL);

	mTooComplex = getChild<LLButton>("too_complex");
	mTooComplex->setClickedCallback(onClickTooComplex, this);
	mTextTooComplex = getChild<LLTextBox>("too_complex_reports");

	mBtnHealth = getChild<LLButton>("health");
	mBtnHealth->setClickedCallback(onClickHealth, this);
	mTextHealth = getChild<LLTextBox>("HealthText");

	mBtnNoFly = getChild<LLButton>("no_fly");
	mBtnNoFly->setClickedCallback(onClickFly, this);

	mBtnNoBuild = getChild<LLButton>("no_build");
	mBtnNoBuild->setClickedCallback(onClickBuild, this);

	mBtnNoScript = getChild<LLButton>("no_scripts");
	mBtnNoScript->setClickedCallback(onClickScripts, this);

	mBtnNoPush = getChild<LLButton>("no_push");
	mBtnNoPush->setClickedCallback(onClickPush, this);

	mBtnNoVoice = getChild<LLButton>("no_voice");
	mBtnNoVoice->setClickedCallback(onClickVoice, this);

	mBtnNoSee = getChild<LLButton>("no_see");
	mBtnNoSee->setClickedCallback(onClickSee, this);

	mBtnNoPathFinding = getChild<LLButton>("no_path_finding");
	mBtnNoPathFinding->setClickedCallback(onClickPathFinding, this);

	mBtnDirtyNavMesh = getChild<LLButton>("dirty_nav_mesh");
	mBtnDirtyNavMesh->setClickedCallback(onClickDirtyNavMesh, this);

	mBtnBuyLand = getChild<LLButton>("buy_land");
	mBtnBuyLand->setClickedCallback(onClickBuyLand, this);

	mBtnNotificationsOn = getChild<LLButton>("notifications_on");
	mBtnNotificationsOn->setClickedCallback(onClickNotifications, this);
	mBtnNotificationsOff = getChild<LLButton>("notifications_off");
	mBtnNotificationsOff->setClickedCallback(onClickNotifications, this);
	mTextNotifications = getChild<LLTextBox>("notifications");

	mBtnAdult = getChild<LLButton>("adult");
	mBtnAdult->setClickedCallback(onClickAdult, this);

	mBtnMature = getChild<LLButton>("mature");
	mBtnMature->setClickedCallback(onClickMature, this);

	mBtnPG = getChild<LLButton>("pg");
	mBtnPG->setClickedCallback(onClickPG, this);

	mTextTime = getChild<LLTextBox>("TimeText");
	mTextTime->setClickedCallback(onClickTime, this);

	mTextBalance = getChild<LLTextBox>("BalanceText");
	mTextBalance->setClickedCallback(onClickBalance);

	mBtnBuyMoney = getChild<LLButton>("buy_money");
	mBtnBuyMoney->setClickedCallback(onClickBalance, this);

	mTextFPS = getChild<LLTextBox>("fps");
	mTextFPS->setClickedCallback(onClickFPS);

	bool search_bar = gSavedSettings.getBool("ShowSearchBar");
	mBtnSearch = getChild<LLButton>("search_btn");
	mBtnSearch->setVisible(search_bar);
	mBtnSearch->setClickedCallback(onClickSearch, this);
	mBtnSearchBevel = getChild<LLButton>("menubar_search_bevel_bg");
	mBtnSearchBevel->setVisible(search_bar);
	mLineEditSearch = getChild<LLLineEditor>("search_editor");
	mLineEditSearch->setVisible(search_bar);
	mLineEditSearch->setCommitCallback(onCommitSearch);
	mLineEditSearch->setCallbackUserData(this);

	// Adding Net Stat Graph
	S32 x = getRect().getWidth() - 2;
	S32 y = 0;
	LLRect r;
	r.set(x - SIM_STAT_WIDTH, y + gMenuBarHeight - 1, x, y + 1);
	mSGBandwidth = new LLStatGraph("BandwidthGraph", r);
	mSGBandwidth->setFollows(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	mSGBandwidth->setStat(&gViewerStats.mKBitStat);
	std::string text = getString("bandwidth_tooltip") + " ";
	mSGBandwidth->setLabel(text);
	mSGBandwidth->setUnits("kbps", "Mbps");
	mSGBandwidth->setUnitDivisor(1024.f);
	mSGBandwidth->setPrecision(0);
	mSGBandwidth->setLogScale();
	// Logarithmic indicators thresholds are always expressed in percent of the
	// full range.
	mSGBandwidth->setThreshold(0, 0.85f);
	mSGBandwidth->setThreshold(1, 0.95f);
	mSGBandwidth->setThreshold(2, 0.98f);
	mSGBandwidth->setClickedCallback(onClickStatGraph);
	setNetworkBandwidth();
	addChild(mSGBandwidth);
	x -= SIM_STAT_WIDTH + 2;

	r.set(x - SIM_STAT_WIDTH, y + gMenuBarHeight - 1, x, y + 1);
	mSGPacketLoss = new LLStatGraph("PacketLossPercent", r);
	mSGPacketLoss->setFollows(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	mSGPacketLoss->setStat(&gViewerStats.mPacketsLostPercentStat);
	text = getString("packet_loss_tooltip") + " ";
	mSGPacketLoss->setLabel(text);
	mSGPacketLoss->setUnits("%");
	mSGPacketLoss->setMax(1.f);
	mSGPacketLoss->setThreshold(0, 0.1f);
	mSGPacketLoss->setThreshold(1, 0.25f);
	mSGPacketLoss->setThreshold(2, 0.5f);
	mSGPacketLoss->setPrecision(1);
	mSGPacketLoss->setPerSec(false);
	mSGPacketLoss->setClickedCallback(onClickStatGraph);
	addChild(mSGPacketLoss);
	x -= SIM_STAT_WIDTH + 2;

	mTextStat = getChild<LLTextBox>("stat_btn");
	mTextStat->setClickedCallback(onClickStatGraph);

	mRefreshAgentParcelTimer.setTimerExpirySec(PARCEL_TIMER_EXPIRY);

	setIcons();

	// The status bar can never get focused
	setFocusRoot(false);
	setMouseOpaque(false);
	setIsChrome(true);
}

LLStatusBar::~LLStatusBar()
{
	gStatusBarp = NULL;
}

void LLStatusBar::setIcons()
{
	mUseOldIcons = gSavedSettings.getBool("UseOldStatusBarIcons");
	std::string prefix = mUseOldIcons ? "legacy_status_" : "status_";
	mBtnNoFly->setImages(prefix + "no_fly.tga");
	mBtnNoBuild->setImages(prefix + "no_build.tga");
	mBtnNoScript->setImages(prefix + "no_scripts.tga");
	mBtnNoPush->setImages(prefix + "no_push.tga");
	mBtnNoVoice->setImages(prefix + "no_voice.tga");
	mBtnNoSee->setImages(prefix + "no_see.tga");
	mDirty = true;
}

void LLStatusBar::setNetworkBandwidth()
{
	// Change the indicator max scale
	mSGBandwidth->setMax(mNetworkDown ? 0.f : mAbsoluteMaxBandwidth);

	// Refresh the indicator tool tip suffix with the max encountered bandwidth
	// so far.
	static const char* max_kpbs = " / %dkbps";
	static const char* max_mbps = " / %dMbps";
	const char* fmt;
	S32 maxbw = mAbsoluteMaxBandwidth;
	if (maxbw >= 1024)
	{
		maxbw /= 1024;
		fmt = max_mbps;
	}
	else
	{
		fmt = max_kpbs;
	}
	mSGBandwidth->setLabelSuffix(llformat(fmt, maxbw));
}

//virtual
void LLStatusBar::draw()
{
	if (!gMenuBarViewp) return;

	if (mDirty ||
		mUpdateTimer.getElapsedTimeF32() >= STATUS_REFRESH_INTERVAL ||
		!mHealthTimer.hasExpired() || !mNotificationsTimer.hasExpired())
	{
		mDirty = false;
		refresh();
		mUpdateTimer.reset();
	}

	if (isBackgroundVisible())
	{
		gl_drop_shadow(0, getRect().getHeight(), getRect().getWidth(), 0,
					   LLUI::sColorDropShadow, LLUI::sDropShadowFloater);
	}
	LLPanel::draw();
}

// Update of visibility
void LLStatusBar::refresh()
{
	bool net_down_now = false;
	// Note: absolute maximum = 1Gbps
	F32 cur_bandwidth = llmin(mSGBandwidth->getStat()->getMeanPerSec(),
							  1000000.f);
	if (cur_bandwidth > 0.f)
	{
		mLastZeroBandwidthTime = 0.f;
	}
	else if (mLastZeroBandwidthTime == 0.f)
	{
		mLastZeroBandwidthTime = gFrameTimeSeconds;
	}
	else if (gFrameTimeSeconds - mLastZeroBandwidthTime >= NET_TIMEOUT)
	{
		net_down_now = true;
	}
	bool showing = true;
	bool update_bw_scale = mNetworkDown != net_down_now;
	if (net_down_now)
	{
		showing = ((S32)(gFrameTimeSeconds * FLASH_FREQUENCY) & 1) != 0;
	}
	else if (cur_bandwidth > mAbsoluteMaxBandwidth)
	{
		mAbsoluteMaxBandwidth = cur_bandwidth;
		update_bw_scale = true;
	}
	if (update_bw_scale)
	{
		mNetworkDown = net_down_now;
		setNetworkBandwidth();
	}
	mSGBandwidth->setVisible(mVisibility && showing);

	// Clock display, switchable between SLT, UTC or local time
	struct tm* internal_time;
	std::string time_string;
	std::string time_zone;
	if (mTimeMode == TIME_MODE_SL)
	{
		// Get current UTC time, adjusted for the user's clock being off.
		time_t utc_time = time_corrected();
		// Convert to Pacific, based on server's opinion of whether
		// It is daylight savings time there.
		internal_time = utc_to_pacific_time(utc_time, gPacificDaylightTime);
		time_zone = gPacificDaylightTime ? " PDT" : " PST";
	}
	else if (mTimeMode == TIME_MODE_UTC)
	{
		// Get current UTC time, adjusted for the user's clock being off.
		time_t utc_time = time_corrected();
		internal_time = utc_time_to_tm(utc_time);
		time_zone = " UTC";
	}
	else
	{
		time_t local_time = computer_time();
		internal_time = local_time_to_tm(local_time);
	}
	static LLCachedControl<std::string> short_date_format(gSavedSettings,
														  "ShortTimeFormat");
	timeStructToFormattedString(internal_time, short_date_format, time_string);
	mTextTime->setText(time_string + time_zone);

	static LLCachedControl<std::string> long_date_format(gSavedSettings,
														 "LongDateFormat");
	timeStructToFormattedString(internal_time, long_date_format, time_string);
	mTextTime->setToolTip(time_string);

	LLRect r;
	const S32 menu_right = gMenuBarViewp->getRightmostMenuEdge();
	S32 x = menu_right + MENU_PARCEL_SPACING;
	S32 y = 0;

	// Reshape menu bar to its content's width
	if (menu_right != gMenuBarViewp->getRect().getWidth())
	{
		gMenuBarViewp->reshape(menu_right,
							   gMenuBarViewp->getRect().getHeight());
	}

	LLViewerRegion* region = gAgent.getRegion();
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();

	LLRect brect;

	showing = gViewerParcelMgr.allowAgentDamage(region, parcel);
	mTextHealth->setVisible(showing);
	if (showing)
	{
		// Set visibility based on flashing
		if (!mHealthTimer.hasExpired())
		{
			showing = ((S32)(mHealthTimer.getElapsedTimeF32() *
							 FLASH_FREQUENCY) & 1) != 0;
		}

		// Health
		brect = mBtnHealth->getRect();
		r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
		mBtnHealth->setRect(r);
		x += brect.getWidth();

		static const S32 health_width =
			S32(LLFontGL::getFontSansSerifSmall()->getWidth("100%")) + 2;
		r.set(x, y + TEXT_HEIGHT - 2, x + health_width, y);
		mTextHealth->setRect(r);
		x += health_width;
	}
	mBtnHealth->setVisible(showing);

	showing = !gViewerParcelMgr.allowAgentFly(region, parcel);
	mBtnNoFly->setVisible(showing);
	if (showing)
	{
		brect = mBtnNoFly->getRect();
		r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
		mBtnNoFly->setRect(r);
		x += brect.getWidth();
	}

	showing = !gViewerParcelMgr.allowAgentBuild();
	mBtnNoBuild->setVisible(showing);
	if (showing)
	{
		brect = mBtnNoBuild->getRect();
		// No Build Zone
		r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
		mBtnNoBuild->setRect(r);
		x += brect.getWidth();
	}

	showing = !gViewerParcelMgr.allowAgentScripts(region, parcel);
	mBtnNoScript->setVisible(showing);
	if (showing)
	{
		brect = mBtnNoScript->getRect();
		r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
		mBtnNoScript->setRect(r);
		x += brect.getWidth();
	}

	showing = !gViewerParcelMgr.allowAgentPush(region, parcel);
	mBtnNoPush->setVisible(showing);
	if (showing)
	{
		brect = mBtnNoPush->getRect();
		r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
		mBtnNoPush->setRect(r);
		x += brect.getWidth();
	}

	showing = gIsInSecondLife ? !gViewerParcelMgr.allowAgentVoice()
							  : !parcel || !parcel->getParcelFlagAllowVoice();
	mBtnNoVoice->setVisible(showing);
	if (showing)
	{
		brect = mBtnNoVoice->getRect();
		r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
		mBtnNoVoice->setRect(r);
		x += brect.getWidth();
	}

	showing = parcel && parcel->getHaveNewParcelLimitData() &&
			  !parcel->getSeeAVs();
	mBtnNoSee->setVisible(showing);
	if (showing)
	{
		brect = mBtnNoSee->getRect();
		r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
		mBtnNoSee->setRect(r);
		x += brect.getWidth();
	}

	// *HACK: layout tweak
	if (!mUseOldIcons)
	{
		x += 6;
	}

	showing = false;
	bool navmesh_dirty = false;
	if (gOverlayBarp)
	{
		showing = gOverlayBarp->isNavmeshRebaking();
		navmesh_dirty = showing || gOverlayBarp->isNavmeshDirty();
	}
	mBtnDirtyNavMesh->setVisible(navmesh_dirty);
	if (navmesh_dirty)
	{
		mBtnNoPathFinding->setVisible(false);
		brect = mBtnDirtyNavMesh->getRect();
		r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
		mBtnDirtyNavMesh->setRect(r);
		x += brect.getWidth();
		mBtnDirtyNavMesh->setEnabled(!showing);
	}
	else
	{
		bool no_path_finding = region && region->hasDynamicPathfinding() &&
							   !region->dynamicPathfindingEnabled();
		mBtnNoPathFinding->setVisible(no_path_finding);
		if (no_path_finding)
		{
			mBtnDirtyNavMesh->setVisible(false);
			brect = mBtnNoPathFinding->getRect();
			r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
			mBtnNoPathFinding->setRect(r);
			x += brect.getWidth();
		}
	}

	showing = parcel && !parcel->isPublic() &&
			  gViewerParcelMgr.canAgentBuyParcel(parcel, false);
	mBtnBuyLand->setVisible(showing);
	if (showing)
	{
		// *HACK: layout tweak
		x += 9;
		brect = mBtnBuyLand->getRect();
		r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
		mBtnBuyLand->setRect(r);
		x += brect.getWidth();
	}

	// Hide all maturity buttons
	mBtnAdult->setVisible(false);
	mBtnMature->setVisible(false);
	mBtnPG->setVisible(false);

	std::string location_name, parcel_name;
	if (region)
	{
		// Show the right maturity button. Note: we do not display any icon if
		// the rating is unknown
		LLButton* maturity_btn = NULL;
		U8 sim_access = region->getSimAccess();
		if (sim_access == SIM_ACCESS_PG)
		{
			maturity_btn = mBtnPG;
		}
		else if (sim_access == SIM_ACCESS_MATURE)
		{
			maturity_btn = mBtnMature;
		}
		else if (sim_access == SIM_ACCESS_ADULT)
		{
			maturity_btn = mBtnAdult;
		}
		if (maturity_btn)
		{
			maturity_btn->setVisible(true);
			brect = maturity_btn->getRect();
			// *HACK: layout tweak
			x += 6;
			y = 1;
			r.setOriginAndSize(x, y, brect.getWidth(), brect.getHeight());
			maturity_btn->setRect(r);
			x += brect.getWidth();
		}

		const LLVector3& agent_pos_region = gAgent.getPositionAgent();
		S32 pos_x = lltrunc(agent_pos_region.mV[VX]);
		S32 pos_y = lltrunc(agent_pos_region.mV[VY]);
		S32 pos_z = lltrunc(agent_pos_region.mV[VZ]);

		location_name = region->getName();
		if (parcel)
		{
			parcel_name = parcel->getName();
			location_name += llformat(" %d, %d, %d - %s", pos_x, pos_y, pos_z,
									  parcel_name.c_str());

			if (mRefreshAgentParcelTimer.hasExpired())
			{
				mRefreshAgentParcelTimer.reset();
				mRefreshAgentParcelTimer.setTimerExpirySec(PARCEL_TIMER_EXPIRY);
				gViewerParcelMgr.requestParcelProperties(gAgent.getPositionGlobal());
			}
		}
		else
		{
			parcel_name = "Unknown";
			location_name += llformat(" %d, %d, %d", pos_x, pos_y, pos_z);
		}
	}
	else
	{
		// No region
		parcel_name = "Unknown";
		location_name = "(Unknown)";
	}
//MK
	gRLInterface.mParcelName = parcel_name;
	if (gRLenabled && region && gRLInterface.mContainsShowloc)
	{
		location_name = "(Hidden)";
	}
//mk
	mTextParcelName->setText(location_name);
	static U32 last_event_poll_failures = 0;
	if (mAgentRegionFailedEventPolls != last_event_poll_failures)
	{
		last_event_poll_failures = mAgentRegionFailedEventPolls;
		if (mAgentRegionFailedEventPolls == 0)
		{
			mTextParcelName->setColor(mParcelTextColor);
			mTextParcelName->setToolTip(getString("parcel_tool_tip"));
		}
		// NOTE: keep failures numbers coherent with MAX_EVENT_POLL_HTTP_ERRORS
		// * 2 in lleventpoll.cpp.
		else if (mAgentRegionFailedEventPolls >= 15)
		{
			mTextParcelName->setColor(LLColor4::red);
			mTextParcelName->setToolTip(getString("parcel_tool_tip_red"));
		}
		else if (mAgentRegionFailedEventPolls >= 10)
		{
			mTextParcelName->setColor(LLColor4::orange);
			mTextParcelName->setToolTip(getString("parcel_tool_tip_orange"));
		}
		else
		{
			mTextParcelName->setColor(LLColor4::yellow);
			mTextParcelName->setToolTip(getString("parcel_tool_tip_yellow"));
		}
	}

	S32 new_right = getRect().getWidth();

	// Stats graph pseudo-button (textbox) rect
	r = mTextStat->getRect();
	r.translate(new_right - r.mRight, 0);
	mTextStat->setRect(r);
	new_right -= r.getWidth() + 15;
	mTextStat->setEnabled(true);

	// FPS rect, text and color
	r = mTextFPS->getRect();
	r.translate(new_right - r.mRight, 0);
	mTextFPS->setRect(r);
	new_right -= r.getWidth() + 6;
	F32 fps = gViewerStats.mFPSStat.getMeanPerSec();
	if (gAutomationp)
	{
		gAutomationp->onAveragedFPS(fps, mFrameRateLimited,
									gViewerStats.getRenderTimeStat());
	}
	mTextFPS->setText(llformat("%d", (S32)(fps + 0.5f)));
	static LLColor4 fps_color;
	if (mFrameRateLimited)
	{
		fps_color.set(1.f, 1.f, 1.f, 1.f);
	}
	// The following clamping and parameters have been determined empirically
	// to get the right color range, from red below 10 fps to blue at 60fps and
	// above, going through orange, yellow and green (around 35fps) in between.
	// HB
	else
	{
		if (fps > 60.f)
		{
			fps = 60.f;
		}
		else if (fps < 5.f)
		{
			fps = 5.f;
		}
		constexpr F32 HUE_FACTOR = 0.01f;
		constexpr F32 HUE_OFFSET = 0.94f;
		constexpr F32 SATURATION = 0.9f;
		constexpr F32 LUMINANCE = 0.6f;
		fps_color.setHSL(fps * HUE_FACTOR + HUE_OFFSET, SATURATION, LUMINANCE);
	}
	mTextFPS->setColor(fps_color);

	// Money balance rect
	static LLCachedControl<bool> show_balance(gSavedSettings, "ShowBalance");
	static LLCachedControl<bool> show_buy(gSavedSettings, "ShowBuyCurrency");
	mTextBalance->setVisible(mVisibility && show_balance);
	mBtnBuyMoney->setVisible(mVisibility && !show_balance && show_buy);
	if (show_balance)
	{
		r = mTextBalance->getRect();
		r.translate(new_right - r.mRight, 0);
		mTextBalance->setRect(r);
		new_right -= r.getWidth() + 6;
	}
	else if (show_buy)
	{
		r = mBtnBuyMoney->getRect();
		r.translate(new_right - r.mRight, 0);
		mBtnBuyMoney->setRect(r);
		new_right -= r.getWidth() + 6;
	}

	// Time rect
	r = mTextTime->getRect();
	r.translate(new_right - r.mRight, 0);
	mTextTime->setRect(r);
	new_right -= r.getWidth() + 6;

	static LLCachedControl<bool> show_search_bar(gSavedSettings,
												 "ShowSearchBar");
	showing = mVisibility && show_search_bar;
	if (showing)
	{
		r = mBtnSearchBevel->getRect();
		r.translate(new_right - r.mRight, 0);
		mBtnSearchBevel->setRect(r);

		r = mBtnSearch->getRect();
		r.translate(new_right - r.mRight, 0);
		mBtnSearch->setRect(r);
		new_right -= r.getWidth();

		r = mLineEditSearch->getRect();
		r.translate(new_right - r.mRight, 0);
		mLineEditSearch->setRect(r);
		new_right -= r.getWidth();
	}
	mLineEditSearch->setVisible(showing);
	mBtnSearch->setVisible(showing);
	mBtnSearchBevel->setVisible(showing);

	// Rebaking/complexity icons and text
	r = mTextTooComplex->getRect();
	r.translate(new_right - r.mRight, 0);
	mTextTooComplex->setRect(r);
	new_right -= r.getWidth() + 6;

	showing = gAppearanceMgr.isRebaking();
	U32 too_complex = gViewerStats.getTooComplexReports();
	if (showing || too_complex == 0)
	{
		mTextTooComplex->setVisible(false);
		mTooComplex->setVisible(false);

		r = mBtnRebaking->getRect();
		r.translate(new_right - r.mRight, 0);
		mBtnRebaking->setRect(r);
	}
	else
	{
		mTextTooComplex->setVisible(true);
		mTooComplex->setVisible(true);
		mTextTooComplex->setText(llformat("%d", too_complex));

		r = mTooComplex->getRect();
		r.translate(new_right - r.mRight, 0);
		mTooComplex->setRect(r);
	}
	new_right -= r.getWidth() + 6;
	mBtnRebaking->setVisible(showing);

	// Script error icon
	r = mBtnScriptError->getRect();
	r.translate(new_right - r.mRight, 0);
	mBtnScriptError->setRect(r);
	new_right -= r.getWidth() + 6;
	mBtnScriptError->setVisible(LLFloaterScriptDebug::hasRecentError());

	// Lua function icon
	r = mBtnLuaFunction->getRect();
	r.translate(new_right - r.mRight, 0);
	mBtnLuaFunction->setRect(r);
	new_right -= r.getWidth() + 6;
	mBtnLuaFunction->setVisible(mVisibility && !mLuaCommand.empty());

	S32 left = mBtnLuaFunction->getRect().mLeft;

	S32 notifications = LLNotifyBox::getNotifyBoxCount() +
						LLGroupNotifyBox::getGroupNotifyBoxCount();
	if (notifications > 0)
	{
		showing = LLNotifyBox::areNotificationsShown();
		if (showing)
		{
			// Do not flash the indicator when notifications are shown
			mNotificationsTimer.reset();
		}
		else if (mLastNotifications < notifications)
		{
			// Flash the indicator when notifications are added
			mNotificationsTimer.reset();
			mNotificationsTimer.setTimerExpirySec(FLASH_TIMER_EXPIRY);
		}

		bool visible = true;
		if (!showing && !mNotificationsTimer.hasExpired())
		{
			// Set visibility based on flashing
			visible = ((S32)(mNotificationsTimer.getElapsedTimeF32() *
							 FLASH_FREQUENCY) & 1) != 0;
		}

		mBtnNotificationsOn->setVisible(showing);
		mBtnNotificationsOff->setVisible(!showing && visible);

		mTextNotifications->setText(llformat("%d", notifications));
		mTextNotifications->setVisible(true);

		r = mTextNotifications->getRect();
		r.translate(new_right - r.mRight, 0);
		mTextNotifications->setRect(r);
		new_right -= r.getWidth() + 6;

		r = mBtnNotificationsOn->getRect();
		r.translate(new_right - r.mRight, 0);
		mBtnNotificationsOn->setRect(r);
		mBtnNotificationsOff->setRect(r);
		//new_right -= r.getWidth() + 6;

		left = mBtnNotificationsOn->getRect().mLeft;
	}
	else
	{
		mBtnNotificationsOn->setVisible(false);
		mBtnNotificationsOff->setVisible(false);
		mTextNotifications->setVisible(false);
	}
	mLastNotifications = notifications;

	// Adjust region name and parcel name
	x += 8;

	const S32 parcel_right =
		llmin(left - 10, mTextParcelName->getTextPixelWidth() + x + 5);
	r.set(x + 4, getRect().getHeight() - 1, parcel_right, 0);
	mTextParcelName->setRect(r);
}

void LLStatusBar::setVisibleForMouselook(bool visible)
{
	mVisibility = visible;
	static LLCachedControl<bool> show_balance(gSavedSettings, "ShowBalance");
	static LLCachedControl<bool> show_buy(gSavedSettings, "ShowBuyCurrency");
	mTextBalance->setVisible(visible && show_balance);
	mBtnBuyMoney->setVisible(visible && !show_balance && show_buy);
	mTextTime->setVisible(visible);
	mLineEditSearch->setVisible(visible);
	mBtnSearch->setVisible(visible);
	mBtnSearchBevel->setVisible(visible);
	mSGBandwidth->setVisible(visible);
	mSGPacketLoss->setVisible(visible);
	mBtnLuaFunction->setVisible(visible);
	setBackgroundVisible(visible);
	mDirty = true;
}

void LLStatusBar::setDirtyAgentParcelProperties()
{
	mRefreshAgentParcelTimer.reset();
	// We delay the refresh by two seconds, so to let the time for the parcel
	// properties to be updated (the call to this method being done by the
	// LLViewerParcelMgr::sendParcelPropertiesUpdate() method) and also as a
	// requests rate throttling.
	mRefreshAgentParcelTimer.setTimerExpirySec(2.f);
}

void LLStatusBar::debitBalance(S32 debit)
{
	setBalance(getBalance() - debit);
}

void LLStatusBar::creditBalance(S32 credit)
{
	setBalance(getBalance() + credit);
}

void LLStatusBar::setBalance(S32 balance)
{
	std::string balance_str = "L$" + LLLocale::getMonetaryString(balance);
	mTextBalance->setText(balance_str);
	static const std::string tootlip = getString("balance_tool_tip");
	mBtnBuyMoney->setToolTip(tootlip + " " + balance_str);

	F32 threshold = gSavedSettings.getF32("UISndMoneyChangeThreshold");
	S32 change = mBalance - balance;
	if (mBalance && change && fabs((F32)change) >= threshold)
	{
		if (mBalance > balance)
		{
			make_ui_sound("UISndMoneyChangeDown");
		}
		else
		{
			make_ui_sound("UISndMoneyChangeUp");
		}
	}

	mBalance = balance;
	mDirty = true;
}

//static
void LLStatusBar::sendMoneyBalanceRequest()
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_MoneyBalanceRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_MoneyData);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null);
	gAgent.sendReliableMessage(2);
}

void LLStatusBar::setHealth(S32 health)
{
	mTextHealth->setText(llformat("%d%%", health));

	if (mHealth > health)
	{
		if (mHealth > health + gSavedSettings.getF32("UISndHealthReductionThreshold"))
		{
			bool male_ok = gSavedSettings.getBool("UISndHealthReductionMEnable");
			bool female_ok = gSavedSettings.getBool("UISndHealthReductionFEnable");
			if (male_ok && female_ok)
			{
				male_ok &= isAgentAvatarValid() &&
						   gAgentAvatarp->getSex() == SEX_MALE;
				female_ok &= !male_ok;
			}
			if (male_ok)
			{
				make_ui_sound("UISndHealthReductionM");
			}
			else if (female_ok)
			{
				make_ui_sound("UISndHealthReductionF");
			}
		}

		mHealthTimer.reset();
		mHealthTimer.setTimerExpirySec(FLASH_TIMER_EXPIRY);
	}

	mHealth = health;
	mDirty = true;
}

//static
void LLStatusBar::onClickParcelInfo(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gViewerParcelMgr.selectParcelAt(gAgent.getPositionGlobal());
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		return;
	}
//mk
	LLFloaterLand::showInstance();
}

//static
void LLStatusBar::onClickTime(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		if (++self->mTimeMode >= TIME_MODE_END)
		{
			self->mTimeMode = 0;
		}
		gSavedSettings.setU32("StatusBarTimeMode", self->mTimeMode);
		self->mDirty = true;
	}
}

//static
void LLStatusBar::onClickBalance(void*)
{
	LLFloaterBuyCurrency::buyCurrency();
	sendMoneyBalanceRequest();
}

//static
void LLStatusBar::onClickHealth(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("NotSafe");
}

//static
void LLStatusBar::onClickScriptDebug(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	LLFloaterScriptDebug::show(LLUUID::null);
}

//static
void LLStatusBar::onClickFly(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("NoFly");
}

//static
void LLStatusBar::onClickPush(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("PushRestricted");
}

//static
void LLStatusBar::onClickVoice(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("NoVoice");
}

//static
void LLStatusBar::onClickSee(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("NoSee");
}

//static
void LLStatusBar::onClickBuild(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("NoBuild");
}

//static
void LLStatusBar::onClickPathFinding(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("NoPathFinding");
}

//static
void LLStatusBar::onClickDirtyNavMesh(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("DirtyNavMesh");
}

//static
void LLStatusBar::onClickAdult(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("AdultRating");
}

//static
void LLStatusBar::onClickMature(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("MatureRating");
}

//static
void LLStatusBar::onClickPG(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gNotifications.add("PGRating");
}

//static
void LLStatusBar::onClickNotifications(void*)
{
	LLNotifyBox::setShowNotifications(!LLNotifyBox::areNotificationsShown());
}

//static
void LLStatusBar::onClickTooComplex(void*)
{
	if (!isAgentAvatarValid()) return;

	LLSD args;
	args["REPORTERS"] = llformat("%d", gViewerStats.getComplexityReports());
	args["JELLYDOLLS"] = llformat("%d", gViewerStats.getTooComplexReports());
	args["COMPLEXITY"] = llformat("%d", gAgentAvatarp->getVisualComplexity());
	args["AREA"] = llformat("%.1f", gAgentAvatarp->getAttachmentSurfaceArea());
	args["GEOMETRY"] = llformat("%d",
								gAgentAvatarp->getAttachmentSurfaceBytes() /
								1024);
	U32 attachments = gAgentAvatarp->getNumAttachments();
	args["ATTACHMENTS"] = llformat("%d", attachments);
	args["[SLOTS]"] = llformat("%d", gMaxSelfAttachments - attachments);
	gNotifications.add("AvatarComplexityReport", args);
}

//static
void LLStatusBar::onClickScripts(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}

	LLViewerRegion* region = gAgent.getRegion();
	if (region && region->getRegionFlag(REGION_FLAGS_ESTATE_SKIP_SCRIPTS))
	{
		gNotifications.add("ScriptsStopped");
	}
	else if (region && region->getRegionFlag(REGION_FLAGS_SKIP_SCRIPTS))
	{
		gNotifications.add("ScriptsNotRunning");
	}
	else
	{
		gNotifications.add("NoOutsideScripts");
	}
}

//static
void LLStatusBar::onClickBuyLand(void* data)
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		return;
	}
//mk
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		self->mRefreshAgentParcelTimer.reset();
	}
	gViewerParcelMgr.selectParcelAt(gAgent.getPositionGlobal());
	gViewerParcelMgr.startBuyLand();
}

//static
void LLStatusBar::onCommitSearch(LLUICtrl*, void* data)
{
	// Committing is the same as clicking "search"
	onClickSearch(data);
}

//static
void LLStatusBar::onClickSearch(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self)
	{
		std::string search_text = self->mLineEditSearch->getText();
		HBFloaterSearch::showFindAll(search_text);
	}
}

//static
void LLStatusBar::onClickFPS(void*)
{
	LLFloaterStats::showInstance();
}

//static
void LLStatusBar::onClickStatGraph(void*)
{
	LLFloaterLagMeter::showInstance();
}

//static
void LLStatusBar::setLuaFunctionButton(const std::string& command,
									   const std::string& tooltip)
{
	mLuaCommand = command;
	mBtnLuaFunction->setToolTip(tooltip);
	mDirty = true;
}

//static
void LLStatusBar::onClickLuaFunction(void* data)
{
	LLStatusBar* self = (LLStatusBar*)data;
	if (self && !self->mLuaCommand.empty())
	{
		HBViewerAutomation::eval(self->mLuaCommand);
	}
}
