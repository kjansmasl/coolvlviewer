/**
 * @file llfloaterlagmeter.cpp
 * @brief The "Lag-o-Meter" floater used to tell users what is causing lag.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llfloaterlagmeter.h"

#include "llbutton.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llappviewer.h"
#include "lltexturefetch.h"
#include "llviewercontrol.h"
#include "llviewerstats.h"
#include "llviewertexture.h"

// Do not refresh more than twice a second
constexpr F32 REFRESH_INTERVAL = 0.5f;

const std::string LAG_CRITICAL_IMAGE_NAME = "lag_status_critical.tga";
const std::string LAG_WARNING_IMAGE_NAME  = "lag_status_warning.tga";
const std::string LAG_GOOD_IMAGE_NAME     = "lag_status_good.tga";

LLFloaterLagMeter::LLFloaterLagMeter(const LLSD& key)
:	LLFloater("lag meter"),
	mShrunk(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_lagmeter.xml");

	// Don't let this window take keyboard focus -- it's confusing to
	// lose arrow-key driving when testing lag.
	setIsChrome(true);

	mMinimizeButton = getChild<LLButton>("minimize");
	mMinimizeButton->setClickedCallback(onClickShrink, this);

	mHelpButton = getChild<LLButton>("server_help");

	mClientButton = getChild<LLButton>("client_lagmeter");
	mClientLabel = getChild<LLTextBox>("client");
	mClientText = getChild<LLTextBox>("client_text");
	mClientCause = getChild<LLTextBox>("client_lag_cause");

	mNetworkButton = getChild<LLButton>("network_lagmeter");
	mNetworkLabel = getChild<LLTextBox>("network");
	mNetworkText = getChild<LLTextBox>("network_text");
	mNetworkCause = getChild<LLTextBox>("network_lag_cause");

	mServerButton = getChild<LLButton>("server_lagmeter");
	mServerLabel = getChild<LLTextBox>("server");
	mServerText = getChild<LLTextBox>("server_text");
	mServerCause = getChild<LLTextBox>("server_lag_cause");

	std::string config_string = getString("client_frame_rate_critical_fps",
										   mStringArgs);
	mClientFrameTimeCritical = 1.f / (F32)atof(config_string.c_str());
	config_string = getString("client_frame_rate_warning_fps", mStringArgs);
	mClientFrameTimeWarning = 1.f / (F32)atof(config_string.c_str());

	config_string = getString("network_packet_loss_critical_pct", mStringArgs);
	mNetworkPacketLossCritical = (F32)atof(config_string.c_str());
	config_string = getString("network_packet_loss_warning_pct", mStringArgs);
	mNetworkPacketLossWarning = (F32)atof(config_string.c_str());

	config_string = getString("network_ping_critical_ms", mStringArgs);
	mNetworkPingCritical = (F32)atof(config_string.c_str());
	config_string = getString("network_ping_warning_ms", mStringArgs);
	mNetworkPingWarning = (F32)atof(config_string.c_str());
	config_string = getString("server_frame_rate_critical_fps", mStringArgs);

	mServerFrameTimeCritical = 1000.f / (F32)atof(config_string.c_str());
	config_string = getString("server_frame_rate_warning_fps", mStringArgs);
	mServerFrameTimeWarning = 1000.f / (F32)atof(config_string.c_str());
	config_string = getString("server_single_process_max_time_ms",
							  mStringArgs);
	mServerSingleProcessMaxTime = (F32)atof(config_string.c_str());

	config_string = getString("max_width_px", mStringArgs);
	mMaxWidth = atoi(config_string.c_str());
	config_string = getString("min_width_px", mStringArgs);
	mMinWidth = atoi(config_string.c_str());

	mStringArgs["CLIENT_FRAME_RATE_CRITICAL"] =
		getString("client_frame_rate_critical_fps");
	mStringArgs["CLIENT_FRAME_RATE_WARNING"] =
		getString("client_frame_rate_warning_fps");

	mStringArgs["NETWORK_PACKET_LOSS_CRITICAL"] =
		getString("network_packet_loss_critical_pct");
	mStringArgs["NETWORK_PACKET_LOSS_WARNING"] =
		getString("network_packet_loss_warning_pct");

	mStringArgs["NETWORK_PING_CRITICAL"] =
		getString("network_ping_critical_ms");
	mStringArgs["NETWORK_PING_WARNING"] =
		getString("network_ping_warning_ms");

	mStringArgs["SERVER_FRAME_RATE_CRITICAL"] =
		getString("server_frame_rate_critical_fps");
	mStringArgs["SERVER_FRAME_RATE_WARNING"] =
		getString("server_frame_rate_warning_fps");

	// Were we shrunk last time ?
	if (gSavedSettings.getBool("LagMeterShrunk"))
	{
		shrink();
	}
}

void LLFloaterLagMeter::onClose(bool app_quitting)
{
	// Save the shrunk status for next time
	gSavedSettings.setBool("LagMeterShrunk", mShrunk);
	// Expand so that we save the proper window rectangle
	if (mShrunk)
	{
		expand();
	}
	LLFloater::onClose(app_quitting);
}

void LLFloaterLagMeter::draw()
{
	if (mUpdateTimer.getElapsedTimeF32() >= REFRESH_INTERVAL)
	{
		refresh();
		mUpdateTimer.reset();
	}
	LLFloater::draw();
}

void LLFloaterLagMeter::refresh()
{
	determineClient();
	determineNetwork();
	determineServer();
}

void LLFloaterLagMeter::determineClient()
{
	F32 client_frame_time = gViewerStats.mFPSStat.getMeanDuration();

	if (!gFocusMgr.getAppHasFocus())
	{
		mClientButton->setImageUnselected(LAG_GOOD_IMAGE_NAME);
		mClientText->setText(getString("client_frame_time_window_bg_msg",
							 mStringArgs));
		mClientCause->setText(LLStringUtil::null);
		return;
	}
	else if (client_frame_time >= mClientFrameTimeCritical)
	{
		mClientButton->setImageUnselected(LAG_CRITICAL_IMAGE_NAME);
		mClientText->setText(getString("client_frame_time_critical_msg",
							 mStringArgs));
	}
	else if (client_frame_time >= mClientFrameTimeWarning)
	{
		mClientButton->setImageUnselected(LAG_WARNING_IMAGE_NAME);
		mClientText->setText(getString("client_frame_time_warning_msg",
							 mStringArgs));
	}
	else
	{
		mClientButton->setImageUnselected(LAG_GOOD_IMAGE_NAME);
		mClientText->setText(getString("client_frame_time_normal_msg",
							 mStringArgs));
		mClientCause->setText(LLStringUtil::null);
		return;
	}

	static LLCachedControl<F32> draw_distance(gSavedSettings, "RenderFarClip");
	if (draw_distance > 256)
	{
		mClientCause->setText(getString("client_draw_distance_cause_msg",
							  mStringArgs));
	}
	else if (gTextureFetchp->getApproxNumRequests() > 16)
	{
		mClientCause->setText(getString("client_texture_loading_cause_msg",
							  mStringArgs));
	}
	else if (LLViewerTexture::sBoundTexMemoryMB >
				LLViewerTexture::sMaxBoundTexMemMB)
	{
		mClientCause->setText(getString("client_texture_memory_cause_msg",
							  mStringArgs));
	}
	else
	{
		mClientCause->setText(getString("client_complex_objects_cause_msg",
							  mStringArgs));
	}
}

void LLFloaterLagMeter::determineNetwork()
{
	F32 packet_loss = gViewerStats.mPacketsLostPercentStat.getMean();
	F32 ping_time = gViewerStats.mSimPingStat.getMean();
	bool find_cause_loss = false;
	bool find_cause_ping = false;

	// *FIXME: We cannot blame a large ping time on anything in particular if
	// the frame rate is low, because a low frame rate is a sure recipe for
	// crappy ping times right now until the network handlers are de-synched
	// from the rendering.
	F32 frame_time_ms = 1000.f * gViewerStats.mFPSStat.getMeanDuration();

	if (packet_loss >= mNetworkPacketLossCritical)
	{
		mNetworkButton->setImageUnselected(LAG_CRITICAL_IMAGE_NAME);
		mNetworkText->setText(getString("network_packet_loss_critical_msg",
										mStringArgs));
		find_cause_loss = true;
	}
	else if (ping_time >= mNetworkPingCritical)
	{
		mNetworkButton->setImageUnselected(LAG_CRITICAL_IMAGE_NAME);
		if (frame_time_ms < mNetworkPingCritical)
		{
			mNetworkText->setText(getString("network_ping_critical_msg",
											mStringArgs));
			find_cause_ping = true;
		}
	}
	else if (packet_loss >= mNetworkPacketLossWarning)
	{
		mNetworkButton->setImageUnselected(LAG_WARNING_IMAGE_NAME);
		mNetworkText->setText(getString("network_packet_loss_warning_msg",
										mStringArgs));
		find_cause_loss = true;
	}
	else if (ping_time >= mNetworkPingWarning)
	{
		mNetworkButton->setImageUnselected(LAG_WARNING_IMAGE_NAME);
		if (frame_time_ms < mNetworkPingWarning)
		{
			mNetworkText->setText(getString("network_ping_warning_msg",
											mStringArgs));
			find_cause_ping = true;
		}
	}
	else
	{
		mNetworkButton->setImageUnselected(LAG_GOOD_IMAGE_NAME);
		mNetworkText->setText(getString("network_performance_normal_msg",
										mStringArgs));
	}

	if (find_cause_loss)
 	{
		mNetworkCause->setText(getString("network_packet_loss_cause_msg",
										 mStringArgs));
 	}
	else if (find_cause_ping)
	{
		mNetworkCause->setText(getString("network_ping_cause_msg",
										 mStringArgs));
	}
	else
	{
		mNetworkCause->setText(LLStringUtil::null);
	}
}

void LLFloaterLagMeter::determineServer()
{
	F32 sim_frame_time = gViewerStats.mSimFrameMsec.getCurrent();
	if (sim_frame_time >= mServerFrameTimeCritical)
	{
		mServerButton->setImageUnselected(LAG_CRITICAL_IMAGE_NAME);
		mServerText->setText(getString("server_frame_time_critical_msg",
									   mStringArgs));
	}
	else if (sim_frame_time >= mServerFrameTimeWarning)
	{
		mServerButton->setImageUnselected(LAG_WARNING_IMAGE_NAME);
		mServerText->setText(getString("server_frame_time_warning_msg",
									   mStringArgs));
	}
	else
	{
		mServerButton->setImageUnselected(LAG_GOOD_IMAGE_NAME);
		mServerText->setText(getString("server_frame_time_normal_msg",
									   mStringArgs));
		mServerCause->setText(LLStringUtil::null);
		return;
	}

	if (gViewerStats.mSimSimPhysicsMsec.getCurrent() > mServerSingleProcessMaxTime)
	{
		mServerCause->setText(getString("server_physics_cause_msg",
							  mStringArgs));
		return;
	}

	if (gViewerStats.mSimScriptMsec.getCurrent() > mServerSingleProcessMaxTime)
	{
		mServerCause->setText(getString("server_scripts_cause_msg",
							  mStringArgs));
		return;
	}

	if (gViewerStats.mSimNetMsec.getCurrent() > mServerSingleProcessMaxTime)
	{
		mServerCause->setText(getString("server_net_cause_msg",
							  mStringArgs));
		return;
	}

	if (gViewerStats.mSimAgentMsec.getCurrent() > mServerSingleProcessMaxTime)
	{
		mServerCause->setText(getString("server_agent_cause_msg",
							  mStringArgs));
		return;
	}

	if (gViewerStats.mSimImagesMsec.getCurrent() > mServerSingleProcessMaxTime)
	{
		mServerCause->setText(getString("server_images_cause_msg",
							  mStringArgs));
		return;
	}

	mServerCause->setText(getString("server_generic_cause_msg", mStringArgs));
}

void LLFloaterLagMeter::expand()
{
	setTitle(getString("max_title_msg", mStringArgs));

	// Make left edge appear to expand
	S32 delta_width = mMaxWidth - mMinWidth;
	LLRect r = getRect();
	r.translate(-delta_width, 0);
	setRect(r);
	reshape(mMaxWidth, getRect().getHeight());

	std::string text = getString("client_text_msg", mStringArgs) + ":";
	mClientLabel->setText(text);
	text = getString("network_text_msg", mStringArgs) + ":";
	mNetworkLabel->setText(text);
	text = getString("server_text_msg", mStringArgs) + ":";
	mServerLabel->setValue(text);

	// Usually "<<"
	mMinimizeButton->setLabel(getString("smaller_label", mStringArgs));

	mMinimizeButton->setFocus(false);

	mClientText->setVisible(true);
	mClientCause->setVisible(true);

	mNetworkText->setVisible(true);
	mNetworkCause->setVisible(true);

	mServerText->setVisible(true);
	mServerCause->setVisible(true);

	mHelpButton->setVisible(true);

	mShrunk = false;
}

void LLFloaterLagMeter::shrink()
{
	setTitle(getString("min_title_msg", mStringArgs));

	// make left edge appear to collapse
	S32 delta_width = mMaxWidth - mMinWidth;
	LLRect r = getRect();
	r.translate(delta_width, 0);
	setRect(r);
	reshape(mMinWidth, getRect().getHeight());

	mClientLabel->setValue(LLSD(getString("client_text_msg", mStringArgs)));
	mNetworkLabel->setValue(LLSD(getString("network_text_msg", mStringArgs)));
	mServerLabel->setValue(LLSD(getString("server_text_msg", mStringArgs)));

	// usually ">>"
	mMinimizeButton->setLabel(getString("bigger_label", mStringArgs));

	mMinimizeButton->setFocus(false);

	mClientText->setVisible(false);
	mClientCause->setVisible(false);

	mNetworkText->setVisible(false);
	mNetworkCause->setVisible(false);

	mServerText->setVisible(false);
	mServerCause->setVisible(false);

	mHelpButton->setVisible(false);

	mShrunk = true;
}

//static
void LLFloaterLagMeter::onClickShrink(void* data)
{
	LLFloaterLagMeter* self = (LLFloaterLagMeter*)data;
	if (!self) return;

	if (self->mShrunk)
	{
		self->expand();
	}
	else
	{
		self->shrink();
	}
}
