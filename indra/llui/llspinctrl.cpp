/**
 * @file llspinctrl.cpp
 * @brief LLSpinCtrl base class
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

#include "llspinctrl.h"

#include "llbutton.h"
#include "llcontrol.h"
#include "llgl.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "lllocale.h"
#include "llmath.h"
#include "lltextbox.h"

constexpr U32 MAX_SPIN_STR_LEN = 32;

static const std::string LL_SPIN_CTRL_TAG = "spinner";
static LLRegisterWidget<LLSpinCtrl> r24(LL_SPIN_CTRL_TAG);

LLSpinCtrl::LLSpinCtrl(const std::string& name, const LLRect& spin_rect,
					   const std::string& label, const LLFontGL* font,
					   void (*commit_callback)(LLUICtrl*, void*),
					   void* callback_user_data, F32 initial_value,
					   F32 min_value, F32 max_value, F32 increment,
					   const std::string& control_name, S32 label_width)
:	LLUICtrl(name, spin_rect, true, commit_callback, callback_user_data,
			 FOLLOWS_LEFT | FOLLOWS_TOP),
	mValue(initial_value),
	mInitialValue(initial_value),
	mMaxValue(max_value),
	mMinValue(min_value),
	mIncrement(increment),
	mPrecision(3),
	mLabelBox(NULL),
	mTextEnabledColor(LLUI::sLabelTextColor),
	mTextDisabledColor(LLUI::sLabelDisabledColor),
	mHasBeenSet(false),
	mDirty(true)
{
	S32 top = getRect().getHeight();
	S32 bottom = top - 2 * SPINCTRL_BTN_HEIGHT;
	S32 centered_top = top;
	S32 centered_bottom = bottom;
	S32 btn_left = 0;

	LLRect rect;

	// Label
	if (!label.empty())
	{
		rect.set(0, centered_top, label_width, centered_bottom);
		mLabelBox = new LLTextBox("SpinCtrl Label", rect, label, font);
		addChild(mLabelBox);

		btn_left += rect.mRight + SPINCTRL_SPACING;
	}

	S32 btn_right = btn_left + SPINCTRL_BTN_WIDTH;

	// Spin buttons
	LLFontGL* btnfont = LLFontGL::getFontSansSerif();

	rect.set(btn_left, top, btn_right, top - SPINCTRL_BTN_HEIGHT);
	std::string out_id = "UIImgBtnSpinUpOutUUID";
	std::string in_id = "UIImgBtnSpinUpInUUID";
	mUpBtn = new LLButton("SpinCtrl Up", rect, out_id, in_id, NULL,
						  onUpBtn, this, btnfont);
	mUpBtn->setFollowsLeft();
	mUpBtn->setFollowsBottom();
	mUpBtn->setHeldDownCallback(onUpBtn);
	mUpBtn->setTabStop(false);
	addChild(mUpBtn);

	rect.set(btn_left, top - SPINCTRL_BTN_HEIGHT, btn_right, bottom);
	out_id = "UIImgBtnSpinDownOutUUID";
	in_id = "UIImgBtnSpinDownInUUID";
	mDownBtn = new LLButton("SpinCtrl Down", rect, out_id, in_id, NULL,
							onDownBtn, this, btnfont);
	mDownBtn->setFollowsLeft();
	mDownBtn->setFollowsBottom();
	mDownBtn->setHeldDownCallback(onDownBtn);
	mDownBtn->setTabStop(false);
	addChild(mDownBtn);

	// Line editor
	rect.set(btn_right + 1, centered_top, getRect().getWidth(),
					   centered_bottom);
	mEditor = new LLLineEditor("SpinCtrl Editor", rect, LLStringUtil::null,
							   font, MAX_SPIN_STR_LEN, onEditorCommit, NULL,
							   NULL, this, LLLineEditor::prevalidateFloat);
	mEditor->setFollowsLeft();
	mEditor->setFollowsBottom();
	mEditor->setFocusReceivedCallback(onEditorGainFocus, this);
	mEditor->setFocusLostCallback(onEditorLostFocus, this);
	mEditor->setIgnoreTab(true);
	addChild(mEditor);
	updateEditor();

	setUseBoundingRect(true);
}

F32 clamp_precision(F32 value, S32 decimal_precision)
{
	// powf() is not perfect
	F64 clamped_value = value;
	for (S32 i = 0; i < decimal_precision; ++i)
	{
		clamped_value *= 10.0;
	}

	clamped_value = floor(clamped_value + 0.5);

	for (S32 i = 0; i < decimal_precision; ++i)
	{
		clamped_value /= 10.0;
	}

	return (F32)clamped_value;
}

//static
void LLSpinCtrl::onUpBtn(void* userdata)
{
	LLSpinCtrl* self = (LLSpinCtrl*) userdata;
	if (self && self->getEnabled())
	{
		// Use getValue()/setValue() to force reload from/to control
		F32 val = (F32)self->getValue().asReal() + self->mIncrement;
		val = clamp_precision(val, self->mPrecision);
		val = llmin(val, self->mMaxValue);

		if (self->mValidateCallback)
		{
			F32 saved_val = (F32)self->getValue().asReal();
			self->setValue(val);
			if (!self->mValidateCallback(self, self->mCallbackUserData))
			{
				self->setValue(saved_val);
				self->reportInvalidData();
				self->updateEditor();
				return;
			}
		}
		else
		{
			self->setValue(val);
		}

		self->updateEditor();
		self->onCommit();
	}
}

//static
void LLSpinCtrl::onDownBtn(void* userdata)
{
	LLSpinCtrl* self = (LLSpinCtrl*)userdata;
	if (self && self->getEnabled())
	{
		F32 val = (F32)self->getValue().asReal() - self->mIncrement;
		val = clamp_precision(val, self->mPrecision);
		val = llmax(val, self->mMinValue);

		if (self->mValidateCallback)
		{
			F32 saved_val = (F32)self->getValue().asReal();
			self->setValue(val);
			if (!self->mValidateCallback(self, self->mCallbackUserData))
			{
				self->setValue(saved_val);
				self->reportInvalidData();
				self->updateEditor();
				return;
			}
		}
		else
		{
			self->setValue(val);
		}

		self->updateEditor();
		self->onCommit();
	}
}

//static
void LLSpinCtrl::onEditorGainFocus(LLFocusableElement* caller, void* userdata)
{
	LLSpinCtrl* self = (LLSpinCtrl*)userdata;
	if (self && caller && self->mEditor == caller)
	{
		self->onFocusReceived();
	}
}

//static
void LLSpinCtrl::onEditorLostFocus(LLFocusableElement* caller, void* userdata)
{
	LLSpinCtrl* self = (LLSpinCtrl*)userdata;
	if (self && caller && self->mEditor == caller)
	{
		self->onFocusLost();

		if (!self->mEditor->isDirty())
		{
			LLLocale locale(LLLocale::USER_LOCALE);
			std::string val_str = self->mEditor->getText();
			if ((F32)atof(val_str.c_str()) != (F32)self->getValue().asReal())
			{
				// Editor was focused when value update arrived, the string in
				// editor is different from the one in spin control. Since
				// editor is not dirty, it won't commit, so either attempt to
				// commit value from editor or revert to a more recent value
				// from spin control.
				self->updateEditor();
			}
		}
	}
}

//virtual
void LLSpinCtrl::setValue(const LLSD& value)
{
	F32 v = (F32)value.asReal();
	if (mValue != v || !mHasBeenSet)
	{
		mHasBeenSet = true;
		mValue = v;

		if (!mEditor->hasFocus())
		{
			updateEditor();
		}
	}
}

// No matter if mEditor has the focus, update the value
//virtual
void LLSpinCtrl::forceSetValue(const LLSD& value)
{
	F32 v = (F32)value.asReal();
	if (mValue != v || !mHasBeenSet)
	{
		mHasBeenSet = true;
		mValue = v;

		updateEditor();
		mEditor->resetScrollPosition();
	}
}

//virtual
void LLSpinCtrl::clear()
{
	setValue(mMinValue);
	mEditor->clear();
	mHasBeenSet = false;
}

void LLSpinCtrl::updateEditor()
{
	LLLocale locale(LLLocale::USER_LOCALE);

	// Do not display very small negative values as -0.000
	F32 displayed_value = clamp_precision((F32)getValue().asReal(),
										  mPrecision);
#if 0
	if (S32(displayed_value * powf(10.f, mPrecision)) == 0)
	{
		displayed_value = 0.f;
	}
#endif

	std::string format = llformat("%%.%df", mPrecision);
	std::string text = llformat(format.c_str(), displayed_value);
	mEditor->setText(text);
}

//static
void LLSpinCtrl::onEditorCommit(LLUICtrl* caller, void* userdata)
{
	LLSpinCtrl* self = (LLSpinCtrl*)userdata;
	if (!self || caller != self->mEditor) return;

	bool success = false;

	std::string text = self->mEditor->getText();
	if (LLLineEditor::postvalidateFloat(text))
	{
		LLLocale locale(LLLocale::USER_LOCALE);
		F32 val = (F32)atof(text.c_str());

		if (val < self->mMinValue) val = self->mMinValue;
		if (val > self->mMaxValue) val = self->mMaxValue;

		if (self->mValidateCallback)
		{
			F32 saved_val = self->mValue;
			self->mValue = val;
			if (self->mValidateCallback(self, self->mCallbackUserData))
			{
				success = true;
				self->onCommit();
			}
			else
			{
				self->mValue = saved_val;
			}
		}
		else
		{
			self->mValue = val;
			self->onCommit();
			success = true;
		}
	}
	self->updateEditor();

	if (success)
	{
		// We commited and clamped value; try to display as much as possible
		self->mEditor->resetScrollPosition();
	}
	else
	{
		self->reportInvalidData();
	}
}

void LLSpinCtrl::forceEditorCommit()
{
	onEditorCommit(mEditor, this);
}

//virtual
void LLSpinCtrl::setFocus(bool b)
{
	LLUICtrl::setFocus(b);
	mEditor->setFocus(b);
	mDirty = true;
}

//virtual
void LLSpinCtrl::setEnabled(bool b)
{
	LLView::setEnabled(b);
	mEditor->setEnabled(b);
	mDirty = true;
}

//virtual
void LLSpinCtrl::setTentative(bool b)
{
	mEditor->setTentative(b);
	LLUICtrl::setTentative(b);
}

bool LLSpinCtrl::isMouseHeldDown() const
{
	return mDownBtn->hasMouseCapture() || mUpBtn->hasMouseCapture();
}

//virtual
void LLSpinCtrl::onCommit()
{
	setTentative(false);
	setControlValue(mValue);
	LLUICtrl::onCommit();
}

//virtual
void LLSpinCtrl::setPrecision(S32 precision)
{
	if (precision < 0 || precision > 10)
	{
		llwarns << "Precision out of range, ignoring." << llendl;
		llassert(false);
	}
	else
	{
		mPrecision = precision;
		updateEditor();
	}
}

void LLSpinCtrl::setLabel(const std::string& label)
{
	if (mLabelBox)
	{
		mLabelBox->setText(label);
	}
	else
	{
		llwarns << "Attempting to set label on LLSpinCtrl constructed without one "
				<< getName() << llendl;
	}
}

void LLSpinCtrl::setAllowEdit(bool allow_edit)
{
	mEditor->setEnabled(allow_edit);
}

//virtual
void LLSpinCtrl::onTabInto()
{
	mEditor->onTabInto();
}

void LLSpinCtrl::reportInvalidData()
{
	make_ui_sound("UISndBadKeystroke");
}

//virtual
void LLSpinCtrl::draw()
{
	if (mDirty && mLabelBox)
	{
		mLabelBox->setColor(getEnabled() ? mTextEnabledColor
										 : mTextDisabledColor);
		mDirty = false;
	}
	LLUICtrl::draw();
}

//virtual
bool LLSpinCtrl::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (clicks > 0)
	{
		while (clicks--)
		{
			onDownBtn(this);
		}
	}
	else
	{
		while (clicks++)
		{
			onUpBtn(this);
		}
	}

	return true;
}

//virtual
bool LLSpinCtrl::handleKeyHere(KEY key, MASK mask)
{
	if (mEditor->hasFocus())
	{
		if (key == KEY_ESCAPE && mask == MASK_NONE)
		{
			// Text editors do not support revert normally (due to user
			// confusion) but not allowing revert on a spinner seems dangerous
			updateEditor();
			mEditor->resetScrollPosition();
			mEditor->setFocus(false);
			return true;
		}
		if (key == KEY_UP)
		{
			onUpBtn(this);
			return true;
		}
		if (key == KEY_DOWN)
		{
			onDownBtn(this);
			return true;
		}
	}
	return false;
}

//virtual
LLXMLNodePtr LLSpinCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_SPIN_CTRL_TAG);

	node->createChild("decimal_digits", true)->setIntValue(mPrecision);

	if (mLabelBox)
	{
		node->createChild("label", true)->setStringValue(mLabelBox->getText());
		node->createChild("label_width",
						  true)->setIntValue(mLabelBox->getRect().getWidth());
	}

	node->createChild("initial_val", true)->setFloatValue(mInitialValue);

	node->createChild("min_val", true)->setFloatValue(mMinValue);

	node->createChild("max_val", true)->setFloatValue(mMaxValue);

	node->createChild("increment", true)->setFloatValue(mIncrement);

	addColorXML(node, mTextEnabledColor, "text_enabled_color",
				"LabelTextColor");
	addColorXML(node, mTextDisabledColor, "text_disabled_color",
				"LabelDisabledColor");

	return node;
}

//static
LLView* LLSpinCtrl::fromXML(LLXMLNodePtr node, LLView* parent,
							LLUICtrlFactory* factory)
{
	std::string name = LL_SPIN_CTRL_TAG;
	node->getAttributeString("name", name);

	std::string label;
	node->getAttributeString("label", label);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	LLFontGL* font = LLView::selectFont(node);

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

	S32 label_width = llmin(40, rect.getWidth() - 40);
	node->getAttributeS32("label_width", label_width);

	bool allow_text_entry = true;
	node->getAttributeBool("allow_text_entry", allow_text_entry);

	LLUICtrlCallback callback = NULL;

	if (label.empty())
	{
		label.assign(node->getValue());
	}

	LLSpinCtrl* spinner = new LLSpinCtrl(name, rect, label, font,
										 callback, NULL,
										 initial_value, min_value, max_value,
										 increment, LLStringUtil::null,
										 label_width);

	spinner->setPrecision(precision);

	spinner->initFromXML(node, parent);
	spinner->setAllowEdit(allow_text_entry);

	return spinner;
}
