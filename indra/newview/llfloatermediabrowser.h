/**
 * @file llfloatermediabrowser.cpp
 * @brief Web browser floaters
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERHTMLHELP_H
#define LL_LLFLOATERHTMLHELP_H

#include "llfloater.h"
#include "llhtmlhelp.h"

#include "llmediactrl.h"

class LLButton;
class LLComboBox;
class LLParcel;
class LLPluginClassMedia;
class LLTextBox;

class LLViewerHtmlHelp final : public LLHtmlHelp
{
public:
	LLViewerHtmlHelp();
	~LLViewerHtmlHelp() override;

	void show() override;
	void show(std::string start_url) override;
	void show(std::string start_url, std::string title);

	static bool onClickF1HelpLoadURL(const LLSD& notification,
									 const LLSD& response);
};

class LLFloaterMediaBrowser final : public LLFloater,
									public LLViewerMediaObserver
{
protected:
	LOG_CLASS(LLFloaterMediaBrowser);

public:
	LLFloaterMediaBrowser(const LLSD& media_url);
	~LLFloaterMediaBrowser() override;

	bool postBuild() override;
	void onClose(bool app_quitting) override;
	void draw() override;

	// Inherited from LLViewerMediaObserver
	void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent evt) override;

	void openMedia(const std::string& media_url, bool trusted = false);
	void openMedia(const std::string& media_url, const std::string& target,
				   bool trusted = false);

	void setCurrentURL(const std::string& url);

	static LLFloaterMediaBrowser* getInstance(const LLSD& media_url);
	static LLFloaterMediaBrowser* showInstance(const LLSD& media_url,
											   bool trusted = false);

private:
	void buildURLHistory();
	void geometryChanged(S32 x, S32 y, S32 width, S32 height);

	static void onEnterAddress(LLUICtrl* ctrl, void* user_data);
	static void onClickRefresh(void* user_data);
	static void onClickBack(void* user_data);
	static void onClickForward(void* user_data);
	static void onClickGo(void* user_data);
	static void onClickClose(void* user_data);
	static void onClickOpenWebBrowser(void* user_data);
	static void onClickAssign(void* user_data);
	static void onClickRewind(void* user_data);
	static void onClickPlay(void* user_data);
	static void onClickStop(void* user_data);
	static void onClickSeek(void* user_data);

private:
	LLMediaCtrl*			mBrowser;
	LLParcel*				mParcel;
	LLButton*				mBackButton;
	LLButton*				mForwardButton;
	LLButton*				mReloadButton;
	LLButton*				mRewindButton;
	LLButton*				mPlayButton;
	LLButton*				mPauseButton;
	LLButton*				mStopButton;
	LLButton*				mSeekButton;
	LLButton*				mGoButton;
	LLButton*				mCloseButton;
	LLButton*				mBrowserButton;
	LLButton*				mAssignButton;
	LLComboBox* 			mAddressCombo;
	LLTextBox*				mLoadingText;
	std::string				mInitalUrl;
	std::string				mCurrentURL;

	typedef std::vector<LLFloaterMediaBrowser*> instances_vec_t;
	static instances_vec_t	sInstances;
};

extern LLViewerHtmlHelp gViewerHtmlHelp;

#endif  // LL_LLFLOATERHTMLHELP_H
