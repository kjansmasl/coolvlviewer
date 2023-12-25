/**
 * @file llstat.cpp
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

#include "linden_common.h"

#include "llstat.h"

#include "llframetimer.h"
#include "llsdserialize.h"

LLTimer LLStat::sTimer;
LLFrameTimer LLStat::sFrameTimer;

LLStat::LLStat(U32 num_bins, bool use_frame_timer)
:	mUseFrameTimer(use_frame_timer),
	mNumValues(0),
	mLastValue(0.f),
	mLastTime(0.f),
	mNumBins(num_bins),
	mCurBin(num_bins - 1),
	mNextBin(0)
{
	llassert_always(mNumBins > 0);
	init();
}

LLStat::~LLStat()
{
	delete[] mBins;
	delete[] mBeginTime;
	delete[] mTime;
	delete[] mDT;
}

void LLStat::init()
{
	mBins = new F32[mNumBins];
	mBeginTime = new F64[mNumBins];
	mTime = new F64[mNumBins];
	mDT = new F32[mNumBins];
	for (U32 i = 0; i < mNumBins; ++i)
	{
		mBins[i] = 0.f;
		mBeginTime[i] = 0.0;
		mTime[i] = 0.0;
		mDT[i] = 0.f;
	}
}

void LLStat::reset()
{
	mNumValues = 0;
	mLastValue = 0.f;
	mCurBin = mNumBins - 1;
	delete[] mBins;
	delete[] mBeginTime;
	delete[] mTime;
	delete[] mDT;
	init();
}

void LLStat::addValueTime(F64 time, F32 value)
{
	if (mNumValues < mNumBins)
	{
		++mNumValues;
	}

	// Increment the bin counters.
	if ((U32)(++mCurBin) == mNumBins)
	{
		mCurBin = 0;
	}
	if ((U32)(++mNextBin) == mNumBins)
	{
		mNextBin = 0;
	}

	mBins[mCurBin] = value;
	mTime[mCurBin] = time;
	mDT[mCurBin] = (F32)(mTime[mCurBin] - mBeginTime[mCurBin]);
	// This value is used to prime the min/max calls
	mLastTime = mTime[mCurBin];
	mLastValue = value;

	// Set the begin time for the next stat segment.
	mBeginTime[mNextBin] = mTime[mCurBin];
	mTime[mNextBin] = mTime[mCurBin];
	mDT[mNextBin] = 0.f;
}

void LLStat::start()
{
	if (mUseFrameTimer)
	{
		mBeginTime[mNextBin] = sFrameTimer.getElapsedSeconds();
	}
	else
	{
		mBeginTime[mNextBin] = sTimer.getElapsedTimeF64();
	}
}

void LLStat::addValue(F32 value)
{
	if (mNumValues < mNumBins)
	{
		++mNumValues;
	}

	// Increment the bin counters.
	if ((U32)(++mCurBin) == mNumBins)
	{
		mCurBin = 0;
	}
	if ((U32)(++mNextBin) == mNumBins)
	{
		mNextBin = 0;
	}

	mBins[mCurBin] = value;
	if (mUseFrameTimer)
	{
		mTime[mCurBin] = sFrameTimer.getElapsedSeconds();
	}
	else
	{
		mTime[mCurBin] = sTimer.getElapsedTimeF64();
	}
	mDT[mCurBin] = (F32)(mTime[mCurBin] - mBeginTime[mCurBin]);

	// This value is used to prime the min/max calls
	mLastTime = mTime[mCurBin];
	mLastValue = value;

	// Set the begin time for the next stat segment.
	mBeginTime[mNextBin] = mTime[mCurBin];
	mTime[mNextBin] = mTime[mCurBin];
	mDT[mNextBin] = 0.f;
}

F32 LLStat::getMax() const
{
	F32 current_max = mLastValue;
	for (U32 i = 0; i < mNumBins && i < mNumValues; ++i)
	{
		// Skip the bin we are currently filling.
		if (i != (U32)mNextBin && mBins[i] > current_max)
		{
			current_max = mBins[i];
		}
	}
	return current_max;
}

F32 LLStat::getMean() const
{
	F32 current_mean = 0.f;
	U32 samples = 0;
	for (U32 i = 0; i < mNumBins && i < mNumValues; ++i)
	{
		// Skip the bin we are currently filling.
		if (i != (U32)mNextBin)
		{
			current_mean += mBins[i];
			++samples;
		}
	}
	return samples != 0 ? current_mean / (F32)samples : 0.f;
}

F32 LLStat::getMin() const
{
	F32 current_min = mLastValue;
	for (U32 i = 0; i < mNumBins && i < mNumValues; ++i)
	{
		// Skip the bin we are currently filling.
		if (i != (U32)mNextBin && mBins[i] < current_min)
		{
			current_min = mBins[i];
		}
	}
	return current_min;
}

F32 LLStat::getSum() const
{
	F32 sum = 0.f;
	for (U32 i = 0; i < mNumBins && i < mNumValues; ++i)
	{
		// Skip the bin we are currently filling.
		if (i != (U32)mNextBin)
		{
			sum += mBins[i];
		}
	}
	return sum;
}

F32 LLStat::getSumDuration() const
{
	F32 sum = 0.f;
	for (U32 i = 0; i < mNumBins && i < mNumValues; ++i)
	{
		// Skip the bin we are currently filling.
		if (i != (U32)mNextBin)
		{
			sum += mDT[i];
		}
	}
	return sum;
}

F32 LLStat::getPrev(S32 age) const
{
	S32 bin = mCurBin - age;
	while (bin < 0)
	{
		bin += mNumBins;
	}
	// Bogus for bin we are currently working on, so return 0 in that case
	return bin == mNextBin ? 0.f : mBins[bin];
}

F32 LLStat::getPrevPerSec(S32 age) const
{
	S32 bin = mCurBin - age;
	while (bin < 0)
	{
		bin += mNumBins;
	}
	// Bogus for bin we are currently working on, so return 0 in that case
	return bin == mNextBin || mDT[bin] == 0.f ? 0.f : mBins[bin] / mDT[bin];
}

F64 LLStat::getPrevBeginTime(S32 age) const
{
	S32 bin = mCurBin - age;
	while (bin < 0)
	{
		bin += mNumBins;
	}
	// Bogus for bin we are currently working on, so return 0 in that case
	return bin == mNextBin ? 0.0 : mBeginTime[bin];
}

F64 LLStat::getPrevTime(S32 age) const
{
	S32 bin = mCurBin - age;
	while (bin < 0)
	{
		bin += mNumBins;
	}
	// Bogus for bin we are currently working on, so return 0 in that case
	return bin == mNextBin ? 0.0 : mTime[bin];
}

F32 LLStat::getMeanPerSec() const
{
	F32 value = 0.f;
	F32 dt = 0.f;
	for (U32 i = 0; i < mNumBins && i < mNumValues; ++i)
	{
		// Skip the bin we are currently filling.
		if (i != (U32)mNextBin)
		{
			value += mBins[i];
			dt += mDT[i];
		}
	}
	return dt > 0.f ? value / dt : 0.f;
}

F32 LLStat::getMeanDuration() const
{
	F32 dur = 0.f;
	U32 count = 0;
	for (U32 i = 0; i < mNumBins && i < mNumValues; ++i)
	{
		if (i == (U32)mNextBin)
		{
			continue;
		}
		dur += mDT[i];
		++count;
	}
	return count > 0 ? dur / (F32)count : 0.f;
}

F32 LLStat::getMaxPerSec() const
{
	F32 value;
	if (mNextBin != 0 && mDT[0] != 0.f)
	{
		value = mBins[0] / mDT[0];
	}
	else if (mNumValues > 0 && mDT[1] != 0.f)
	{
		value = mBins[1] / mDT[1];
	}
	else
	{
		value = 0.f;
	}
	for (U32 i = 0; i < mNumBins && i < mNumValues; ++i)
	{
		F32 dt = mDT[i];
		// Skip the bin we are currently filling.
		if (i != (U32)mNextBin && dt > 0.f)
		{
			value = llmax(value, mBins[i] / dt);
		}
	}
	return value;
}

F32 LLStat::getMinPerSec() const
{
	F32 value;
	if (mNextBin != 0 && mDT[0] != 0.f)
	{
		value = mBins[0] / mDT[0];
	}
	else if (mNumValues > 0 && mDT[1] != 0.f)
	{
		value = mBins[1] / mDT[1];
	}
	else
	{
		value = 0.f;
	}
	for (U32 i = 0; i < mNumBins && i < mNumValues; ++i)
	{
		F32 dt = mDT[i];
		// Skip the bin we are currently filling.
		if (i != (U32)mNextBin && dt > 0.f)
		{
			value = llmin(value, mBins[i] / dt);
		}
	}
	return value;
}

F32 LLStat::getMinDuration() const
{
	F32 dur = 0.f;
	for (U32 i = 0; i < mNumBins && i < mNumValues; ++i)
	{
		dur = llmin(dur, mDT[i]);
	}
	return dur;
}
