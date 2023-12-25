/**
 * @file llinterp.cpp
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

#include "llinterp.h"

void LLInterpLinear::update(F32 time)
{
	F32 target_frac = (time - mStartTime) / mDuration;
	F32 dfrac = target_frac - mCurFrac;
	if (target_frac >= 0.f)
	{
		mActive = true;
	}

	if (target_frac > 1.f)
	{
		mCurVal = mEndVal;
		mCurFrac = 1.f;
		mCurTime = time;
		mDone = true;
		return;
	}

	target_frac = llmax(0.f, target_frac);

	if (dfrac >= 0.f)
	{
		F32 total_frac = 1.f - mCurFrac;
		F32 inc_frac = dfrac / total_frac;
		mCurVal = inc_frac * mEndVal + (1.f - inc_frac) * mCurVal;
		mCurTime = time;
	}
	else
	{
		F32 total_frac = mCurFrac - 1.f;
		F32 inc_frac = dfrac / total_frac;
		mCurVal = inc_frac * mStartVal + (1.f - inc_frac) * mCurVal;
		mCurTime = time;
	}

	mCurFrac = target_frac;
}
