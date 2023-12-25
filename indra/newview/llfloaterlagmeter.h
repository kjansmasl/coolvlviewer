/**
 * @file llfloaterlagmeter.h
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

#ifndef LLFLOATERLAGMETER_H
#define LLFLOATERLAGMETER_H

#include "llfloater.h"
#include "llstring.h"

class LLFrameTimer;
class LLViewerStats;

class LLFloaterLagMeter final : public LLFloater,
							    public LLFloaterSingleton<LLFloaterLagMeter>
{
	friend class LLUISingleton<LLFloaterLagMeter,
							   VisibilityPolicy<LLFloater> >;

public:
	void draw() override;
	void refresh() override;
	void onClose(bool app_quitting) override;

private:
	LLFloaterLagMeter(const LLSD& key);

	void determineClient();
	void determineNetwork();
	void determineServer();

	void expand();
	void shrink();

	static void onClickShrink(void* data);

private:
	LLButton*					mMinimizeButton;
	LLButton*					mHelpButton;
	LLButton*					mClientButton;
	LLButton*					mNetworkButton;
	LLButton*					mServerButton;
	LLTextBox*					mClientLabel;
	LLTextBox*					mClientText;
	LLTextBox*					mClientCause;
	LLTextBox*					mNetworkLabel;
	LLTextBox*					mNetworkText;
	LLTextBox*					mNetworkCause;
	LLTextBox*					mServerLabel;
	LLTextBox*					mServerText;
	LLTextBox*					mServerCause;

	LLStringUtil::format_map_t	mStringArgs;

	LLFrameTimer				mUpdateTimer;

	S32							mMaxWidth;
	S32							mMinWidth;

	F32							mClientFrameTimeCritical;
	F32							mClientFrameTimeWarning;

	F32							mNetworkPacketLossCritical;
	F32							mNetworkPacketLossWarning;
	F32							mNetworkPingCritical;
	F32							mNetworkPingWarning;

	F32							mServerFrameTimeCritical;
	F32							mServerFrameTimeWarning;
	F32							mServerSingleProcessMaxTime;

	bool						mShrunk;
};

#endif
