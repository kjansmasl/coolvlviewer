/**
 * @file llpanelexperiencelog.h
 * @brief LLPanelExperienceLog class definition
 *
 * $LicenseInfo:firstyear=2014&license=viewergpl$
 *
 * Copyright (c) 2014, Linden Research, Inc.
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

#ifndef LL_LLPANELEXPERIENCELOG_H
#define LL_LLPANELEXPERIENCELOG_H

#include "boost/signals2.hpp"

#include "llpanel.h"

class LLButton;
class LLCheckBoxCtrl;
class LLScrollListCtrl;
class LLSpinCtrl;

class LLPanelExperienceLog final : public LLPanel
{
public:
	LLPanelExperienceLog();
	~LLPanelExperienceLog() override	{}

	bool postBuild() override;
	void refresh() override;

	static LLPanelExperienceLog* create();

protected:
	static void onClear(void* data);
	static void onNext(void* data);
	static void onNotify(void* data);
	static void onPrev(void* data);
	static void onProfileExperience(void* data);
	static void onReportExperience(void* data);

	static void onSelectionChanged(LLUICtrl* ctrl, void* data);
	static void onNotifyChanged(LLUICtrl* ctrl, void* data);
	static void onLogSizeChanged(LLUICtrl* ctrl, void* data);

	LLSD getSelectedEvent();

private:
	LLButton*							mClearBtn;
	LLButton*							mNextBtn;
	LLButton*							mPrevBtn;
	LLButton*							mNotifyBtn;
	LLButton*							mProfileBtn;
	LLButton*							mReportBtn;
	LLCheckBoxCtrl*						mNotifyAllCheck;
	LLScrollListCtrl*					mEventList;
	LLSpinCtrl*							mLogSizeSpin;

	S32									mPageSize;
	S32									mCurrentPage;

	boost::signals2::scoped_connection	mNewEvent;
};

#endif // LL_LLPANELEXPERIENCELOG_H
