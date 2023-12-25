/**
 * @file llnotificationsconsole.cpp
 * @brief Debugging console for unified notifications.
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

// Work-around for a spurious and bogus "LLNotificationChannelPtr( ... _1) may
// be used uninitialized" warning with new GCC versions (v11+ at least). HB
#if LL_GNUC
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include "llfloaternotificationsconsole.h"

#include "llbutton.h"
#include "llcombobox.h"
#include "llnotifications.h"
#include "llpanel.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llviewertexteditor.h"

constexpr S32 NOTIFICATION_PANEL_HEADER_HEIGHT = 20;
constexpr S32 HEADER_PADDING = 38;

///////////////////////////////////////////////////////////////////////////////
// LLNotificationChannelPanel class
///////////////////////////////////////////////////////////////////////////////

class LLNotificationChannelPanel : public LLPanel
{
public:
	LLNotificationChannelPanel(const std::string& channel_name);
	~LLNotificationChannelPanel();
	bool postBuild();

private:
	bool update(const LLSD& payload, bool passed_filter);
	static void toggleClick(void* user_data);
	static void onClickNotification(void* user_data);
	static void onClickNotificationReject(void* user_data);

private:
	LLNotificationChannelPtr mChannelPtr;
	LLNotificationChannelPtr mChannelRejectsPtr;

	LLScrollListCtrl* mNotifList;
	LLScrollListCtrl* mNotifRejectsList;
};

LLNotificationChannelPanel::LLNotificationChannelPanel(const std::string& channel_name)
:	LLPanel(channel_name),
	mNotifList(NULL),
	mNotifRejectsList(NULL)
{
	mChannelPtr = gNotifications.getChannel(channel_name);
	mChannelRejectsPtr =
		LLNotificationChannelPtr(LLNotificationChannel::buildChannel(channel_name + "rejects",
																	 mChannelPtr->getParentChannelName(),
																	 !boost::bind(mChannelPtr->getFilter(),
																				  _1)));
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_notifications_channel.xml");
}

LLNotificationChannelPanel::~LLNotificationChannelPanel()
{
	// Userdata for all records is a LLNotification* we need to clean up
	std::vector<LLScrollListItem*> data_list = mNotifList->getAllData();
	for (std::vector<LLScrollListItem*>::iterator it = data_list.begin(),
												  end = data_list.end();
		 it != end; ++it)
	{
		LLScrollListItem* item = *it;
		LLNotification* notif = (LLNotification*)item->getUserdata();
		delete notif;
		notif = NULL;
	}
}

bool LLNotificationChannelPanel::postBuild()
{
	LLButton* header_button = getChild<LLButton>("header");
	header_button->setLabel(mChannelPtr->getName());
	header_button->setClickedCallback(toggleClick, this);

	mChannelPtr->connectChanged(boost::bind(&LLNotificationChannelPanel::update,
											this, _1, true));
	mChannelRejectsPtr->connectChanged(boost::bind(&LLNotificationChannelPanel::update,
												   this, _1, false));

	mNotifList = getChild<LLScrollListCtrl>("notifications_list");
	mNotifList->setDoubleClickCallback(onClickNotification);
	mNotifList->setCallbackUserData(this);

	mNotifRejectsList = getChild<LLScrollListCtrl>("notification_rejects_list");
	mNotifRejectsList->setDoubleClickCallback(onClickNotificationReject);
	mNotifRejectsList->setCallbackUserData(this);

	return true;
}

//static
void LLNotificationChannelPanel::toggleClick(void* user_data)
{
	LLNotificationChannelPanel* self = (LLNotificationChannelPanel*)user_data;
	if (!self) return;

	LLButton* buttonp = self->getChild<LLButton>("header");
	bool state = buttonp->getToggleState();

	LLLayoutStack* stackp = dynamic_cast<LLLayoutStack*>(self->getParent());
	if (stackp)
	{
		stackp->collapsePanel(self, state);
	}

	// Turn off tab stop for collapsed panel
	self->mNotifList->setTabStop(!state);
	self->mNotifList->setVisible(!state);
	self->mNotifRejectsList->setTabStop(!state);
	self->mNotifRejectsList->setVisible(!state);
}

//static
void LLNotificationChannelPanel::onClickNotification(void* user_data)
{
	LLNotificationChannelPanel* self = (LLNotificationChannelPanel*)user_data;
	if (!self || !gFloaterViewp) return;

	LLScrollListCtrl* listp = self->mNotifList;
	if (!listp || !listp->getFirstSelected()) return;

	LLNotification* notifp =
		(LLNotification*)listp->getFirstSelected()->getUserdata();
	if (!notifp) return;

	LLFloaterNotification* childp = new LLFloaterNotification(notifp);
	if (childp)
	{
		LLFloater* parentp = gFloaterViewp->getParentFloater(self);
		if (parentp)
		{
			parentp->addDependentFloater(childp);
		}
	}
}

//static
void LLNotificationChannelPanel::onClickNotificationReject(void* user_data)
{
	LLNotificationChannelPanel* self = (LLNotificationChannelPanel*)user_data;
	if (!self) return;

	LLScrollListCtrl* list = self->mNotifRejectsList;
	if (!list || !list->getFirstSelected()) return;

	void* data = list->getFirstSelected()->getUserdata();
	if (!data) return;

	LLFloaterNotification* childp;
	childp = new LLFloaterNotification((LLNotification*)data);

	if (childp && gFloaterViewp)
	{
		LLFloater* parentp = gFloaterViewp->getParentFloater(self);
		if (parentp)
		{
			parentp->addDependentFloater(childp);
		}
	}
}

bool LLNotificationChannelPanel::update(const LLSD& payload,
										bool passed_filter)
{
	LLNotificationPtr notification = gNotifications.find(payload["id"].asUUID());
	if (notification)
	{
		LLSD row;
		row["columns"][0]["value"] = notification->getName();
		row["columns"][0]["column"] = "name";

		row["columns"][1]["value"] = notification->getMessage();
		row["columns"][1]["column"] = "content";

		row["columns"][2]["value"] = notification->getDate();
		row["columns"][2]["column"] = "date";
		row["columns"][2]["type"] = "date";

		LLScrollListItem* sli = passed_filter ?
			getChild<LLScrollListCtrl>("notifications_list")->addElement(row) :
			getChild<LLScrollListCtrl>("notification_rejects_list")->addElement(row);
		sli->setUserdata(new LLNotification(notification->asLLSD()));
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterNotificationConsole class
///////////////////////////////////////////////////////////////////////////////

LLFloaterNotificationConsole::LLFloaterNotificationConsole(const LLSD& key)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_notifications_console.xml");
}

void LLFloaterNotificationConsole::onClose(bool app_quitting)
{
	setVisible(false);
}

bool LLFloaterNotificationConsole::postBuild()
{
	mLayoutStack = getChild<LLLayoutStack>("notification_channels");

	// These are in the order of processing
	addChannel("Unexpired");
	addChannel("Ignore");
	addChannel("Visible", true);
	// All the ones below attach to the Visible channel
	addChannel("History");
	addChannel("Alerts");
	addChannel("AlertModal");
	addChannel("Group Notifications");
	addChannel("Notifications");
	addChannel("NotificationTips");

	LLButton* buttonp = getChild<LLButton>("add_notification");
	buttonp->setClickedCallback(onClickAdd, this);

	mNotifTypesCombo = getChild<LLComboBox>("notification_types");
	LLNotifications::TemplateNames names = gNotifications.getTemplateNames();
	for (LLNotifications::TemplateNames::iterator it = names.begin(),
												  end = names.end();
		 it != end; ++it)
	{
		mNotifTypesCombo->add(*it);
	}
	mNotifTypesCombo->sortByName();

	return true;
}

void LLFloaterNotificationConsole::addChannel(const std::string& name,
											  bool open)
{
	LLNotificationChannelPanel* panelp = new LLNotificationChannelPanel(name);
	mLayoutStack->addPanel(panelp, 0, NOTIFICATION_PANEL_HEADER_HEIGHT, true,
						   true, LLLayoutStack::ANIMATE);

	LLButton* buttonp = panelp->getChild<LLButton>("header");
	buttonp->setToggleState(!open);
	mLayoutStack->collapsePanel(panelp, !open);

	updateResizeLimits();
}

void LLFloaterNotificationConsole::removeChannel(const std::string& name)
{
	LLPanel* panelp = getChild<LLPanel>(name.c_str(), true, false);
	if (panelp)
	{
		mLayoutStack->removePanel(panelp);
		delete panelp;
	}

	updateResizeLimits();
}

void LLFloaterNotificationConsole::updateResizeLimits()
{
	setResizeLimits(getMinWidth(),
					LLFLOATER_HEADER_SIZE + HEADER_PADDING +
					(NOTIFICATION_PANEL_HEADER_HEIGHT + 3) *
					mLayoutStack->getNumPanels());
}

//static
void LLFloaterNotificationConsole::onClickAdd(void* data)
{
	LLFloaterNotificationConsole* self = (LLFloaterNotificationConsole*)data;
	if (!self) return;

	std::string message_name = self->mNotifTypesCombo->getValue().asString();
	if (!message_name.empty())
	{
		gNotifications.add(message_name, LLSD());
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterNotification class
///////////////////////////////////////////////////////////////////////////////

LLFloaterNotification::LLFloaterNotification(LLNotification* notifp)
	// Do not store the pointer on the notification, but its Id !  HB
:	mNotificationId(notifp->getID())
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_notification.xml");

	setTitle(notifp->getName());
	getChild<LLViewerTextEditor>("payload")->setText(notifp->getMessage());

	mResponsesCombo = getChild<LLComboBox>("response");
	LLNotificationFormPtr formp = notifp->getForm();
	if (!formp)
	{
		mResponsesCombo->setEnabled(false);
		return;
	}

	mResponsesCombo->setCommitCallback(onCommitResponse);
	mResponsesCombo->setCallbackUserData(this);

	std::string text;
	LLSD form_sd = formp->asLLSD();
	for (LLSD::array_const_iterator form_item = form_sd.beginArray();
		 form_item != form_sd.endArray(); ++form_item)
	{
		if ((*form_item)["type"].asString() == "button")
		{
			text = (*form_item)["text"].asString();
			mResponsesCombo->addSimpleElement(text);
		}
	}
	mResponsesCombo->setEnabled(mResponsesCombo->getItemCount() > 0);
}

void LLFloaterNotification::respond()
{
	LLNotificationPtr notifp = gNotifications.find(mNotificationId);
	if (notifp)	// NULL *will* happen after the notification is gone !  HB
	{
		LLSD response = notifp->getResponseTemplate();
		response[mResponsesCombo->getSelectedValue().asString()] = true;
		notifp->respond(response);
	}
}

//static
void LLFloaterNotification::onCommitResponse(LLUICtrl*, void* userdata)
{
	LLFloaterNotification* self = (LLFloaterNotification*)userdata;
	if (self)
	{
		self->respond();
	}
}
