/**
 * @file llfloaterpreference.cpp
 * @brief Global preferences with and without persistence.
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

/*
 * App-wide preferences.  Note that these are not per-user,
 * because we need to load many preferences before we have
 * a login name.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterpreference.h"

#include "llbutton.h"
#include "lldir.h"
#include "llresizehandle.h"			// For RESIZE_HANDLE_WIDTH
#include "llscrollbar.h"			// For SCROLLBAR_SIZE
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llcommandhandler.h"
#include "llfloaterabout.h"
#include "llgridmanager.h"
#include "hbpanelgrids.h"
#include "llpanellogin.h"
#include "llprefsskins.h"
#include "llprefschat.h"
#include "hbprefscool.h"
#include "llprefsgeneral.h"
#include "llprefsgraphics.h"
#include "llprefsim.h"
#include "llprefsinput.h"
#include "llprefsmedia.h"
#include "llprefsnetwork.h"
#include "llprefsnotifications.h"
#include "llprefsvoice.h"
#include "llviewercontrol.h"

constexpr S32 PREF_BORDER = 4;
constexpr S32 PREF_PAD = 5;
constexpr S32 PREF_BUTTON_WIDTH = 70;
constexpr S32 PREF_CATEGORY_WIDTH = 150;
constexpr S32 PREF_FLOATER_MIN_HEIGHT = 2 * SCROLLBAR_SIZE +
										2 * LLPANEL_BORDER_WIDTH + 96;

class LLPreferencesHandler final : public LLCommandHandler
{
public:
	// Requires a trusted browser
	LLPreferencesHandler()
	:	LLCommandHandler("preferences", UNTRUSTED_BLOCK)
	{
	}

	bool handle(const LLSD&, const LLSD&, LLMediaCtrl*) override
	{
		LLFloaterPreference::showInstance();
		return true;
	}
};
LLPreferencesHandler gPreferencesHandler;

// Must be done at run time, not compile time. JC
S32 pref_min_width()
{
	return 2 * PREF_BORDER + 2 * PREF_BUTTON_WIDTH + 2 * PREF_PAD +
		   RESIZE_HANDLE_WIDTH + PREF_CATEGORY_WIDTH;
}

S32 pref_min_height()
{
	return 2 * PREF_BORDER + 3 * (gBtnHeight + PREF_PAD) + PREF_FLOATER_MIN_HEIGHT;
}

LLPreferenceCore::LLPreferenceCore(LLTabContainer* tab_container,
								   LLButton* default_btn)
:	mTabContainer(tab_container)
{
	mPrefsGeneral = new LLPrefsGeneral();
	mTabContainer->addTabPanel(mPrefsGeneral->getPanel(),
							   mPrefsGeneral->getPanel()->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsGeneral->getPanel()->setDefaultBtn(default_btn);

	mPrefsInput = new LLPrefsInput();
	mTabContainer->addTabPanel(mPrefsInput->getPanel(),
							   mPrefsInput->getPanel()->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsInput->getPanel()->setDefaultBtn(default_btn);

	mPrefsNetwork = new LLPrefsNetwork();
	mTabContainer->addTabPanel(mPrefsNetwork, mPrefsNetwork->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsNetwork->setDefaultBtn(default_btn);

	mPrefsGraphics = new LLPrefsGraphics();
	mTabContainer->addTabPanel(mPrefsGraphics->getPanel(),
							   mPrefsGraphics->getPanel()->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsGraphics->getPanel()->setDefaultBtn(default_btn);

	mPrefsMedia = new LLPrefsMedia();
	mTabContainer->addTabPanel(mPrefsMedia->getPanel(),
							   mPrefsMedia->getPanel()->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsMedia->getPanel()->setDefaultBtn(default_btn);

	mPrefsChat = new LLPrefsChat();
	mTabContainer->addTabPanel(mPrefsChat->getPanel(),
							   mPrefsChat->getPanel()->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsChat->getPanel()->setDefaultBtn(default_btn);

	mPrefsIM = new LLPrefsIM();
	mTabContainer->addTabPanel(mPrefsIM->getPanel(),
							   mPrefsIM->getPanel()->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsIM->getPanel()->setDefaultBtn(default_btn);

	mPrefsVoice = new LLPrefsVoice();
	mTabContainer->addTabPanel(mPrefsVoice, mPrefsVoice->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsVoice->setDefaultBtn(default_btn);

	mPrefsNotifications = new LLPrefsNotifications();
	mTabContainer->addTabPanel(mPrefsNotifications->getPanel(),
							   mPrefsNotifications->getPanel()->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsNotifications->getPanel()->setDefaultBtn(default_btn);

	mPrefsSkins = new LLPrefSkins();
	mTabContainer->addTabPanel(mPrefsSkins, mPrefsSkins->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsSkins->setDefaultBtn(default_btn);

	mPrefsCool = new HBPrefsCool();
	mTabContainer->addTabPanel(mPrefsCool->getPanel(),
							   mPrefsCool->getPanel()->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsCool->getPanel()->setDefaultBtn(default_btn);

	mPrefsGrids = new HBPanelGrids();
	mTabContainer->addTabPanel(mPrefsGrids->getPanel(),
							   mPrefsGrids->getPanel()->getLabel(),
							   false, onTabChanged, mTabContainer);
	mPrefsGrids->getPanel()->setDefaultBtn(default_btn);

	if (!mTabContainer->selectTab(gSavedSettings.getS32("LastPrefTab")))
	{
		mTabContainer->selectFirstTab();
	}
}

LLPreferenceCore::~LLPreferenceCore()
{
	if (mPrefsGeneral)
	{
		delete mPrefsGeneral;
		mPrefsGeneral = NULL;
	}
	if (mPrefsInput)
	{
		delete mPrefsInput;
		mPrefsInput = NULL;
	}
	if (mPrefsNetwork)
	{
		delete mPrefsNetwork;
		mPrefsNetwork = NULL;
	}
	if (mPrefsGraphics)
	{
		delete mPrefsGraphics;
		mPrefsGraphics = NULL;
	}
	if (mPrefsMedia)
	{
		delete mPrefsMedia;
		mPrefsMedia = NULL;
	}
	if (mPrefsChat)
	{
		delete mPrefsChat;
		mPrefsChat = NULL;
	}
	if (mPrefsIM)
	{
		delete mPrefsIM;
		mPrefsIM = NULL;
	}
	if (mPrefsNotifications)
	{
		delete mPrefsNotifications;
		mPrefsNotifications = NULL;
	}
	if (mPrefsSkins)
	{
		delete mPrefsSkins;
		mPrefsSkins = NULL;
	}
	if (mPrefsCool)
	{
		delete mPrefsCool;
		mPrefsCool = NULL;
	}
	if (mPrefsGrids)
	{
		delete mPrefsGrids;
		mPrefsGrids = NULL;
	}
}

void LLPreferenceCore::apply()
{
	mPrefsGeneral->apply();
	mPrefsInput->apply();
	mPrefsNetwork->apply();
	mPrefsGraphics->apply();
	mPrefsMedia->apply();
	mPrefsChat->apply();
	mPrefsVoice->apply();
	mPrefsIM->apply();
	mPrefsNotifications->apply();
	mPrefsSkins->apply();
	mPrefsCool->apply();
	mPrefsGrids->apply();
}

void LLPreferenceCore::cancel()
{
	mPrefsGeneral->cancel();
	mPrefsInput->cancel();
	mPrefsNetwork->cancel();
	mPrefsGraphics->cancel();
	mPrefsMedia->cancel();
	mPrefsChat->cancel();
	mPrefsVoice->cancel();
	mPrefsIM->cancel();
	mPrefsNotifications->cancel();
	mPrefsSkins->cancel();
	mPrefsCool->cancel();
	mPrefsGrids->cancel();
}

//static
void LLPreferenceCore::onTabChanged(void* user_data, bool from_click)
{
	LLTabContainer* self = (LLTabContainer*)user_data;
	if (self)
	{
		gSavedSettings.setS32("LastPrefTab", self->getCurrentPanelIndex());
	}
}

void LLPreferenceCore::setPersonalInfo(const std::string& visibility,
									   bool im_via_email,
									   const std::string& email,
									   S32 verified)
{
	mPrefsIM->setPersonalInfo(visibility, im_via_email, email, verified);
}

//////////////////////////////////////////////
// LLFloaterPreference

LLFloaterPreference::LLFloaterPreference(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_preferences.xml");
}

//virtual
bool LLFloaterPreference::postBuild()
{
	mAboutBtn = getChild<LLButton>("About...");
	mAboutBtn->setClickedCallback(onClickAbout, this);

	mApplyBtn = getChild<LLButton>("Apply");
	mApplyBtn->setClickedCallback(onBtnApply, this);

	mCancelBtn = getChild<LLButton>("Cancel");
	mCancelBtn->setClickedCallback(onBtnCancel, this);

	mOKBtn = getChild<LLButton>("OK");
	mOKBtn->setClickedCallback(onBtnOK, this);

	mPreferenceCore = new LLPreferenceCore(getChild<LLTabContainer>("pref core"),
										   getChild<LLButton>("OK"));

	center();

	gAgent.sendAgentUserInfoRequest();
	LLPanelLogin::setAlwaysRefresh(true);

	return true;
}

//virtual
LLFloaterPreference::~LLFloaterPreference()
{
	if (mPreferenceCore)
	{
		delete mPreferenceCore;
		mPreferenceCore = NULL;
	}
}

void LLFloaterPreference::apply()
{
	if (mPreferenceCore)
	{
		mPreferenceCore->apply();
	}
}

void LLFloaterPreference::cancel()
{
	if (mPreferenceCore)
	{
		mPreferenceCore->cancel();
	}
}

//static
void LLFloaterPreference::openInTab(S32 tab)
{
	LLFloaterPreference* self = showInstance();
	if (!self) return;	// Could be out of memory...

	if (tab >= 0 && tab < NUMBER_OF_TABS)
	{
		gSavedSettings.setS32("LastPrefTab", tab);
		if (self->mPreferenceCore)
		{
			self->mPreferenceCore->getTabContainer()->selectTab(tab);
		}
	}
	else
	{
		llwarns << "Invalid tab number" << llendl;
	}
}

//static
void LLFloaterPreference::onClickAbout(void*)
{
	LLFloaterAbout::showInstance();
}

//static
void LLFloaterPreference::onBtnOK(void* userdata)
{
	LLFloaterPreference* self =(LLFloaterPreference*)userdata;
	if (!self) return;

	// Commit any outstanding text entry
	if (self->hasFocus())
	{
		LLUICtrl* cur_focus = gFocusMgr.getKeyboardFocusUICtrl();
		if (cur_focus && cur_focus->acceptsTextInput())
		{
			cur_focus->onCommit();
		}
	}

	if (self->canClose())
	{
		self->apply();
		self->close();
		gAppViewerp->saveGlobalSettings();
	}
	else
	{
		// *TODO ? Show beep, pop up dialog, etc.
		llwarns << "Cannot close preferences !" << llendl;
	}

	LLPanelLogin::refreshLocation();
}

//static
void LLFloaterPreference::onBtnApply(void* userdata)
{
	LLFloaterPreference* self =(LLFloaterPreference*)userdata;
	if (!self) return;

	if (self->hasFocus())
	{
		LLUICtrl* cur_focus = gFocusMgr.getKeyboardFocusUICtrl();
		if (cur_focus && cur_focus->acceptsTextInput())
		{
			cur_focus->onCommit();
		}
	}
	self->apply();

	LLPanelLogin::refreshLocation();
}

void LLFloaterPreference::onClose(bool app_quitting)
{
	LLPanelLogin::setAlwaysRefresh(false);
	cancel(); // Will be a no-op if OK or apply was performed just prior.
	LLFloater::onClose(app_quitting);
}

//static
void LLFloaterPreference::onBtnCancel(void* userdata)
{
	LLFloaterPreference* self =(LLFloaterPreference*)userdata;
	if (!self) return;

	if (self->hasFocus())
	{
		LLUICtrl* cur_focus = gFocusMgr.getKeyboardFocusUICtrl();
		if (cur_focus && cur_focus->acceptsTextInput())
		{
			cur_focus->onCommit();
		}
	}
	self->close(); // side effect will also cancel any unsaved changes.
}

//static
void LLFloaterPreference::updateUserInfo(const std::string& visibility,
										 bool im_via_email,
										 const std::string& email,
										 S32 verified)
{
	LLFloaterPreference* self = findInstance();
	if (self && self->mPreferenceCore)
	{
		self->mPreferenceCore->setPersonalInfo(visibility, im_via_email,
											   email, verified);
	}
}
