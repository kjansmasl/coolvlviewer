/** 
 * @file hbfloateruserauth.h
 * @brief The HBFloaterUserAuth class declaration
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

#ifndef LL_HBFLOATERUSERAUTH_H
#define LL_HBFLOATERUSERAUTH_H

#include "llfloater.h"

class LLButton;
class LLLineEditor;

class HBFloaterUserAuth final : public LLFloater
{
public:
	typedef void (*HBFloaterUserAuthCallback)(const LLUUID auth_id,
											  const std::string username,
											  const std::string password,
											  bool validated);

	~HBFloaterUserAuth() override;

	bool postBuild() override;
	void draw() override;

	static void request(const std::string& host, const std::string& realm,
						const LLUUID& auth_id,
						HBFloaterUserAuthCallback callback);

private:
	HBFloaterUserAuth(const std::string& host, const std::string& realm,
					  const LLUUID& auth_id,
					  HBFloaterUserAuthCallback callback);

	void doCallback(bool validated);

	static void onButtonOK(void* user_data);
	static void onButtonCancel(void* user_data);
	static void onCommitCheckBox(LLUICtrl* ctrl, void* user_data);
	static bool onHandleKeyCallback(KEY key, MASK mask, LLLineEditor* caller,
									void* user_data);
	static void onKeystrokeCallback(LLLineEditor*, void* user_data);

private:
	void				(*mUserAuthCallback)(const LLUUID auth_id,
											 const std::string username,
											 const std::string password,
											 bool validated);

	LLButton*			mOKBtn;
	LLLineEditor*		mUserNameInputLine;
	LLLineEditor*		mPasswordInputLine;

	LLUUID				mAuthId;

	std::string			mHost;
	std::string			mRealm;

	bool				mMustClose;
	bool				mCallbackDone;

	static uuid_list_t	sAuthList;
};

#endif	// LL_HBFLOATERUSERAUTH_H
