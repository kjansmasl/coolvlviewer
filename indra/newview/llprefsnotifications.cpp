/**
 * @file llprefsnotifications.cpp
 * @brief Notifications preferences panel
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Adapted by Henri Beauchamp from llpanelmsgs.cpp
 * Copyright (c) 2001-2009 Linden Research, Inc. (c) 2011 Henri Beauchamp
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

#include "llprefsnotifications.h"

#include "llbutton.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llfirstuse.h"
#include "llviewercontrol.h"

class LLPrefsNotificationsImpl final : public LLPanel
{
public:
	LLPrefsNotificationsImpl();

	~LLPrefsNotificationsImpl() override
	{
	}

	void draw() override;

	void apply();
	void cancel();

	void buildLists();
	void resetAllIgnored();
	void setAllIgnored();
	void refreshValues();

private:
	static void onClickEnablePopup(void* user_data);
	static void onClickDisablePopup(void* user_data);
	static void onClickResetDialogs(void* user_data);
	static void onClickSkipDialogs(void* user_data);

private:
	LLButton*			mEnablePopupBtn;
	LLButton*			mDisablePopupBtn;
	LLScrollListCtrl*	mEnabledPopupsList;
	LLScrollListCtrl*	mDisabledPopupsList;

	U32					mLookAtNotifyDelay;
	bool				mAutoAcceptNewInventory;
	bool				mRejectNewInventoryWhenBusy;
	bool				mShowNewInventory;
	bool				mShowInInventory;
	bool				mNotifyMoneyChange;
	bool				mChatOnlineNotification;
	bool				mHideNotificationsInChat;
	bool				mScriptErrorsAsChat;
	bool				mTeleportHistoryInChat;
};

//-----------------------------------------------------------------------------
LLPrefsNotificationsImpl::LLPrefsNotificationsImpl()
:	LLPanel("Notifications Preferences Panel")
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_preferences_notifications.xml");

	mEnablePopupBtn = getChild<LLButton>("enable_popup");
	mEnablePopupBtn->setClickedCallback(onClickEnablePopup, this);

	mDisablePopupBtn = getChild<LLButton>("disable_popup");
	mDisablePopupBtn->setClickedCallback(onClickDisablePopup, this);

	childSetAction("reset_dialogs_btn", onClickResetDialogs, this);
	childSetAction("skip_dialogs_btn", onClickSkipDialogs, this);

	mEnabledPopupsList = getChild<LLScrollListCtrl>("enabled_popups");
	mDisabledPopupsList = getChild<LLScrollListCtrl>("disabled_popups");

	refreshValues();
	buildLists();
}

void LLPrefsNotificationsImpl::refreshValues()
{
	mLookAtNotifyDelay = gSavedSettings.getU32("LookAtNotifyDelay");
	mAutoAcceptNewInventory = gSavedSettings.getBool("AutoAcceptNewInventory");
	mRejectNewInventoryWhenBusy	=
		gSavedSettings.getBool("RejectNewInventoryWhenBusy");
	mShowNewInventory = gSavedSettings.getBool("ShowNewInventory");
	mShowInInventory = gSavedSettings.getBool("ShowInInventory");
	mNotifyMoneyChange = gSavedSettings.getBool("NotifyMoneyChange");
	mChatOnlineNotification = gSavedSettings.getBool("ChatOnlineNotification");
	mHideNotificationsInChat = gSavedSettings.getBool("HideNotificationsInChat");
	mScriptErrorsAsChat = gSavedSettings.getBool("ScriptErrorsAsChat");
	mTeleportHistoryInChat = gSavedSettings.getBool("TeleportHistoryInChat");
}

void LLPrefsNotificationsImpl::buildLists()
{
	mDisabledPopupsList->deleteAllItems();
	mEnabledPopupsList->deleteAllItems();

	std::string cname;
	for (LLNotifications::TemplateMap::const_iterator
			iter = gNotifications.templatesBegin(),
			end = gNotifications.templatesEnd();
		 iter != end; ++iter)
	{
		LLNotificationTemplatePtr templatep = iter->second;
		LLNotificationFormPtr formp = templatep->mForm;

		LLNotificationForm::EIgnoreType ignore = formp->getIgnoreType();
		if (ignore == LLNotificationForm::IGNORE_NO) continue;

		LLSD row;
		row["columns"][0]["value"] = formp->getIgnoreMessage();
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		row["columns"][0]["width"] = 300;

		LLScrollListItem* item = NULL;

		bool show_popup = gSavedSettings.getWarning(templatep->mName);
		if (!show_popup)
		{
			if (ignore == LLNotificationForm::IGNORE_WITH_LAST_RESPONSE)
			{
				cname = "Default" + templatep->mName;
				LLSD last_response = LLUI::sConfigGroup->getLLSD(cname.c_str());
				if (!last_response.isUndefined())
				{
					for (LLSD::map_const_iterator it = last_response.beginMap();
						 it != last_response.endMap(); ++it)
					{
						if (it->second.asBoolean())
						{
							row["columns"][1]["value"] =
								formp->getElement(it->first)["ignore"].asString();
							break;
						}
					}
				}
				row["columns"][1]["font"] = "SANSSERIF_SMALL";
				row["columns"][1]["width"] = 160;
			}
			item = mDisabledPopupsList->addElement(row, ADD_SORTED);
		}
		else
		{
			item = mEnabledPopupsList->addElement(row, ADD_SORTED);
		}

		if (item)
		{
			item->setUserdata((void*)&iter->first);
		}
	}
}

void LLPrefsNotificationsImpl::draw()
{
	mEnablePopupBtn->setEnabled(mDisabledPopupsList->getFirstSelected() != NULL);
	mDisablePopupBtn->setEnabled(mEnabledPopupsList->getFirstSelected() != NULL);
	LLPanel::draw();
}

void LLPrefsNotificationsImpl::apply()
{
	refreshValues();
}

void LLPrefsNotificationsImpl::cancel()
{
	gSavedSettings.setU32("LookAtNotifyDelay", mLookAtNotifyDelay);
	gSavedSettings.setBool("AutoAcceptNewInventory", mAutoAcceptNewInventory);
	gSavedSettings.setBool("RejectNewInventoryWhenBusy",
						   mRejectNewInventoryWhenBusy);
	gSavedSettings.setBool("ShowNewInventory", mShowNewInventory);
	gSavedSettings.setBool("ShowInInventory", mShowInInventory);
	gSavedSettings.setBool("NotifyMoneyChange", mNotifyMoneyChange);
	gSavedSettings.setBool("ChatOnlineNotification", mChatOnlineNotification);
	gSavedSettings.setBool("HideNotificationsInChat", mHideNotificationsInChat);
	gSavedSettings.setBool("ScriptErrorsAsChat", mScriptErrorsAsChat);
	gSavedSettings.setBool("TeleportHistoryInChat", mTeleportHistoryInChat);
}

void LLPrefsNotificationsImpl::resetAllIgnored()
{
	LLNotifications::TemplateMap::const_iterator iter;
	for (LLNotifications::TemplateMap::const_iterator
			iter = gNotifications.templatesBegin(),
			end = gNotifications.templatesEnd();
		 iter != end; ++iter)
	{
		if (iter->second->mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
		{
			gSavedSettings.setWarning(iter->first, true);
		}
	}
}

void LLPrefsNotificationsImpl::setAllIgnored()
{
	for (LLNotifications::TemplateMap::const_iterator
			iter = gNotifications.templatesBegin(),
			end = gNotifications.templatesEnd();
		 iter != end; ++iter)
	{
		if (iter->second->mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
		{
			gSavedSettings.setWarning(iter->first, false);
		}
	}
}

//static
void LLPrefsNotificationsImpl::onClickEnablePopup(void* user_data)
{
	LLPrefsNotificationsImpl* panelp = (LLPrefsNotificationsImpl*)user_data;
	if (!panelp) return;

	std::vector<LLScrollListItem*> items =
		panelp->mDisabledPopupsList->getAllSelected();
	std::vector<LLScrollListItem*>::iterator iter;
	for (iter = items.begin(); iter != items.end(); ++iter)
	{
		LLNotificationTemplatePtr templatep =
			gNotifications.getTemplate(*(std::string*)((*iter)->getUserdata()));
		if (templatep->mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
		{
			gSavedSettings.setWarning(templatep->mName, true);
		}
	}

	panelp->buildLists();
}

//static
void LLPrefsNotificationsImpl::onClickDisablePopup(void* user_data)
{
	LLPrefsNotificationsImpl* panelp = (LLPrefsNotificationsImpl*)user_data;
	if (!panelp) return;

	std::vector<LLScrollListItem*> items =
		panelp->mDisabledPopupsList->getAllSelected();
	std::vector<LLScrollListItem*>::iterator iter;
	for (iter = items.begin(); iter != items.end(); ++iter)
	{
		LLNotificationTemplatePtr templatep =
			gNotifications.getTemplate(*(std::string*)((*iter)->getUserdata()));
		if (templatep->mForm->getIgnoreType() != LLNotificationForm::IGNORE_NO)
		{
			gSavedSettings.setWarning(templatep->mName, false);
		}
	}

	panelp->buildLists();
}

bool callback_reset_dialogs(const LLSD& notification, const LLSD& response,
							LLPrefsNotificationsImpl* panelp)
{
	if (panelp &&
		LLNotification::getSelectedOption(notification, response) == 0)
	{
		panelp->resetAllIgnored();
		LLFirstUse::resetFirstUse();
		panelp->buildLists();
	}
	return false;
}

//static
void LLPrefsNotificationsImpl::onClickResetDialogs(void* user_data)
{
	gNotifications.add("ResetShowNextTimeDialogs", LLSD(), LLSD(),
					   boost::bind(&callback_reset_dialogs, _1, _2,
								   (LLPrefsNotificationsImpl*)user_data));
}

bool callback_skip_dialogs(const LLSD& notification, const LLSD& response,
						   LLPrefsNotificationsImpl* panelp)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		if (panelp)
		{
			panelp->setAllIgnored();
			LLFirstUse::disableFirstUse();
			panelp->buildLists();
		}
	}
	return false;
}

//static
void LLPrefsNotificationsImpl::onClickSkipDialogs(void* user_data)
{
	gNotifications.add("SkipShowNextTimeDialogs", LLSD(), LLSD(),
					   boost::bind(&callback_skip_dialogs, _1, _2,
								   (LLPrefsNotificationsImpl*)user_data));
}

//---------------------------------------------------------------------------

LLPrefsNotifications::LLPrefsNotifications()
:	impl(*new LLPrefsNotificationsImpl())
{
}

LLPrefsNotifications::~LLPrefsNotifications()
{
	delete &impl;
}

void LLPrefsNotifications::draw()
{
	impl.draw();
}

void LLPrefsNotifications::apply()
{
	impl.apply();
}

void LLPrefsNotifications::cancel()
{
	impl.cancel();
}

LLPanel* LLPrefsNotifications::getPanel()
{
	return &impl;
}
