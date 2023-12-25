/**
 * @file llslider.cpp
 * @brief LLSlider base class
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

#include "linden_common.h"

#include "llslider.h"

#include "llcontrol.h"
#include "llgl.h"
#include "llimagegl.h"
#include "llkeyboard.h"			// For the MASK constants
#include "llwindow.h"

static const std::string LL_SLIDER_TAG = "slider_bar";
static LLRegisterWidget<LLSlider> r21(LL_SLIDER_TAG);

static const std::string LL_VOLUME_SLIDER_CTRL_TAG = "volume_slider";
static LLRegisterWidget<LLSlider> r22(LL_VOLUME_SLIDER_CTRL_TAG);

LLSlider::LLSlider(const std::string& name, const LLRect& rect,
				   void (*on_commit_callback)(LLUICtrl*, void*),
				   void* callback_userdata,
				   F32 initial_value, F32 min_value, F32 max_value,
				   F32 increment, bool volume, const char* control_name)
:	LLUICtrl(name, rect, true, on_commit_callback, callback_userdata,
			 FOLLOWS_LEFT | FOLLOWS_TOP),
	mValue(initial_value),
	mInitialValue(initial_value),
	mMinValue(min_value),
	mMaxValue(max_value),
	mIncrement(increment),
	mVolumeSlider(volume),
	mMouseOffset(0),
	mMouseDownCallback(NULL),
	mMouseUpCallback(NULL),
	mMouseHoverCallback(NULL)
{
	mThumbImage = LLUI::getUIImage("icn_slide-thumb_dark.tga");
	mTrackImage = LLUI::getUIImage("icn_slide-groove_dark.tga");
	mTrackHighlightImage = LLUI::getUIImage("icn_slide-highlight.tga");

	// Properly handle setting the starting thumb rect: do it this way to
	// handle both the operating-on-settings and standalone ways of using this
	setControlName(control_name, NULL);
	setValue(getValueF32());

	updateThumbRect();
	mDragStartThumbRect = mThumbRect;
}

//virtual
void LLSlider::setValue(F32 value, bool from_event)
{
	value = llclamp(value, mMinValue, mMaxValue);

	// Round to nearest increment (bias towards rounding down)
	value -= mMinValue;
	value += mIncrement / 2.0001f;
	value -= fmod(value, mIncrement);
	value += mMinValue;

	if (!from_event && mValue != value)
	{
		setControlValue(value);
	}

	mValue = value;
	updateThumbRect();
}

void LLSlider::updateThumbRect()
{
	F32 t = (mValue - mMinValue) / (mMaxValue - mMinValue);

	S32 thumb_width = mThumbImage->getWidth();
	S32 thumb_height = mThumbImage->getHeight();
	S32 left_edge = thumb_width / 2;
	S32 right_edge = getRect().getWidth() - thumb_width / 2;

	S32 x = left_edge + S32(t * (right_edge - left_edge));
	mThumbRect.mLeft = x - (thumb_width / 2);
	mThumbRect.mRight = mThumbRect.mLeft + thumb_width;
	mThumbRect.mBottom = getLocalRect().getCenterY() - thumb_height / 2;
	mThumbRect.mTop = mThumbRect.mBottom + thumb_height;
}

void LLSlider::setValueAndCommit(F32 value)
{
	F32 old_value = mValue;
	setValue(value);
	if (value != old_value)
	{
		onCommit();
	}
}

//virtual
bool LLSlider::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		if (mMouseHoverCallback)
		{
			mMouseHoverCallback(this, mCallbackUserData);
		}

		S32 thumb_half_width = mThumbImage->getWidth() / 2;
		S32 left_edge = thumb_half_width;
		S32 right_edge = getRect().getWidth() - (thumb_half_width);

		x += mMouseOffset;
		x = llclamp(x, left_edge, right_edge);

		F32 t = F32(x - left_edge) / (right_edge - left_edge);
		setValueAndCommit(t * (mMaxValue - mMinValue) + mMinValue);

		gWindowp->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName()
							   << " (active)" << LL_ENDL;
	}
	else
	{
		gWindowp->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName()
							   << " (inactive)" << LL_ENDL;
	}
	return true;
}

//virtual
bool LLSlider::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
		if (mMouseUpCallback)
		{
			mMouseUpCallback(this, mCallbackUserData);
		}
		make_ui_sound("UISndClickRelease");
	}

	return true;
}

//virtual
bool LLSlider::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Only do sticky-focus on non-chrome widgets
	if (!getIsChrome())
	{
		setFocus(true);
	}
	if (mMouseDownCallback)
	{
		mMouseDownCallback(this, mCallbackUserData);
	}

	if (MASK_CONTROL & mask) // if CTRL is modifying
	{
		setValueAndCommit(mInitialValue);
	}
	else
	{
		// Find the offset of the actual mouse location from the center of the
		// thumb.
		if (mThumbRect.pointInRect(x, y))
		{
			mMouseOffset = mThumbRect.mLeft + mThumbImage->getWidth() / 2 - x;
		}
		else
		{
			mMouseOffset = 0;
		}

		// Start dragging the thumb
		// No handler needed for focus lost since this class has no state that
		// depends on it.
		gFocusMgr.setMouseCapture(this);
		mDragStartThumbRect = mThumbRect;
	}
	make_ui_sound("UISndClick");

	return true;
}

//virtual
bool LLSlider::handleKeyHere(KEY key, MASK mask)
{
	switch (key)
	{
		case KEY_UP:
		case KEY_DOWN:
			// Eat up and down keys to be consistent
			return true;

		case KEY_LEFT:
			setValueAndCommit(getValueF32() - getIncrement());
			return true;

		case KEY_RIGHT:
			setValueAndCommit(getValueF32() + getIncrement());
			return true;

		default:
			break;
	}
	return false;
}

//virtual
void LLSlider::draw()
{
	// Since thumb image might still be decoding, need thumb to accomodate
	// image size
	updateThumbRect();

	// Draw background and thumb.

	// Drawing solids requires texturing be disabled
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	F32 opacity = getEnabled() ? 1.f : 0.3f;
	LLColor4 center_color = LLUI::sSliderThumbCenterColor % opacity;

	// Track
	S32 half_width = mThumbImage->getWidth() / 2;
	S32 half_height = mTrackImage->getHeight() / 2;
	LLRect track_rect(half_width, getLocalRect().getCenterY() + half_height,
					  getRect().getWidth() - half_width,
					  getLocalRect().getCenterY() - half_height);
	LLRect highlight_rect(track_rect.mLeft, track_rect.mTop,
						  mThumbRect.getCenterX(), track_rect.mBottom);
	mTrackImage->draw(track_rect);
	mTrackHighlightImage->draw(highlight_rect);

	// Thumb
	if (hasMouseCapture())
	{
		// Show ghost where thumb was before dragging began.
		mThumbImage->draw(mDragStartThumbRect,
						  LLUI::sSliderThumbCenterColor % 0.3f);
	}
	if (hasFocus())
	{
		// Draw focus highlighting.
		mThumbImage->drawBorder(mThumbRect, gFocusMgr.getFocusColor(),
								gFocusMgr.getFocusFlashWidth());
	}
	// Fill in the thumb.
	mThumbImage->draw(mThumbRect, hasMouseCapture() ?
						LLUI::sSliderThumbOutlineColor : center_color);

	LLUICtrl::draw();
}

//virtual
LLXMLNodePtr LLSlider::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	if (mVolumeSlider)
	{
		node->setName(LL_VOLUME_SLIDER_CTRL_TAG);
	}
	else
	{
		node->setName(LL_SLIDER_TAG);
	}

	node->createChild("initial_val", true)->setFloatValue(getInitialValue());
	node->createChild("min_val", true)->setFloatValue(getMinValue());
	node->createChild("max_val", true)->setFloatValue(getMaxValue());
	node->createChild("increment", true)->setFloatValue(getIncrement());
	node->createChild("volume", true)->setBoolValue(mVolumeSlider);

	return node;
}

//static
LLView* LLSlider::fromXML(LLXMLNodePtr node, LLView* parent,
						  LLUICtrlFactory*)
{
	std::string name = LL_SLIDER_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	F32 initial_value = 0.f;
	node->getAttributeF32("initial_val", initial_value);

	F32 min_value = 0.f;
	node->getAttributeF32("min_val", min_value);

	F32 max_value = 1.f;
	node->getAttributeF32("max_val", max_value);

	F32 increment = 0.1f;
	node->getAttributeF32("increment", increment);

	bool volume = node->hasName(LL_VOLUME_SLIDER_CTRL_TAG);
	if (!volume)
	{
		node->getAttributeBool("volume", volume);
	}

	LLSlider* slider = new LLSlider(name, rect, NULL, NULL, initial_value,
									min_value, max_value, increment, volume);
	slider->initFromXML(node, parent);

	return slider;
}
