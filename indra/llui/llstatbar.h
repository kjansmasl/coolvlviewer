/**
 * @file llstatbar.h
 * @brief A little map of the world with network information
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

#ifndef LL_LLSTATBAR_H
#define LL_LLSTATBAR_H

#include "llframetimer.h"
#include "llview.h"

class LLStat;

class LLStatBar : public LLView
{
protected:
	LOG_CLASS(LLStatBar);

private:
	enum STAT_MODE_FLAG
	{
		STAT_BAR_FLAG = 1,
		STAT_HISTORY_FLAG = 2
	};

public:
	LLStatBar(const std::string& name, const LLRect& rect,
			  const std::string& setting = std::string(),
			  bool default_bar = false, bool default_history = false);

	void draw() override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;

	const std::string& getLabel() const;
	void setLabel(const std::string& label);
	void setUnitLabel(const std::string& unit_label);

	// Returns the height of this object, given the set options:
	LLRect getRequiredRect() override;

private:
	std::string		mLabel;
	std::string		mUnitLabel;
	std::string		mSetting;
	LLFrameTimer	mUpdateTimer;
	F32				mValue;

public:
	LLStat*			mStatp;
	F32				mUpdatesPerSec;
	F32				mMinBar;
	F32				mMaxBar;
	F32				mTickSpacing;
	F32				mLabelSpacing;
	U32				mPrecision;
	bool			mPerSec;			// Use the per sec stats.
	bool			mDisplayBar;		// Display the bar graph.
	bool			mDisplayHistory;
	// Display mean if true, else display current value
	bool			mDisplayMean;
	bool			mNoResize;			// When true, ignore handleMouseDown()
};

#endif
