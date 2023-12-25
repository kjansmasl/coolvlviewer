/**
 * @file llsliderctrl.cpp
 * @brief LLSliderCtrl base class
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

#include "llsliderctrl.h"

#include "llcontrol.h"
#include "llgl.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "lllocale.h"
#include "lltextbox.h"

constexpr U32 MAX_SLIDER_STR_LEN = 10;

static const std::string LL_SLIDER_CTRL_TAG = "slider";
static LLRegisterWidget<LLSliderCtrl> r23(LL_SLIDER_CTRL_TAG);

LLSliderCtrl::LLSliderCtrl(const std::string& name, const LLRect& rect,
						   const std::string& label, const LLFontGL* font,
						   S32 label_width, S32 text_left, bool show_text,
						   bool can_edit_text, bool volume,
						   void (*commit_callback)(LLUICtrl*, void*),
						   void* callback_user_data, F32 initial_value,
						   F32 min_value, F32 max_value, F32 increment,
						   const char* control_name)
:	LLUICtrl(name, rect, true, commit_callback, callback_user_data),
	mFont(font),
	mShowText(show_text),
	mCanEditText(can_edit_text),
	mVolumeSlider(volume),
	mPrecision(3),
	mLabelBox(NULL),
	mLabelWidth(label_width),
	mValue(initial_value),
	mEditor(NULL),
	mTextBox(NULL),
	mTextEnabledColor(LLUI::sLabelTextColor),
	mTextDisabledColor(LLUI::sLabelDisabledColor),
	mSliderMouseUpCallback(NULL),
	mSliderMouseDownCallback(NULL)
{
	S32 top = getRect().getHeight();
	S32 bottom = 0;
	S32 left = 0;

	// Label
	if (!label.empty())
	{
		if (label_width == 0)
		{
			label_width = font->getWidth(label);
		}
		LLRect label_rect(left, top, label_width, bottom);
		mLabelBox = new LLTextBox("SliderCtrl Label", label_rect, label, font);
		addChild(mLabelBox);
	}

	S32 slider_right = getRect().getWidth();
	if (show_text)
	{
		slider_right = text_left - SLIDERCTRL_SPACING;
	}

	S32 slider_left = label_width ? label_width + SLIDERCTRL_SPACING : 0;
	LLRect slider_rect(slider_left, top, slider_right, bottom);
	mSlider = new LLSlider(LL_SLIDER_CTRL_TAG, slider_rect,
						   LLSliderCtrl::onSliderCommit, this, initial_value,
						   min_value, max_value, increment, volume,
						   control_name);
	addChild(mSlider);

	if (show_text)
	{
		LLRect text_rect(text_left, top, getRect().getWidth(), bottom);
		if (can_edit_text)
		{
			mEditor = new LLLineEditor("SliderCtrl Editor", text_rect,
									   LLStringUtil::null, font,
									   MAX_SLIDER_STR_LEN,
									   &LLSliderCtrl::onEditorCommit,
									   NULL, NULL, this,
									   &LLLineEditor::prevalidateFloat);
			mEditor->setFollowsLeft();
			mEditor->setFollowsBottom();
			mEditor->setFocusReceivedCallback(&LLSliderCtrl::onEditorGainFocus,
											  this);
			mEditor->setIgnoreTab(true);
#if 0		// Do not do this, as selecting the entire text is single clicking
			// in some cases and double clicking in others
			mEditor->setSelectAllonFocusReceived(true);
#endif
			addChild(mEditor);
		}
		else
		{
			mTextBox = new LLTextBox("SliderCtrl Text", text_rect,
									 LLStringUtil::null, font);
			mTextBox->setFollowsLeft();
			mTextBox->setFollowsBottom();
			addChild(mTextBox);
		}
	}

	updateText();
}

//virtual
LLSliderCtrl::~LLSliderCtrl()
{
	// Children all cleaned up by default view destructor.
}

//static
void LLSliderCtrl::onEditorGainFocus(LLFocusableElement* caller,
									 void* userdata)
{
	LLSliderCtrl* self = (LLSliderCtrl*)userdata;
	llassert(caller == self->mEditor);
	self->onFocusReceived();
}

//virtual
void LLSliderCtrl::setValue(F32 v, bool from_event)
{
	mSlider->setValue(v, from_event);
	mValue = mSlider->getValueF32();
	updateText();
}

void LLSliderCtrl::setLabel(const std::string& label)
{
	if (mLabelBox)
	{
		mLabelBox->setText(label);
	}
}

//virtual
bool LLSliderCtrl::setLabelArg(const std::string& key,
							   const std::string& text)
{
	bool res = false;
	if (mLabelBox)
	{
		res = mLabelBox->setTextArg(key, text);
		if (res && mLabelWidth == 0)
		{
			S32 label_width = mFont->getWidth(mLabelBox->getText());
			LLRect rect = mLabelBox->getRect();
			S32 prev_right = rect.mRight;
			rect.mRight = rect.mLeft + label_width;
			mLabelBox->setRect(rect);

			S32 delta = rect.mRight - prev_right;
			rect = mSlider->getRect();
			S32 left = rect.mLeft + delta;
			left = llclamp(left, 0, rect.mRight - SLIDERCTRL_SPACING);
			rect.mLeft = left;
			mSlider->setRect(rect);
		}
	}
	return res;
}

//virtual
void LLSliderCtrl::clear()
{
	setValue(0.f);
	if (mEditor)
	{
		mEditor->setText(LLStringUtil::null);
	}
	if (mTextBox)
	{
		mTextBox->setText(LLStringUtil::null);
	}
}

void LLSliderCtrl::setOffLimit(const std::string& off_text, F32 off_value)
{
	mDisplayOff = off_text.size() > 0;
	mOffText = off_text;
	mOffValue = off_value;
	if (mTextBox && !mEditor)
	{
		updateText();
	}
}

void LLSliderCtrl::updateText()
{
	if (mEditor || mTextBox)
	{
		LLLocale locale(LLLocale::USER_LOCALE);

		// Do not display very small negative values as -0.000
		F32 displayed_value = floorf(getValueF32() *
									 powf(10.f, mPrecision) + 0.5f) /
									 powf(10.f, mPrecision);

		std::string format = llformat("%%.%df", mPrecision);
		std::string text = llformat(format.c_str(), displayed_value);
		if (mEditor)
		{
			mEditor->setText(text);
		}
		else if (mDisplayOff && displayed_value == mOffValue)
		{
			mTextBox->setText(mOffText);
		}
		else
		{
			mTextBox->setText(text);
		}
	}
}

void LLSliderCtrl::updateSliderRect()
{
	S32 left = 0;
	S32 right = getRect().getWidth();
	S32 top = getRect().getHeight();
	S32 bottom = 0;
	if (mEditor)
	{
		LLRect editor_rect = mEditor->getRect();
		S32 editor_width = editor_rect.getWidth();
		editor_rect.mRight = right;
		editor_rect.mLeft = right - editor_width;
		mEditor->setRect(editor_rect);

		right -= editor_width + SLIDERCTRL_SPACING;
	}
	if (mTextBox)
	{
		right -= mTextBox->getRect().getWidth() + SLIDERCTRL_SPACING;
	}
	if (mLabelBox)
	{
		left += mLabelBox->getRect().getWidth() + SLIDERCTRL_SPACING;
	}
	mSlider->setRect(LLRect(left, top, right, bottom));
}

//static
void LLSliderCtrl::onEditorCommit(LLUICtrl* caller, void *userdata)
{
	LLSliderCtrl* self = (LLSliderCtrl*) userdata;
	llassert(caller == self->mEditor);

	bool success = false;
	F32 val = self->mValue;
	F32 saved_val = self->mValue;

	std::string text = self->mEditor->getText();
	if (LLLineEditor::postvalidateFloat(text))
	{
		LLLocale locale(LLLocale::USER_LOCALE);
		val = (F32) atof(text.c_str());
		if (self->mSlider->getMinValue() <= val &&
			val <= self->mSlider->getMaxValue())
		{
			if (self->mValidateCallback)
			{
				// Set the value temporarily so that the callback can retrieve
				// it:
				self->setValue(val);
				if (self->mValidateCallback(self, self->mCallbackUserData))
				{
					success = true;
				}
			}
			else
			{
				self->setValue(val);
				success = true;
			}
		}
	}

	if (success)
	{
		self->onCommit();
	}
	else
	{
		if (self->getValueF32() != saved_val)
		{
			self->setValue(saved_val);
		}
		self->reportInvalidData();
	}
	self->updateText();
}

//static
void LLSliderCtrl::onSliderCommit(LLUICtrl* caller, void *userdata)
{
	LLSliderCtrl* self = (LLSliderCtrl*)userdata;
	llassert(caller == self->mSlider);

	bool success = false;
	F32 saved_val = self->mValue;
	F32 new_val = self->mSlider->getValueF32();

	if (self->mValidateCallback)
	{
		// set the value temporarily so that the callback can retrieve it:
		self->mValue = new_val;
		if (self->mValidateCallback(self, self->mCallbackUserData))
		{
			success = true;
		}
	}
	else
	{
		self->mValue = new_val;
		success = true;
	}

	if (success)
	{
		self->onCommit();
	}
	else
	{
		if (self->mValue != saved_val)
		{
			self->setValue(saved_val);
		}
		self->reportInvalidData();
	}
	self->updateText();
}

//virtual
void LLSliderCtrl::setEnabled(bool b)
{
	LLView::setEnabled(b);

	if (mLabelBox)
	{
		mLabelBox->setColor(b ? mTextEnabledColor : mTextDisabledColor);
	}

	mSlider->setEnabled(b);

	if (mEditor)
	{
		mEditor->setEnabled(b);
	}

	if (mTextBox)
	{
		mTextBox->setColor(b ? mTextEnabledColor : mTextDisabledColor);
	}
}

//virtual
void LLSliderCtrl::setTentative(bool b)
{
	if (mEditor)
	{
		mEditor->setTentative(b);
	}
	LLUICtrl::setTentative(b);
}

//virtual
void LLSliderCtrl::onCommit()
{
	setTentative(false);

	if (mEditor)
	{
		mEditor->setTentative(false);
	}

	LLUICtrl::onCommit();
}

//virtual
void LLSliderCtrl::setRect(const LLRect& rect)
{
	LLUICtrl::setRect(rect);
	updateSliderRect();
}

//virtual
void LLSliderCtrl::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLUICtrl::reshape(width, height, called_from_parent);
	updateSliderRect();
}

//virtual
void LLSliderCtrl::setPrecision(S32 precision)
{
	if (precision < 0 || precision > 10)
	{
		llerrs << "LLSliderCtrl::setPrecision - precision out of range"
			   << llendl;
		return;
	}

	mPrecision = precision;
	updateText();
}

void LLSliderCtrl::setSliderMouseDownCallback(void (*cb)(LLUICtrl*, void*))
{
	mSliderMouseDownCallback = cb;
	mSlider->setMouseDownCallback(LLSliderCtrl::onSliderMouseDown);
}

//static
void LLSliderCtrl::onSliderMouseDown(LLUICtrl* caller, void* userdata)
{
	LLSliderCtrl* self = (LLSliderCtrl*) userdata;
	if (self->mSliderMouseDownCallback)
	{
		self->mSliderMouseDownCallback(self, self->mCallbackUserData);
	}
}

void LLSliderCtrl::setSliderMouseUpCallback(void (*cb)(LLUICtrl*, void*))
{
	mSliderMouseUpCallback = cb;
	mSlider->setMouseUpCallback(LLSliderCtrl::onSliderMouseUp);
}

//static
void LLSliderCtrl::onSliderMouseUp(LLUICtrl* caller, void* userdata)
{
	LLSliderCtrl* self = (LLSliderCtrl*) userdata;
	if (self->mSliderMouseUpCallback)
	{
		self->mSliderMouseUpCallback(self, self->mCallbackUserData);
	}
}

//virtual
void LLSliderCtrl::onTabInto()
{
	if (mEditor)
	{
		mEditor->onTabInto();
	}
}

void LLSliderCtrl::reportInvalidData()
{
	make_ui_sound("UISndBadKeystroke");
}

//virtual
void LLSliderCtrl::setControlName(const char* control_name, LLView* context)
{
	LLView::setControlName(control_name, context);
	mSlider->setControlName(control_name, context);
}

//virtual
LLXMLNodePtr LLSliderCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_SLIDER_CTRL_TAG);

	node->createChild("show_text", true)->setBoolValue(mShowText);

	node->createChild("can_edit_text", true)->setBoolValue(mCanEditText);

	node->createChild("volume", true)->setBoolValue(mVolumeSlider);

	node->createChild("decimal_digits", true)->setIntValue(mPrecision);

	if (mLabelBox)
	{
		node->createChild("label", true)->setStringValue(mLabelBox->getText());
	}

	// TomY TODO: Do we really want to export the transient state of the slider?
	node->createChild("value", true)->setFloatValue(mValue);

	if (mSlider)
	{
		node->createChild("initial_val", true)->setFloatValue(mSlider->getInitialValue());
		node->createChild("min_val", true)->setFloatValue(mSlider->getMinValue());
		node->createChild("max_val", true)->setFloatValue(mSlider->getMaxValue());
		node->createChild("increment", true)->setFloatValue(mSlider->getIncrement());
	}
	addColorXML(node, mTextEnabledColor, "text_enabled_color", "LabelTextColor");
	addColorXML(node, mTextDisabledColor, "text_disabled_color", "LabelDisabledColor");

	return node;
}

//static
LLView* LLSliderCtrl::fromXML(LLXMLNodePtr node, LLView* parent,
							  LLUICtrlFactory*)
{
	std::string name = LL_SLIDER_CTRL_TAG;
	node->getAttributeString("name", name);

	std::string label;
	node->getAttributeString("label", label);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	LLFontGL* font = LLView::selectFont(node);

	// *HACK: Font might not be specified.
	if (!font)
	{
		font = LLFontGL::getFontSansSerifSmall();
	}

	S32 label_width = 0;
	node->getAttributeS32("label_width", label_width);

	bool show_text = true;
	node->getAttributeBool("show_text", show_text);

	bool can_edit_text = false;
	node->getAttributeBool("can_edit_text", can_edit_text);

	bool volume = false;
	node->getAttributeBool("volume", volume);

	F32 initial_value = 0.f;
	node->getAttributeF32("initial_val", initial_value);

	F32 min_value = 0.f;
	node->getAttributeF32("min_val", min_value);

	F32 max_value = 1.f;
	node->getAttributeF32("max_val", max_value);

	F32 increment = 0.1f;
	node->getAttributeF32("increment", increment);

	U32 precision = 3;
	node->getAttributeU32("decimal_digits", precision);

	S32 text_left = 0;
	if (show_text)
	{
		// Calculate the size of the text box (log max_value is number of
		// digits - 1 so plus 1)
		if (max_value)
		{
			text_left = font->getWidth("0") *
						((S32)log10f(max_value) + precision + 1);
		}

		if (increment < 1.f)
		{
			// (mostly) take account of decimal point in value
			text_left += font->getWidth(".");
		}

		if (min_value < 0.f || max_value < 0.f)
		{
			// (mostly) take account of minus sign
			text_left += font->getWidth("-");
		}

		// Padding to make things look nicer
		text_left += 8;
	}

	if (label.empty())
	{
		label.assign(node->getTextContents());
	}

	LLUICtrlCallback callback = NULL;
	LLSliderCtrl* slider =
		new LLSliderCtrl(name, rect, label, font, label_width,
						 rect.getWidth() - text_left, show_text, can_edit_text,
						 volume, callback, NULL, initial_value, min_value,
						 max_value, increment);

	slider->setPrecision(precision);

	slider->initFromXML(node, parent);

	slider->updateText();

	return slider;
}
