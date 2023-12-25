/** 
 * @file llstatusbar.h
 * @brief LLStatusBar class definition
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

#ifndef LL_LLSTATUSBAR_H
#define LL_LLSTATUSBAR_H

#include "llframetimer.h"
#include "llpanel.h"
#include "llstatgraph.h"

// "Constants" loaded from settings.xml at start time
extern S32 gStatusBarHeight;

class LLButton;
class LLLineEditor;
class LLStatGraph;
class LLTextBox;
class LLUICtrl;
class LLUUID;

class LLStatusBar final : public LLPanel
{
public:
	LLStatusBar(const LLRect& rect);
	~LLStatusBar() override;
	
	void draw() override;
	void refresh() override;

	LL_INLINE void setDirty()						{ mDirty = true; }

	void setIcons();

	// MANIPULATORS
	void setBalance(S32 balance);
	void debitBalance(S32 debit);
	void creditBalance(S32 credit);

	// Request the latest currency balance from the server
	static void sendMoneyBalanceRequest();

	void setHealth(S32 percent);

	LL_INLINE void setLandCredit(S32 credit)		{ mSquareMetersCredit = credit; }
	LL_INLINE void setLandCommitted(S32 committed)	{ mSquareMetersCommitted = committed; }

	// Causes an agent parcel properties request to be launched and the various
	// status bar icons (no-build, no-scripts, no-fly, no-voice, etc) to be
	// updated accordingly after the reply is received by the parcel manager.
	void setDirtyAgentParcelProperties();

	// some elements should hide in mouselook
	void setVisibleForMouselook(bool visible);

	LL_INLINE S32 getBalance() const				{ return mBalance; }
	LL_INLINE S32 getHealth() const					{ return mHealth; }

	LL_INLINE bool isUserTiered() const				{ return mSquareMetersCredit > 0; }
	LL_INLINE S32 getSquareMetersCredit() const		{ return mSquareMetersCredit; }
	LL_INLINE S32 getSquareMetersCommitted() const	{ return mSquareMetersCommitted; }
	LL_INLINE S32 getSquareMetersLeft() const		{ return mSquareMetersCredit -
															 mSquareMetersCommitted; }

	LL_INLINE void incFailedEventPolls()			{ ++mAgentRegionFailedEventPolls; }
	LL_INLINE void resetFailedEventPolls()			{ mAgentRegionFailedEventPolls = 0; }

	LL_INLINE void setFrameRateLimited(bool b)		{ mFrameRateLimited = b; }

	void setLuaFunctionButton(const std::string& command,
							  const std::string& tooltip);

private:
	enum
	{
		TIME_MODE_SL,
		TIME_MODE_UTC,
		TIME_MODE_LOCAL,
		TIME_MODE_END
	};

	void setNetworkBandwidth();

	static void onClickParcelInfo(void* data);
	static void onClickTime(void* data);
	static void onClickBalance(void*);
	static void onClickHealth(void* data);
	static void onClickFly(void* data);
	static void onClickPush(void* data);
	static void onClickBuild(void* data);
	static void onClickScripts(void* data);
	static void onClickPathFinding(void* data);
	static void onClickDirtyNavMesh(void* data);
	static void onClickAdult(void* data);
	static void onClickMature(void* data);
	static void onClickPG(void* data);
	static void onClickNotifications(void* data);
	static void onClickTooComplex(void* data);
	static void onClickVoice(void* data);
	static void onClickSee(void* data);
	static void onClickBuyLand(void* data);
	static void onClickScriptDebug(void* data);
	static void onCommitSearch(LLUICtrl*, void* data);
	static void onClickSearch(void* data);
	static void onClickFPS(void*);
	static void onClickStatGraph(void*);
	static void onClickLuaFunction(void* data);

private:
	LLColor4		mParcelTextColor;

	LLTextBox*		mTextFPS;
	LLTextBox*		mTextBalance;
	LLTextBox*		mTextHealth;
	LLTextBox*		mTextTime;
	LLTextBox*		mTextParcelName;
	LLTextBox*		mTextStat;
	LLTextBox*		mTextNotifications;
	LLTextBox*		mTextTooComplex;

	LLStatGraph*	mSGBandwidth;
	LLStatGraph*	mSGPacketLoss;

	LLLineEditor*	mLineEditSearch;

	LLButton*		mBtnHealth;
	LLButton*		mBtnNoFly;
	LLButton*		mBtnBuyLand;
	LLButton*		mBtnNoBuild;
	LLButton*		mBtnNoScript;
	LLButton*		mBtnNoPush;
	LLButton*		mBtnNoVoice;
	LLButton*		mBtnNoSee;
	LLButton*		mBtnNoPathFinding;
	LLButton*		mBtnDirtyNavMesh;
	LLButton*		mBtnAdult;
	LLButton*		mBtnMature;
	LLButton*		mBtnPG;
	LLButton*		mBtnNotificationsOn;
	LLButton*		mBtnNotificationsOff;
	LLButton*		mBtnScriptError;
	LLButton*		mBtnRebaking;
	LLButton*		mTooComplex;
	LLButton*		mBtnSearch;
	LLButton*		mBtnSearchBevel;
	LLButton*		mBtnLuaFunction;
	LLButton*		mBtnBuyMoney;

	std::string		mLuaCommand;

	U32				mTimeMode;
	S32				mBalance;
	S32				mHealth;
	S32				mLastNotifications;
	S32				mSquareMetersCredit;
	S32				mSquareMetersCommitted;
	U32				mAgentRegionFailedEventPolls;
	F32				mLastZeroBandwidthTime;
	F32				mAbsoluteMaxBandwidth;

	LLFrameTimer	mHealthTimer;
	LLFrameTimer	mUpdateTimer;
	LLFrameTimer	mNotificationsTimer;
	LLFrameTimer	mRefreshAgentParcelTimer;

	bool			mVisibility;
	bool			mDirty;
	bool			mUseOldIcons;
	bool			mNetworkDown;
	bool			mFrameRateLimited;
};

extern LLStatusBar*	gStatusBarp;

// *HACK: Status bar owns your cached money balance. JC
LL_INLINE bool can_afford_transaction(S32 cost)
{
	return cost <= 0 || (gStatusBarp && gStatusBarp->getBalance() >= cost);
}

#endif
