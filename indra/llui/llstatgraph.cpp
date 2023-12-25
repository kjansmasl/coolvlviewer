/**
 * @file llstatgraph.cpp
 * @brief Simpler compact stat graph with tooltip
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * Copyright (c) 2009-2021, Henri Beauchamp.
 * Changes by Henri Beauchamp:
 *  - Allow a special color with full bar display for out of range indications.
 *  - Allow the use of a logarithmic scale indicator.
 *  - Allow the use of a multiplier for the unit and a suffix for the tool tip.
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

#include "llstatgraph.h"

#include "llgl.h"
#include "llrender.h"

LLStatGraph::LLStatGraph(const std::string& name, const LLRect& rect)
:	LLView(name, rect, true),
	mStatp(NULL),
	mMin(0.f),
	mMax(125.f),
	mLogScale(false),
	mPerSec(true),
	mValue(0.f),
	mDivisor(0.f),
	mPrecision(0),
	mClickedCallback(NULL),
	mCallbackUserData(NULL)
{
	setToolTip(name);

	mThresholdColors[0] = LLColor4(0.f, 1.f, 0.f, 1.f);
	mThresholdColors[1] = LLColor4(1.f, 1.f, 0.f, 1.f);
	mThresholdColors[2] = LLColor4(1.f, 0.f, 0.f, 1.f);
	mThresholdColors[3] = LLColor4(0.75f, 0.75f, 0.75f, 1.f);

	mThresholds[0] = 50.f;
	mThresholds[1] = 75.f;
	mThresholds[2] = 95.f;

	updateRange();
}

void LLStatGraph::updateRange()
{
	if (mMax <= mMin)
	{
		mRange = 0.f;
		return;
	}
	mRange = mMax - mMin;
	if (mLogScale)
	{
		mRange = logf(mRange);
		F32 max = llmax(mThresholds[0], mThresholds[1], mThresholds[2]);
		if (max <= 0.f || max > 1.f)
		{
			// Logarithmic indicators thresholds are always expressed in
			// percent of the full range...
			mThresholds[0] = 0.5f;
			mThresholds[1] = 0.75f;
			mThresholds[2] = 0.95f;
		}
	}
}

//virtual
void LLStatGraph::draw()
{
	if (mStatp)
	{
		if (mPerSec)
		{
			mValue = mStatp->getMeanPerSec();
		}
		else
		{
			mValue = mStatp->getMean();
		}
	}

	// Note: we want to draw a full bar (with the special mThresholdColors[3]
	// color) when mMax <= mMin (used in the status bar bandwidth indicator for
	// disconnected network condition). HB
	F32 frac = 1.f;
	if (mValue <= mMin)
	{
		frac = 0.f;
	}
	else if (mRange > 0.f)
	{
		if (mLogScale)
		{
			frac = logf(mValue - mMin) / mRange;
		}
		else
		{
			frac = (mValue - mMin) / mRange;
		}
		if (frac > 1.f)
		{
			frac = 1.f;
		}
	}

	if (mUpdateTimer.getElapsedTimeF32() > 0.5f)
	{
		mUpdateTimer.reset();
		std::string format_str = llformat("%%s%%.%df%%s", mPrecision);
		F32 value = mValue;
		const char* unit;
		if (mDivisor > 0.f && value >= mDivisor && !mUnit2.empty())
		{
			unit = mUnit2.c_str();
			value /= mDivisor;
		}
		else
		{
			unit = mUnit1.c_str();
		}
		std::string tooltip = llformat(format_str.c_str(), mLabel.c_str(),
									   value, unit);
		if (!mLabelSuffix.empty())
		{
			tooltip += mLabelSuffix;
		}
		setToolTip(tooltip);
	}

	static const LLColor4 bg_color(LLUI::sMenuDefaultBgColor);
	gGL.color4fv(bg_color.mV);
	gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, true);

	gGL.color4fv(LLColor4::black.mV);
	gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0, false);

	LLColor4* color;
	if (mMax <= mMin)
	{
		color = &mThresholdColors[3];
	}
	else
	{
		F32 val = mLogScale ? frac : mValue;
		U32 i;
		for (i = 0; i < 2; ++i)
		{
			if (mThresholds[i] > val)
			{
				break;
			}
		}
		color = &mThresholdColors[i];
	}
	gGL.color4fv(color->mV);
	gl_rect_2d(1, ll_round(frac * getRect().getHeight()),
			   getRect().getWidth() - 1, 0, true);
}

bool LLStatGraph::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mClickedCallback)
	{
		(*mClickedCallback)(mCallbackUserData);
		return true;
	}

	return LLView::handleMouseDown(x, y, mask);
}

bool LLStatGraph::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// If we handled the mouse down event ourselves as a "click", then we must
	// handle the mouse up event as well (click = mouse down + mouse up)...
	return mClickedCallback || LLView::handleMouseUp(x, y, mask);
}

void LLStatGraph::setClickedCallback(void (*cb)(void*), void* userdata)
{
	mClickedCallback = cb;
	if (userdata)
	{
		mCallbackUserData = userdata;
	}
}
