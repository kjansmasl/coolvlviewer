/**
 * @file llcheckboxctrl.cpp
 * @brief LLCheckBoxCtrl base class
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

// The mutants are coming!

#include "linden_common.h"

#include "llcheckboxctrl.h"

#include "llcontrol.h"
#include "llgl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

static const std::string LL_CHECK_BOX_CTRL_TAG = "check_box";
static LLRegisterWidget<LLCheckBoxCtrl> r02(LL_CHECK_BOX_CTRL_TAG);

LLCheckBoxCtrl::LLCheckBoxCtrl(const std::string& name,
							   const LLRect& rect,
							   const std::string& label,
							   const LLFontGL* font,
							   void (*commit_callback)(LLUICtrl*, void*),
							   void* callback_user_data,
							   bool initial_value,
							   bool use_radio_style,
							   const char* control_name)
:	LLUICtrl(name, rect, true, commit_callback, callback_user_data,
			 FOLLOWS_LEFT | FOLLOWS_TOP),
	mTextEnabledColor(LLUI::sLabelTextColor),
	mTextDisabledColor(LLUI::sLabelDisabledColor),
	mRadioStyle(use_radio_style),
	mInitialValue(initial_value),
	mSetValue(initial_value)
{
	if (font)
	{
		mFont = font;
	}
	else
	{
		mFont = LLFontGL::getFontSansSerifSmall();
	}

	// Must be big enough to hold all children
	setUseBoundingRect(true);

	// Label (add a little space to make sure text actually renders)
	constexpr S32 FUDGE = 10;
	S32 text_width = mFont->getWidth(label) + FUDGE;
	S32 text_height = ll_roundp(mFont->getLineHeight());
	LLRect label_rect;
	label_rect.setOriginAndSize(LLCHECKBOXCTRL_HPAD + LLCHECKBOXCTRL_BTN_SIZE +
								LLCHECKBOXCTRL_SPACING,
								LLCHECKBOXCTRL_VPAD + 1,
								text_width + LLCHECKBOXCTRL_HPAD, text_height);
#if 0
	// *HACK Get rid of this with SL-55508...
	// this allows blank check boxes and radio boxes for now
	std::string local_label = label;
	if (local_label.empty())
	{
		local_label = " ";
	}
	mLabel = new LLTextBox("CheckboxCtrl Label", label_rect, local_label,
						   mFont);
#else
	mLabel = new LLTextBox("CheckboxCtrl Label", label_rect, label, mFont);
#endif

	mLabel->setFollowsLeft();
	mLabel->setFollowsBottom();
	addChild(mLabel);

	// Button
	// Note: button cover the label by extending all the way to the right.
	LLRect btn_rect;
	btn_rect.setOriginAndSize(LLCHECKBOXCTRL_HPAD, LLCHECKBOXCTRL_VPAD,
							  LLCHECKBOXCTRL_BTN_SIZE +
							  LLCHECKBOXCTRL_SPACING + text_width +
							  LLCHECKBOXCTRL_HPAD,
		llmax(text_height, LLCHECKBOXCTRL_BTN_SIZE) + LLCHECKBOXCTRL_VPAD);
	std::string active_true_id, active_false_id;
	std::string inactive_true_id, inactive_false_id;
	if (mRadioStyle)
	{
		active_true_id = "UIImgRadioActiveSelectedUUID";
		active_false_id = "UIImgRadioActiveUUID";
		inactive_true_id = "UIImgRadioInactiveSelectedUUID";
		inactive_false_id = "UIImgRadioInactiveUUID";
		mButton = new LLButton("Radio control button", btn_rect,
							   active_false_id, active_true_id, control_name,
							   &LLCheckBoxCtrl::onButtonPress, this,
							   LLFontGL::getFontSansSerif());
		mButton->setDisabledImages(inactive_false_id, inactive_true_id);
		mButton->setHoverGlowStrength(0.35f);
	}
	else
	{
		active_false_id = "UIImgCheckboxActiveUUID";
		active_true_id = "UIImgCheckboxActiveSelectedUUID";
		inactive_true_id = "UIImgCheckboxInactiveSelectedUUID";
		inactive_false_id = "UIImgCheckboxInactiveUUID";
		mButton = new LLButton("Checkbox control button", btn_rect,
							   active_false_id, active_true_id, control_name,
							   &LLCheckBoxCtrl::onButtonPress, this,
							   LLFontGL::getFontSansSerif());
		mButton->setDisabledImages(inactive_false_id, inactive_true_id);
		mButton->setHoverGlowStrength(0.35f);
	}
	mButton->setIsToggle(true);
	mButton->setToggleState(initial_value);
	mButton->setFollowsLeft();
	mButton->setFollowsBottom();
	mButton->setCommitOnReturn(false);
	addChild(mButton);
}

LLCheckBoxCtrl::~LLCheckBoxCtrl()
{
	// Children all cleaned up by default view destructor.
}

//static
void LLCheckBoxCtrl::onButtonPress(void* userdata)
{
	LLCheckBoxCtrl* self = (LLCheckBoxCtrl*)userdata;
	if (!self) return;

	if (self->mRadioStyle)
	{
		self->setValue(true);
	}

	self->setControlValue(self->getValue());
	// *HACK: because buttons do not normally commit
	self->onCommit();

	if (!self->getIsChrome())
	{
		self->setFocus(true);
		self->onFocusReceived();
	}
}

void LLCheckBoxCtrl::onCommit()
{
	if (getEnabled())
	{
		setTentative(false);
		LLUICtrl::onCommit();
	}
}

void LLCheckBoxCtrl::setEnabled(bool b)
{
	LLView::setEnabled(b);
	mButton->setEnabled(b);
}

void LLCheckBoxCtrl::clear()
{
	setValue(false);
}

void LLCheckBoxCtrl::reshape(S32 width, S32 height, bool called_from_parent)
{
	// Stretch or shrink bounding rectangle of label when rebuilding UI at new
	// scale
	constexpr S32 FUDGE = 10;
	S32 text_width = mFont->getWidth(mLabel->getText()) + FUDGE;
	S32 text_height = ll_roundp(mFont->getLineHeight());
	LLRect label_rect;
	label_rect.setOriginAndSize(LLCHECKBOXCTRL_HPAD + LLCHECKBOXCTRL_BTN_SIZE +
								LLCHECKBOXCTRL_SPACING,
								LLCHECKBOXCTRL_VPAD, text_width, text_height);
	mLabel->setRect(label_rect);

	LLRect btn_rect;
	btn_rect.setOriginAndSize(LLCHECKBOXCTRL_HPAD, LLCHECKBOXCTRL_VPAD,
							  LLCHECKBOXCTRL_BTN_SIZE +
							  LLCHECKBOXCTRL_SPACING + text_width,
							  llmax(text_height, LLCHECKBOXCTRL_BTN_SIZE));
	mButton->setRect(btn_rect);

	LLUICtrl::reshape(width, height, called_from_parent);
}

void LLCheckBoxCtrl::draw()
{
	if (getEnabled())
	{
		mLabel->setColor(mTextEnabledColor);
	}
	else
	{
		mLabel->setColor(mTextDisabledColor);
	}

	// Draw children
	LLUICtrl::draw();
}

//virtual
void LLCheckBoxCtrl::setValue(const LLSD& value)
{
	mButton->setValue(value);
}

//virtual
LLSD LLCheckBoxCtrl::getValue() const
{
	return mButton->getValue();
}

void LLCheckBoxCtrl::setLabel(const std::string& label)
{
	mLabel->setText(label);
	reshape(getRect().getWidth(), getRect().getHeight(), false);
}

std::string LLCheckBoxCtrl::getLabel() const
{
	return mLabel->getText();
}

bool LLCheckBoxCtrl::setLabelArg(const std::string& key,
								 const std::string& text)
{
	bool res = mLabel->setTextArg(key, text);
	reshape(getRect().getWidth(), getRect().getHeight(), false);
	return res;
}

//virtual
const std::string& LLCheckBoxCtrl::getControlName() const
{
	return mButton->getControlName();
}

//virtual
void LLCheckBoxCtrl::setControlName(const char* control_name, LLView* context)
{
	mButton->setControlName(control_name, context);
}

// Returns true if the user has modified this control.
//virtual
bool LLCheckBoxCtrl::isDirty() const
{
	return mButton && mSetValue != mButton->getToggleState();
}

// Clear dirty state
//virtual
void LLCheckBoxCtrl::resetDirty()
{
	if (mButton)
	{
		mSetValue = mButton->getToggleState();
	}
}

//virtual
LLXMLNodePtr LLCheckBoxCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_CHECK_BOX_CTRL_TAG);

	node->createChild("label", true)->setStringValue(mLabel->getText());

	node->createChild("initial_value", true)->setBoolValue(mInitialValue);

	node->createChild("font", true)->setStringValue(LLFontGL::nameFromFont(mFont));

	node->createChild("radio_style", true)->setBoolValue(mRadioStyle);

	return node;
}

//static
LLView* LLCheckBoxCtrl::fromXML(LLXMLNodePtr node, LLView* parent,
								LLUICtrlFactory* factory)
{
	std::string name = "checkbox";
	node->getAttributeString("name", name);

	std::string label("");
	node->getAttributeString("label", label);

	LLFontGL* font = LLView::selectFont(node);

	// if true, draw radio button style icons:
	bool radio_style = false;
	node->getAttributeBool("radio_style", radio_style);

	LLUICtrlCallback callback = NULL;

	if (label.empty())
	{
		label.assign(node->getTextContents());
	}

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	LLCheckBoxCtrl* checkbox = new LLCheckBoxCtrl(name, rect, label, font,
												  callback, NULL, false,
												  radio_style);

	bool initial_value = checkbox->getValue().asBoolean();
	node->getAttributeBool("initial_value", initial_value);

	LLColor4 color = checkbox->mTextEnabledColor;
	LLUICtrlFactory::getAttributeColor(node,"text_enabled_color", color);
	checkbox->setEnabledColor(color);

	color = checkbox->mTextDisabledColor;
	LLUICtrlFactory::getAttributeColor(node,"text_disabled_color", color);
	checkbox->setDisabledColor(color);

	checkbox->setValue(initial_value);

	checkbox->initFromXML(node, parent);

	return checkbox;
}
