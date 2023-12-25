/**
 * @file llfloaterchat.h
 * @brief LLFloaterChat class definition
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
 * Actually the "Chat History" floater.
 * Should be llfloaterchathistory, not llfloaterchat.
 */

#ifndef LL_LLFLOATERCHAT_H
#define LL_LLFLOATERCHAT_H

#include <map>

#include "llfloater.h"

class LLAvatarName;
class LLButton;
class LLChat;
class LLChatBar;
class LLPanelActiveSpeakers;
class LLViewerTextEditor;

class LLFloaterChat final : public LLFloater,
						    public LLUISingleton<LLFloaterChat, LLFloaterChat>
{
protected:
	LOG_CLASS(LLFloaterChat);

public:
	LLFloaterChat(const LLSD& seed);
	~LLFloaterChat() override		{}

	void setVisible(bool b) override;
	void draw() override;
	bool postBuild() override;
	void onClose(bool app_quitting) override;
	void onVisibilityChange(bool cur_visibility) override;
	void setMinimized(bool minimize) override;
	void onFocusReceived() override;
	void onFocusLost() override;

	void updateConsoleVisibility();

	static void setHistoryCursorAndScrollToEnd();
	static bool isOwnNameInText(const std::string& text_line);

	// Add chat to console and history list.
	// Color based on source, type, distance.
	static void addChat(LLChat& chat, bool from_im = false,
						bool local_agent = false);

	// Add chat to history alone.
	static void addChatHistory(LLChat& chat, bool log_to_file = true);

	static void loadHistory();

	// Visibility policy for LLUISingleton
	static bool visible(LLFloater* instance, const LLSD& key);
	static void show(LLFloater* instance, const LLSD& key);
	static void hide(LLFloater* instance, const LLSD& key);

	static void focus();
	static bool isFocused();

	static void resolveSLURLs(const LLChat& chat);

	static void substituteSLURL(const LLUUID& id, const std::string& slurl,
								const std::string& substitute);
	static void substitutionDone(const LLUUID& id);

private:
	static void* createSpeakersPanel(void* data);
	static void* createChatPanel(void* data);

	static void onClickMute(void* data);
	static void onClickToggleShowMute(LLUICtrl* ctrl, void* data);
	static void onClickToggleActiveSpeakers(void* userdata);

	static void chatFromLog(S32 type, const LLSD& data, void* userdata);

private:
	LLChatBar*				mChatBarPanel;

	LLPanelActiveSpeakers*	mSpeakerPanel;

	LLButton*				mToggleActiveSpeakersBtn;

	LLViewerTextEditor*		mHistoryWithoutMutes;
	LLViewerTextEditor*		mHistoryWithMutes;

	uuid_list_t				mPendingIds;

	bool					mFocused;
};

extern const std::string gChatFloaterName;

#endif
