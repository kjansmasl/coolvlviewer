/**
 * @file lleventtimer.cpp
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

#include "linden_common.h"

#include "lleventtimer.h"

#include "lldate.h"
#include "lltimer.h"

LLEventTimer::LLEventTimer(const LLDate& time)
:	mPeriod(F32(time.secondsSinceEpoch() - LLTimer::getEpochSeconds()))
{
}

//static
void LLEventTimer::stepFrame()
{
	std::vector<LLEventTimer*> completed_timers;

	for (auto& timer : instance_snapshot())
	{
		F32 et = timer.mEventTimer.getElapsedTimeF32();
		if (timer.mEventTimer.getStarted() && et > timer.mPeriod)
		{
			timer.mEventTimer.reset();
			if (timer.tick())
			{
				completed_timers.push_back(&timer);
			}
		}
	}

	for (U32 i = 0, count = completed_timers.size(); i < count; ++i)
	{
		delete completed_timers[i];
	}
}
