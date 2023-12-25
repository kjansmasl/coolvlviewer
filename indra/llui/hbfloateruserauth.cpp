/** 
 * @file hbfloateruserauth.cpp
 * @brief The HBFloaterUserAuth class definition
 *
 * $LicenseInfo:firstyear=2015&license=viewergpl$
 * 
 * Copyright (c) 2015, Henri Beauchamp
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

#include "hbfloateruserauth.h"

#include "llapp.h"				// For isQuitting()
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "lluictrlfactory.h"

// NOTE: we allow an empty password field, since it might be a valid login.
#define ALLOW_EMPTY_PASSWORD 1

//static
uuid_list_t HBFloaterUserAuth::sAuthList;

//static
void HBFloaterUserAuth::request(const std::string& host,
								const std::string& realm,
								const LLUUID& auth_id,
								HBFloaterUserAuthCallback callback)
{
	if (!sAuthList.count(auth_id))
	{
		(void)new HBFloaterUserAuth(host, realm, auth_id, callback);
	}
}

HBFloaterUserAuth::HBFloaterUserAuth(const std::string& host,
									 const std::string& realm,
									 const LLUUID& auth_id,
									 HBFloaterUserAuthCallback callback)
:	mUserAuthCallback(callback),
	mAuthId(auth_id),
	mHost(host),
	mRealm(realm),
	mMustClose(false),
	mCallbackDone(false)
{
	sAuthList.emplace(auth_id);
    LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_user_auth.xml");
}

//virtual
HBFloaterUserAuth::~HBFloaterUserAuth()
{
	if (!mCallbackDone && !LLApp::isQuitting())
	{
		doCallback(false);
	}
	sAuthList.erase(mAuthId);
}

//virtual
bool HBFloaterUserAuth::postBuild()
{
	mUserNameInputLine = getChild<LLLineEditor>("user_name");
	mUserNameInputLine->setOnHandleKeyCallback(onHandleKeyCallback, this);
	mUserNameInputLine->setKeystrokeCallback(onKeystrokeCallback);
	mUserNameInputLine->setCallbackUserData(this);

	mPasswordInputLine = getChild<LLLineEditor>("password");
	mPasswordInputLine->setOnHandleKeyCallback(onHandleKeyCallback, this);
	mPasswordInputLine->setCallbackUserData(this);
	mPasswordInputLine->setDrawAsterixes(true);

	childSetCommitCallback("show_password", onCommitCheckBox, this);

	mOKBtn = getChild<LLButton>("ok");
	mOKBtn->setClickedCallback(onButtonOK, this);
	mOKBtn->setEnabled(false);

	childSetAction("cancel", onButtonCancel, this);

	setTitle(getTitle() + " " + mHost);

	childSetTextArg("prompt_text", "[REALM]", mRealm);

	center();

	return true;
}

//virtual
void HBFloaterUserAuth::draw()
{
	if (mMustClose)
	{
		onButtonOK(this);
	}
	else
	{
		LLFloater::draw();
	}
}

void HBFloaterUserAuth::doCallback(bool validated)
{
	if (!mCallbackDone)
	{
		if (mUserAuthCallback)
		{
			mUserAuthCallback(mAuthId, mUserNameInputLine->getText(),
							  mPasswordInputLine->getText(), validated);
		}
		mCallbackDone = true;
	}
}

//static
void HBFloaterUserAuth::onButtonOK(void* user_data)
{
	HBFloaterUserAuth* self = (HBFloaterUserAuth*)user_data;
	if (self)
	{
		self->doCallback(true);
		self->close();
	}
}

//static
void HBFloaterUserAuth::onButtonCancel(void* user_data)
{
	HBFloaterUserAuth* self = (HBFloaterUserAuth*)user_data;
	if (self)
	{
		self->close();
	}
}

//static
void HBFloaterUserAuth::onCommitCheckBox(LLUICtrl* ctrl, void* user_data)
{
	HBFloaterUserAuth* self = (HBFloaterUserAuth*)user_data;
	if (self && ctrl)
	{
		LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
		self->mPasswordInputLine->setDrawAsterixes(!check->get());
#if 0	// Does not work...
		self->mPasswordInputLine->setFocus(true);
#endif
	}
}

//static
bool HBFloaterUserAuth::onHandleKeyCallback(KEY key, MASK mask,
											LLLineEditor* caller,
											void* user_data)
{
	bool handled = false;

	HBFloaterUserAuth* self = (HBFloaterUserAuth*)user_data;
	if (self && key == KEY_RETURN && mask == MASK_NONE &&
		!self->mUserNameInputLine->getText().empty())
	{
		if (caller == self->mUserNameInputLine)
		{
			self->mPasswordInputLine->setFocus(true);
			handled = true;
		}
#if ALLOW_EMPTY_PASSWORD
		else
#else
		else if (!self->mPasswordInputLine->getText().empty())
#endif
		{
			self->mMustClose = true;
			handled = true;
		}
	}

	return handled;
}

//static
void HBFloaterUserAuth::onKeystrokeCallback(LLLineEditor*, void* user_data)
{
	HBFloaterUserAuth* self = (HBFloaterUserAuth*)user_data;
	if (self)
	{
		bool cant_validate =
#if !ALLOW_EMPTY_PASSWORD
		self->mPasswordInputLine->getText().empty()) ||
#endif
		self->mUserNameInputLine->getText().empty();

		self->mOKBtn->setEnabled(!cant_validate);
	}
}
