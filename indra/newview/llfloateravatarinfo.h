/**
 * @file llfloateravatarinfo.h
 * @brief LLFloaterAvatarInfo class definition
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

/**
 * Avatar information as shown in a floating window from right-click
 * Profile.  Used for editing your own avatar info.  Just a wrapper
 * for LLPanelAvatar, shared with the Find directory.
 */

#ifndef LL_LLFLOATERAVATARINFO_H
#define LL_LLFLOATERAVATARINFO_H

#include "llpanelavatar.h"
#include "llpreview.h"

class LLFloaterAvatarInfo : public LLPreview
{
protected:
	LOG_CLASS(LLFloaterAvatarInfo);

public:
	LLFloaterAvatarInfo(const LLUUID& avatar_id);
	~LLFloaterAvatarInfo() override;

	static void* createPanelAvatar(void* data);

	bool postBuild() override;

	void draw() override;

	bool canClose() override;

	void loadAsset() override;
	EAssetStatus getAssetStatus() override;

	// Lists the agents groups, based on the info held in LLAgent, and flagging
	// hidden groups as such, via the use of an Italics font and an explanatory
	// tool tip. HB
	void listAgentGroups();

	// Core method, does not do anything funny with online status or tab
	// selection.
	static LLFloaterAvatarInfo* show(const LLUUID& avatar_id);

	static void showFromObject(const LLUUID& avatar_id,
							   std::string tab_name = std::string());

	static void showFromDirectory(const LLUUID& avatar_id);

	static void showFromFriend(const LLUUID& agent_id, bool online);

	static void showFromProfile(const LLUUID& avatar_id, LLRect rect);

	static LLFloaterAvatarInfo* getInstance(const LLUUID& id);
	static void showProfileCallback(S32 option, void* userdata);
	static void callbackLoadAvatarName(const LLUUID& agent_id,
									   const LLAvatarName& avatar_name);

	static std::string getProfileURL(const std::string& user_name);

private:
	LLUUID			mAvatarID;			// For which avatar is this window ?
	LLPanelAvatar*	mPanelAvatarp;
	EOnlineStatus	mSuggestedOnlineStatus;

	typedef fast_hmap<LLUUID, LLFloaterAvatarInfo*> instances_map_t;
	static instances_map_t sInstances;
};

#endif
