/**
 * @file llprefsim.cpp
 * @author James Cook, Richard Nelson
 * @brief Instant messsage preferences panel
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llprefsim.h"

#include "lldir.h"
#include "llcheckboxctrl.h"
#include "hbfileselector.h"
#include "llnotifications.h"
#include "llpanel.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llgridmanager.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llweb.h"

// Some constants
static const char* const VISIBILITY_DEFAULT = "default";
static const char* const VISIBILITY_HIDDEN = "hidden";

class LLPrefsIMImpl final : public LLPanel
{
public:
	LLPrefsIMImpl();
	~LLPrefsIMImpl() override;

	bool postBuild() override;
	void draw() override;

	void apply();
	void cancel();
	void setPersonalInfo(const std::string& visibility, bool im_via_email,
						 const std::string& email, S32 verified);

private:
	void enableHistory();
	void enableBacklog();

	static void setLogPathCallback(std::string& dir_name, void* user_data);
	static void onClickLogPath(void* user_data);
	static void onCommitLogging(LLUICtrl* ctrl, void* user_data);
	static void onCommitBacklog(LLUICtrl* ctrl, void* user_data);
	static void onClickEmailSettings(void* user_data);
	static void onOpenHelp(void* user_data);

private:
	LLCheckBoxCtrl*			mOnlineVisibilityCheck;
	LLCheckBoxCtrl*			mSendIMToEmailCheck;
	LLTextBox*				mEmailSettingsTextBox;

	std::string				mDirectoryVisibility;

	U32						mGroupIMSnoozeDuration;

	bool					mGotPersonalInfo;
	bool					mIMViaEmail;
	bool					mHideOnlineStatus;

	static LLPrefsIMImpl*	sInstance;
};

LLPrefsIMImpl* LLPrefsIMImpl::sInstance = NULL;

LLPrefsIMImpl::LLPrefsIMImpl()
:	LLPanel(std::string("IM Prefs Panel")),
	mGotPersonalInfo(false),
	mIMViaEmail(false)
{
	sInstance = this;
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_preferences_im.xml");
}

//virtual
LLPrefsIMImpl::~LLPrefsIMImpl()
{
	sInstance = NULL;
}

//virtual
bool LLPrefsIMImpl::postBuild()
{
	// Help button
	childSetAction("busy_response_help", onOpenHelp, this);

	// Do not enable the following controls until we get personal data

	mOnlineVisibilityCheck = getChild<LLCheckBoxCtrl>("online_visibility");
	mOnlineVisibilityCheck->setEnabled(false);

	mSendIMToEmailCheck = getChild<LLCheckBoxCtrl>("send_im_to_email");
	mSendIMToEmailCheck->setLabelArg("[EMAIL]", getString("log_in_to_change"));
	mSendIMToEmailCheck->setEnabled(false);
	// Note: support for setting the IM to email redirection with the viewer
	// has been removed from SL in November 2021... In SL we replace the check
	// box with a text box that, when clicked, opens the corresponding account
	// settings web page on SL's site... HB
	mEmailSettingsTextBox = getChild<LLTextBox>("email_settings_text");
	if (gIsInSecondLife && LLStartUp::isLoggedIn())
	{
		mEmailSettingsTextBox->setColor(LLTextEditor::getLinksColor());
		mEmailSettingsTextBox->setClickedCallback(onClickEmailSettings, this);
		mEmailSettingsTextBox->setVisible(true);
		mSendIMToEmailCheck->setVisible(false);
	}
	else
	{
		mEmailSettingsTextBox->setVisible(false);
		mSendIMToEmailCheck->setVisible(true);
	}

	childDisable("log_instant_messages");
	childDisable("log_chat");
	childDisable("log_show_history");
	childDisable("backlog_size");
	childDisable("log_open_in_built_in_browser");
	childDisable("log_path_button");
	childDisable("busy_response");
	childDisable("busy_response_when_away");
	childDisable("log_instant_messages_timestamp");
	childDisable("log_server_fetch");
	childDisable("log_chat_timestamp");
	childDisable("log_chat_IM");
	childDisable("logfile_name_datestamp");
	childDisable("logfile_name_resident");
	childSetVisible("logfile_name_resident", gIsInSecondLife);

	// Set the other controls flolowing the corresponding settings

	childSetText("busy_response", getString("log_in_to_change"));
	childSetValue("busy_response_when_away",
				  gSavedPerAccountSettings.getBool("BusyResponseWhenAway"));

	childSetValue("include_im_in_chat_console",
				  gSavedSettings.getBool("IMInChatConsole"));
	childSetValue("show_timestamps_check",
				  gSavedSettings.getBool("IMShowTimestamps"));
	childSetValue("open_on_incoming_check",
				  gSavedSettings.getBool("IMOpenSessionOnIncoming"));

	childSetText("log_path_string",
				 gSavedPerAccountSettings.getString("InstantMessageLogPath"));
	childSetValue("log_instant_messages",
				  gSavedPerAccountSettings.getBool("LogInstantMessages"));
	childSetValue("log_chat",
				  gSavedPerAccountSettings.getBool("LogChat"));
	childSetValue("log_show_history",
				  gSavedPerAccountSettings.getBool("LogShowHistory"));
	childSetValue("backlog_size",
				  (S32)gSavedPerAccountSettings.getU32("LogShowHistoryMaxSize"));
	childSetValue("log_open_in_built_in_browser",
				  gSavedPerAccountSettings.getBool("OpenIMLogsInBuiltInBrowser"));
	childSetValue("log_instant_messages_timestamp",
				  gSavedPerAccountSettings.getBool("IMLogTimestamp"));
	childSetValue("log_server_fetch",
				  gSavedPerAccountSettings.getBool("FetchGroupChatHistory"));
	childSetValue("log_chat_timestamp",
				  gSavedPerAccountSettings.getBool("LogChatTimestamp"));
	childSetValue("log_chat_IM",
				  gSavedPerAccountSettings.getBool("LogChatIM"));
	childSetValue("log_date_timestamp",
				  gSavedSettings.getBool("LogTimestampDate"));
	childSetValue("log_seconds_timestamp",
				  gSavedSettings.getBool("LogTimestampSeconds"));
	childSetValue("logfile_name_datestamp",
				  gSavedPerAccountSettings.getBool("LogFileNamewithDate"));
	childSetValue("logfile_name_resident",
				  gSavedPerAccountSettings.getBool("LogFileNameWithoutResident"));

	childSetAction("log_path_button", onClickLogPath, this);
	childSetCommitCallback("log_chat", onCommitLogging, this);
	childSetCommitCallback("log_instant_messages", onCommitLogging, this);
	childSetCommitCallback("log_show_history", onCommitBacklog, this);

	mGroupIMSnoozeDuration = gSavedSettings.getU32("GroupIMSnoozeDuration");

	return true;
}

//virtual
void LLPrefsIMImpl::draw()
{
	static bool was_selector_in_use = false;

	bool is_selector_in_use = HBFileSelector::isInUse();
	if (is_selector_in_use != was_selector_in_use)
	{
		was_selector_in_use = is_selector_in_use;
		childSetEnabled("log_path_button", !is_selector_in_use);
	}

	LLPanel::draw();
}

void LLPrefsIMImpl::enableHistory()
{
	bool log_ims = childGetValue("log_instant_messages").asBoolean();
	bool log_chat = childGetValue("log_chat").asBoolean();

	childSetEnabled("log_path_button", log_ims || log_chat);

	childSetEnabled("log_show_history", log_ims);
	enableBacklog();
}

void LLPrefsIMImpl::enableBacklog()
{
	bool enable = childGetValue("log_instant_messages").asBoolean() &&
				  childGetValue("log_show_history").asBoolean();

	childSetEnabled("backlog_size", enable);
	childSetEnabled("log_open_in_built_in_browser", enable);
	enable = childGetValue("log_show_history").asBoolean() &&
			 gAgent.hasRegionCapability("ChatSessionRequest");
	childSetEnabled("log_server_fetch", enable);
}

void LLPrefsIMImpl::cancel()
{
	gSavedSettings.setU32("GroupIMSnoozeDuration", mGroupIMSnoozeDuration);
}

void LLPrefsIMImpl::apply()
{
	LLTextEditor* busy = getChild<LLTextEditor>("busy_response");
	LLWString busy_response;
	if (busy)
	{
		busy_response = busy->getWText();
	}
	LLWStringUtil::replaceTabsWithSpaces(busy_response, 4);
	LLWStringUtil::replaceChar(busy_response, '\n', '^');
	LLWStringUtil::replaceChar(busy_response, ' ', '%');

	// Needed since cancel() is called on panel closing !
	mGroupIMSnoozeDuration = gSavedSettings.getU32("GroupIMSnoozeDuration");

	if (mGotPersonalInfo)
	{
		gSavedPerAccountSettings.setString("BusyModeResponse",
										   wstring_to_utf8str(busy_response));
		gSavedPerAccountSettings.setBool("BusyResponseWhenAway",
										 childGetValue("busy_response_when_away").asBoolean());

		gSavedSettings.setBool("IMInChatConsole",
							   childGetValue("include_im_in_chat_console").asBoolean());
		gSavedSettings.setBool("IMShowTimestamps",
							   childGetValue("show_timestamps_check").asBoolean());
		gSavedSettings.setBool("IMOpenSessionOnIncoming",
							   childGetValue("open_on_incoming_check").asBoolean());

		gSavedPerAccountSettings.setString("InstantMessageLogPath",
										   childGetText("log_path_string"));
		gSavedPerAccountSettings.setBool("LogInstantMessages",
										 childGetValue("log_instant_messages").asBoolean());
		gSavedPerAccountSettings.setBool("LogChat",
										 childGetValue("log_chat").asBoolean());
		gSavedPerAccountSettings.setBool("LogShowHistory",
										 childGetValue("log_show_history").asBoolean());
		gSavedPerAccountSettings.setU32("LogShowHistoryMaxSize",
										childGetValue("backlog_size").asInteger());
		gSavedPerAccountSettings.setBool("OpenIMLogsInBuiltInBrowser",
										 childGetValue("log_open_in_built_in_browser").asInteger());
		gSavedPerAccountSettings.setBool("IMLogTimestamp",
										 childGetValue("log_instant_messages_timestamp").asBoolean());
		gSavedPerAccountSettings.setBool("FetchGroupChatHistory",
										 childGetValue("log_server_fetch").asBoolean());
		gSavedPerAccountSettings.setBool("LogChatTimestamp",
										 childGetValue("log_chat_timestamp").asBoolean());
		gSavedPerAccountSettings.setBool("LogChatIM",
										 childGetValue("log_chat_IM").asBoolean());
		gSavedSettings.setBool("LogTimestampDate",
							   childGetValue("log_date_timestamp").asBoolean());
		gSavedSettings.setBool("LogTimestampSeconds",
							   childGetValue("log_seconds_timestamp").asBoolean());
		gSavedPerAccountSettings.setBool("LogFileNamewithDate",
										 childGetValue("logfile_name_datestamp").asBoolean());
		gSavedPerAccountSettings.setBool("LogFileNameWithoutResident", childGetValue("logfile_name_resident").asBoolean());

		gDirUtilp->setChatLogsDir(gSavedPerAccountSettings.getString("InstantMessageLogPath"));

		gDirUtilp->setPerAccountChatLogsDir(LLGridManager::getInstance()->getGridLabel(),
											gLoginFirstName, gLoginLastName);
		LLFile::mkdir(gDirUtilp->getPerAccountChatLogsDir());

		bool new_im_via_email = mSendIMToEmailCheck->get();
		bool new_hide_online = mOnlineVisibilityCheck->get();
		if (new_im_via_email != mIMViaEmail ||
			new_hide_online != mHideOnlineStatus)
		{
			// This hack is because we are representing several different
			// possible strings with a single checkbox. Since most users
			// can only select between 2 values, we represent it as a
			// checkbox. This breaks down a little bit for liaisons, but
			// works out in the end.
			if (new_hide_online != mHideOnlineStatus)
			{
				if (new_hide_online)
				{
					mDirectoryVisibility = VISIBILITY_HIDDEN;
				}
				else
				{
					mDirectoryVisibility = VISIBILITY_DEFAULT;
				}
				// Update showonline value, otherwise multiple applies won't
				// work
				mHideOnlineStatus = new_hide_online;
			}
			gAgent.sendAgentUpdateUserInfo(new_im_via_email,
										   mDirectoryVisibility);
		}
	}
}

void LLPrefsIMImpl::setPersonalInfo(const std::string& visibility,
									bool im_via_email,
									const std::string& email, S32 verified)
{
	mGotPersonalInfo = true;
	mIMViaEmail = im_via_email;
	mDirectoryVisibility = visibility;

	if (visibility == VISIBILITY_DEFAULT)
	{
		mHideOnlineStatus = false;
		mOnlineVisibilityCheck->setEnabled(true);
	}
	else if (visibility == VISIBILITY_HIDDEN)
	{
		mHideOnlineStatus = true;
		mOnlineVisibilityCheck->setEnabled(true);
	}
	else
	{
		mHideOnlineStatus = true;
	}

	std::string email_status;
	switch (verified)
	{
		case 0:
			email_status = getString("unverified");
			break;

		case 1:
			email_status = getString("verified");
			break;

		default:
			email_status = getString("unknown");
	}

	mSendIMToEmailCheck->setEnabled(verified);
	mSendIMToEmailCheck->set(im_via_email);
	mSendIMToEmailCheck->setToolTip(email_status);
	mEmailSettingsTextBox->setToolTip(email_status);

	mOnlineVisibilityCheck->set(mHideOnlineStatus);
	mOnlineVisibilityCheck->setLabelArg("[DIR_VIS]", mDirectoryVisibility);

	childEnable("log_instant_messages");
	childEnable("log_chat");
	childEnable("busy_response");
	childEnable("busy_response_when_away");
	childEnable("log_instant_messages_timestamp");
	if (gAgent.hasRegionCapability("ChatSessionRequest"))
	{
		childEnable("log_server_fetch");
	}
	childEnable("log_chat_timestamp");
	childEnable("log_chat_IM");
	childEnable("logfile_name_datestamp");
	childEnable("logfile_name_resident");

	// RN: get wide string so replace char can work (requires fixed-width
	// encoding).
	LLWString busy_response =
		utf8str_to_wstring(gSavedPerAccountSettings.getString("BusyModeResponse"));
	LLWStringUtil::replaceChar(busy_response, '^', '\n');
	LLWStringUtil::replaceChar(busy_response, '%', ' ');
	childSetText("busy_response", wstring_to_utf8str(busy_response));

	enableHistory();
	enableBacklog();

	std::string display_email(email);
	if (display_email.empty())
	{
		display_email = getString("unset");
	}
	// Truncate the e-mail address if it is too long (to prevent going off
	// the edge of the dialog).
	else if (display_email.size() > 30)
	{
		display_email.resize(30);
		display_email += "...";
	}
	mSendIMToEmailCheck->setLabelArg("[EMAIL]", display_email);
	mSendIMToEmailCheck->setToolTipArg("[EMAIL]", display_email);
	mEmailSettingsTextBox->setToolTipArg("[EMAIL]", display_email);
}

// static
void LLPrefsIMImpl::setLogPathCallback(std::string& dir_name, void* user_data)
{
	LLPrefsIMImpl* self = (LLPrefsIMImpl*)user_data;
	if (!self || self != sInstance)
	{
		gNotifications.add("PreferencesClosed");
		return;
	}
	if (!dir_name.empty())
	{
		self->childSetText("log_path_string", dir_name);
	}
}

// static
void LLPrefsIMImpl::onClickLogPath(void* user_data)
{
	LLPrefsIMImpl* self = (LLPrefsIMImpl*)user_data;
	if (self)
	{
		std::string suggestion = self->childGetText("log_path_string");
		HBFileSelector::pickDirectory(suggestion, setLogPathCallback, self);
	}
}

// static
void LLPrefsIMImpl::onCommitLogging(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsIMImpl* self=(LLPrefsIMImpl*)user_data;
	if (self)
	{
		self->enableHistory();
	}
}

// static
void LLPrefsIMImpl::onCommitBacklog(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsIMImpl* self= (LLPrefsIMImpl*)user_data;
	if (self)
	{
		self->enableBacklog();
	}
}

void LLPrefsIMImpl::onClickEmailSettings(void* user_data)
{
	LLPrefsIMImpl* self = (LLPrefsIMImpl*)user_data;
	if (self)
	{
		LLWeb::loadURL(self->getString("sl_email_url"));
	}
}

void LLPrefsIMImpl::onOpenHelp(void* user_data)
{
	LLPrefsIMImpl* self = (LLPrefsIMImpl*)user_data;
	if (self)
	{
		LLSD args;
		args["MESSAGE"] = self->getString("help_text");
		gNotifications.add("GenericAlert", args);
	}
}

//---------------------------------------------------------------------------

LLPrefsIM::LLPrefsIM()
:	impl(* new LLPrefsIMImpl())
{
}

LLPrefsIM::~LLPrefsIM()
{
	delete &impl;
}

void LLPrefsIM::apply()
{
	impl.apply();
}

void LLPrefsIM::cancel()
{
	impl.cancel();
}

void LLPrefsIM::setPersonalInfo(const std::string& visibility,
								bool im_via_email, const std::string& email,
								S32 verified)
{
	impl.setPersonalInfo(visibility, im_via_email, email, verified);
}

LLPanel* LLPrefsIM::getPanel()
{
	return &impl;
}
