/**
 * @file llfloatertos.cpp
 * @brief Terms of Service Agreement dialog
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

#include "llfloatertos.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcorehttputil.h"
#include "lluictrlfactory.h"

#include "llappviewer.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewertexteditor.h"

// static
LLFloaterTOS* LLFloaterTOS::sInstance = NULL;

//static
LLFloaterTOS* LLFloaterTOS::show(ETOSType type, const std::string& message,
								 bool start_modal)
{
	if (sInstance &&
		(sInstance->mType != type || sInstance->mMessage != message))
	{
		sInstance->close();
	}
	if (!sInstance)
	{
		sInstance = new LLFloaterTOS(type, message);

		if (type != TOS_CRITICAL_MESSAGE)
		{
			LLUICtrlFactory::getInstance()->buildFloater(sInstance,
														 "floater_tos.xml");
		}
		else
		{
			LLUICtrlFactory::getInstance()->buildFloater(sInstance,
														 "floater_critical.xml");
		}
	}
	if (sInstance && start_modal)
	{
		sInstance->startModal();
	}
	return sInstance;
}

LLFloaterTOS::LLFloaterTOS(ETOSType type, const std::string& message)
:	LLModalDialog(std::string(" "), 100, 100),
	mType(type),
	mMessage(message),
	mAgreeCheck(NULL),
	mWebBrowser(NULL),
	mLoadingScreenLoaded(false),
	mSiteAlive(false),
	mRealNavigateBegun(false)
{
	sInstance = this;
}

LLFloaterTOS::~LLFloaterTOS()
{
	sInstance = NULL;
}

bool LLFloaterTOS::postBuild()
{
	mContinueButton = getChild<LLButton>("Continue");
	mContinueButton->setClickedCallback(onContinue, this);

	childSetAction("Cancel", onCancel, this);

	LLTextEditor* editor = getChild<LLTextEditor>("tos_text");
	if (mType == TOS_CRITICAL_MESSAGE)
	{
		// this displays the critical message
		editor->setHandleEditKeysDirectly(true);
		editor->setEnabled(false);
		editor->setWordWrap(true);
		editor->setFocus(true);
		editor->setValue(LLSD(mMessage));

		return true;
	}
	// hide the SL text widget if we're displaying TOS with using a browser
	// widget.
	editor->setVisible(false);

	mAgreeCheck = getChild<LLCheckBoxCtrl>("agree_chk");
	mAgreeCheck->setCommitCallback(updateAgree);
	mAgreeCheck->setCallbackUserData(this);
	// If type TOS disable Agree to TOS radio button until the page has fully
	// loaded, else enable it.
	mAgreeCheck->setEnabled(mType != TOS_TOS);

	mWebBrowser = getChild<LLMediaCtrl>("tos_html");
	if (mType == TOS_TOS)
	{
		// start to observe it so we see navigate complete events
		mWebBrowser->addObserver(this);
		// Don't use the real_url parameter for this browser instance: it
		// may finish loading before we get to add our observer. Store the
		// URL separately and navigate here instead.
		mWebBrowser->navigateTo(getString("loading_url"));		
	}
	else
	{
		mWebBrowser->navigateToLocalPage("tpv", "tpv.html");
	}
#if 1
	LLPluginClassMedia* media_plugin = mWebBrowser->getMediaPlugin();
	if (media_plugin)
	{
		// All links should be opened in external browser
		media_plugin->setOverrideClickTarget("_external");
	}
#endif
	return true;
}

void LLFloaterTOS::setSiteIsAlive(bool alive)
{
	mSiteAlive = alive;

	// only do this for TOS pages
	if (mType == TOS_TOS)
	{
		// if the contents of the site was retrieved
		if (alive)
		{
			if (mWebBrowser && mSiteAlive && !mRealNavigateBegun)
			{
				// navigate to the "real" page
				mRealNavigateBegun = true;
				mWebBrowser->navigateTo(getString("real_url"));
			}
		}
		else if (mAgreeCheck)
		{
			// normally this is set when navigation to TOS page navigation
			// completes (so you can't accept before TOS loads) but if the
			// page is unavailable, we need to do this now
			mAgreeCheck->setEnabled(true);
		}
	}
}

// virtual
void LLFloaterTOS::draw()
{
	// draw children
	LLModalDialog::draw();
}

//virtual
void LLFloaterTOS::handleMediaEvent(LLPluginClassMedia*, EMediaEvent event)
{
	if (event == MEDIA_EVENT_NAVIGATE_COMPLETE)
	{
		// skip past the loading screen navigate complete
		if (!mLoadingScreenLoaded)
		{
			mLoadingScreenLoaded = true;
			if (mType == TOS_TOS)
			{
				gCoros.launch("LLFloaterTOS::testSiteIsAliveCoro",
							  boost::bind(&LLFloaterTOS::testSiteIsAliveCoro,
										  getHandle(), getString("real_url")));
			}
		}
		else if (mRealNavigateBegun && mAgreeCheck)
		{
			llinfos << "Navigate complete" << llendl;
			// enable Agree to TOS radio button now that page has loaded
			mAgreeCheck->setEnabled(true);
		}
	}
}

//static
void LLFloaterTOS::testSiteIsAliveCoro(LLHandle<LLFloater> handle,
									   const std::string& url)
{
    if (handle.isDead())
    {
		return;	// Floater gone. Ignore and bail out silently.
	}

	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setHeadersOnly(true);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("testSiteIsAliveCoro");
	LLSD result = adapter.getAndSuspend(url, options);

	LLFloaterTOS* self = handle.isDead() ? NULL
										 : dynamic_cast<LLFloaterTOS*>(handle.get());
	if (!self)
	{
		llwarns << "Dialog canceled before response." << llendl;
		return;
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);

	sInstance->setSiteIsAlive(bool(status));
}

// static
void LLFloaterTOS::updateAgree(LLUICtrl*, void* userdata)
{
	LLFloaterTOS* self = (LLFloaterTOS*)userdata;
	if (self && self->mAgreeCheck)
	{
		self->mContinueButton->setEnabled(self->mAgreeCheck->get());
	}
}

// static
void LLFloaterTOS::onContinue(void* userdata)
{
	LLFloaterTOS* self = (LLFloaterTOS*)userdata;
	if (!self) return;

	llinfos << "User agrees with TOS." << llendl;
	if (self->mType == TOS_TOS)
	{
		gAcceptTOS = true;
	}
	else if (self->mType == TOS_CRITICAL_MESSAGE)
	{
		gAcceptCriticalMessage = true;
	}
	else
	{
		gSavedSettings.setBool("FirstRunTPV", false);
		LLStartUp::setStartupState(STATE_LOGIN_WAIT);
		self->close(); // Destroys this object
		return;
	}

	// Go back and finish authentication
	LLStartUp::setStartupState(STATE_LOGIN_AUTH_INIT);

	self->close(); // Destroys this object
}

// static
void LLFloaterTOS::onCancel(void* userdata)
{
	LLFloaterTOS* self = (LLFloaterTOS*)userdata;
	if (!self) return;

	llinfos << "User disagrees with TOS." << llendl;
	gNotifications.add("MustAgreeToLogIn", LLSD(), LLSD(),
					   LLStartUp::loginAlertDone);
	if (self->mType == TOS_FIRST_TPV_USE)
	{
		LLStartUp::setStartupState(STATE_LOGIN_WAIT);
		gAppViewerp->forceQuit();
	}
	else
	{
		LLStartUp::setStartupState(STATE_LOGIN_SHOW);
	}

	// reset state for next time we come to TOS
	self->mLoadingScreenLoaded = false;
	self->mSiteAlive = false;
	self->mRealNavigateBegun = false;

	self->close(); // destroys this object
}
