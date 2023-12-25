/**
 * @file llpanellogin.cpp
 * @brief Login dialog and logo display
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

#include "llviewerprecompiledheaders.h"

#include "llpanellogin.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldir.h"
#include "llhttpconstants.h"
#include "lllineeditor.h"
#include "llmd5.h"
#include "llnotifications.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llurlhistory.h"
#include "llversionviewer.h"

#include "llappviewer.h"
#include "llcommandhandler.h"			// For secondlife:///app/login/
#include "llfloaterabout.h"
#include "llfloatermediabrowser.h"
#include "llfloaterpreference.h"
#if LL_DEBUG
#include "llfloatertos.h"
#endif
#include "llgridmanager.h"
#include "llmediactrl.h"
#include "llslurl.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"			// to link into child list
#include "llweb.h"

LLPanelLogin* LLPanelLogin::sInstance = NULL;

class LLLoginRefreshHandler final : public LLCommandHandler
{
public:
	// Do not allow from external browsers
	LLLoginRefreshHandler()
	:	LLCommandHandler("login_refresh", UNTRUSTED_BLOCK)
	{
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
		{
			LLPanelLogin::loadLoginPage();
		}
		return true;
	}
};

LLLoginRefreshHandler gLoginRefreshHandler;

class LLLoginLocationAutoHandler final : public LLCommandHandler
{
public:
	LLLoginLocationAutoHandler()
	:	LLCommandHandler("location_login", UNTRUSTED_BLOCK)
	{
	}

	bool handle(const LLSD& tokens, const LLSD&, LLMediaCtrl*) override
	{
		if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
		{
			size_t params = tokens.size();
			if (params == 0 || params > 4)
			{
				return false;
			}

			// Unescape is important: URIs with spaces are escaped in this code
			// path and the code to log into a region does not support that.
			std::string region = LLURI::unescape(tokens[0].asString());

			LLVector3 pos(128.f, 128.f, 0.f);
			if (params >= 2)
			{
				pos.mV[VX] = tokens[1].asReal();
			}
			if (params >= 3)
			{
				pos.mV[VY] = tokens[2].asReal();
			}
			if (params == 4)
			{
				pos.mV[VZ] = tokens[3].asReal();
			}

			LLSLURL slurl(region, pos);
			LLStartUp::setStartSLURL(slurl);

			LLPanelLogin::onClickConnect(NULL);
		}

		return true;
	}	
};

LLLoginLocationAutoHandler gLoginLocationAutoHandler;


LLPanelLogin::LLPanelLogin(const LLRect& rect, void (*callback)(S32, void*),
						   void* cb_data)
:	LLPanel("panel_login", LLRect(0, 600, 800, 0), false),	// Not bordered
	mWebBrowser(NULL),
	mCallback(callback),
	mCallbackData(cb_data)
{
	setFocusRoot(true);

	setBackgroundVisible(false);
	setBackgroundOpaque(true);

	// Instance management
	if (sInstance)
	{
		llwarns << "Duplicate instance of login view deleted" << llendl;
		delete sInstance;

		// Do not leave bad pointer in gFocusMgr
		gFocusMgr.setDefaultKeyboardFocus(NULL);
	}
	sInstance = this;

	// Add to front so we are the bottom-most child
	gViewerWindowp->getRootView()->addChildAtEnd(this);

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_login.xml");

	reshape(rect.getWidth(), rect.getHeight());
}

//virtual
LLPanelLogin::~LLPanelLogin()
{
	sInstance = NULL;

	if (gFocusMgr.getDefaultKeyboardFocus() == this)
	{
		gFocusMgr.setDefaultKeyboardFocus(NULL);
	}
}

//virtual
bool LLPanelLogin::postBuild()
{
	// Background image
	mLogoImage = LLUI::getUIImage("startup_logo.png");
	if (mLogoImage.isNull())
	{
		llwarns << "Missing background image: verify the viewer installation !"
				<< llendl;
	}

	mFirstNameCombo = getChild<LLComboBox>("first_name_combo");
	mFirstNameCombo->setSuppressTentative(true);
	mFirstNameCombo->setCommitCallback(onSelectLoginEntry);
	mFirstNameCombo->setFocusLostCallback(onLoginComboLostFocus);
	mFirstNameCombo->setPrevalidate(LLLineEditor::prevalidatePrintableNoSpace);
	mFirstNameCombo->setCallbackUserData(this);

	mLastNameEditor = getChild<LLLineEditor>("last_name_edit");
	mLastNameEditor->setPrevalidate(LLLineEditor::prevalidatePrintableNoSpace);
	mLastNameEditor->setCommitCallback(onLastNameEditLostFocus);
	mLastNameEditor->setCallbackUserData(this);

	mPasswordEditor = getChild<LLLineEditor>("password_edit");
	mPasswordEditor->setDrawAsterixes(true);
	mPasswordEditor->setCommitCallback(mungePassword);
	mPasswordEditor->setKeystrokeCallback(onPassKey);
	mPasswordEditor->setCallbackUserData(this);

	// MFA token input (hidden and disabled by default)
	mTokenEditor = getChild<LLLineEditor>("mfa_token_edit");
	mTokenEditor->setEnabled(false);
	mTokenEditor->setVisible(false);
	mTokenText = getChild<LLTextBox>("mfa_token_text");
	mTokenText->setVisible(false);

	mRememberLoginCheck = getChild<LLCheckBoxCtrl>("remember_check");
	if (gAppViewerp->isSecondInstanceSiblingViewer())
	{
		// Hide this check box when its status is ignored, i.e. when we are not
		// the first running Cool VL Viewer instance.
		mRememberLoginCheck->setVisible(false);
	}

	mRegionCombo = getChild<LLComboBox>("regionuri_edit");
	mRegionCombo->setAllowTextEntry(true, 256, false);

	// Iterate on URI list adding to combobox (could not figure out how to add
	// them all in one call)... and also append the command line value we might
	// have gotten to the URLHistory
	LLSD regionuri_history = LLURLHistory::getURLHistory("regionuri");
	for (LLSD::array_iterator it = regionuri_history.beginArray(),
							  end = regionuri_history.endArray();
		 it != end; ++it)
	{
		mRegionCombo->addSimpleElement(it->asString());
	}

	mStartLocationCombo = getChild<LLComboBox>("start_location_combo");
	mStartLocationCombo->setAllowTextEntry(true, 128, false);

	// The XML file loads the combo with the following labels:
	// 0 - "My Home"
	// 1 - "My Last Location"
	// 2 - "<Type region name>"
	LLSLURL slurl = LLStartUp::getStartSLURL();
	LLSLURL::SLURL_TYPE slurl_type = slurl.getType();
	std::string sim_string = slurl.getLocationString();
	if (slurl_type == LLSLURL::LOCATION)
	{
		// Replace "<Type region name>" with this region name
		mStartLocationCombo->remove(2);
		mStartLocationCombo->add(sim_string);
		mStartLocationCombo->setTextEntry(sim_string);
		mStartLocationCombo->setCurrentByIndex(2);
	}
	else if (slurl_type == LLSLURL::HOME_LOCATION)
	{
		mStartLocationCombo->setCurrentByIndex(0);
	}
	else if (slurl_type == LLSLURL::LAST_LOCATION ||
			 gSavedSettings.getBool("LoginLastLocation"))
	{
		mStartLocationCombo->setCurrentByIndex(1);
	}
	else
	{
		mStartLocationCombo->setCurrentByIndex(0);
	}

	mStartLocationCombo->setCommitCallback(onStartLocationComboCommit);
	mStartLocationCombo->setFocusLostCallback(onStartLocationComboLostFocus);
	mStartLocationCombo->setCallbackUserData(this);

	mStartLocationText = getChild<LLTextBox>("start_location_text");

	mServerCombo = getChild<LLComboBox>("server_combo");
	mServerCombo->setCommitCallback(onSelectServer);
	mServerCombo->setFocusLostCallback(onServerComboLostFocus);
	mServerCombo->setCallbackUserData(this);

	mConnectButton = getChild<LLButton>("connect_btn");
	mConnectButton->setClickedCallback(onClickConnect, this);
	setDefaultBtn(mConnectButton);

	std::string channel = gSavedSettings.getString("VersionChannelName");
	std::string version = llformat("%d.%d.%d.%d", LL_VERSION_MAJOR,
								   LL_VERSION_MINOR, LL_VERSION_BRANCH,
								   LL_VERSION_RELEASE);
	LLTextBox* channel_text = getChild<LLTextBox>("channel_text");
	channel_text->setTextArg("[CHANNEL]", channel);
	channel_text->setTextArg("[VERSION]", version);
	channel_text->setClickedCallback(onClickVersion);
	// Change Z sort of clickable text to be behind buttons
	sendChildToBack(channel_text);

	LLGridManager* gm = LLGridManager::getInstance();

	mForgotPassText = getChild<LLTextBox>("forgot_password_text");
	mForgotPassText->setClickedCallback(onClickForgotPassword);
	if (gm->getPasswordURL().empty())
	{
		mForgotPassText->setVisible(false);
	}
	// Change Z sort of clickable text to be behind buttons
	sendChildToBack(mForgotPassText);

	mCreateAccountText = getChild<LLTextBox>("create_new_account_text");
	mCreateAccountText->setClickedCallback(onClickNewAccount);
	if (gm->getAccountURL().empty())
	{
		mCreateAccountText->setVisible(false);
	}
	// Change Z sort of clickable text to be behind buttons
	sendChildToBack(mCreateAccountText);

	// Get the web browser control
	mWebBrowser = getChild<LLMediaCtrl>("login_html");
	mWebBrowser->addObserver(this);
	// Need to handle login secondlife:///app/ URLs
	mWebBrowser->setTrusted(true);
	// Do not make it a tab stop until SL-27594 is fixed
	mWebBrowser->setTabStop(false);

	// Load login history
	std::string login_hist_filepath =
		gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
									   "saved_grids_login.xml");
	mLoginHistoryData = LLSavedLogins::loadFile(login_hist_filepath);
	if (mLoginHistoryData.size() > 0)
	{
		const LLSavedLogins::list_t& logins = mLoginHistoryData.getEntries();
		for (LLSavedLogins::list_const_rit_t it = logins.rbegin(),
											 rend = logins.rend();
			 it != rend; ++it)
		{
			LLSD entry = it->asLLSD();
			if (entry.isMap()) 
			{
				mFirstNameCombo->add(it->getDisplayString(), entry);
			}
		}
		setFields(*mLoginHistoryData.getEntries().rbegin(), false, false);
	}

	// Load the loading page
	loadLoadingPage();

	reshapeBrowser();
	refreshLocation();
	loadLoginPage();

#if LL_FMOD
	childShow("fmod");
#else
	childHide("fmod");
#endif

	return true;
}

// Force-resize the panel and the web browser (XUI does not seem to be enough
// to do this, probably because this panel got no parent floater).
void LLPanelLogin::reshapeBrowser()
{
	S32 offset = atoi(getString("bottom_y_offset").c_str());
	LLRect rect = getRect();
	LLRect html_rect;
	html_rect.setCenterAndSize(rect.getCenterX(),
							   rect.getCenterY() + offset / 2,
							   rect.getWidth() + 1,
							   rect.getHeight() - offset);
	mWebBrowser->setRect(html_rect);
	mWebBrowser->reshape(html_rect.getWidth(), html_rect.getHeight());
	reshape(rect.getWidth(), rect.getHeight());
}

void LLPanelLogin::mungePassword(LLUICtrl* ctrl, void* user_data)
{
	LLPanelLogin* self = (LLPanelLogin*)user_data;
	LLLineEditor* editor = (LLLineEditor*)ctrl;
	if (!self || !editor) return;

	std::string password = editor->getText();

	// Re-md5 if we have changed at all
	if (password != self->mIncomingPassword)
	{
		LLMD5 pass((unsigned char*)password.c_str());
		char munged_password[MD5HEX_STR_SIZE];
		pass.hex_digest(munged_password);
		self->mMungedPassword = munged_password;
	}
}

//virtual
void LLPanelLogin::draw()
{
	gGL.pushMatrix();
	{
		F32 image_aspect = 1.333333f;
		S32 width = getRect().getWidth();
		S32 height = getRect().getHeight();
		F32 view_aspect = (F32)width / (F32)height;
		// Stretch image to maintain aspect ratio
		if (image_aspect > view_aspect)
		{
			gGL.translatef(-0.5f * (image_aspect / view_aspect - 1.f) *
						   width, 0.f, 0.f);
			gGL.scalef(image_aspect / view_aspect, 1.f, 1.f);
		}

		// Draw a background box in black
		gl_rect_2d(0, height - 264, width, 264, LLColor4::black);
		if (mLogoImage.notNull())
		{
			// Draw the bottom part of the background image; just the blue
			// background to the native client UI
			mLogoImage->draw(0, -264, width + 8, mLogoImage->getHeight());
		}
	}
	gGL.popMatrix();

	LLPanel::draw();
}

//virtual
bool LLPanelLogin::handleKeyHere(KEY key, MASK mask)
{
	if (key == 'P' && mask == MASK_CONTROL)
	{
		LLFloaterPreference::showInstance();
		return true;
	}

	if (mask == MASK_NONE)
	{
		if (key == KEY_F1)
		{
			llinfos << "Spawning HTML help window" << llendl;
			gViewerHtmlHelp.show();
			return true;
		}

#if LL_DEBUG
		if (key == KEY_F2)
		{
			llinfos << "Spawning floater TOS window" << llendl;
			LLFloaterTOS::show(LLFloaterTOS::TOS_TOS);
			return true;
		}
#endif

		if (key == KEY_RETURN)
		{
			// Let the panel handle UICtrl processing: calls onClickConnect()
			return LLPanel::handleKeyHere(key, mask);
		}
	}

	return LLPanel::handleKeyHere(key, mask);
}

//virtual
void LLPanelLogin::setFocus(bool b)
{
	if (b != hasFocus())
	{
		if (b)
		{
			LLPanelLogin::giveFocus();
		}
		else
		{
			LLPanel::setFocus(b);
		}
	}
}

//static
void LLPanelLogin::giveFocus()
{
	if (sInstance)
	{
		// Grab focus and move cursor to first blank input field
		std::string first = sInstance->mFirstNameCombo->getValue().asString();
		std::string pass = sInstance->mPasswordEditor->getValue().asString();

		if (first.empty())
		{
			// User does not have a name, so start there.
			sInstance->mFirstNameCombo->setFocusText(true);
		}
		else if (pass.empty())
		{
			// User saved his name but not his password. Move focus to password
			// field.
			sInstance->mPasswordEditor->setFocus(true);
			sInstance->mPasswordEditor->selectAll();
		}
		else
		{
			// Else, we have both name and password. We get here waiting for
			// the login to happen.
			sInstance->mConnectButton->setFocus(true);
		}
	}
}

//static
void LLPanelLogin::show(void (*callback)(S32, void*), void* callback_data)
{
	if (sInstance)
	{
		llinfos << "Refreshing the login screen" << llendl;
		sInstance->mCallback = callback;
		sInstance->mCallbackData = callback_data;
		sInstance->setVisible(true);
	}
	else
	{
		llinfos << "Initializing the login screen" << llendl;
		new LLPanelLogin(gViewerWindowp->getVirtualWindowRect(),
						 callback, callback_data);
	}

	if (!gFocusMgr.getKeyboardFocus())
	{
		// Grab focus and move cursor to first enabled control
		sInstance->setFocus(true);
	}

	// Make sure that focus always goes here (and use the latest sInstance
	// that was just created)
	gFocusMgr.setDefaultKeyboardFocus(sInstance);
}

//static
void LLPanelLogin::showTokenInputLine(bool show)
{
	if (sInstance)
	{
		sInstance->mTokenEditor->setEnabled(show);
		sInstance->mTokenEditor->setVisible(show);
		sInstance->mTokenText->setVisible(show);
	}
}

//static
std::string LLPanelLogin::getToken()
{
	std::string token;
	if (sInstance && sInstance->mTokenEditor->getEnabled())
	{
		token = sInstance->mTokenEditor->getValue().asString();
	}
	return token;
}

//static
void LLPanelLogin::hide()
{
	if (sInstance)
	{
		sInstance->setVisible(false);
	}
}

//static
void LLPanelLogin::setFields(const std::string& firstname,
						     const std::string& lastname,
						     const std::string& hashed_password)
{
	if (!sInstance)
	{
		llwarns << "No login view shown !" << llendl;
		llassert(false);
		return;
	}

	sInstance->mLastNameEditor->setText(lastname);

	llassert_always(firstname.find(' ') == std::string::npos);
	sInstance->mFirstNameCombo->setLabel(firstname);

	std::string filler;
	if (!hashed_password.empty())
	{
		// This is a MD5 hex digest of a password. We do not actually use the
		// password input field; fill it with characters so we get a nice row
		// of asterixes.
		filler = "0123456789012345";
	}
	sInstance->mPasswordEditor->setText(filler);
	sInstance->mIncomingPassword = filler;
	sInstance->mMungedPassword = hashed_password;

	LL_DEBUGS("Login") << "Login credentials: User: " << firstname << " "
					   << lastname << " - Password hash: "
#if LL_DEBUG_LOGIN_PASSWORD
					   << sInstance->mMungedPassword
#endif
					   << LL_ENDL;
}

//static
void LLPanelLogin::setFields(const LLSavedLoginEntry& entry, bool take_focus,
							 bool load_page)
{
	if (!sInstance)
	{
		llwarns << "No login view shown !" << llendl;
		return;
	}

	sInstance->mFirstNameCombo->setLabel(entry.getFirstName());
	sInstance->mFirstNameCombo->resetDirty();
	sInstance->mFirstNameCombo->resetTextDirty();

	sInstance->mLastNameEditor->setText(entry.getLastName());
	sInstance->mLastNameEditor->resetDirty();

	if (entry.getPassword().empty())
	{
		sInstance->mPasswordEditor->clear();
		sInstance->mIncomingPassword.clear();
		sInstance->mMungedPassword.clear();
	}
	else
	{
		const std::string filler = "0123456789012345";
		sInstance->mPasswordEditor->setText(filler);
		sInstance->mIncomingPassword = filler;
		sInstance->mMungedPassword = entry.getPassword();
	}

	LL_DEBUGS("Login") << "Login credentials: User: "
					   << entry.getFirstName() << " " << entry.getLastName()
#if LL_DEBUG_LOGIN_PASSWORD
					   << " - Password hash: " << sInstance->mMungedPassword
#endif
					   << LL_ENDL;

	// Check entry to avoid infinite loop
	if (sInstance->mServerCombo->getSimple() != entry.getGridName())
	{
		// Same string as used in login_show().
		sInstance->mServerCombo->setSimple(entry.getGridName());
	}

	LLGridManager* gm = LLGridManager::getInstance();

	if (entry.getGrid() == GRID_INFO_OTHER)
	{
		gm->setGridURI(entry.getGridURI().asString());
		gm->setHelperURI(entry.getHelperURI().asString());
		gm->setLoginPageURI(entry.getLoginPageURI().asString());
	}

	EGridInfo entry_grid = entry.getGrid();

	if (entry_grid == GRID_INFO_OTHER || entry_grid != gm->getGridChoice())
	{
		// Load the loading page first
		if (load_page)
		{
			loadLoadingPage();
		}

		gm->setGridChoice(entry_grid);

		// Grid changed so show new splash screen (possibly)
		if (load_page)
		{
			loadLoginPage();
		}
	}

	if (take_focus)
	{
		giveFocus();
	}
}

//static
void LLPanelLogin::clearServers()
{
	if (sInstance)
	{
		sInstance->mServerCombo->removeall();
	}
	else
	{
		llwarns << "Attempted clearServers with no login view shown" << llendl;
	}
}

//static
void LLPanelLogin::addServer(const std::string& server, S32 domain_name)
{
	if (sInstance)
	{
		sInstance->mServerCombo->add(server, LLSD(domain_name));
		sInstance->mServerCombo->setCurrentByIndex(0);
	}
	else
	{
		llwarns << "Attempted addServer with no login view shown" << llendl;
	}
}

//static
void LLPanelLogin::getFields(std::string& firstname, std::string& lastname,
							 std::string& password)
{
	if (!sInstance)
	{
		llwarns << "Attempted getFields with no login view shown" << llendl;
		return;
	}

	firstname = sInstance->mFirstNameCombo->getValue().asString();
	LLStringUtil::trim(firstname);

	lastname = sInstance->mLastNameEditor->getValue().asString();
	LLStringUtil::trim(lastname);

	password = sInstance->mMungedPassword;

	LL_DEBUGS("Login") << "Login credentials: User: " << firstname << " "
					   << lastname
#if LL_DEBUG_LOGIN_PASSWORD
					   << " - Password hash: " << password
#endif
					   << LL_ENDL;
}

//static
bool LLPanelLogin::isGridComboDirty()
{
	return sInstance ? sInstance->mServerCombo->isDirty() : false;
}

//static
void LLPanelLogin::getLocation(std::string& location)
{
	if (sInstance)
	{
		location = sInstance->mStartLocationCombo->getValue().asString();
		if (location == sInstance->getString("my_home"))
		{
			location = LLSLURL::SIM_LOCATION_HOME;
		}
		else if (location == sInstance->getString("last_location"))
		{
			location = LLSLURL::SIM_LOCATION_LAST;
		}
	}
	else
	{
		llwarns << "Attempted getLocation with no login view shown" << llendl;
	}
}

//static
void LLPanelLogin::refreshLocation()
{
	if (!sInstance) return;

	LLSLURL slurl = LLStartUp::getStartSLURL();
	LLSLURL::SLURL_TYPE slurl_type = slurl.getType();
	if (slurl_type == LLSLURL::LOCATION)
	{
		sInstance->mStartLocationCombo->setCurrentByIndex(2);
		sInstance->mStartLocationCombo->setTextEntry(slurl.getLocationString());
	}
	else if (slurl_type == LLSLURL::HOME_LOCATION)
	{
		sInstance->mStartLocationCombo->setCurrentByIndex(0);
	}
	else if (slurl_type == LLSLURL::LAST_LOCATION ||
			 gSavedSettings.getBool("LoginLastLocation"))
	{
		sInstance->mStartLocationCombo->setCurrentByIndex(1);
	}
	else
	{
		sInstance->mStartLocationCombo->setCurrentByIndex(0);
	}

	// Do Not show regionuri box if legacy
	sInstance->mRegionCombo->setVisible(false);
}

//static
void LLPanelLogin::close()
{
	if (sInstance)
	{
		gViewerWindowp->getRootView()->removeChild(sInstance);

		gFocusMgr.setDefaultKeyboardFocus(NULL);

		delete sInstance;
		sInstance = NULL;
	}
}

//static
void LLPanelLogin::setAlwaysRefresh(bool refresh)
{
	if (sInstance && LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
	{
		sInstance->mWebBrowser->setAlwaysRefresh(refresh);
	}
}

void LLPanelLogin::loadLoadingPage()
{
	if (sInstance)
	{
		sInstance->mWebBrowser->navigateToLocalPage("loading", "loading.html");
		ms_sleep(250); // Let some time to the plugin (0.25s) to display the page
	}
}

void LLPanelLogin::loadLoginPage()
{
	if (!sInstance) return;

	LLGridManager* gm = LLGridManager::getInstance();
	std::string login_page = gm->getLoginPageURI();
	if (login_page.empty())
	{
		sInstance->mWebBrowser->navigateToLocalPage("splash", "splash.html");
		return;
	}

	std::ostringstream out;
	out << login_page;

	// Use the right delimeter depending on how LLURI parses the URL
	LLURI login_page_uri = LLURI(login_page);
	std::string first_query_delimiter = "&";
	if (login_page_uri.queryMap().size() == 0)
	{
		first_query_delimiter = "?";
	}

	// Language
	std::string language = LLUI::getLanguage();
	out << first_query_delimiter << "lang=" << language;

#if 0	// Avoid this: this may cause bad loading of login screens in SL...
		// The User-Agent string already provides the channel and version info
		// to the login page server anyway.

	// Channel and Version
	std::string version = llformat("%d.%d.%d.%d", LL_VERSION_MAJOR,
								   LL_VERSION_MINOR, LL_VERSION_BRANCH,
								   LL_VERSION_RELEASE);
	std::string channel = gSavedSettings.getString("VersionChannelName");

	char* curl_channel = curl_escape(channel.c_str(), 0);
	char* curl_version = curl_escape(version.c_str(), 0);

	out << "&channel=" << curl_channel << "&version=" << curl_version;

	curl_free(curl_channel);
	curl_free(curl_version);
#endif

	// Grid
	const std::string& uri = gm->getGridURI();
	if (uri.find(".aditi.lindenlab.") != std::string::npos)
	{
		// Only add the grid info for the beta grid in SL, so to get the
		// corresponding login screen.
		out << "&grid=aditi";
		gIsInProductionGrid = false;
	}
	else
	{
		gIsInProductionGrid = true;
	}

	// Set the viewer menu bar background color, depending on the production/
	// beta grid alternatives (follows gIsInProductionGrid value as set above).
	gViewerWindowp->setMenuBackgroundColor();
	gm->setMenuColor();
	gLoginMenuBarViewp->setBackgroundColor(gMenuBarViewp->getBackgroundColor());

	// navigate to the "real" page
	sInstance->mWebBrowser->navigateTo(out.str(), HTTP_CONTENT_TEXT_HTML);

	sInstance->mCreateAccountText->setVisible(!gm->getAccountURL().empty());
	sInstance->mForgotPassText->setVisible(!gm->getPasswordURL().empty());
}

//static
void LLPanelLogin::selectFirstElement()
{
	if (sInstance)
	{
		LL_DEBUGS("Login") << "Selecting first entry in list." << LL_ENDL;
		sInstance->mServerCombo->setCurrentByIndex(0);
		LLPanelLogin::onSelectServer(NULL, sInstance);
	}
}

//---------------------------------------------------------------------------
// Protected methods
//---------------------------------------------------------------------------

//static
void LLPanelLogin::onClickConnect(void*)
{
	if (sInstance && sInstance->mCallback)
	{
		// JC - Make sure the fields all get committed.
		sInstance->setFocus(false);

		std::string first = sInstance->mFirstNameCombo->getValue().asString();
		std::string last = sInstance->mLastNameEditor->getValue().asString();
		if (!first.empty() && !last.empty())
		{
			// has both first and last name typed
			sInstance->mCallback(0, sInstance->mCallbackData);
		}
		else
		{
			gNotifications.add("MustHaveAccountToLogIn", LLSD(), LLSD(),
							   LLPanelLogin::newAccountAlertCallback);
		}
	}
}

//static
bool LLPanelLogin::newAccountAlertCallback(const LLSD& notification,
										   const LLSD& response)
{
	sInstance->setFocus(true);
	return false;
}

//static
void LLPanelLogin::onClickNewAccount(void*)
{
	std::string new_account = LLGridManager::getInstance()->getAccountURL();
	if (!new_account.empty())
	{
		LLWeb::loadURLExternal(new_account);
	}
}

//static
void LLPanelLogin::onClickVersion(void*)
{
	LLFloaterAbout::showInstance();
}

//static
void LLPanelLogin::onClickForgotPassword(void*)
{
	std::string password_url = LLGridManager::getInstance()->getPasswordURL();
	if (!password_url.empty())
	{
		LLWeb::loadURLExternal(password_url);
	}
}

//static
void LLPanelLogin::onPassKey(LLLineEditor* caller, void*)
{
	if (!caller) return;	// Paranoia

	static bool caps_lock_notified = false;
	if (!caps_lock_notified && gKeyboardp &&
		gKeyboardp->getKeyDown(KEY_CAPSLOCK))
	{
		gNotifications.add("CapsKeyOn");
		caps_lock_notified = true;
	}

	static bool pass_max_len_notified = false;
	if (!pass_max_len_notified && caller->getWText().size() > 16)
	{
		std::string grid_name = LLGridManager::getInstance()->getGridLabel();
		LLStringUtil::toLower(grid_name);
		if (grid_name.find("secondlife") != std::string::npos)
		{
			gNotifications.add("SLPasswordLength");
			pass_max_len_notified = true;
		}
	}
}

//static
void LLPanelLogin::onStartLocationComboCommit(LLUICtrl* ctrl, void*)
{
	if (sInstance && ctrl == sInstance->mStartLocationCombo)
	{
		std::string location;
		sInstance->getLocation(location);
		LLStartUp::setStartSLURL(LLSLURL(location));
	}
}

//static
void LLPanelLogin::onStartLocationComboLostFocus(LLFocusableElement* fe, void*)
{
	if (sInstance && fe == sInstance->mStartLocationCombo)
	{
		std::string location;
		sInstance->getLocation(location);
		LLStartUp::setStartSLURL(LLSLURL(location));
	}
}

//static
void LLPanelLogin::onSelectServer(LLUICtrl*, void* user_data)
{
	LLPanelLogin* self = (LLPanelLogin*)user_data;
	if (!self) return;

	// This method is only called by one thread, so we can use a static here.
	static bool recursing = false;
	if (recursing)
	{
		return;
	}
	recursing = true;

	// LLPanelLogin::onServerComboLostFocus(LLFocusableElement* fe, void*)
	// calls this method.

	// The user twiddled with the grid choice ui. Apply the selection to the
	// grid setting.
	std::string grid_name;
	EGridInfo grid_index;
	LLSD combo_val = sInstance->mServerCombo->getValue();
	if (combo_val.type() == LLSD::TypeInteger)
	{
		grid_index =
			(EGridInfo)sInstance->mServerCombo->getValue().asInteger();
		grid_name = sInstance->mServerCombo->getSimple();
	}
	else
	{
		// No valid selection, return other
		grid_index = GRID_INFO_OTHER;
		grid_name = combo_val.asString();
	}

	LLGridManager* gm = LLGridManager::getInstance();

	// This new selection will override preset uris from the command line.
	if (grid_index != GRID_INFO_OTHER)
	{
		gm->setGridChoice(grid_index);
	}
	else
	{
		gm->setGridChoice(grid_name);
	}

	// Get the newly selected and properly formatted grid name.
	grid_name = gm->getGridLabel();

	// Find a saved login entry that uses this grid, if any.
	bool found = false;
	const LLSavedLogins::list_t& entries =
		sInstance->mLoginHistoryData.getEntries();
	for (LLSavedLogins::list_const_rit_t it = entries.rbegin(),
										 rend = entries.rend();
		 it != rend; ++it)
	{
		if (!it->asLLSD().isMap())
		{
			continue;
		}
		if (it->getGridName() == grid_name)
		{
		  	if (!gm->nameEdited())
			{
				// Change the other fields to match this grid.
				setFields(*it, false);
			}
			else	// Probably creating a new account.
			{
				// Likely the current password is for a different grid.
				clearPassword();
			}
			found = true;
			break;
		}
	}
	if (!found)
	{
		// If the grid_name starts with 'http[s]://' then we have to assume it
		// is a new loginuri, set on the commandline.
		if (grid_name.substr(0, 4) == "http")
		{
			// Use it as login uri.
			gm->setGridURI(grid_name);
			// And set the login page if it was given.
			std::string uri = gSavedSettings.getString("LoginPage");
			if (!uri.empty())
			{
				gm->setLoginPageURI(uri);
			}
			uri = gSavedSettings.getString("CmdLineHelperURI");
			if (!uri.empty())
			{
				gm->setHelperURI(uri);
			}
		}
		clearPassword();
	}

	// Load the loading page first
	loadLoadingPage();

	// Grid changed so show new splash screen (possibly)
	loadLoginPage();

	recursing = false;
}

void LLPanelLogin::onServerComboLostFocus(LLFocusableElement* fe, void*)
{
	if (sInstance && fe == sInstance->mServerCombo)
	{
		onSelectServer(sInstance->mServerCombo, sInstance);
	}
}

//static
void LLPanelLogin::onLastNameEditLostFocus(LLUICtrl* ctrl, void* data)
{
	if (sInstance && ctrl == sInstance->mLastNameEditor &&
		sInstance->mLastNameEditor->isDirty())
	{
		clearPassword();
		LLGridManager::getInstance()->setNameEdited(true);
	}
}

//static
void LLPanelLogin::onSelectLoginEntry(LLUICtrl* ctrl, void* data)
{
	if (sInstance && ctrl == sInstance->mFirstNameCombo)
	{
		LLSD selected_entry = sInstance->mFirstNameCombo->getSelectedValue();
		if (!selected_entry.isUndefined())
		{
			LLSavedLoginEntry entry(selected_entry);
			setFields(entry);
		}
		// This stops the automatic matching of the first name to a
		// selected grid.
		LLGridManager::getInstance()->setNameEdited(true);
	}
}

//static
void LLPanelLogin::onLoginComboLostFocus(LLFocusableElement* fe, void*)
{
	if (sInstance && fe == sInstance->mFirstNameCombo)
	{
		if (sInstance->mFirstNameCombo->isTextDirty())
		{
			clearPassword();
		}
		onSelectLoginEntry(sInstance->mFirstNameCombo, NULL);
	}
}

//static
void LLPanelLogin::clearPassword()
{
	sInstance->mPasswordEditor->clear();
	sInstance->mIncomingPassword.clear();
	sInstance->mMungedPassword.clear();
}
