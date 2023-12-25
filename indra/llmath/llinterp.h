/**
 * @file llinterp.h
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

#ifndef LL_LLINTERP_H
#define LL_LLINTERP_H

#include "llpreprocessor.h"
#include "stdtypes.h"

// There used to be several interpolator class templates derived from a base
// LLInterp class template, with different data types, but only the linear type
// with the F32 data type was ever used in the viewer code, so I removed the
// others, and made LLInterpLinear into a non-virtual, non-template class. HB

class LLInterpLinear
{
public:
	LL_INLINE LLInterpLinear()
	:	mStartVal(0.f),
		mEndVal(0.f),
		mCurVal(0.f),
		mStartTime(0.f),
		mCurTime(0.f),
		mEndTime(1.f),
		mDuration(1.f),
		mCurFrac(0.f),
		mDone(false),
		mActive(false)
	{
	}

	LL_INLINE void start()
	{
		mCurVal = mStartVal;
		mCurTime = mStartTime;
		mDone = mActive = false;
		mCurFrac = 0.f;
	}

	void update(F32 time);

	LL_INLINE const F32& getCurVal() const			{ return mCurVal; }

	LL_INLINE void setStartVal(const F32& val)		{ mStartVal = val; }
	LL_INLINE const F32& getStartVal() const		{ return mStartVal; }

	LL_INLINE void setEndVal(const F32& val)		{ mEndVal = val; }

	LL_INLINE const F32& getEndVal() const			{ return mEndVal; }

	LL_INLINE void setStartTime(F32 time)
	{
		mStartTime = time;
		mDuration = mEndTime - mStartTime;
	}

	LL_INLINE F32 getStartTime() const				{ return mStartTime; }

	LL_INLINE void setEndTime(F32 time)
	{
		mEndTime = time;
		mDuration = mEndTime - mStartTime;
	}

	LL_INLINE F32 getEndTime() const				{ return mEndTime; }

	LL_INLINE bool isActive() const					{ return mActive; }
	LL_INLINE bool isDone() const					{ return mDone; }

	LL_INLINE F32 getCurFrac() const				{ return mCurFrac; }

protected:
	F32		mStartTime;
	F32		mEndTime;
	F32		mDuration;
	F32		mCurTime;
	F32		mCurFrac;

	F32		mStartVal;
	F32		mEndVal;
	F32		mCurVal;

	bool	mActive;
	bool	mDone;
};

#endif // LL_LLINTERP_H
