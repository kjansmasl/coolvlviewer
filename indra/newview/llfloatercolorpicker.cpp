/** 
 * @file llfloatercolorpicker.cpp
 * @brief Generic system color picker
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright(c) 2004-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file("Source Code") is provided by Linden Lab
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

#include "llviewerprecompiledheaders.h"

#include <sstream>
#include <iomanip>

#include "llfloatercolorpicker.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lldraghandle.h"
#include "llgl.h"
#include "llimage.h"
#include "llimagegl.h"
#include "lllineeditor.h"
#include "llmousehandler.h"
#include "llrender.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llwindow.h"

#include "lltoolmgr.h"
#include "lltoolpipette.h"
#include "llviewercontrol.h"

constexpr F32 CONTEXT_CONE_IN_ALPHA = 0.f;
constexpr F32 CONTEXT_CONE_OUT_ALPHA = 1.f;
constexpr F32 CONTEXT_FADE_TIME = 0.08f;

LLFloaterColorPicker::LLFloaterColorPicker(LLColorSwatchCtrl* swatch,
										   bool show_apply_immediately)
:	LLFloater("color picker"),
	mComponents(3),
	mMouseDownInLumRegion(false),
	mMouseDownInHueRegion(false),
	mMouseDownInSwatch(false),
	// *TODO: Specify this in XML
	mRGBViewerImageLeft(140),
	mRGBViewerImageTop(356),
	mRGBViewerImageWidth(256),
	mRGBViewerImageHeight(256),
	mLumRegionLeft(mRGBViewerImageLeft + mRGBViewerImageWidth + 16),
	mLumRegionTop(mRGBViewerImageTop),
	mLumRegionWidth(16),
	mLumRegionHeight(mRGBViewerImageHeight),
	mLumMarkerSize(6),
	// *TODO: Specify this in XML
	mSwatchRegionLeft(12),
	mSwatchRegionTop(190),
	mSwatchRegionWidth(116),
	mSwatchRegionHeight(60),
	// *TODO: Specify this in XML
	mPaletteCols(16),
	mPaletteRows(2),
	mHighlightEntry(-1),
	mPaletteRegionLeft(11),
	mPaletteRegionTop(100 - 8),
	mPaletteRegionWidth(mLumRegionLeft + mLumRegionWidth - 10),
	mPaletteRegionHeight(40),
	mSwatch(swatch),
	mActive(true),
	mCancelled(false),
	mCanApplyImmediately(show_apply_immediately),
	mContextConeOpacity(0.f)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_color_picker.xml");
}

//virtual
LLFloaterColorPicker::~LLFloaterColorPicker()
{
	// Shut down pipette tool if active
	stopUsingPipette();

	mPalette.clear();
}

//virtual
bool LLFloaterColorPicker::postBuild()
{
	mCancelBtn = getChild<LLButton>("cancel_btn");
    mCancelBtn->setClickedCallback(onClickCancel);
    mCancelBtn->setCallbackUserData(this);

	mSelectBtn = getChild<LLButton>("select_btn");
    mSelectBtn->setClickedCallback(onClickSelect);
    mSelectBtn->setCallbackUserData(this);
	mSelectBtn->setFocus(true);

	mPipetteBtn = getChild<LLButton>("color_pipette");

	mPipetteBtn->setImages("eye_button_inactive.tga", "eye_button_active.tga");

	mPipetteBtn->setClickedCallback(onClickPipette);
	mPipetteBtn->setCallbackUserData(this);

	mApplyImmediateCheck = getChild<LLCheckBoxCtrl>("apply_immediate");
	mApplyImmediateCheck->set(gSavedSettings.getBool("ApplyColorImmediately"));
	mApplyImmediateCheck->setCommitCallback(onImmediateCheck);
	mApplyImmediateCheck->setCallbackUserData(this);

	childSetCommitCallback("rspin", onTextCommit, this);
	childSetCommitCallback("gspin", onTextCommit, this);
	childSetCommitCallback("bspin", onTextCommit, this);
	childSetCommitCallback("hspin", onTextCommit, this);
	childSetCommitCallback("sspin", onTextCommit, this);
	childSetCommitCallback("lspin", onTextCommit, this);

	// Create a RGB type area (not really RGB but it got R, G & B in it)
	LLPointer<LLImageRaw> raw = new LLImageRaw(mRGBViewerImageWidth,
											   mRGBViewerImageHeight,
											   mComponents);
	U8* bits = raw->getData();
	S32 linesize = mRGBViewerImageWidth * mComponents;
	for (S32 y = 0; y < mRGBViewerImageHeight; ++y)
	{
		for (S32 x = 0; x < linesize; x += mComponents)
		{
			F32 r, g, b;
			hslToRgb((F32)x / (F32)(linesize - 1),
					 (F32)y / (F32)(mRGBViewerImageHeight - 1),
					 0.5f, r, g, b);

			*(bits + x + y * linesize) = (U8)(r * 255.f);
			*(bits + x + y * linesize + 1) = (U8)(g * 255.f);
			*(bits + x + y * linesize + 2) = (U8)(b * 255.f);
		}
	}
	mRGBImage = LLViewerTextureManager::getLocalTexture((LLImageRaw*)raw,
														false);
	gGL.getTexUnit(0)->bind(mRGBImage);
	mRGBImage->setAddressMode(LLTexUnit::TAM_CLAMP);

	// Create palette
	for (S32 each = 0; each < mPaletteCols * mPaletteRows; ++each)
	{
		std::ostringstream codec;
		codec << "ColorPaletteEntry" << std::setfill('0') << std::setw(2)
			  << each + 1;
		mPalette.emplace_back(gSavedSettings.getColor4(codec.str().c_str()));
	}

	if (!mCanApplyImmediately)
	{
		mApplyImmediateCheck->setEnabled(false);
		mApplyImmediateCheck->set(false);
	}

	setVisible(false);

	return true;
}

void LLFloaterColorPicker::showUI()
{
	mCancelled = false;
	setVisible(true);
	setFocus(true);
	open();
}

void LLFloaterColorPicker::initUI(F32 r, F32 g, F32 b)
{
	// Start catching lose-focus events from entry widgets
	enableTextCallbacks(true);

	// Under some circumstances, we get rogue values that can be calmed by
	// clamping...
	r = llclamp(r, 0.f, 1.f);
	g = llclamp(g, 0.f, 1.f);
	b = llclamp(b, 0.f, 1.f);

	// Store initial value in case cancel or revert is selected
	setOrigRgb(r, g, b);

	// Starting point for current value to
	setCurRgb(r, g, b);

	// Update text entry fields
	updateTextEntry();
}

F32 LLFloaterColorPicker::hueToRgb(F32 val1, F32 val2, F32 hue)
{
	if (hue < 0.f)
	{
		hue += 1.f;
	}
	else if (hue > 1.f)
	{
		hue -= 1.f;
	}
	if (6.f * hue < 1.f)
	{
		return val1 + (val2 - val1) * 6.f * hue;
	}
	if (2.f * hue < 1.f)
	{
		return val2;
	}
	if (3.f * hue < 2.f)
	{
		return val1 + (val2 - val1) * (4.f - hue * 6.f);
	}
	return val1;
}

void LLFloaterColorPicker::hslToRgb(F32 h, F32 s, F32 l, F32& r, F32& g,
									F32& b)
{
	if (s < 0.00001f)
	{
		r = g = b = l;
	}
	else
	{
		F32 inter_val2;
		if (l < 0.5f)
		{
			inter_val2 = l * (1.f + s);
		}
		else
		{
			inter_val2 = l + s - s * l;
		}

		F32 inter_val1 = 2.f * l - inter_val2;

		r = hueToRgb(inter_val1, inter_val2, h + (1.f / 3.f));
		g = hueToRgb(inter_val1, inter_val2, h);
		b = hueToRgb(inter_val1, inter_val2, h - (1.f / 3.f));
	}
}

void LLFloaterColorPicker::setCurRgb(F32 r, F32 g, F32 b)
{
	// Save current RGB
	mCurR = r;
	mCurG = g;
	mCurB = b;

	// Update corresponding HSL values and
	LLColor3(r, g, b).calcHSL(&mCurH, &mCurS, &mCurL);

	// Color changed so update text fields(fixes SL-16968)
    // *HACK: turn off the callback wilst we update the text or we recurse
	// ourselves into oblivion. CP: this was required when I first wrote the
	// code but this may not be necessary anymore; leaving it there just in
	// case
    enableTextCallbacks(false);
    updateTextEntry();
    enableTextCallbacks(true);
}

void LLFloaterColorPicker::setCurHsl(F32 h, F32 s, F32 l)
{
	// Save current HSL
	mCurH = h;
	mCurS = s;
	mCurL = l;

	// Update corresponding RGB values and
	hslToRgb(h, s, l, mCurR, mCurG, mCurB);
}

void LLFloaterColorPicker::onClickCancel(void* data)
{
	LLFloaterColorPicker* self = (LLFloaterColorPicker*)data;
	if (self)
	{
		self->cancelSelection();
		self->close();
	}
}

void LLFloaterColorPicker::onClickSelect(void* data)
{
	LLFloaterColorPicker* self = (LLFloaterColorPicker*)data;
	if (self)
	{
		self->mCancelled = false;
		// Apply to selection
		LLColorSwatchCtrl::onColorChanged(self->mSwatch,
										  LLColorSwatchCtrl::COLOR_SELECT);
		self->close();
	}
}

void LLFloaterColorPicker::onClickPipette(void* data)
{
	LLFloaterColorPicker* self = (LLFloaterColorPicker*)data;
	if (self)
	{
		if (self->mPipetteBtn->getToggleState())
		{
			gToolMgr.clearTransientTool();
		}
		else
		{
			gToolPipette.setSelectCallback(onColorSelect, self);
			gToolMgr.setTransientTool(&gToolPipette);
		}
	}
}

void LLFloaterColorPicker::onTextCommit(LLUICtrl* ctrl, void* data)
{
	LLFloaterColorPicker* self = (LLFloaterColorPicker*)data;
	if (self)
	{
		self->onTextEntryChanged(ctrl);
	}
}

void LLFloaterColorPicker::onImmediateCheck(LLUICtrl* ctrl, void* data)
{
	LLFloaterColorPicker* self = (LLFloaterColorPicker*)data;
	if (self)
	{
		bool apply = self->mApplyImmediateCheck->get();
		gSavedSettings.setBool("ApplyColorImmediately", apply);
		if (apply)
		{
			self->mCancelled = false;
			LLColorSwatchCtrl::onColorChanged(self->mSwatch,
											  LLColorSwatchCtrl::COLOR_CHANGE);
		}
	}
}

void LLFloaterColorPicker::onColorSelect(const LLTextureEntry& te, void* data)
{
	LLFloaterColorPicker* self = (LLFloaterColorPicker*)data;
	if (self)
	{
		self->setCurRgb(te.getColor().mV[VRED], te.getColor().mV[VGREEN],
						te.getColor().mV[VBLUE]);
		if (self->mApplyImmediateCheck->get())
		{
			self->mCancelled = false;
			LLColorSwatchCtrl::onColorChanged(self->mSwatch,
											  LLColorSwatchCtrl::COLOR_CHANGE);
		}
	}
}

//virtual
void LLFloaterColorPicker::onMouseCaptureLost()
{
	setMouseDownInHueRegion(false);
	setMouseDownInLumRegion(false);
}

//virtual
void LLFloaterColorPicker::draw()
{
	LLRect swatch_rect;
	mSwatch->localRectToOtherView(mSwatch->getLocalRect(), &swatch_rect, this);
	// Draw context cone connecting color picker with color swatch in parent
	// floater
	LLRect local_rect = getLocalRect();
	if (gFocusMgr.childHasKeyboardFocus(this) && mSwatch->isInVisibleChain() &&
		mContextConeOpacity > 0.001f)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		LLGLEnable(GL_CULL_FACE);
		gGL.begin(LLRender::TRIANGLE_STRIP);
		gGL.color4f(0.f, 0.f, 0.f,
					CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
		gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
		gGL.color4f(0.f, 0.f, 0.f,
					CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
		gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mTop);
		gGL.color4f(0.f, 0.f, 0.f,
					CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
		gGL.vertex2i(local_rect.mRight, local_rect.mTop);
		gGL.color4f(0.f, 0.f, 0.f,
					CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
		gGL.vertex2i(swatch_rect.mRight, swatch_rect.mTop);
		gGL.color4f(0.f, 0.f, 0.f,
					CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
		gGL.vertex2i(local_rect.mRight, local_rect.mBottom);
		gGL.color4f(0.f, 0.f, 0.f,
					CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
		gGL.vertex2i(swatch_rect.mRight, swatch_rect.mBottom);
		gGL.color4f(0.f, 0.f, 0.f,
					CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
		gGL.vertex2i(local_rect.mLeft, local_rect.mBottom);
		gGL.color4f(0.f, 0.f, 0.f,
					CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
		gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mBottom);
		gGL.color4f(0.f, 0.f, 0.f,
					CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
		gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
		gGL.color4f(0.f, 0.f, 0.f,
					CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
		gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mTop);
		gGL.end();
	}

	F32 critical_damp = LLCriticalDamp::getInterpolant(CONTEXT_FADE_TIME);
	if (gFocusMgr.childHasMouseCapture(getDragHandle()))
	{
		static LLCachedControl<F32> picker_context_opacity(gSavedSettings,
														   "PickerContextOpacity");
		mContextConeOpacity = lerp(mContextConeOpacity,
								   (F32)picker_context_opacity, critical_damp);
	}
	else
	{
		mContextConeOpacity = lerp(mContextConeOpacity, 0.f, critical_damp);
	}

	mPipetteBtn->setToggleState(gToolMgr.isCurrentTool(&gToolPipette));
	mApplyImmediateCheck->setEnabled(mActive && mCanApplyImmediately);
	mSelectBtn->setEnabled(mActive);

	// Base floater stuff
	LLFloater::draw();

	// Draw image for RGB area(not really RGB but you'll see what I mean...
	gl_draw_image(mRGBViewerImageLeft,
				  mRGBViewerImageTop - mRGBViewerImageHeight, mRGBImage,
				  LLColor4::white);

	// Update 'cursor' into RGB Section
	S32 x = (S32)((F32)mRGBViewerImageWidth * mCurH) - 8;
	S32 y = (S32)((F32)mRGBViewerImageHeight * mCurS) - 8;
	gl_line_2d(mRGBViewerImageLeft + x,
			   mRGBViewerImageTop - mRGBViewerImageHeight + y + 8,
			   mRGBViewerImageLeft + x + 16,
			   mRGBViewerImageTop - mRGBViewerImageHeight + y + 8,
			   LLColor4::black);

	gl_line_2d(mRGBViewerImageLeft + x + 8,
			   mRGBViewerImageTop - mRGBViewerImageHeight + y,
			   mRGBViewerImageLeft + x + 8,
			   mRGBViewerImageTop - mRGBViewerImageHeight + y + 16,
			   LLColor4::black);

	// Create rgb area outline
	gl_rect_2d(mRGBViewerImageLeft, mRGBViewerImageTop - mRGBViewerImageHeight,
			   mRGBViewerImageLeft + mRGBViewerImageWidth, mRGBViewerImageTop,
			   LLColor4::black, false);

	// Draw luminance slider
	for (S32 y = 0; y < mLumRegionHeight; ++y)
	{
		F32 r, g, b;
		hslToRgb(mCurH, mCurS,(F32)y / (F32)mLumRegionHeight, r, g, b);

		gl_rect_2d(mLumRegionLeft, mLumRegionTop - mLumRegionHeight + y,
				   mLumRegionLeft + mLumRegionWidth, 
				   mLumRegionTop - mLumRegionHeight + y - 1, 
				   LLColor4(r, g, b, 1.f));
	}


	// Draw luninance marker
	S32 start_x = mLumRegionLeft + mLumRegionWidth;
	S32 start_y = mLumRegionTop - mLumRegionHeight +
				 (S32)(mLumRegionHeight * mCurL);
	gl_triangle_2d(start_x, start_y,
				   start_x + mLumMarkerSize, start_y - mLumMarkerSize,
				   start_x + mLumMarkerSize, start_y + mLumMarkerSize,
				   LLColor4::black, true);

	// Draw luminance slider outline
	gl_rect_2d(mLumRegionLeft, mLumRegionTop - mLumRegionHeight,
			   mLumRegionLeft + mLumRegionWidth, mLumRegionTop,
			   LLColor4::black, false);

	// Draw selected color swatch
	gl_rect_2d(mSwatchRegionLeft, mSwatchRegionTop - mSwatchRegionHeight,
			   mSwatchRegionLeft + mSwatchRegionWidth, mSwatchRegionTop,
			   LLColor4(mCurR, mCurG, mCurB, 1.0f), true);

	// Draw selected color swatch outline
	gl_rect_2d(mSwatchRegionLeft, mSwatchRegionTop - mSwatchRegionHeight,
			   mSwatchRegionLeft + mSwatchRegionWidth, mSwatchRegionTop,
			   LLColor4::black, false);

	// Color palette code is a little more involved so break it out into its
	// own method
	drawPalette();
}

// Finds a complimentary color to the one passed in that can be used to
// highlight
const LLColor4& LLFloaterColorPicker::getComplementaryColor(const LLColor4& bg_col)
{
	// Going to base calculation on luminance
	F32 h, s, l;
	bg_col.calcHSL(&h, &s, &l);
	// Fairly simple heuristic for now...
	return l < 0.005f ? LLColor4::white : LLColor4::black;
}

// Draws the color palette
void LLFloaterColorPicker::drawPalette()
{
	S32 cur_entry = 0;
	S32 palette_size = mPalette.size();
	for (S32 y = 0; y < mPaletteRows && cur_entry < palette_size; ++y)
	{
		for (S32 x = 0; x < mPaletteCols && cur_entry < palette_size; ++x)
		{
			// Calculate position
			S32 x1 = mPaletteRegionLeft + (mPaletteRegionWidth * x) /
					 mPaletteCols;
			S32 y1 = mPaletteRegionTop - (mPaletteRegionHeight * y) /
					 mPaletteRows;
			S32 x2 = mPaletteRegionLeft + (mPaletteRegionWidth * (x + 1)) /
					 mPaletteCols;
			S32 y2 = mPaletteRegionTop - (mPaletteRegionHeight * (y + 1)) /
					 mPaletteRows;

			// Draw palette entry color
			gl_rect_2d(x1 + 2, y1 - 2, x2 - 2, y2 + 2, mPalette[cur_entry++],
					   true);
			gl_rect_2d(x1 + 1, y1 - 1, x2 - 1, y2 + 1, LLColor4::black, false);
		}
	}

	// If there is something to highlight(mouse down in swatch & hovering over
	// palette)
	if (mHighlightEntry >= 0)
	{
		// Extract row/column from palette index
		S32 col = mHighlightEntry % mPaletteCols;
		S32 row = mHighlightEntry / mPaletteCols;

		// Calculate position of this entry
		S32 x1 = mPaletteRegionLeft +
				 (mPaletteRegionWidth * col) / mPaletteCols;
		S32 y1 = mPaletteRegionTop -
				 (mPaletteRegionHeight * row) / mPaletteRows;
		S32 x2 = mPaletteRegionLeft +
				 (mPaletteRegionWidth * (col + 1)) / mPaletteCols;
		S32 y2 = mPaletteRegionTop -
				 (mPaletteRegionHeight * (row + 1)) / mPaletteRows;

		// Center position of entry
		S32 x0 = x1 + (x2 - x1) / 2;
		S32 y0 = y1 - (y1 - y2) / 2;

		// Find a color that works well as a highlight color
		LLColor4 hl_col(getComplementaryColor(mPalette[mHighlightEntry]));

		// Mark a cross for entry that is being hovered
		gl_line_2d(x0 - 4, y0 - 4, x0 + 4, y0 + 4, hl_col);
		gl_line_2d(x0 + 4, y0 - 4, x0 - 4, y0 + 4, hl_col);
	}
}

// Updates text entry values for RGB/HSL (cannot be done in draw() since this
// overwrites input
void LLFloaterColorPicker::updateTextEntry()
{
	// Set values in spinners
	childSetValue("rspin", mCurR * 255.f);
	childSetValue("gspin", mCurG * 255.f);
	childSetValue("bspin", mCurB * 255.f);
	childSetValue("hspin", mCurH * 360.f);
	childSetValue("sspin", mCurS * 100.f);
	childSetValue("lspin", mCurL * 100.f);
}

// Turns on or off text entry commit call backs
void LLFloaterColorPicker::enableTextCallbacks(bool stateIn)
{
	if (stateIn)
	{
		childSetCommitCallback("rspin", onTextCommit, this);
		childSetCommitCallback("gspin", onTextCommit, this);
		childSetCommitCallback("bspin", onTextCommit, this);
		childSetCommitCallback("hspin", onTextCommit, this);
		childSetCommitCallback("sspin", onTextCommit, this);
		childSetCommitCallback("lspin", onTextCommit, this);
	}
	else
	{
		childSetCommitCallback("rspin", 0, this);
		childSetCommitCallback("gspin", 0, this);
		childSetCommitCallback("bspin", 0, this);
		childSetCommitCallback("hspin", 0, this);
		childSetCommitCallback("sspin", 0, this);
		childSetCommitCallback("lspin", 0, this);
	}
}

void LLFloaterColorPicker::onTextEntryChanged(LLUICtrl* ctrl)
{
	std::string name = ctrl->getName();
	if (name == "rspin" || name == "gspin" || name == "bspin")
	{
		// A value in RGB boxes changed
		F32 r = mCurR;
		F32 g = mCurG;
		F32 b = mCurB;
		// Update component value with new value from text
		if (name == "rspin")
		{
			r = (F32)ctrl->getValue().asReal() / 255.f;
		}
		else if (name == "gspin")
		{
			g = (F32)ctrl->getValue().asReal() / 255.f;
		}
		else if (name == "bspin")
		{
			b = (F32)ctrl->getValue().asReal() / 255.f;
		}
		// Update current RGB and sync current HSL
		setCurRgb(r, g, b);
	}
	else if (name == "hspin" || name == "sspin" || name == "lspin")
	{
		// A value in HSL boxes changed
		F32 h = mCurH;
		F32 s = mCurS;
		F32 l = mCurL;
		if (name == "hspin")
		{
			h = (F32)ctrl->getValue().asReal() / 360.f;
		}
		else if (name == "sspin")
		{
			s = (F32)ctrl->getValue().asReal() * 0.01f;
		}
		else if (name == "lspin")
		{
			l = (F32)ctrl->getValue().asReal() * 0.01f;
		}
		// Update current HSL and sync current RGB
		setCurHsl(h, s, l);
	}
	else
	{
		llwarns << "Unknown control name: " << name << llendl;
		return;
	}

	// *HACK: turn off the call back wilst we update the text or we recurse
	// ourselves into oblivion
	enableTextCallbacks(false);
	updateTextEntry();
	enableTextCallbacks(true);

	if (mApplyImmediateCheck->get())
	{
		mCancelled = false;
		LLColorSwatchCtrl::onColorChanged(mSwatch,
										  LLColorSwatchCtrl::COLOR_CHANGE);
	}
}

bool LLFloaterColorPicker::updateRgbHslFromPoint(S32 x, S32 y)
{
	if (x >= mRGBViewerImageLeft &&
		x <= mRGBViewerImageLeft + mRGBViewerImageWidth &&
		y <= mRGBViewerImageTop &&
		y >= mRGBViewerImageTop - mRGBViewerImageHeight)
	{
		if (mCurL >= 1.f)
		{
			// Give the user a minimum of feedback on the hue, when adjustment
			// is started from pure white... The rationale is that if they are
			// trying to adjust the hue, it is obviously because they do not
			// want a pure white. A luminance of 0.99 is "99" (for a maximum of
			// 100) in the corresponding spinner. HB
			mCurL = 0.99f;
		}
		// Update HSL (and therefore RGB) based on new H & S and current L
		setCurHsl((F32)(x - mRGBViewerImageLeft) / (F32)mRGBViewerImageWidth,
				  (F32)(y - mRGBViewerImageTop + mRGBViewerImageHeight) /
				  (F32)mRGBViewerImageHeight, mCurL);
		// Indicate a value changed
		return true;
	}
	else if (x >= mLumRegionLeft && y <= mLumRegionTop &&
			 x <= mLumRegionLeft + mLumRegionWidth &&
			 y >= mLumRegionTop - mLumRegionHeight)
	{

		// Update HSL(and therefore RGB) based on current HS and new L
		setCurHsl(mCurH, mCurS,
				  (F32)(y - mRGBViewerImageTop + mRGBViewerImageHeight) /
				  (F32)mRGBViewerImageHeight);
		// Indicate a value changed
		return true;
	}

	return false;
}

//virtual
bool LLFloaterColorPicker::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Make it the frontmost
	if (gFloaterViewp)
	{
		gFloaterViewp->bringToFront(this);
	}

	// Rectangle containing RGB area
	LLRect rgb_rect(mRGBViewerImageLeft, mRGBViewerImageTop,
					mRGBViewerImageLeft + mRGBViewerImageWidth,
					mRGBViewerImageTop - mRGBViewerImageHeight);
	if (rgb_rect.pointInRect(x, y))
	{
		gFocusMgr.setMouseCapture(this);
		// Mouse button down
		setMouseDownInHueRegion(true);
		// Update all values based on initial click
		updateRgbHslFromPoint(x, y);
		// Required: do not drag floater here.
		return true;
	}

	// Rectangle luminosity RGB area
	LLRect lum_rect(mLumRegionLeft, mLumRegionTop,
					mLumRegionLeft + mLumRegionWidth + mLumMarkerSize,
					mLumRegionTop - mLumRegionHeight);
	if (lum_rect.pointInRect(x, y))
	{
		gFocusMgr.setMouseCapture(this);
		// Mouse button down
		setMouseDownInLumRegion(true);
		// Required: do not drag floater here.
		return true;
	}

	// Rectangle containing swatch area
	LLRect swatch_rect(mSwatchRegionLeft, mSwatchRegionTop,
					   mSwatchRegionLeft + mSwatchRegionWidth,
					   mSwatchRegionTop - mSwatchRegionHeight);
	if (swatch_rect.pointInRect(x, y))
	{
		setMouseDownInSwatch(true);
		// Required: do not drag floater here.
		return true;
	}
	setMouseDownInSwatch(false);

	// Rectangle containing palette area
	LLRect rect(mPaletteRegionLeft, mPaletteRegionTop,
				mPaletteRegionLeft + mPaletteRegionWidth,
				mPaletteRegionTop - mPaletteRegionHeight);
	if (rect.pointInRect(x, y))
	{
		// Release keyboard focus so we can change text values
		if (gFocusMgr.childHasKeyboardFocus(this))
		{
			mSelectBtn->setFocus(true);
		}

		// Calculate which palette index we selected
		S32 c = ((x - mPaletteRegionLeft) * mPaletteCols) /
				mPaletteRegionWidth;
		S32 r = (y - mPaletteRegionTop + mPaletteRegionHeight) *
				mPaletteRows / mPaletteRegionHeight;

		U32 index = (mPaletteRows - r - 1) * mPaletteCols + c;
		if (index <= mPalette.size())
		{
			const LLColor4& selected = mPalette[index];
			setCurRgb(selected[0], selected[1], selected[2]);

			if (mApplyImmediateCheck->get())
			{
				mCancelled = false;
				LLColorSwatchCtrl::onColorChanged(mSwatch,
												  LLColorSwatchCtrl::COLOR_CHANGE);
			}

			// *HACK: turn off the call back wilst we update the text or we
			// recurse ourselves into oblivion
			enableTextCallbacks(false);
			updateTextEntry();
			enableTextCallbacks(true);
		}

		return true;
	}

	// Dispatch to base class for the rest of things
	return LLFloater::handleMouseDown(x, y, mask);
}

//virtual
bool LLFloaterColorPicker::handleHover(S32 x, S32 y, MASK mask)
{
	// If we are the front most window
	if (isFrontmost())
	{
		// Mouse was pressed within region
		if (mMouseDownInHueRegion || mMouseDownInLumRegion)
		{
			S32 clamped_x, clamped_y;
			if (mMouseDownInHueRegion)
			{
				clamped_x = llclamp(x, mRGBViewerImageLeft,
									mRGBViewerImageLeft + mRGBViewerImageWidth);
				clamped_y = llclamp(y, mRGBViewerImageTop - mRGBViewerImageHeight,
									mRGBViewerImageTop);
			}
			else
			{
				clamped_x = llclamp(x, mLumRegionLeft,
									mLumRegionLeft + mLumRegionWidth);
				clamped_y = llclamp(y, mLumRegionTop - mLumRegionHeight,
									mLumRegionTop);
			}

			// Update the stored RGB/HSL values using the mouse position.
			// Returns true if RGB was updated
			if (updateRgbHslFromPoint(clamped_x, clamped_y))
			{
				// Update text entry fields
				updateTextEntry();

#if 0			// RN: apparently changing color when dragging generates too
				// much traffic and results in sporadic updates
				if (mApplyImmediateCheck->get())
				{
					// Commit changed color to swatch subject
					mCancelled = false;
					LLColorSwatchCtrl::onColorChanged(mSwatch);
				}
#endif
			}
		}

		mHighlightEntry = -1;

		if (mMouseDownInSwatch)
		{
			gWindowp->setCursor(UI_CURSOR_ARROWDRAG);

			// If cursor if over a palette entry
			LLRect rect(mPaletteRegionLeft, mPaletteRegionTop,
						mPaletteRegionLeft + mPaletteRegionWidth,
						mPaletteRegionTop - mPaletteRegionHeight);
			if (rect.pointInRect(x, y))
			{
				// Find row/column in palette
				S32 x_delta = ((x - mPaletteRegionLeft) * mPaletteCols) /
							  mPaletteRegionWidth;
				S32 y_delta = ((mPaletteRegionTop - y - 1) * mPaletteRows) /
							  mPaletteRegionHeight;

				// Calculate the entry 0...n-1 to highlight and set variable to
				// next draw() picks it up
				mHighlightEntry = x_delta + y_delta * mPaletteCols;
			}

			return true;
		}
	}

	// Dispatch to base class for the rest of things
	return LLFloater::handleHover(x, y, mask);
}

//virtual
void LLFloaterColorPicker::onClose(bool app_quitting)
{
	// RN: this is consistent with texture picker in that closing the window
	// leaves the current selection to change this to "close to cancel",
	// uncomment the following line
	LLFloater::onClose(app_quitting);
}

// Reverts state once mouse button is released
//virtual
bool LLFloaterColorPicker::handleMouseUp(S32 x, S32 y, MASK mask)
{
	gWindowp->setCursor(UI_CURSOR_ARROW);

	if (mMouseDownInHueRegion || mMouseDownInLumRegion)
	{
		if (mApplyImmediateCheck->get())
		{
			mCancelled = false;
			LLColorSwatchCtrl::onColorChanged(mSwatch,
											  LLColorSwatchCtrl::COLOR_CHANGE);
		}
	}

	// Rectangle containing palette area
	LLRect rect(mPaletteRegionLeft, mPaletteRegionTop,
				mPaletteRegionLeft + mPaletteRegionWidth,
				mPaletteRegionTop - mPaletteRegionHeight);
	if (rect.pointInRect(x, y))
	{
		if (mMouseDownInSwatch)
		{
			S32 palette_size = mPalette.size();
			S32 cur_entry = 0;
			for (S32 row = 0; row < mPaletteRows && cur_entry < palette_size;
				 ++row)
			{
				for (S32 column = 0;
					 column < mPaletteCols && cur_entry < palette_size;
					 ++column)
				{
					S32 left = mPaletteRegionLeft +
							   (mPaletteRegionWidth * column) / mPaletteCols;
					S32 top = mPaletteRegionTop -
							  (mPaletteRegionHeight * row) / mPaletteRows;
					S32 right = mPaletteRegionLeft +
								(mPaletteRegionWidth * (column + 1)) /
								mPaletteCols;
					S32 bottom = mPaletteRegionTop -
								 (mPaletteRegionHeight * (row + 1)) /
								 mPaletteRows;

					// Rect is flipped vertically when testing here
					LLRect dropRect(left, top, right, bottom);

					if (dropRect.pointInRect(x, y))
					{
						mPalette[cur_entry].set(mCurR, mCurG, mCurB, 1.f);
						std::ostringstream codec;
						codec << "ColorPaletteEntry" << std::setfill('0')
							  << std::setw(2) << cur_entry + 1;
						const std::string s(codec.str());
						gSavedSettings.setColor4(s.c_str(),
												 mPalette[cur_entry]);
					}

					++cur_entry;
				}
			}
		}
	}

	// Mouse button not down anymore
	setMouseDownInHueRegion(false);
	setMouseDownInLumRegion(false);

	// Mouse button not down in color swatch anymore
	mMouseDownInSwatch = false;

	if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
	}

	// Dispatch to base class for the rest of things
	return LLFloater::handleMouseUp(x, y, mask);
}

// Cancels current color selection, reverts to original and closes picker
void LLFloaterColorPicker::cancelSelection()
{
	// Avoid potential infinite loop since LLColorSwatchCtrl::onColorChanged()
	// could re-trigger a cancelSelection() call via its callback. HB
	if (mCancelled)
	{
		return;
	}
	mCancelled = true;

	// Restore the previous color selection
	setCurRgb(mOrigR, mOrigG, mOrigB);
	
	// We are going away and when we do and the entry widgets lose focus, they
	// do bad things so turn them off
	enableTextCallbacks(false);

	// Update in world item with original color via current swatch
	LLColorSwatchCtrl::onColorChanged(mSwatch, LLColorSwatchCtrl::COLOR_CANCEL);

	// Hide picker dialog
	setVisible(false);
}

void LLFloaterColorPicker::setMouseDownInHueRegion(bool mouse_down_in_region)
{
	mMouseDownInHueRegion = mouse_down_in_region;
	if (mouse_down_in_region && gFocusMgr.childHasKeyboardFocus(this))
	{
		// Get focus out of spinners so that they can update freely
		mSelectBtn->setFocus(true);
	}
}

void LLFloaterColorPicker::setMouseDownInLumRegion(bool mouse_down_in_region)
{
	mMouseDownInLumRegion = mouse_down_in_region;
	if (mouse_down_in_region && gFocusMgr.childHasKeyboardFocus(this))
	{
		// Get focus out of spinners so that they can update freely
		mSelectBtn->setFocus(true);
	}
}

void LLFloaterColorPicker::setMouseDownInSwatch(bool mouse_down_in_swatch)
{
	mMouseDownInSwatch = mouse_down_in_swatch;
	if (mouse_down_in_swatch && gFocusMgr.childHasKeyboardFocus(this))
	{
		// Get focus out of spinners so that they can update freely
		mSelectBtn->setFocus(true);
	}
}

void LLFloaterColorPicker::setActive(bool active) 
{ 
	// Shut down pipette tool if active
	if (!active && mPipetteBtn->getToggleState())
	{
		stopUsingPipette();
	}
	mActive = active; 
}

void LLFloaterColorPicker::stopUsingPipette()
{
	if (gToolMgr.isCurrentTool(&gToolPipette))
	{
		gToolMgr.clearTransientTool();
	}
}
