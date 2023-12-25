/**
 * @file llstat.h
 * @brief Runtime statistics accumulation.
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

#ifndef LL_LLSTAT_H
#define LL_LLSTAT_H

#include "llframetimer.h"

class LLSD;

class LLStat
{
public:
	LLStat(U32 num_bins = 32, bool use_frame_timer = false);
	~LLStat();

	void reset();

	// Start the timer for the current "frame", otherwise uses the time tracked
	// from the last addValue
	void start();

	// Adds the current value being tracked, and tracks the DT.
	void addValue(F32 value = 1.f);

	LL_INLINE void addValue(S32 value)				{ addValue((F32)value); }
	LL_INLINE void addValue(U32 value)				{ addValue((F32)value); }

	LL_INLINE void setBeginTime(F64 time)			{ mBeginTime[mNextBin] = time; }

	void addValueTime(F64 time, F32 value = 1.f);

	LL_INLINE S32 getCurBin() const					{ return mCurBin; }
	LL_INLINE S32 getNextBin() const				{ return mNextBin; }

	LL_INLINE F32 getCurrent() const				{ return mBins[mCurBin]; }
	LL_INLINE F32 getCurrentPerSec() const			{ return mBins[mCurBin] / mDT[mCurBin]; }
	LL_INLINE F64 getCurrentBeginTime() const		{ return mBeginTime[mCurBin]; }
	LL_INLINE F64 getCurrentTime() const			{ return mTime[mCurBin]; }
	LL_INLINE F32 getCurrentDuration() const		{ return mDT[mCurBin]; }

	// Age is how many "addValues" previously - zero is current
	F32 getPrev(S32 age) const;
	F32 getPrevPerSec(S32 age) const;
	F64 getPrevBeginTime(S32 age) const;
	F64 getPrevTime(S32 age) const;

	LL_INLINE F32 getBin(S32 bin) const				{ return mBins[bin]; }
	LL_INLINE F32 getBinPerSec(S32 bin) const		{ return mBins[bin] / mDT[bin]; }
	LL_INLINE F64 getBinBeginTime(S32 bin) const	{ return mBeginTime[bin]; }
	LL_INLINE F64 getBinTime(S32 bin) const			{ return mTime[bin]; }

	F32 getMax() const;
	F32 getMaxPerSec() const;

	F32 getMean() const;
	F32 getMeanPerSec() const;
	F32 getMeanDuration() const;

	F32 getMin() const;
	F32 getMinPerSec() const;
	F32 getMinDuration() const;

	F32 getSum() const;
	F32 getSumDuration() const;

	LL_INLINE U32 getNumValues() const				{ return mNumValues; }
	LL_INLINE S32 getNumBins() const				{ return mNumBins; }

	LL_INLINE F64 getLastTime() const				{ return mLastTime; }

private:
	void init();

private:
	U32					mNumValues;
	U32					mNumBins;
	F32					mLastValue;
	F64					mLastTime;
	F32*				mBins;
	F64*				mBeginTime;
	F64*				mTime;
	F32*				mDT;
	S32					mCurBin;
	S32					mNextBin;
	bool				mUseFrameTimer;

	static LLTimer		sTimer;
	static LLFrameTimer	sFrameTimer;
};

#endif // LL_LLSTAT_H
