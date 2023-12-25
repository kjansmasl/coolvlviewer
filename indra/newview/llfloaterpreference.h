/**
 * @file llfloaterpreference.h
 * @brief LLPreferenceCore class definition
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

#ifndef LL_LLFLOATERPREFERENCE_H
#define LL_LLFLOATERPREFERENCE_H

#include "llfloater.h"
#include "lltabcontainer.h"

class LLButton;
class LLMessageSystem;
class HBPanelGrids;
class LLPrefSkins;
class LLPrefsChat;
class HBPrefsCool;
class LLPrefsGeneral;
class LLPrefsGraphics;
class LLPrefsIM;
class LLPrefsInput;
class LLPrefsMedia;
class LLPrefsNetwork;
class LLPrefsNotifications;
class LLPrefsVoice;

class LLPreferenceCore
{
public:
	LLPreferenceCore(LLTabContainer* tab_container, LLButton* default_btn);
	~LLPreferenceCore();

	void apply();
	void cancel();

	LL_INLINE LLTabContainer* getTabContainer()		{ return mTabContainer; }

	void setPersonalInfo(const std::string& visibility, bool im_via_email,
						 const std::string& email, S32 verified);

	static void onTabChanged(void* user_data, bool from_click);

private:
	LLTabContainer*			mTabContainer;
	HBPanelGrids*			mPrefsGrids;
	LLPrefSkins*			mPrefsSkins;
	LLPrefsGeneral*			mPrefsGeneral;
	LLPrefsGraphics*		mPrefsGraphics;
	LLPrefsMedia*			mPrefsMedia;
	LLPrefsNetwork*			mPrefsNetwork;
	LLPrefsChat*			mPrefsChat;
	HBPrefsCool*			mPrefsCool;
	LLPrefsVoice*			mPrefsVoice;
	LLPrefsIM*				mPrefsIM;
	LLPrefsInput*			mPrefsInput;
	LLPrefsNotifications*	mPrefsNotifications;
};

// Floater to control preferences (display, audio, bandwidth, general.
class LLFloaterPreference final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterPreference>
{
	friend class LLUISingleton<LLFloaterPreference,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterPreference);

public:
	~LLFloaterPreference() override;

	bool postBuild() override;
	void onClose(bool app_quitting) override;

	void apply();
	void cancel();

	static void openInTab(S32 tab);

	// Static data update, called from message handler
	static void updateUserInfo(const std::string& visibility,
							   bool im_via_email, const std::string& email,
							   S32 verified = -1);

	// Refreshes all the graphics preferences menus
	static void refreshEnabledGraphics();

	// Keep this in sync with the tabs order in the floater
	enum PrefTabsIndexes {
		GENERAL_TAB = 0,
		INPUT_AND_CAMERA_TAB,
		NETWORK_AND_WEB_TAB,
		GRAPHICS_TAB,
		AUDIO_AND_MEDIA_TAB,
		TEXT_CHAT_TAB,
		IM_AND_LOGS_TAB,
		VOICE_CHAT_TAB,
		NOTIFICATIONS_TAB,
		SKINS_TAB,
		COOL_FEATURES_TAB,
		GRIDS_LIST_TAB,
		NUMBER_OF_TABS
	};

protected:
	// Open only via either openInTab() or the LLFloaterSingleton interface
	// (i.e. showInstance() or toggleInstance()).
	LLFloaterPreference(const LLSD&);

	static void	onClickAbout(void*);
	static void	onBtnOK(void*);
	static void	onBtnCancel(void*);
	static void	onBtnApply(void*);

protected:
	LLPreferenceCore*	mPreferenceCore;

	LLButton*			mAboutBtn;
	LLButton*			mOKBtn;
	LLButton*			mCancelBtn;
	LLButton*			mApplyBtn;
};

#endif  // LL_LLPREFERENCEFLOATER_H
