/**
 * @file llfloaterstats.h
 * @brief Container for statistics view
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

#ifndef LL_LLFLOATERSTATS_H
#define LL_LLFLOATERSTATS_H

#include "llfloater.h"

class LLContainerView;
class LLStatBar;
class LLStatView;
class LLScrollableContainer;

class LLFloaterStats final : public LLFloater,
							 public LLFloaterSingleton<LLFloaterStats>

{
	friend class LLUISingleton<LLFloaterStats, VisibilityPolicy<LLFloater> >;

public:
	LLFloaterStats(const LLSD& val);
	~LLFloaterStats() override;

	void reshape(S32 width, S32 height, bool call_from_parent = true) override;
	void onOpen() override;
	void onClose(bool app_quitting) override;
	void draw() override;

	void addStatView(LLStatView* stat);

private:
	void setFPSStatBarRange(U32 average);
	void setBWStatBarRange(U32 max);
	void buildStats();

private:
	LLContainerView*		mStatsContainer;
	LLScrollableContainer*	mScrollContainer;
	LLStatBar*				mFPSStatBar;
	LLStatBar*				mBWStatBar;
	U32						mLastFPSAverageCount;
	F32						mLastStatRangeChange;
	F32						mStatBarLastMaxBW;
	F32						mStatBarMaxFPS;
	F32						mCurrentMaxBW;
	F32						mCurrentMaxFPS;
};

#endif
