/**
 * @file llstatgraph.h
 * @brief Simpler compact stat graph with tooltip
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * Copyright (c) 2009-2021, Henri Beauchamp.
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

#ifndef LL_LLSTATGRAPH_H
#define LL_LLSTATGRAPH_H

#include "llframetimer.h"
#include "llstat.h"
#include "llview.h"
#include "llcolor4.h"

class LLStatGraph : public LLView
{
public:
	LLStatGraph(const std::string& name, const LLRect& rect);

	void draw() override;

	LL_INLINE void setStat(LLStat* statp)					{ mStatp = statp; }

	LL_INLINE void setLabel(const std::string& label)		{ mLabel = label; }

	LL_INLINE void setLabelSuffix(const std::string& s)		{ mLabelSuffix = s; }

	LL_INLINE void setUnits(const std::string& unit1,
							const std::string& unit2 = LLStringUtil::null)
	{
		mUnit1 = unit1;
		mUnit2 = unit2;
	}

	// This is the divisor to apply to the value to switch from unit 1 to
	// unit 2. When not specified (or <= 0.f), the switch does not happen.
	LL_INLINE void setUnitDivisor(F32 divisor)				{ mDivisor = divisor; }

	LL_INLINE void setPrecision(U32 precision)				{ mPrecision = precision; }

	// Note: logarithmic indicators thresholds are always expressed in % of the
	// full range, while the non-log thresholds are an absolute value. HB
	LL_INLINE void setThreshold(U32 i, F32 t)
	{
		if (i < 3)
		{
			mThresholds[i] = t;
		}
	}

	LL_INLINE void setThresholdColor(U32 i, const LLColor4& color)
	{
		if (i < 4)
		{
			mThresholdColors[i] = color;
		}
	}

	LL_INLINE void setMin(F32 min)							{ mMin = min; updateRange(); }
	LL_INLINE void setMax(F32 max)							{ mMax = max; updateRange(); }
	LL_INLINE void setLogScale(bool b = true)				{ mLogScale = b; updateRange(); }
	LL_INLINE void setPerSec(bool b = true)					{ mPerSec = b; }

	LL_INLINE void setValue(const LLSD& value) override		{ mValue = (F32)value.asReal(); }
	LL_INLINE F32 getValueF32()								{ return mValue; }
	LL_INLINE LLStat* getStat()								{ return mStatp; }

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	void setClickedCallback(void (*cb)(void*), void* userdata = NULL);

private:
	void updateRange();

public:
	LLStat*			mStatp;

private:
	void			(*mClickedCallback)(void* data);
	void*			mCallbackUserData;

	std::string		mLabel;
	std::string		mLabelSuffix;
	std::string		mUnit1;
	std::string		mUnit2;

	LLColor4		mThresholdColors[4];
	F32				mThresholds[3];

	LLFrameTimer	mUpdateTimer;

	F32				mValue;
	F32				mMin;
	F32				mMax;
	F32				mRange;
	F32				mDivisor;
	U32				mPrecision;		// Number of digits of precision after dot

	bool			mLogScale;
	bool			mPerSec;
};

#endif  // LL_LLSTATGRAPH_H
