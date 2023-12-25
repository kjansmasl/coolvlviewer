/**
 * @file llpanellogin.h
 * @brief Login username entry fields.
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

#ifndef LL_LLPANELLOGIN_H
#define LL_LLPANELLOGIN_H

#include "llpanel.h"
#include "llpointer.h"

#include "llmediactrl.h"
#include "llsavedlogins.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLLoginLocationAutoHandler;
class LLTextBox;
class LLUIImage;

class LLPanelLogin final : public LLPanel, public LLViewerMediaObserver
{
	friend class LLLoginLocationAutoHandler;

protected:
	LOG_CLASS(LLPanelLogin);

public:
	LLPanelLogin(const LLRect& rect, void (*callback)(S32, void*),
				 void *callback_data);
	~LLPanelLogin() override;

	bool postBuild() override;
	bool handleKeyHere(KEY key, MASK mask) override;
	void draw() override;
	void setFocus(bool b) override;

	static LL_INLINE LLPanelLogin* getInstance()	{ return sInstance; }
	static LL_INLINE bool isVisible()				{ return sInstance && sInstance->getVisible(); }

	static void show(void (*callback)(S32, void*), void* callback_data = NULL);
	static void hide();

	// Sets the values of the displayed fields.
	static void setFields(const std::string& firstname,
						  const std::string& lastname,
						  const std::string& hashed_password);

	// Sets the values of the displayed fields from a populated history entry.
	static void setFields(const LLSavedLoginEntry& entry,
						  bool take_focus = true, bool load_page = true);

	static void clearServers();
	static void addServer(const std::string& server, S32 domain_name);
	static void refreshLocation();

	static void getFields(std::string& firstname, std::string& lastname,
						  std::string& password);

	// MFA token, when needed
	static void showTokenInputLine(bool show);
	static std::string getToken();

	static bool isGridComboDirty();
	static void getLocation(std::string &location);

	static void close();

	static void loadLoadingPage();
	static void loadLoginPage();
	static void giveFocus();
	static void setAlwaysRefresh(bool refresh);
	static void mungePassword(LLUICtrl* caller, void* user_data);

	// Returns the login history data. It will be empty if the instance does
	// not exist.
	static LLSavedLogins getLoginHistory()
	{
		return sInstance ? sInstance->mLoginHistoryData : LLSavedLogins();
	}

	static void selectFirstElement();

protected:
	static void onClickConnect(void*);

private:
	void reshapeBrowser();

	static void onClickNewAccount(void*);
	static bool newAccountAlertCallback(const LLSD& notification,
										const LLSD& response);
	static void onClickVersion(void*);
	static void onClickForgotPassword(void*);
	static void onPassKey(LLLineEditor* caller, void* user_data);
	static void onSelectServer(LLUICtrl*, void* user_data);
	static void onServerComboLostFocus(LLFocusableElement*, void*);
	static void onStartLocationComboCommit(LLUICtrl* ctrl, void*);
	static void onLastNameEditLostFocus(LLUICtrl* ctrl, void* user_data);
	static void onSelectLoginEntry(LLUICtrl*, void*);
	static void onLoginComboLostFocus(LLFocusableElement* fe, void*);
	static void onStartLocationComboLostFocus(LLFocusableElement* fe, void*);
	static void setPasswordMaxLength();
	static void clearPassword();

private:
	LLMediaCtrl*			mWebBrowser;
	LLComboBox*				mServerCombo;
	LLComboBox*				mRegionCombo;
	LLComboBox*				mStartLocationCombo;
	LLComboBox*				mFirstNameCombo;
	LLLineEditor*			mLastNameEditor;
	LLLineEditor*			mPasswordEditor;
	LLLineEditor*			mTokenEditor;
	LLCheckBoxCtrl*			mRememberLoginCheck;
	LLTextBox*				mTokenText;
	LLTextBox*				mForgotPassText;
	LLTextBox*				mCreateAccountText;
	LLTextBox*				mStartLocationText;
	LLButton*				mConnectButton;

	LLPointer<LLUIImage>	mLogoImage;

	void					(*mCallback)(S32 option, void* userdata);
	void*					mCallbackData;

	std::string				mIncomingPassword;
	std::string				mMungedPassword;

	LLSavedLogins			mLoginHistoryData;

	static LLPanelLogin*	sInstance;
};

std::string load_password_from_disk();
void save_password_to_disk(const char* hashed_password);

#endif
