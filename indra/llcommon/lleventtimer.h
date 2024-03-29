/**
 * @file lleventtimer.h
 * @brief Cross-platform objects for doing timing
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_EVENTTIMER_H
#define LL_EVENTTIMER_H

#include "llinstancetracker.h"
#include "lltimer.h"

class LLDate;

// Class for scheduling a function to be called periodically (the timing is
// imprecise since it is conditionned by the duration of each frame).

class LLEventTimer : public LLInstanceTracker<LLEventTimer>
{
	// This class is the only one that should be allowed to call stepFrame()
	friend class LLApp;

public:
	// Period is the amount of time between each call to tick() in seconds
	LL_INLINE LLEventTimer(F32 period)
	:	mPeriod(period)
	{
	}

	LLEventTimer(const LLDate& time);

	// Method to be called at the supplied frequency. Normally returns false;
	// true will delete the timer after the method returns.
	virtual bool tick() = 0;

private:
	// Called exclusively by LLApp::stepFrame()
	static void stepFrame();

protected:
	LLTimer	mEventTimer;
	F32		mPeriod;
};

#endif //LL_EVENTTIMER_H
