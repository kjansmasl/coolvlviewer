/**
 * @file llbutton.cpp
 * @brief LLButton base class
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

#include "llbutton.h"

#include "llcriticaldamp.h"
#include "llhtmlhelp.h"
#include "llkeyboard.h"
#include "llrender.h"
#include "llwindow.h"

static const std::string LL_BUTTON_TAG = "button";
static LLRegisterWidget<LLButton> r01(LL_BUTTON_TAG);

// Globals loaded from settings.xml
S32	LLBUTTON_ORIG_H_PAD	= 6; // Pre-zoomable UI
S32	gButtonHPad		= 10;
S32	gButtonVPad		= 1;
S32 gBtnHeightSmall	= 16;
S32 gBtnHeight		= 20;
S32 BORDER_SIZE		= 1;

LLButton::LLButton(const std::string& name, const LLRect& rect,
				   const char* control_name,
				   void (*click_callback)(void*), void* callback_data)
:	LLUICtrl(name, rect, true, NULL, NULL),
	mClickedCallback(click_callback),
	mMouseHoverCallback(NULL),
	mMouseDownCallback(NULL),
	mMouseUpCallback(NULL),
	mHeldDownCallback(NULL),
	mGLFont(NULL),
	mMouseDownFrame(0),
	mHeldDownDelay(0.5f),		// seconds until held-down callback is called
	mHeldDownFrameDelay(0),
	mImageUnselected(NULL),
	mImageSelected(NULL),
	mImageHoverSelected(NULL),
	mImageHoverUnselected(NULL),
	mImageDisabled(NULL),
	mImageDisabledSelected(NULL),
	mToggleState(false),
	mIsToggle(false),
	mScaleImage(true),
	mDropShadowedText(true),
	mBorderEnabled(false),
	mFlashing(false),
	mHAlign(LLFontGL::HCENTER),
	mLeftHPad(gButtonHPad),
	mRightHPad(gButtonHPad),
	mHoverGlowStrength(0.15f),
	mCurGlowStrength(0.f),
	mNeedsHighlight(false),
	mCommitOnReturn(true),
	mImagep(NULL)
{
	mUnselectedLabel = name;
	mSelectedLabel = name;

	setImageUnselected("button_enabled_32x128.tga");
	setImageSelected("button_enabled_selected_32x128.tga");
	setImageDisabled("button_disabled_32x128.tga");
	setImageDisabledSelected("button_disabled_32x128.tga");

	mImageColor = LLUI::sButtonImageColor;
	mDisabledImageColor = LLUI::sButtonImageColor;

	init(click_callback, callback_data, NULL, control_name);
}

LLButton::LLButton(const std::string& name, const LLRect& rect,
				   const std::string& unselected_image_name,
				   const std::string& selected_image_name,
				   const char* control_name,
				   void (*click_callback)(void*), void* callback_data,
				   const LLFontGL* font,
				   const std::string& unselected_label,
				   const std::string& selected_label)
:	LLUICtrl(name, rect, true, NULL, NULL),
	mClickedCallback(click_callback),
	mMouseHoverCallback(NULL),
	mMouseDownCallback(NULL),
	mMouseUpCallback(NULL),
	mHeldDownCallback(NULL),
	mGLFont(NULL),
	mMouseDownFrame(0),
	mHeldDownDelay(0.5f),		// Seconds until held-down callback is called
	mHeldDownFrameDelay(0),
	mImageUnselected(NULL),
	mImageSelected(NULL),
	mImageHoverSelected(NULL),
	mImageHoverUnselected(NULL),
	mImageDisabled(NULL),
	mImageDisabledSelected(NULL),
	mToggleState(false),
	mIsToggle(false),
	mScaleImage(true),
	mDropShadowedText(true),
	mBorderEnabled(false),
	mFlashing(false),
	mHAlign(LLFontGL::HCENTER),
	mLeftHPad(gButtonHPad),
	mRightHPad(gButtonHPad),
	mHoverGlowStrength(0.25f),
	mCurGlowStrength(0.f),
	mNeedsHighlight(false),
	mCommitOnReturn(true),
	mImagep(NULL)
{
	mUnselectedLabel = unselected_label;
	mSelectedLabel = selected_label;

	// By default, disabled color is same as enabled
	mImageColor = LLUI::sButtonImageColor;
	mDisabledImageColor = LLUI::sButtonImageColor;

	if (!unselected_image_name.empty())
	{
		// User-specified image; do not use fixed borders unless requested
		setImageUnselected(unselected_image_name);
		setImageDisabled(unselected_image_name);

		mDisabledImageColor.mV[VALPHA] = 0.5f;
		mScaleImage = false;
	}
	else
	{
		setImageUnselected("button_enabled_32x128.tga");
		setImageDisabled("button_disabled_32x128.tga");
	}

	if (!selected_image_name.empty())
	{
		// User-specified image; do not use fixed borders unless requested
		setImageSelected(selected_image_name);
		setImageDisabledSelected(selected_image_name);

		mDisabledImageColor.mV[VALPHA] = 0.5f;
		mScaleImage = false;
	}
	else
	{
		setImageSelected("button_enabled_selected_32x128.tga");
		setImageDisabledSelected("button_disabled_32x128.tga");
	}

	init(click_callback, callback_data, font, control_name);
}

void LLButton::init(void (*click_callback)(void*), void* callback_data,
					const LLFontGL* font, const char* control_name)
{
	mGLFont = font ? font : LLFontGL::getFontSansSerif();

	// *HACK: to make sure there is space for at least one character
	if (getRect().getWidth() - mRightHPad - mLeftHPad < mGLFont->getWidth(" "))
	{
		// Use old defaults
		mLeftHPad = LLBUTTON_ORIG_H_PAD;
		mRightHPad = LLBUTTON_ORIG_H_PAD;
	}

	mCallbackUserData = callback_data;
	mMouseDownTimer.stop();

	setControlName(control_name, NULL);

	mUnselectedLabelColor = LLUI::sButtonLabelColor;
	mSelectedLabelColor = LLUI::sButtonLabelSelectedColor;
	mDisabledLabelColor = LLUI::sButtonLabelDisabledColor;
	mDisabledSelectedLabelColor = LLUI::sButtonLabelSelectedDisabledColor;
	mFlashBgColor = LLUI::sButtonFlashBgColor;

	mImageOverlayAlignment = LLFontGL::HCENTER;
	mImageOverlayColor = LLColor4::white;
}

LLButton::~LLButton()
{
 	if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
	}
}

// *HACK: committing a button is the same as instantly clicking it.
//virtual
void LLButton::onCommit()
{
	// WARNING: Sometimes clicking a button destroys the floater or panel
	// containing it. Therefore we need to call mClickedCallback LAST,
	// otherwise this becomes deleted memory.
	LLUICtrl::onCommit();

	if (mMouseDownCallback)
	{
		(*mMouseDownCallback)(mCallbackUserData);
	}

	if (mMouseUpCallback)
	{
		(*mMouseUpCallback)(mCallbackUserData);
	}

	if (getSoundFlags() & MOUSE_DOWN)
	{
		make_ui_sound("UISndClick");
	}

	if (getSoundFlags() & MOUSE_UP)
	{
		make_ui_sound("UISndClickRelease");
	}

	if (mIsToggle)
	{
		toggleState();
	}

	// Do this last, as it can result in destroying this button
	if (mCommitCallback)
	{
		(*mCommitCallback)(this, mCallbackUserData);
	}
	else if (mClickedCallback)
	{
		(*mClickedCallback)(mCallbackUserData);
	}
}

bool LLButton::handleUnicodeCharHere(llwchar uni_char)
{
	bool handled = false;
	if (uni_char == ' ' && gKeyboardp && !gKeyboardp->getKeyRepeated(' '))
	{
		if (mIsToggle)
		{
			toggleState();
		}

		handled = true;

		if (mCommitCallback)
		{
			(*mCommitCallback)(this, mCallbackUserData);
		}
		else if (mClickedCallback)
		{
			(*mClickedCallback)(mCallbackUserData);
		}
	}
	return handled;
}

bool LLButton::handleKeyHere(KEY key, MASK mask)
{
	bool handled = false;
	if (mCommitOnReturn && KEY_RETURN == key && mask == MASK_NONE &&
		gKeyboardp && !gKeyboardp->getKeyRepeated(key))
	{
		if (mIsToggle)
		{
			toggleState();
		}

		handled = true;

		if (mCommitCallback)
		{
			(*mCommitCallback)(this, mCallbackUserData);
		}
		else if (mClickedCallback)
		{
			(*mClickedCallback)(mCallbackUserData);
		}
	}
	return handled;
}

bool LLButton::handleMouseDown(S32, S32, MASK)
{
	// Route future Mouse messages here preemptively (release on mouse up).
	gFocusMgr.setMouseCapture(this);

	if (hasTabStop() && !getIsChrome())
	{
		setFocus(true);
	}

	if (mMouseDownCallback)
	{
		(*mMouseDownCallback)(mCallbackUserData);
	}

	mMouseDownTimer.start();
	mMouseDownFrame = (S32)LLFrameTimer::getFrameCount();

	if (getSoundFlags() & MOUSE_DOWN)
	{
		make_ui_sound("UISndClick");
	}

	return true;
}

bool LLButton::handleMouseUp(S32 x, S32 y, MASK)
{
	// We only handle the click if the click both started and ended within us
	if (hasMouseCapture())
	{
		// Always release the mouse
		gFocusMgr.setMouseCapture(NULL);

		// Regardless of where mouseup occurs, handle callback
		if (mMouseUpCallback)
		{
			(*mMouseUpCallback)(mCallbackUserData);
		}

		mMouseDownTimer.stop();
		mMouseDownTimer.reset();

		// DO THIS AT THE VERY END to allow the button to be destroyed as a
		// result of being clicked.
		// If mouse-up in the widget, it has been clicked
		if (pointInView(x, y))
		{
			if (getSoundFlags() & MOUSE_UP)
			{
				make_ui_sound("UISndClickRelease");
			}

			if (mIsToggle)
			{
				toggleState();
			}

			if (mCommitCallback)
			{
				(*mCommitCallback)(this, mCallbackUserData);
			}
			else if (mClickedCallback)
			{
				(*mClickedCallback)(mCallbackUserData);
			}
		}
	}

	return true;
}

bool LLButton::handleHover(S32, S32, MASK)
{
	LLMouseHandler* other_captor = gFocusMgr.getMouseCapture();
	mNeedsHighlight = other_captor == NULL || other_captor == this ||
					  // This following bit is to support modal dialogs
					  (other_captor->isView() &&
					   hasAncestor((LLView*)other_captor));

	if (mMouseHoverCallback)
	{
		(*mMouseHoverCallback)(mCallbackUserData);
	}

	if (mMouseDownTimer.getStarted() && mHeldDownCallback)
	{
		F32 elapsed = getHeldDownTime();
		if (mHeldDownDelay <= elapsed && mHeldDownFrameDelay <=
				(S32)LLFrameTimer::getFrameCount() - mMouseDownFrame)
		{
			mHeldDownCallback(mCallbackUserData);
		}
	}

	// We only handle the click if the click both started and ended within us
	gWindowp->setCursor(UI_CURSOR_ARROW);
	LL_DEBUGS("UserInput") << "Hover handled by " << getName() << LL_ENDL;

	return true;
}

//virtual
void LLButton::draw()
{
	bool flash = false;
	if (mFlashing)
	{
		F32 elapsed = mFlashingTimer.getElapsedTimeF32();
		S32 flash_count = S32(elapsed * LLUI::sButtonFlashRate * 2.f);
		// Flash on or off ?
		flash = flash_count % 2 == 0 ||
				flash_count > 2 * LLUI::sButtonFlashCount;
	}

	bool pressed_by_keyboard = false;
	if (hasFocus() && gKeyboardp)
	{
		pressed_by_keyboard = gKeyboardp->getKeyDown(' ') ||
							  (mCommitOnReturn &&
							   gKeyboardp->getKeyDown(KEY_RETURN));
	}

	// Unselected image assignments
	S32 local_mouse_x;
	S32 local_mouse_y;
	LLUI::getCursorPositionLocal(this, &local_mouse_x, &local_mouse_y);

	bool pressed = pressed_by_keyboard || mToggleState ||
				   (hasMouseCapture() &&
					pointInView(local_mouse_x, local_mouse_y));

	bool use_glow_effect = false;
	LLColor4 glow_color = LLColor4::white;
	U32 glow_type = LLRender::BT_ADD_WITH_ALPHA;
	if (mNeedsHighlight)
	{
		if (pressed)
		{
			if (mImageHoverSelected)
			{
				mImagep = mImageHoverSelected;
			}
			else
			{
				mImagep = mImageSelected;
				use_glow_effect = true;
			}
		}
		else if (mImageHoverUnselected)
		{
			mImagep = mImageHoverUnselected;
		}
		else
		{
			mImagep = mImageUnselected;
			use_glow_effect = true;
		}
	}
	else if (pressed)
	{
		mImagep = mImageSelected;
	}
	else
	{
		mImagep = mImageUnselected;
	}

	if (mFlashing)
	{
		use_glow_effect = true;
		glow_type = LLRender::BT_ALPHA; // blend the glow
		if (mNeedsHighlight) // highlighted AND flashing
		{
			// Average between flash and highlight colour, with sum of the
			// opacity
			glow_color = (glow_color * 0.5f + mFlashBgColor * 0.5f) % 2.f;
		}
		else
		{
			glow_color = mFlashBgColor;
		}
	}

	// Override if more data is available
	// *HACK: Use gray checked state to mean either:
	//   enabled and tentative
	// or
	//   disabled but checked
	bool enabled = getEnabled();
	if (!mImageDisabledSelected.isNull() &&
		((enabled && getTentative()) || (!enabled && pressed)))
	{
		mImagep = mImageDisabledSelected;
	}
	else if (!mImageDisabled.isNull() && !enabled && !pressed)
	{
		mImagep = mImageDisabled;
	}

	if (mNeedsHighlight && !mImagep)
	{
		use_glow_effect = true;
	}

	// Figure out appropriate color for the text
	LLColor4 label_color;

	// Label changes when button state changes, not when pressed
	if (enabled)
	{
		if (mToggleState)
		{
			label_color = mSelectedLabelColor;
		}
		else
		{
			label_color = mUnselectedLabelColor;
		}
	}
	else if (mToggleState)
	{
		label_color = mDisabledSelectedLabelColor;
	}
	else
	{
		label_color = mDisabledLabelColor;
	}

	// Unselected label assignments
	LLWString label;

	if (mToggleState)
	{
		if (enabled || mDisabledSelectedLabel.empty())
		{
			label = mSelectedLabel;
		}
		else
		{
			label = mDisabledSelectedLabel;
		}
	}
	else if (enabled || mDisabledLabel.empty())
	{
		label = mUnselectedLabel;
	}
	else
	{
		label = mDisabledLabel;
	}

	// Overlay with keyboard focus border
	if (hasFocus())
	{
		F32 lerp_amt = gFocusMgr.getFocusFlashAmt();
		drawBorder(gFocusMgr.getFocusColor(),
				   ll_roundp(lerp(1.f, 3.f, lerp_amt)));
	}

	if (use_glow_effect)
	{
		mCurGlowStrength = lerp(mCurGlowStrength,
								mFlashing ? (flash ? 1.f : 0.f)
										  : mHoverGlowStrength,
								LLCriticalDamp::getInterpolant(0.05f));
	}
	else
	{
		mCurGlowStrength = lerp(mCurGlowStrength, 0.f,
								LLCriticalDamp::getInterpolant(0.05f));
	}

	// Draw button image, if available. Otherwise draw basic rectangular
	// button.
	if (mImagep.notNull())
	{
		if (mScaleImage)
		{
			mImagep->draw(getLocalRect(),
						  enabled ? mImageColor : mDisabledImageColor);
			if (mCurGlowStrength > 0.01f)
			{
				gGL.setSceneBlendType(glow_type);
				mImagep->drawSolid(0, 0, getRect().getWidth(),
								   getRect().getHeight(),
								   glow_color % mCurGlowStrength);
				gGL.setSceneBlendType(LLRender::BT_ALPHA);
			}
		}
		else
		{
			mImagep->draw(0, 0,
						  enabled ? mImageColor : mDisabledImageColor);
			if (mCurGlowStrength > 0.01f)
			{
				gGL.setSceneBlendType(glow_type);
				mImagep->drawSolid(0, 0, glow_color % mCurGlowStrength);
				gGL.setSceneBlendType(LLRender::BT_ALPHA);
			}
		}
	}
	else
	{
		// No image
		llwarns << "No image for button " << getName() << llendl;
		// Draw it in pink so we can find it
		gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0,
				   LLColor4::pink1, false);
	}

	// Let overlay image and text play well together
	S32 text_left = mLeftHPad;
	S32 text_right = getRect().getWidth() - mRightHPad;
	S32 text_width = getRect().getWidth() - mLeftHPad - mRightHPad;

	// Draw overlay image
	if (mImageOverlay.notNull())
	{
		// Get max width and height (discard level 0)
		S32 overlay_width = mImageOverlay->getWidth();
		S32 overlay_height = mImageOverlay->getHeight();

		F32 scale_factor = llmin((F32)getRect().getWidth() /
								 (F32)overlay_width,
								 (F32)getRect().getHeight() /
								 (F32)overlay_height, 1.f);
		overlay_width = ll_roundp((F32)overlay_width * scale_factor);
		overlay_height = ll_roundp((F32)overlay_height * scale_factor);

		S32 center_x = getLocalRect().getCenterX();
		S32 center_y = getLocalRect().getCenterY();

		// *HACK: for "depressed" buttons
		if (pressed)
		{
			--center_y;
			++center_x;
		}

		// Fade out overlay images on disabled buttons
		LLColor4 overlay_color = mImageOverlayColor;
		if (!enabled)
		{
			overlay_color.mV[VALPHA] = 0.5f;
		}

		switch (mImageOverlayAlignment)
		{
			case LLFontGL::LEFT:
				text_left += overlay_width + 1;
				text_width -= overlay_width + 1;
				mImageOverlay->draw(mLeftHPad, center_y - overlay_height / 2,
									overlay_width, overlay_height,
									overlay_color);
				break;

			case LLFontGL::HCENTER:
				mImageOverlay->draw(center_x - overlay_width / 2,
									center_y - overlay_height / 2,
									overlay_width, overlay_height,
									overlay_color);
				break;

			case LLFontGL::RIGHT:
				text_right -= overlay_width + 1;
				text_width -= overlay_width + 1;
				mImageOverlay->draw(getRect().getWidth() - mRightHPad -
									overlay_width,
									center_y - overlay_height / 2,
									overlay_width, overlay_height,
									overlay_color);
				break;

			default:
				// Draw nothing
				break;
		}
	}

	// Draw label
	if (!label.empty())
	{
		LLWStringUtil::trim(label);

		S32 x;
		switch (mHAlign)
		{
			case LLFontGL::RIGHT:
				x = text_right;
				break;

			case LLFontGL::HCENTER:
				x = getRect().getWidth() / 2;
				break;

			case LLFontGL::LEFT:
			default:
				x = text_left;
		}

		S32 y_offset = 2 + (getRect().getHeight() - 20) / 2;

		if (pressed)
		{
			--y_offset;
			++x;
		}

		mGLFont->render(label, 0, (F32)x, (F32)(gButtonVPad + y_offset),
						label_color, mHAlign, LLFontGL::BOTTOM,
						mDropShadowedText ? LLFontGL::DROP_SHADOW_SOFT
										  : LLFontGL::NORMAL,
						U32_MAX, text_width, NULL, false, false);
	}

	if (sDebugRects	|| (LLView::sEditingUI && LLView::sEditingUIView == this))
	{
		drawDebugRect();
	}

	// Reset hover status for next frame
	mNeedsHighlight = false;
}

void LLButton::drawBorder(const LLColor4& color, S32 size)
{
	if (mScaleImage)
	{
		mImagep->drawBorder(getLocalRect(), color, size);
	}
	else
	{
		mImagep->drawBorder(0, 0, color, size);
	}
}

void LLButton::setClickedCallback(void (*cb)(void*), void* userdata)
{
	mClickedCallback = cb;
	if (userdata)
	{
		mCallbackUserData = userdata;
	}
}

void LLButton::setToggleState(bool b)
{
	if (b != mToggleState)
	{
		setControlValue(b); // Will fire LLControlVariable callbacks (if any)
		mToggleState = b;	// May or may not be redundant
	}
}

void LLButton::setFlashing(bool b)
{
	if (b != mFlashing)
	{
		mFlashing = b;
		mFlashingTimer.reset();
	}
}

bool LLButton::toggleState()
{
	setToggleState(!mToggleState);
	return mToggleState;
}

void LLButton::setImageUnselected(LLUIImagePtr image)
{
	mImageUnselected = image;
}

void LLButton::setImages(const std::string& image_name,
						 const std::string& selected_name)
{
	setImageUnselected(image_name);
	setImageSelected(selected_name);
}

void LLButton::setImages(const std::string& image_name)
{
	setImageUnselected(image_name);
	setImageSelected(image_name);
}

void LLButton::setImageSelected(LLUIImagePtr image)
{
	mImageSelected = image;
}

void LLButton::setImageColor(const LLColor4& c)
{
	mImageColor = c;
}

void LLButton::setColor(const LLColor4& color)
{
	setImageColor(color);
}

void LLButton::setAlpha(F32 alpha)
{
	mImageColor.setAlpha(alpha);
	mDisabledImageColor.setAlpha(alpha * 0.5f);
}

void LLButton::setImageDisabled(LLUIImagePtr image)
{
	mImageDisabled = image;
	mDisabledImageColor = mImageColor;
	mDisabledImageColor.mV[VALPHA] *= 0.5f;
}

void LLButton::setImageDisabledSelected(LLUIImagePtr image)
{
	mImageDisabledSelected = image;
	mDisabledImageColor = mImageColor;
	mDisabledImageColor.mV[VALPHA] *= 0.5f;
}

void LLButton::setDisabledImages(const std::string& image_name,
								 const std::string& selected_name,
								 const LLColor4& c)
{
	setImageDisabled(image_name);
	setImageDisabledSelected(selected_name);
	mDisabledImageColor = c;
}

void LLButton::setImageHoverSelected(LLUIImagePtr image)
{
	mImageHoverSelected = image;
}

void LLButton::setDisabledImages(const std::string& image_name,
								 const std::string& selected_name)
{
	LLColor4 clr = mImageColor;
	clr.mV[VALPHA] *= .5f;
	setDisabledImages(image_name, selected_name, clr);
}

void LLButton::setImageHoverUnselected(LLUIImagePtr image)
{
	mImageHoverUnselected = image;
}

void LLButton::setHoverImages(const std::string& image_name,
							  const std::string& selected_name)
{
	setImageHoverUnselected(image_name);
	setImageHoverSelected(selected_name);
}

void LLButton::setImageOverlay(const std::string& image_name,
							   LLFontGL::HAlign alignment,
							   const LLColor4& color)
{
	if (image_name.empty())
	{
		mImageOverlay = NULL;
	}
	else
	{
		mImageOverlay = LLUI::getUIImage(image_name);
		mImageOverlayAlignment = alignment;
		mImageOverlayColor = color;
	}
}

void LLButton::setImageOverlay(LLUIImagePtr image,
							   LLFontGL::HAlign alignment,
							   const LLColor4& color)
{
	mImageOverlay = image;
	mImageOverlayAlignment = alignment;
	mImageOverlayColor = color;
}

void LLButton::onMouseCaptureLost()
{
	mMouseDownTimer.stop();
	mMouseDownTimer.reset();
}

void LLButton::setImageUnselected(const std::string& image_name)
{
	setImageUnselected(LLUI::getUIImage(image_name));
	mImageUnselectedName = image_name;
}

void LLButton::setImageSelected(const std::string& image_name)
{
	setImageSelected(LLUI::getUIImage(image_name));
	mImageSelectedName = image_name;
}

void LLButton::setImageHoverSelected(const std::string& image_name)
{
	setImageHoverSelected(LLUI::getUIImage(image_name));
	mImageHoverSelectedName = image_name;
}

void LLButton::setImageHoverUnselected(const std::string& image_name)
{
	setImageHoverUnselected(LLUI::getUIImage(image_name));
	mImageHoverUnselectedName = image_name;
}

void LLButton::setImageDisabled(const std::string& image_name)
{
	setImageDisabled(LLUI::getUIImage(image_name));
	mImageDisabledName = image_name;
}

void LLButton::setImageDisabledSelected(const std::string& image_name)
{
	setImageDisabledSelected(LLUI::getUIImage(image_name));
	mImageDisabledSelectedName = image_name;
}

void LLButton::addImageAttributeToXML(LLXMLNodePtr node,
									  const std::string& image_name,
									  const LLUUID&	image_id,
									  const std::string& xml_tag_name) const
{
	if (!image_name.empty())
	{
		node->createChild(xml_tag_name.c_str(),
						  true)->setStringValue(image_name);
	}
	else if (image_id.notNull())
	{
		node->createChild((xml_tag_name + "_id").c_str(),
						  true)->setUUIDValue(image_id);
	}
}

//virtual
LLXMLNodePtr LLButton::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_BUTTON_TAG);

	node->createChild("label", true)->setStringValue(getLabelUnselected());
	node->createChild("label_selected",
					  true)->setStringValue(getLabelSelected());
	node->createChild("font",
					  true)->setStringValue(LLFontGL::nameFromFont(mGLFont));
	node->createChild("halign",
					  true)->setStringValue(LLFontGL::nameFromHAlign(mHAlign));

	addImageAttributeToXML(node, mImageUnselectedName, mImageUnselectedID,
						   "image_unselected");
	addImageAttributeToXML(node, mImageSelectedName, mImageSelectedID,
						   "image_selected");
	addImageAttributeToXML(node, mImageHoverSelectedName,
						   mImageHoverSelectedID, "image_hover_selected");
	addImageAttributeToXML(node, mImageHoverUnselectedName,
						   mImageHoverUnselectedID, "image_hover_unselected");
	addImageAttributeToXML(node, mImageDisabledName, mImageDisabledID,
						   "image_disabled");
	addImageAttributeToXML(node, mImageDisabledSelectedName,
						   mImageDisabledSelectedID,
						   "image_disabled_selected");

	node->createChild("scale_image", true)->setBoolValue(mScaleImage);

	return node;
}

void clicked_help(void* data)
{
	LLButton* self = (LLButton*)data;
	if (self && LLUI::sHtmlHelp)
	{
		LLUI::sHtmlHelp->show(self->getHelpURL());
	}
}

//static
LLView* LLButton::fromXML(LLXMLNodePtr node, LLView* parent,
						  LLUICtrlFactory* factory)
{
	std::string name = LL_BUTTON_TAG;
	node->getAttributeString("name", name);

	std::string label = name;
	node->getAttributeString("label", label);

	std::string label_selected = label;
	node->getAttributeString("label_selected", label_selected);

	LLFontGL* font = selectFont(node);

	std::string	image_unselected;
	if (node->hasAttribute("image_unselected"))
	{
		node->getAttributeString("image_unselected", image_unselected);
	}

	std::string	image_selected;
	if (node->hasAttribute("image_selected"))
	{
		node->getAttributeString("image_selected", image_selected);
	}

	std::string	image_hover_selected;
	if (node->hasAttribute("image_hover_selected"))
	{
		node->getAttributeString("image_hover_selected", image_hover_selected);
	}

	std::string	image_hover_unselected;
	if (node->hasAttribute("image_hover_unselected"))
	{
		node->getAttributeString("image_hover_unselected",
								 image_hover_unselected);
	}

	std::string	image_disabled_selected;
	if (node->hasAttribute("image_disabled_selected"))
	{
		node->getAttributeString("image_disabled_selected",
								 image_disabled_selected);
	}

	std::string	image_disabled;
	if (node->hasAttribute("image_disabled"))
	{
		node->getAttributeString("image_disabled", image_disabled);
	}

	std::string	image_overlay;
	node->getAttributeString("image_overlay", image_overlay);

	LLFontGL::HAlign image_overlay_alignment = LLFontGL::HCENTER;
	std::string overlay_align_str;
	if (node->hasAttribute("image_overlay_alignment"))
	{
		node->getAttributeString("image_overlay_alignment",
								 overlay_align_str);
		image_overlay_alignment = LLFontGL::hAlignFromName(overlay_align_str);
	}

	LLButton* button = new LLButton(name, LLRect(), image_unselected,
									image_selected, NULL, NULL,
									parent, font, label, label_selected);

	node->getAttributeS32("pad_right", button->mRightHPad);
	node->getAttributeS32("pad_left", button->mLeftHPad);

	bool is_toggle = button->getIsToggle();
	node->getAttributeBool("toggle", is_toggle);
	button->setIsToggle(is_toggle);

	if (image_hover_selected != LLStringUtil::null)
	{
		button->setImageHoverSelected(image_hover_selected);
	}

	if (image_hover_unselected != LLStringUtil::null)
	{
		button->setImageHoverUnselected(image_hover_unselected);
	}

	if (image_disabled_selected != LLStringUtil::null)
	{
		button->setImageDisabledSelected(image_disabled_selected);
	}

	if (image_disabled != LLStringUtil::null)
	{
		button->setImageDisabled(image_disabled);
	}

	if (image_overlay != LLStringUtil::null)
	{
		button->setImageOverlay(image_overlay, image_overlay_alignment);
	}

	if (node->hasAttribute("halign"))
	{
		LLFontGL::HAlign halign = selectFontHAlign(node);
		button->setHAlign(halign);
	}

	if (node->hasAttribute("scale_image"))
	{
		bool needs_scale = false;
		node->getAttributeBool("scale_image", needs_scale);
		button->setScaleImage(needs_scale);
	}

	if (label.empty())
	{
		button->setLabelUnselected(node->getTextContents());
	}
	if (label_selected.empty())
	{
		button->setLabelSelected(node->getTextContents());
	}

	if (node->hasAttribute("help_url"))
	{
		std::string	help_url;
		node->getAttributeString("help_url", help_url);
		button->setHelpURLCallback(help_url);
	}

	button->initFromXML(node, parent);

	return button;
}

void LLButton::setHelpURLCallback(const std::string& help_url)
{
	mHelpURL = help_url;
	setClickedCallback(clicked_help, this);
}
