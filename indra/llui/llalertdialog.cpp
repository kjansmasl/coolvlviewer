/**
 * @file llalertdialog.cpp
 * @brief LLAlertDialog base class
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

#include "llalertdialog.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llkeyboard.h"
#include "llfunctorregistry.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llnotifications.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llxmlnode.h"

constexpr S32 MAX_ALLOWED_MSG_WIDTH = 400;
constexpr F32 DEFAULT_BUTTON_DELAY = 0.5f;
constexpr S32 DIALOG_VPAD = 16;
constexpr S32 DIALOG_HPAD = 25;
constexpr S32 BTN_HPAD = 8;

// Static members
LLControlGroup* LLAlertDialog::sSettings = NULL;
LLAlertDialog::URLLoader* LLAlertDialog::sURLLoader;

//static
void LLAlertDialog::initClass()
{
	LLNotificationChannel::buildChannel("Alerts", "Visible",
										LLNotificationFilters::filterBy<std::string>(&LLNotification::getType,
																					 "alert"));
	LLNotificationChannel::buildChannel("AlertModal", "Visible",
										LLNotificationFilters::filterBy<std::string>(&LLNotification::getType,
																					 "alertmodal"));
	gNotifications.getChannel("Alerts")->connectChanged(boost::bind(&onNewNotification,
																	_1,
																	false));
	gNotifications.getChannel("AlertModal")->connectChanged(boost::bind(&onNewNotification,
																		_1,
																		true));
}

//static
bool LLAlertDialog::onNewNotification(const LLSD& notify, bool is_modal)
{
	LLNotificationPtr notif = gNotifications.find(notify["id"].asUUID());
	if (notif)
	{
		if (notify["sigtype"].asString() == "add" ||
			notify["sigtype"].asString() == "load")
		{
			LLAlertDialog* dialog = new LLAlertDialog(notif, is_modal);
			dialog->show();
		}
		else if (notify["sigtype"].asString() == "change")
		{
			LLAlertDialog* dialog = getNamedInstance(notif->getID()).get();
			if (dialog)
			{
				dialog->show();
			}
			else
			{
				LLAlertDialog* dialog = new LLAlertDialog(notif, is_modal);
				dialog->show();
			}
		}
	}

	return false;
}

LLAlertDialog::LLAlertDialog(LLNotificationPtr notification, bool modal)
											// dummy size, will reshape below
:	LLModalDialog(notification->getLabel(), 100, 100, modal),
	LLInstanceTracker<LLAlertDialog, LLUUID>(notification->getID()),
	mDefaultOption(0),
	mCheck(NULL),
	mCaution(notification->getPriority() >= NOTIFICATION_PRIORITY_HIGH),
	mLabel(notification->getName()),
	mLineEditor(NULL),
	mNote(notification)
{
	mFont = LLFontGL::getFontSansSerif();
	const S32 line_height = llfloor(mFont->getLineHeight() + 0.99f);
	constexpr S32 EDITOR_HEIGHT = 20;

	LLNotificationFormPtr form = mNote->getForm();
	std::string edit_text_name;
	std::string edit_text_contents;
	bool is_password = false;

	setBackgroundVisible(true);
	setBackgroundOpaque(true);

	typedef std::vector<std::pair<std::string, std::string> > options_t;
	options_t supplied_options;

	// For now, get LLSD to iterator over form elements
	LLSD form_sd = form->asLLSD();

	S32 option_index = 0;
	for (LLSD::array_const_iterator it = form_sd.beginArray();
		 it != form_sd.endArray(); ++it)
	{
		std::string type = (*it)["type"].asString();
		if (type == "button")
		{
			if ((*it)["default"])
			{
				mDefaultOption = option_index;
			}

			supplied_options.emplace_back((*it)["name"].asString(),
										  (*it)["text"].asString());

			if (option_index == mNote->getURLOption())
			{
				mButtonData.emplace_back(this, nullptr, mNote->getURL());
			}
			else
			{
				mButtonData.emplace_back(this);
			}

			++option_index;
		}
		else if (type == "text")
		{
			edit_text_contents = (*it)["value"].asString();
			edit_text_name = (*it)["name"].asString();
		}
		else if (type == "password")
		{
			edit_text_contents = (*it)["value"].asString();
			edit_text_name = (*it)["name"].asString();
			is_password = true;
		}
	}

	// Buttons
	options_t options;
	if (supplied_options.empty())
	{
		options.emplace_back("close", "Close");
		// Add data for ok button.
		mButtonData.emplace_back(this);
		mDefaultOption = 0;
	}
	else
	{
		options = supplied_options;
	}

	S32 num_options = options.size();

	// Calc total width of buttons
	S32 button_width = 0;
	S32 sp = mFont->getWidth("OO");
	for (S32 i = 0; i < num_options; ++i)
	{
		S32 w = S32(mFont->getWidth(options[i].second) + 0.99f) + sp +
				2 * gButtonHPad;
		button_width = llmax(w, button_width);
	}
	S32 btn_total_width = button_width;
	if (num_options > 1)
	{
		btn_total_width = num_options * button_width +
						  (num_options - 1) * BTN_HPAD;
	}

	// Message: create text box using raw string, as text has been structured
	// deliberately. Use size of created text box to generate dialog box size.
	std::string msg = mNote->getMessage();
	llwarns << "Alert: " << msg << llendl;
	LLTextBox* msg_box = new LLTextBox("Alert message", msg,
									   (F32)MAX_ALLOWED_MSG_WIDTH, mFont);

	const LLRect& text_rect = msg_box->getRect();
	S32 dialog_width = llmax(btn_total_width, text_rect.getWidth()) +
					    2 * DIALOG_HPAD;
	S32 dialog_height = text_rect.getHeight() + 3 * DIALOG_VPAD + gBtnHeight;

	if (hasTitleBar())
	{
		dialog_height += line_height; // room for title bar
	}

	// It's ok for the edit text body to be empty, but we want the name to
	// exist if we're going to draw it
	if (!edit_text_name.empty())
	{
		dialog_height += EDITOR_HEIGHT + DIALOG_VPAD;
		dialog_width = llmax(dialog_width,
							 (S32)(mFont->getWidth(edit_text_contents) + 0.99f));
	}

	if (mCaution)
	{
		// Make room for the caution icon.
		dialog_width += 32 + DIALOG_HPAD;
	}

	reshape(dialog_width, dialog_height, false);

	S32 msg_y = getRect().getHeight() - DIALOG_VPAD;
	S32 msg_x = DIALOG_HPAD;
	if (hasTitleBar())
	{
		msg_y -= line_height; // room for title
	}

	if (mCaution)
	{
		LLIconCtrl* icon =
			new LLIconCtrl("icon",  LLRect(msg_x, msg_y, msg_x + 32, msg_y - 32),
						   "notify_caution_icon.tga");
		icon->setMouseOpaque(false);
		addChild(icon);
		msg_x += 32 + DIALOG_HPAD;
		msg_box->setColor(LLUI::sColorsGroup->getColor("AlertCautionTextColor"));
	}
	else
	{
		msg_box->setColor(LLUI::sColorsGroup->getColor("AlertTextColor"));
	}

	LLRect rect;
	rect.setLeftTopAndSize(msg_x, msg_y, text_rect.getWidth(),
						   text_rect.getHeight());
	msg_box->setRect(rect);
	addChild(msg_box);

	// Buttons
	S32 button_left = (getRect().getWidth() - btn_total_width) / 2;

	for (S32 i = 0; i < num_options; ++i)
	{
		LLRect button_rect;
		button_rect.setOriginAndSize(button_left, DIALOG_VPAD, button_width,
									 gBtnHeight);

		LLButton* btn = new LLButton(options[i].first, button_rect, "", "", "",
									 NULL, NULL, mFont, options[i].second,
									 options[i].second);

		mButtonData[i].mButton = btn;

		btn->setClickedCallback(&LLAlertDialog::onButtonPressed,
								(void*)(&mButtonData[i]));

		addChild(btn);

		if (i == mDefaultOption)
		{
			btn->setFocus(true);
		}

		button_left += button_width + BTN_HPAD;
	}

	// (Optional) Edit Box
	if (!edit_text_name.empty())
	{
		S32 y = (DIALOG_VPAD + DIALOG_VPAD / 2) + gBtnHeight;
		mLineEditor = new LLLineEditor(edit_text_name,
									   LLRect(DIALOG_HPAD, y + EDITOR_HEIGHT,
											  dialog_width - DIALOG_HPAD, y),
									   edit_text_contents,
									   LLFontGL::getFontSansSerif(),
									   STD_STRING_STR_LEN);

		// Make sure all edit keys get handled properly (DEV-22396)
		mLineEditor->setHandleEditKeysDirectly(true);

		addChild(mLineEditor);
	}

	if (mLineEditor)
	{
		mLineEditor->setDrawAsterixes(is_password);

		setEditTextArgs(notification->getSubstitutions());
	}

	std::string ignore_label;
	LLNotificationForm::EIgnoreType form_type = form->getIgnoreType();
	if (form_type == LLNotificationForm::IGNORE_WITH_DEFAULT_RESPONSE)
	{
		setCheckBox(gNotifications.getGlobalString("skipnexttime"),
					ignore_label);
	}
	else if (form_type == LLNotificationForm::IGNORE_WITH_LAST_RESPONSE)
	{
		setCheckBox(gNotifications.getGlobalString("alwayschoose"),
					ignore_label);
	}
}

// All logic for deciding not to show an alert is done here, so that the alert
// is valid until show() is called.
bool LLAlertDialog::show()
{
	// If this is a caution message, change the color and add an icon.
	setBackgroundColor(mCaution ? LLUI::sAlertCautionBoxColor
								: LLUI::sAlertBoxColor);

	startModal();
	gFloaterViewp->adjustToFitScreen(this);
	open();
 	setFocus(true);
	if (mLineEditor)
	{
		mLineEditor->setFocus(true);
		mLineEditor->selectAll();
	}
	if (mDefaultOption >= 0)
	{
		// Delay before enabling default button
		mDefaultBtnTimer.start();
		mDefaultBtnTimer.setTimerExpirySec(DEFAULT_BUTTON_DELAY);
	}

	// Attach to floater if necessary
	LLUUID context_key = mNote->getPayload()["context"].asUUID();
	LLFloaterNotificationContext* contextp =
		dynamic_cast<LLFloaterNotificationContext*>(
			LLNotificationContext::getNamedInstance(context_key).get());
	if (contextp && contextp->getFloater())
	{
		contextp->getFloater()->addDependentFloater(this, false);
	}
	return true;
}

bool LLAlertDialog::setCheckBox(const std::string& check_title,
								const std::string& check_control)
{
	const S32 line_height = llfloor(mFont->getLineHeight() + 0.99f);

	// Extend dialog for "check next time"
	S32 max_msg_width = getRect().getWidth() - 2 * DIALOG_HPAD;
	S32 check_width = S32(mFont->getWidth(check_title) + 0.99f) + 16;
	max_msg_width = llmax(max_msg_width, check_width);
	S32 dialog_width = max_msg_width + 2 * DIALOG_HPAD;

	S32 dialog_height = getRect().getHeight();
	dialog_height += line_height + line_height / 2;

	reshape(dialog_width, dialog_height, false);

	S32 msg_x = (getRect().getWidth() - max_msg_width) / 2;

	LLRect check_rect;
	check_rect.setOriginAndSize(msg_x,
								DIALOG_VPAD + gBtnHeight + line_height / 2,
								max_msg_width, line_height);

	mCheck = new LLCheckBoxCtrl("check", check_rect, check_title, mFont,
								onClickIgnore, this);
	addChild(mCheck);

	return true;
}

void LLAlertDialog::setVisible(bool visible)
{
	LLModalDialog::setVisible(visible);

	if (visible)
	{
		centerOnScreen();
		make_ui_sound("UISndAlert");
	}
}

void LLAlertDialog::onClose(bool app_quitting)
{
	LLModalDialog::onClose(app_quitting);
}

bool LLAlertDialog::hasTitleBar() const
{
	return isMinimizeable() || isCloseable() ||
		   // Or if it has a title...
		   (getCurrentTitle() != "" && getCurrentTitle() != " ");
}

//virtual
bool LLAlertDialog::handleKeyHere(KEY key, MASK mask)
{
	if (KEY_RETURN == key && mask == MASK_NONE)
	{
		LLModalDialog::handleKeyHere(key, mask);
		return true;
	}
	else if (KEY_RIGHT == key)
	{
		focusNextItem(false);
		return true;
	}
	else if (KEY_LEFT == key)
	{
		focusPrevItem(false);
		return true;
	}
	else if (KEY_TAB == key && mask == MASK_NONE)
	{
		focusNextItem(false);
		return true;
	}
	else if (KEY_TAB == key && mask == MASK_SHIFT)
	{
		focusPrevItem(false);
		return true;
	}
	else
	{
		return LLModalDialog::handleKeyHere(key, mask);
	}
}

//virtual
void LLAlertDialog::draw()
{
	// If the default button timer has just expired, activate the default
	// button
	if (mDefaultBtnTimer.hasExpired() && mDefaultBtnTimer.getStarted())
	{
		// prevent this block from being run more than once:
		mDefaultBtnTimer.stop();
		setDefaultBtn(mButtonData[mDefaultOption].mButton);
	}

	gl_drop_shadow(0, getRect().getHeight(), getRect().getWidth(), 0,
				   LLUI::sColorDropShadow, LLUI::sDropShadowFloater);

	LLModalDialog::draw();
}

void LLAlertDialog::setEditTextArgs(const LLSD& edit_args)
{
	if (mLineEditor)
	{
		std::string msg = mLineEditor->getText();
		mLineEditor->setText(msg);
	}
	else
	{
		llwarns << "Call done on dialog with no line editor" << llendl;
	}
}

//static
void LLAlertDialog::onButtonPressed(void* userdata)
{
	ButtonData* button_data = (ButtonData*)userdata;
	LLAlertDialog* self = button_data->mSelf;
	if (!self || !button_data) return;

	LLSD response = self->mNote->getResponseTemplate();
	if (self->mLineEditor)
	{
		response[self->mLineEditor->getName()] = self->mLineEditor->getValue();
	}
	response[button_data->mButton->getName()] = true;

	// If we declared a URL and chose the URL option, go to the url
	if (!button_data->mURL.empty() && sURLLoader)
	{
		sURLLoader->load(button_data->mURL);
	}

	self->mNote->respond(response);	// New notification reponse
	self->close();					// Delete self
}

//static
void LLAlertDialog::onClickIgnore(LLUICtrl* ctrl, void* user_data)
{
	LLAlertDialog* self = (LLAlertDialog*)user_data;
	if (!self) return;

	// Checkbox sometimes means "hide and do the default" and other times means
	// "warn me again". Yuck. JC
	bool check = ctrl->getValue();
	if (self->mNote->getForm()->getIgnoreType() ==
			LLNotificationForm::IGNORE_SHOW_AGAIN)
	{
		// Question was "show again" so invert value to get "ignore"
		check = !check;
	}

	self->mNote->setIgnored(check);
}
