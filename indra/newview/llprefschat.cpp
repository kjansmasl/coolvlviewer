/** 
 * @file llprefschat.cpp
 * @author James Cook, Richard Nelson
 * @brief Chat preferences panel
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

#include "llprefschat.h"

#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llradiogroup.h"
#include "llstylemap.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"

#include "llviewercontrol.h"

class LLPrefsChatImpl final : public LLPanel
{
public:
	LLPrefsChatImpl();
	~LLPrefsChatImpl() override		{}

	void apply();
	void cancel();

private:
	static void onCommitChatFullWidth(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBoxedMessages(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckChatBubbles(LLUICtrl* ctrl, void* user_data);
	static void onCommitTabAutoCompleteName(LLUICtrl* ctrl, void* user_data);
	void refreshValues();

private:
	S32	mChatSize;
	S32	mChatMaxLines;
	U32 mPlayChatAnims;
	U32 mLinksForChattingObjects;
	F32	mChatPersist;
	F32	mConsoleOpacity;
	F32	mBubbleOpacity;
	LLColor4 mSystemChatColor;
	LLColor4 mUserChatColor;
	LLColor4 mAgentChatColor;
	LLColor4 mIMChatColor;
	LLColor4 mObjectChatColor;
	LLColor4 mDirectChatColor;
	LLColor4 mOwnerSayChatColor;
	LLColor4 mBGChatColor;
	LLColor4 mScriptErrorColor;
	LLColor4 mHTMLLinkColor;
	bool mChatFullWidth;
	bool mDisableMessagesSpacing;
	bool mConsoleBoxPerMessage;
	bool mAutoFocusChat;
	bool mCloseChatOnReturn;
	bool mShowTimestamps;
	bool mPlayTypingAnim;
	bool mPlayTypingSound;
	bool mShowTypingInfo;
	bool mChatBubbles;
	bool mTabAutoCompleteName;
	bool mSelectAutoCompletedPart;
};

LLPrefsChatImpl::LLPrefsChatImpl()
:	LLPanel(std::string("Chat Preferences Panel"))
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_chat.xml");
	childSetCommitCallback("chat_full_width_check", onCommitChatFullWidth, this);
	childSetCommitCallback("console_box_per_message_check", onCommitCheckBoxedMessages, this);
	childSetCommitCallback("bubble_text_chat", onCommitCheckChatBubbles, this);
	childSetCommitCallback("tab_auto_complete_name_check", onCommitTabAutoCompleteName, this);
	refreshValues(); // initialize member data from saved settings
	childSetEnabled("disable_messages_spacing_check", !mConsoleBoxPerMessage);
	childSetEnabled("show_typing_info_check", !mChatBubbles);
	childSetEnabled("select_auto_completed_part_check", mTabAutoCompleteName);
}

//static
void LLPrefsChatImpl::onCommitChatFullWidth(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsChatImpl* self = (LLPrefsChatImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check && check->get() != (bool)self->mChatFullWidth)
	{
		gNotifications.add("InEffectAfterRestart");
	}
}

//static
void LLPrefsChatImpl::onCommitCheckBoxedMessages(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsChatImpl* self = (LLPrefsChatImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		self->childSetEnabled("disable_messages_spacing_check", !check->get());
	}
}

//static
void LLPrefsChatImpl::onCommitCheckChatBubbles(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsChatImpl* self = (LLPrefsChatImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		self->childSetEnabled("show_typing_info_check", !check->get());
	}
}

//static
void LLPrefsChatImpl::onCommitTabAutoCompleteName(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsChatImpl* self = (LLPrefsChatImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		self->childSetEnabled("select_auto_completed_part_check", check->get());
	}
}

void LLPrefsChatImpl::refreshValues()
{
	//set values
	mChatSize					= gSavedSettings.getS32("ChatFontSize");
	mChatMaxLines				= gSavedSettings.getU32("ChatConsoleMaxLines");
	mPlayChatAnims				= gSavedSettings.getU32("PlayChatAnims");
	mLinksForChattingObjects	= gSavedSettings.getU32("LinksForChattingObjects");
	mConsoleOpacity				= gSavedSettings.getF32("ConsoleBackgroundOpacity");
	mBubbleOpacity				= gSavedSettings.getF32("ChatBubbleOpacity");
	mChatPersist				= gSavedSettings.getF32("ChatPersistTime");
	mShowTimestamps				= gSavedSettings.getBool("ChatShowTimestamps");
	mChatBubbles				= gSavedSettings.getBool("UseChatBubbles");
	mChatFullWidth				= gSavedSettings.getBool("ChatFullWidth");
	mDisableMessagesSpacing		= gSavedSettings.getBool("DisableMessagesSpacing");
	mConsoleBoxPerMessage		= gSavedSettings.getBool("ConsoleBoxPerMessage");
	mAutoFocusChat				= gSavedSettings.getBool("AutoFocusChat");
	mCloseChatOnReturn			= gSavedSettings.getBool("CloseChatOnReturn");
	mPlayTypingAnim				= gSavedSettings.getBool("PlayTypingAnim"); 
	mPlayTypingSound			= gSavedSettings.getBool("UISndTypingEnable"); 
	mShowTypingInfo				= gSavedSettings.getBool("ShowTypingInfo"); 
	mTabAutoCompleteName		= gSavedSettings.getBool("TabAutoCompleteName"); 
	mSelectAutoCompletedPart	= gSavedSettings.getBool("SelectAutoCompletedPart"); 
	mSystemChatColor			= gSavedSettings.getColor4("SystemChatColor");
	mUserChatColor				= gSavedSettings.getColor4("UserChatColor");
	mAgentChatColor				= gSavedSettings.getColor4("AgentChatColor");
	mIMChatColor				= gSavedSettings.getColor4("IMChatColor");
	mObjectChatColor			= gSavedSettings.getColor4("ObjectChatColor");
	mDirectChatColor			= gSavedSettings.getColor4("DirectChatColor");
	mOwnerSayChatColor			= gSavedSettings.getColor4("llOwnerSayChatColor");
	mBGChatColor				= gSavedSettings.getColor4("BackgroundChatColor");
	mScriptErrorColor			= gSavedSettings.getColor4("ScriptErrorColor");
	mHTMLLinkColor				= gSavedSettings.getColor4("HTMLLinkColor");
}

void LLPrefsChatImpl::cancel()
{
	gSavedSettings.setS32("ChatFontSize",				mChatSize);
	gSavedSettings.setU32("ChatConsoleMaxLines",		mChatMaxLines);
	gSavedSettings.setU32("PlayChatAnims",				mPlayChatAnims);
	gSavedSettings.setU32("LinksForChattingObjects",	mLinksForChattingObjects);
	gSavedSettings.setF32("ChatPersistTime",			mChatPersist);
	gSavedSettings.setF32("ConsoleBackgroundOpacity",	mConsoleOpacity);
	gSavedSettings.setF32("ChatBubbleOpacity",			mBubbleOpacity);
	gSavedSettings.setBool("ChatShowTimestamps",		mShowTimestamps);
	gSavedSettings.setBool("UseChatBubbles",			mChatBubbles);
	gSavedSettings.setBool("ChatFullWidth",				mChatFullWidth);
	gSavedSettings.setBool("DisableMessagesSpacing",	mDisableMessagesSpacing);
	gSavedSettings.setBool("ConsoleBoxPerMessage",		mConsoleBoxPerMessage);
	gSavedSettings.setBool("AutoFocusChat",				mAutoFocusChat);
	gSavedSettings.setBool("CloseChatOnReturn",			mCloseChatOnReturn);
	gSavedSettings.setBool("PlayTypingAnim",			mPlayTypingAnim); 
	gSavedSettings.setBool("UISndTypingEnable",			mPlayTypingSound); 
	gSavedSettings.setBool("ShowTypingInfo",			mShowTypingInfo); 
	gSavedSettings.setBool("TabAutoCompleteName",		mTabAutoCompleteName); 
	gSavedSettings.setBool("SelectAutoCompletedPart",	mSelectAutoCompletedPart); 
	gSavedSettings.setColor4("SystemChatColor",			mSystemChatColor);
	gSavedSettings.setColor4("UserChatColor",			mUserChatColor);
	gSavedSettings.setColor4("AgentChatColor",			mAgentChatColor);
	gSavedSettings.setColor4("IMChatColor",				mIMChatColor);
	gSavedSettings.setColor4("ObjectChatColor",			mObjectChatColor);
	gSavedSettings.setColor4("DirectChatColor",			mDirectChatColor);
	gSavedSettings.setColor4("llOwnerSayChatColor",		mOwnerSayChatColor);
	gSavedSettings.setColor4("BackgroundChatColor",		mBGChatColor);
	gSavedSettings.setColor4("ScriptErrorColor",		mScriptErrorColor);
	gSavedSettings.setColor4("HTMLLinkColor",			mHTMLLinkColor);
}

void LLPrefsChatImpl::apply()
{
	// member values become the official values and cancel becomes a no-op.
	refreshValues();
	LLTextEditor::setLinksColor(mHTMLLinkColor);
}

//---------------------------------------------------------------------------

LLPrefsChat::LLPrefsChat()
:	impl(* new LLPrefsChatImpl())
{
}

LLPrefsChat::~LLPrefsChat()
{
	delete &impl;
}

void LLPrefsChat::apply()
{
	impl.apply();
	gStyleMap.update();
}

void LLPrefsChat::cancel()
{
	impl.cancel();
}

LLPanel* LLPrefsChat::getPanel()
{
	return &impl;
}
