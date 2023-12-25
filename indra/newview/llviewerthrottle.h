/**
 * @file llviewerthrottle.h
 * @brief LLViewerThrottle class header file
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

#ifndef LL_LLVIEWERTHROTTLE_H
#define LL_LLVIEWERTHROTTLE_H

#include <vector>

#include "llframetimer.h"
#include "llstring.h"
#include "llthrottle.h"

class LLViewerThrottleGroup
{
protected:
	LOG_CLASS(LLViewerThrottleGroup);

public:
	LLViewerThrottleGroup();
	LLViewerThrottleGroup(const U32 settings[TC_EOF]);

	LLViewerThrottleGroup operator*(F32 frac) const;
	LLViewerThrottleGroup operator+(const LLViewerThrottleGroup& b) const;
	LLViewerThrottleGroup operator-(const LLViewerThrottleGroup& b) const;

	LL_INLINE U32 getTotal()					{ return mThrottleTotal; }
	void sendToSim() const;

	void dump();

protected:
	U32 mThrottles[TC_EOF];
	U32 mThrottleTotal;
};

class LLViewerThrottle
{
protected:
	LOG_CLASS(LLViewerThrottle);

public:
	LLViewerThrottle();

	void setMaxBandwidth(U32 kbps, bool from_event = false);

	void load();
	void save() const;
	void sendToSim() const;

	LL_INLINE U32 getMaxBandwidth() const		{ return mMaxBandwidth; }
	LL_INLINE U32 getCurrentBandwidth() const	{ return mCurrentBandwidth; }

	void updateDynamicThrottle();
	void resetDynamicThrottle();

	LLViewerThrottleGroup getThrottleGroup(U32 bandwidth_kbps);

	static const std::string sNames[TC_EOF];

protected:
	LLViewerThrottleGroup				mCurrent;
	LLFrameTimer						mUpdateTimer;
	std::vector<LLViewerThrottleGroup>	mPresets;
	U32									mMaxBandwidth;
	U32									mCurrentBandwidth;
	F32									mThrottleFrac;
};

extern LLViewerThrottle gViewerThrottle;

#endif // LL_LLVIEWERTHROTTLE_H
