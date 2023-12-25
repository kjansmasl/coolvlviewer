/**
 * @file llfloatercolorpicker.h
 * @brief Generic system color picker
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright(c) 2004-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 *("GPL"), unless you have obtained a separate licensing agreement
 *("Other License"), formally executed by you and Linden Lab.  Terms of
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

#ifndef LL_LLFLOATERCOLORPICKER_H
#define LL_LLFLOATERCOLORPICKER_H

#include <vector>

#include "llfloater.h"
#include "llpointer.h"
#include "llspinctrl.h"
#include "lltextureentry.h"

#include "llcolorswatch.h"

class LLButton;
class LLCheckBoxCtrl;

class LLFloaterColorPicker final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterColorPicker);

public:
	LLFloaterColorPicker(LLColorSwatchCtrl* swatch,
						 bool show_apply_immediately = false);
	~LLFloaterColorPicker() override;

	void initUI(F32 r, F32 g, F32 b);
	void showUI();
	void cancelSelection();

	// Mutator for original RGB value
	LL_INLINE void setOrigRgb(F32 r, F32 g, F32 b)
	{
		mOrigR = r;
		mOrigG = g;
		mOrigB = b;
	}

	// Mutator for current RGB value (also syncs HSL values)
	void setCurRgb(F32 r, F32 g, F32 b);

	// Mutator for current HSL value (also syncs RGB values)
	void setCurHsl(F32 h, F32 s, F32 l);

	// Accessors for current RGB value
	LL_INLINE F32 getCurR()						{ return mCurR; }
	LL_INLINE F32 getCurG()						{ return mCurG; }
	LL_INLINE F32 getCurB()						{ return mCurB; }

	void setActive(bool active);

protected:
	bool postBuild() override;
	void draw() override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	void onMouseCaptureLost() override;
	void onClose(bool app_quitting) override;

	void stopUsingPipette();

	// Mutators for mouse buttons pressed in region
	void setMouseDownInHueRegion(bool mouse_down_in_region);
	void setMouseDownInLumRegion(bool mouse_down_in_region);
	void setMouseDownInSwatch(bool mouse_down_in_swatch);

	// Updates current RGB/HSL values based on point in picker
	bool updateRgbHslFromPoint(S32 x, S32 y);

	// Called when text entries (RGB/HSL etc) are changed by user
	void onTextEntryChanged(LLUICtrl* ctrl);

	// Updates text entry fields with current RGB/HSL
	void updateTextEntry();

	// Converts RGB to HSL and vice-versa
	void hslToRgb(F32 h, F32 s, F32 l, F32& r, F32& g, F32& b);
	F32 hueToRgb(F32 h1, F32 h2, F32 h3);

	// Callbacks
	static void onClickCancel(void* data);
	static void onClickSelect(void* data);
	static void onClickPipette(void* data);
	static void onTextCommit(LLUICtrl* ctrl, void* data);
	static void onImmediateCheck(LLUICtrl* ctrl, void* data);
	static void onColorSelect(const LLTextureEntry& te, void *data);

private:
	// Turns on or off text entry commit call backs
	void enableTextCallbacks(bool stateIn);

	// Draws color selection palette
	void drawPalette();

	// Finds a complementary color to the one passed in that can be used to
	// highlight
	const LLColor4& getComplementaryColor(const LLColor4& bg_col);

private:
	LLButton*					mSelectBtn;
	LLButton*					mCancelBtn;
	LLButton*					mPipetteBtn;
	LLCheckBoxCtrl*				mApplyImmediateCheck;

	// Current swatch in use
	LLColorSwatchCtrl*			mSwatch;

	// Image used to compose color grid
	LLPointer<LLViewerTexture>	mRGBImage;

	std::vector<LLColor4>		mPalette;

	// Original RGB values
	F32							mOrigR;
	F32							mOrigG;
	F32							mOrigB;
	// Current RGB values
	F32							mCurR;
	F32							mCurG;
	F32							mCurB;
	// Current HSL values
	F32							mCurH;
	F32							mCurS;
	F32							mCurL;

	F32							mContextConeOpacity;

	S32							mHighlightEntry;

	const S32					mComponents;

	const S32					mRGBViewerImageLeft;
	const S32					mRGBViewerImageTop;
	const S32					mRGBViewerImageWidth;
	const S32					mRGBViewerImageHeight;

	const S32					mLumRegionLeft;
	const S32					mLumRegionTop;
	const S32					mLumRegionWidth;
	const S32					mLumRegionHeight;
	const S32					mLumMarkerSize;

	// Preview of the current color.
	const S32					mSwatchRegionLeft;
	const S32					mSwatchRegionTop;
	const S32					mSwatchRegionWidth;
	const S32					mSwatchRegionHeight;

	const S32					mPaletteCols;
	const S32					mPaletteRows;

	const S32					mPaletteRegionLeft;
	const S32					mPaletteRegionTop;
	const S32					mPaletteRegionWidth;
	const S32					mPaletteRegionHeight;

	// Are we actively tied to some output ?
	bool						mActive;

	// Set to true when we have been cancelled (used to avoid cancel callbacks
	// recursions). HB
	bool						mCancelled;

	// Enable/disable immediate updates
	bool						mCanApplyImmediately;

	bool						mMouseDownInLumRegion;
	bool						mMouseDownInHueRegion;
	bool						mMouseDownInSwatch;
};

#endif // LL_LLFLOATERCOLORPICKER_H
