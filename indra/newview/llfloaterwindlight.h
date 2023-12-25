/**
 * @file llfloaterwindlight.h
 * @brief LLFloaterWindlight class definition
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERWINDLIGHT_H
#define LL_LLFLOATERWINDLIGHT_H

#include "llfloater.h"

class LLButton;
class LLColorSwatchCtrl;
class LLPanelWLDayCycle;
class LLPanelWLSky;
class LLPanelWLWater;
class LLSliderCtrl;
class LLTextBox;

class LLFloaterWindlight final : public LLFloater,
								 public LLFloaterSingleton<LLFloaterWindlight>
{
	friend class LLUISingleton<LLFloaterWindlight,
							   VisibilityPolicy<LLFloater> >;
	friend class LLPanelWLDayCycle;
	friend class LLPanelWLSky;
	friend class LLPanelWLWater;

protected:
	LOG_CLASS(LLFloaterWindlight);

public:
	bool postBuild() override;
	void refresh() override;
	void draw() override;

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LLFloaterWindlight(const LLSD&);

	// Used internally to set the Windlight animator time value.
	static void setDayTime(F32 time);

	// Used to create our three sub-panels
	static void* createPanelDayCycle(void* data);
	static void* createPanelSky(void* data);
	static void* createPanelWater(void* data);

	// Handles time of day change
	static void onChangeDayTime(LLUICtrl* ctrl, void*);

	// Handles cloud coverage change
	static void onChangeCloudCoverage(LLUICtrl* ctrl, void*);

	// Handles change in water fog density
	static void onChangeWaterFogDensity(LLUICtrl* ctrl, void* userdata);

	// Handles change in under water fog density
	static void onChangeUnderWaterFogMod(LLUICtrl* ctrl, void* userdata);

	// Handles change in water fog color
	static void onChangeWaterColor(LLUICtrl* ctrl, void* userdata);

	// Preview current sky and water in the Extended Environment renderer
	static void onPreviewAsEE(void* userdata);

	// Converts the present time to a digital clock time
	static std::string timeToString(F32 cur_time);

private:
	LLPanelWLDayCycle*	mPanelDayCycle;
	LLPanelWLSky*		mPanelSky;
	LLPanelWLWater*		mPanelWater;
	LLColorSwatchCtrl*	mEnvWaterColor;
	LLSliderCtrl*		mEnvTimeSlider;
	LLSliderCtrl*		mEnvCloudSlider;
	LLSliderCtrl*		mEnvWaterFogSlider;
	LLTextBox*			mEnvTimeText;
	LLTextBox*			mEnvWaterColorText;
	LLButton*			mPreviewBtn;
};

#endif
