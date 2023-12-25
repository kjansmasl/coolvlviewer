/**
 * @file llchatbar.h
 * @brief LLChatBar class definition
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

#ifndef LL_LLCHATBAR_H
#define LL_LLCHATBAR_H

#include "llpanel.h"
#include "llframetimer.h"

#include "llchat.h"
#include "llviewercontrol.h"

class LLButton;
class LLChatBarGestureObserver;
class LLComboBox;
class LLFlyoutButton;
class LLFrameTimer;
class LLLineEditor;
class LLUICtrl;

constexpr S32 CHAT_BAR_HEIGHT = 28;

class LLChatBar final : public LLPanel
{
protected:
	LOG_CLASS(LLChatBar);

public:
	LLChatBar(const std::string& name);
	LLChatBar(const std::string& name, const LLRect& rect);
		~LLChatBar() override;

	bool postBuild() override;

	void reshape(S32 width, S32 height, bool called_from_parent) override;
	void refresh() override;

	bool handleKeyHere(KEY key, MASK mask) override;

	// Adjust buttons and input field for width
	void layout();

	void refreshGestures();

	// Move cursor into chat input field.
	void setKeyboardFocus(bool b);

	// Ignore arrow keys for chat bar
	void setIgnoreArrowKeys(bool b);

	bool hasTextEditor();
	bool inputEditorHasFocus();
	std::string getCurrentChat();

	// Since chat bar logic is reused for chat history gesture combo box might
	// not be a direct child
	void setGestureCombo(LLComboBox* combo);

	// Send a chat (after stripping /20foo channel chats). "animate" triggers
	// the nodding, whispering or shouting animations.
	void sendChatFromViewer(LLWString wtext, EChatType type, bool animate,
							bool lua_propagate = true);
	void sendChatFromViewer(const std::string& utf8text, EChatType type,
							bool animate, bool lua_propagate = true);

	// If input of the form "/20foo" or "/20 foo", returns "foo" and channel
	// 20. Otherwise returns input and channel 0.
	LLWString stripChannelNumber(const LLWString& mesg, S32* channel);

	static void startChat(const char* line);
	static void stopChat();

	static std::string getMatchingAvatarName(const std::string& match);

private:
	void setVisible(bool visible) override;

	void sendChat(EChatType type);
	void updateChat();

	static void toggleChatHistory(void*);

	static void	onClickSay(LLUICtrl*, void* userdata);
	static void onClickOpenTextEditor(void* userdata);

	static void	onTabClick(void* userdata);
	static void	onInputEditorKeystroke(LLLineEditor* caller, void* userdata);
	static void	onInputEditorScrolled(LLLineEditor* caller, void* userdata);
	static void	onInputEditorFocusLost(LLFocusableElement* caller, void*);
	static void	onInputEditorGainFocus(LLFocusableElement* caller, void*);

	static void onCommitGesture(LLUICtrl* ctrl, void* data);

private:
	LLButton*					mOpenTextEditorButton;
	LLButton*					mHistoryButton;
	LLComboBox*					mGestureCombo;
	LLFlyoutButton*				mSayFlyoutButton;
	LLLineEditor*				mInputEditor;

	LLFrameTimer				mGestureLabelTimer;

	// Which non-zero channel did we last chat on?
	S32							mLastSpecialChatChannel;

	LLChatBarGestureObserver*	mObserver;

	bool 						mSecondary;
	bool						mIsBuilt;
	bool						mHasScrolledOnce;
	bool						mLastSwappedShortcuts;

	typedef std::set<std::string> av_names_list_t;
	static av_names_list_t		sIgnoredNames;

public:
	static bool					sSwappedShortcuts;
};

extern LLChatBar* gChatBarp;

#endif
