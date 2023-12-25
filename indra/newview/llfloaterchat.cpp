/**
 * @file llfloaterchat.cpp
 * @brief LLFloaterChat class implementation
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
 * Actually the "Chat History" floater.
 * Should be llfloaterchathistory, not llfloaterchat.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterchat.h"

#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llconsole.h"
#include "llstylemap.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llchatbar.h"
#include "llfloateractivespeakers.h"
#include "llfloaterchatterbox.h"
#include "llfloatermute.h"
#include "llfloaterscriptdebug.h"
#include "lllogchat.h"
#include "llmutelist.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewergesture.h"			// For triggering gestures
#include "llviewermenu.h"
#include "llviewertexteditor.h"
#include "llvoavatarself.h"
#include "llslurl.h"
#include "llstatusbar.h"
#include "hbviewerautomation.h"
#include "llweb.h"

// This name is used by and should stay in sync with the one used in
// floater_chat_history*.xml files. HB
const std::string gChatFloaterName = "chat";

static std::set<std::string> sHighlightWords;

//
// Helper functions
//

LLColor4 get_agent_chat_color(const LLChat& chat)
{
	if (gSavedSettings.getBool("HighlightOwnNameInChat"))
	{
		std::string text = chat.mText;
		size_t name_pos = text.find(chat.mFromName);
		if (name_pos == 0)
		{
			text = text.substr(chat.mFromName.length());
			if (text.find(": ") == 0)
			{
				text = text.substr(2);
			}
			else
			{
				text = text.substr(1);
			}
		}
		if (gAutomationp)
		{
			LLColor4 color;
			if (gAutomationp->onChatTextColoring(chat.mFromID, chat.mFromName,
												 text, color))
			{
				return color;
			}
		}
		if (LLFloaterChat::isOwnNameInText(text))
		{
			return gSavedSettings.getColor4("OwnNameChatColor");
		}
	}

	return gSavedSettings.getColor4("AgentChatColor");
}

LLColor4 get_text_color(const LLChat& chat)
{
	if (chat.mMuted)
	{
		return LLColor4(0.8f, 0.8f, 0.8f, 1.f);
	}

	LLColor4 text_color;

	switch (chat.mSourceType)
	{
		case CHAT_SOURCE_SYSTEM:
		case CHAT_SOURCE_UNKNOWN:
			text_color = gSavedSettings.getColor4("SystemChatColor");
			break;

		case CHAT_SOURCE_AGENT:
			if (gAgentID == chat.mFromID)
			{
				text_color = gSavedSettings.getColor4("UserChatColor");
			}
			else
			{
				text_color = get_agent_chat_color(chat);
			}
			break;

		case CHAT_SOURCE_OBJECT:
			if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
			{
			text_color = gSavedSettings.getColor4("ScriptErrorColor");
			}
			else if (chat.mChatType == CHAT_TYPE_OWNER)
			{
				// Message from one of our own objects
				text_color = gSavedSettings.getColor4("llOwnerSayChatColor");
			}
			else if (chat.mChatType == CHAT_TYPE_DIRECT)
			{
				// Used both for llRegionSayTo() and llInstantMesssage()
				// since there is no real reason to distinguish one from
				// another (both are seen only by us and the object may
				// pertain to anyone, us included).
				text_color = gSavedSettings.getColor4("DirectChatColor");
			}
			else
			{
				// Public object chat
				text_color = gSavedSettings.getColor4("ObjectChatColor");
			}
			break;

		default:
			text_color.setToWhite();
	}

	if (!chat.mPosAgent.isExactlyZero())
	{
		LLVector3 pos_agent = gAgent.getPositionAgent();
		F32 distance = dist_vec(pos_agent, chat.mPosAgent);
		if (distance > gAgent.getNearChatRadius())
		{
			// Diminish far-off chat
			text_color.mV[VALPHA] = 0.8f;
		}
	}

	return text_color;
}

void add_timestamped_line(LLViewerTextEditor* edit, LLChat chat,
						  const LLColor4& color)
{
	if (!edit) return;

	std::string line = chat.mText;

	bool prepend_newline = true;
	if (gSavedSettings.getBool("ChatShowTimestamps"))
	{
		edit->appendTime(prepend_newline);
		prepend_newline = false;
	}

	// If the msg is from an agent (not yourself though), extract out the
	// sender name and replace it with the hotlinked name.
	if (chat.mSourceType == CHAT_SOURCE_AGENT && chat.mFromID.notNull())
	{
		chat.mURL = llformat("secondlife:///app/agent/%s/about",
							 chat.mFromID.asString().c_str());
	}

	// If the chat line has an associated url, link it up to the name.
	if (!chat.mURL.empty() && (line.length() > chat.mFromName.length() &&
		(chat.mFromName.empty() || line.find(chat.mFromName, 0) == 0)))
	{
//MK
		if (!gRLenabled || !gRLInterface.mContainsShownames)
		{
//mk
			size_t pos;
			if (chat.mFromName.empty() ||
				chat.mFromName.find_first_not_of(' ') == std::string::npos)
			{
				// Name is empty... Set the link on the first word instead
				// (skipping leading spaces and the ':' separator)...
				pos = line.find_first_not_of(" :");
				if (pos == std::string::npos)
				{
					// No word found !
					pos = line.length();
					line += " ";
				}
				else
				{
					pos = line.find(' ', pos);
					if (pos == std::string::npos)
					{
						// Only one word in the line...
						pos = line.length();
						line += " ";
					}
				}
			}
			else
			{
				pos = chat.mFromName.length() + 1;
			}
			std::string start_line = line.substr(0, pos);
			line = line.substr(pos);
			const LLStyleSP& sourceStyle = gStyleMap.lookup(chat.mFromID,
															chat.mURL);
			edit->appendStyledText(start_line, false, prepend_newline,
								   sourceStyle);
			prepend_newline = false;
//MK
		}
//mk
	}
	edit->appendColoredText(line, false, prepend_newline, color);
}

void log_chat_text(const LLChat& chat)
{
	std::string histstr = chat.mText;
	static LLCachedControl<bool> stamp(gSavedPerAccountSettings,
									   "LogChatTimestamp");
	if (stamp)
	{
		histstr = LLLogChat::timestamp() + histstr;
	}
	LLLogChat::saveHistory(LLStringUtil::null, histstr);
}

bool make_words_list()
{
	static std::string nicknames;
	static LLCachedControl<std::string> saved_nicks(gSavedPerAccountSettings,
												    "HighlightNicknames");
	bool changed = false;
	if (nicknames != std::string(saved_nicks))
	{
		nicknames = saved_nicks;
		changed = true;
	}

	LLAvatarName avatar_name;
	bool do_highlight = gSavedPerAccountSettings.getBool("HighlightDisplayName") &&
						LLAvatarNameCache::useDisplayNames() &&
						LLAvatarNameCache::get(gAgentID, &avatar_name);

	static std::string display_name;
	static bool highlight_display_name = false;
	if (do_highlight != highlight_display_name)
	{
		highlight_display_name = do_highlight;
		changed = true;
		if (!highlight_display_name)
		{
			display_name.clear();
		}
	}

	std::string name;
	if (highlight_display_name)
	{
		if (!avatar_name.mIsDisplayNameDefault)
		{
			name = avatar_name.mDisplayName;
			LLStringUtil::toLower(name);
		}

		if (name != display_name)
		{
			display_name = name;
			changed = true;
		}
	}

	if (changed && isAgentAvatarValid())
	{
		// Rebuild the whole list
		sHighlightWords.clear();

		// First, fetch the avatar name (note: we do not use
		// gSavedSettings.getString("[First/Last]Name") here,
		// because those are not set when using --autologin).
		LLNameValue* firstname = gAgentAvatarp->getNVPair("FirstName");
		LLNameValue* lastname = gAgentAvatarp->getNVPair("LastName");
		name.assign(firstname->getString());
		LLStringUtil::toLower(name);
		sHighlightWords.emplace(name);
		name.assign(lastname->getString());
		if (name != "Resident" && sHighlightWords.count(name) == 0)
		{
			LLStringUtil::toLower(name);
			sHighlightWords.emplace(name);
		}

		std::string part;

		if (!display_name.empty())
		{
			name = display_name;
			size_t index;
			while ((index = name.find(' ')) != std::string::npos)
			{
				part = name.substr(0, index);
				name = name.substr(index + 1);
				if (part.length() > 3)
				{
					sHighlightWords.emplace(part);
				}
			}
			if (name.length() > 3 && sHighlightWords.count(name) == 0)
			{
				sHighlightWords.emplace(name);
			}
		}

		if (!nicknames.empty())
		{
			name = nicknames;
			LLStringUtil::toLower(name);
			// Accept space and comma separated list
			LLStringUtil::replaceChar(name, ' ', ',');

			size_t index;
			while ((index = name.find(',')) != std::string::npos)
			{
				part = name.substr(0, index);
				name = name.substr(index + 1);
				if (part.length() > 2)
				{
					sHighlightWords.emplace(part);
				}
			}
			if (name.length() > 2 && sHighlightWords.count(name) == 0)
			{
				sHighlightWords.emplace(name);
			}
		}
	}

	return changed;
}

//
// Member Functions
//
LLFloaterChat::LLFloaterChat(const LLSD& seed)
:	LLFloater(gChatFloaterName, "FloaterChatRect", "", RESIZE_YES, 440, 100,
			  DRAG_ON_TOP, MINIMIZE_NO, CLOSE_YES),
	mChatBarPanel(NULL),
	mSpeakerPanel(NULL),
	mToggleActiveSpeakersBtn(NULL),
	mHistoryWithoutMutes(NULL),
	mHistoryWithMutes(NULL),
	mFocused(false)
{
	std::string xml_file;
	if (gSavedSettings.getBool("UseOldChatHistory"))
	{
		xml_file = "floater_chat_history2.xml";
	}
	else
	{
		xml_file = "floater_chat_history.xml";
		mFactoryMap["chat_panel"] = LLCallbackMap(createChatPanel, this);
	}
	mFactoryMap["active_speakers_panel"] = LLCallbackMap(createSpeakersPanel,
														 this);

	// false so to not automatically open singleton floaters (as result of
	// getInstance())
	LLUICtrlFactory::getInstance()->buildFloater(this, xml_file,
												 &getFactoryMap(), false);
}

//virtual
bool LLFloaterChat::postBuild()
{
	if (mChatBarPanel)
	{
		mChatBarPanel->setGestureCombo(getChild<LLComboBox>("Gesture",
															true, false));
	}

	childSetCommitCallback("show mutes", onClickToggleShowMute, this);

	mHistoryWithoutMutes = getChild<LLViewerTextEditor>("Chat History Editor");
	mHistoryWithoutMutes->setPreserveSegments(true);
	mHistoryWithoutMutes->setCustomMenuType("chat_history");
	mHistoryWithMutes = getChild<LLViewerTextEditor>("Chat History Editor with mute");
	mHistoryWithMutes->setPreserveSegments(true);
	mHistoryWithMutes->setVisible(false);
	mHistoryWithMutes->setCustomMenuType("chat_history");

	mToggleActiveSpeakersBtn = getChild<LLButton>("toggle_active_speakers_btn");
	mToggleActiveSpeakersBtn->setClickedCallback(onClickToggleActiveSpeakers,
												 this);

	return true;
}

//virtual
void LLFloaterChat::setVisible(bool visible)
{
	LLFloater::setVisible(visible);
	gSavedSettings.setBool("ShowChatHistory", visible);
}

//virtual
void LLFloaterChat::draw()
{
	bool active_speakers_panel = mSpeakerPanel && mSpeakerPanel->getVisible();
	mToggleActiveSpeakersBtn->setValue(active_speakers_panel);
	if (active_speakers_panel)
	{
		mSpeakerPanel->refreshSpeakers();
	}

	if (mChatBarPanel)
	{
		mChatBarPanel->refresh();
	}

	LLFloater::draw();
}

//virtual
void LLFloaterChat::onClose(bool app_quitting)
{
	if (!app_quitting)
	{
		gSavedSettings.setBool("ShowChatHistory", false);
	}
	setVisible(false);
	mFocused = false;
}

//virtual
void LLFloaterChat::onVisibilityChange(bool new_visibility)
{
	// Hide the chat overlay when our history is visible.
	updateConsoleVisibility();

	// stop chat history tab from flashing when it appears
	if (new_visibility)
	{
		LLFloaterChatterBox::getInstance()->setFloaterFlashing(this, false);
	}

	LLFloater::onVisibilityChange(new_visibility);
}

//virtual
void LLFloaterChat::setMinimized(bool minimized)
{
	LLFloater::setMinimized(minimized);
	updateConsoleVisibility();
}

//virtual
void LLFloaterChat::onFocusReceived()
{
	// This keeps track of the panel focus, independently of the keyboard
	// focus (which might get stolen by the main chat bar). Also, we don't
	// register a focused event if the chat floater got its own chat bar
	// (in which case the latter will actually receive the keyboard focus).
	if (!mChatBarPanel)
	{
		mFocused = true;
	}
}

//virtual
void LLFloaterChat::onFocusLost()
{
	mFocused = false;
}

void LLFloaterChat::updateConsoleVisibility()
{
	if (!gConsolep) return;

	// Determine whether we should show console due to not being visible
	gConsolep->setVisible(isMinimized() ||	// are we minimized ?
						  // are we not in part of UI being drawn ?
						  !isInVisibleChain() ||
						  // are we hosted in a minimized floater ?
						  (getHost() && getHost()->isMinimized()));
}

//static
void LLFloaterChat::addChatHistory(LLChat& chat, bool log_to_file)
{
	LLFloaterChat* self = LLFloaterChat::getInstance(LLSD());

	static LLCachedControl<bool> log_chat(gSavedPerAccountSettings, "LogChat");
	if (log_to_file && log_chat)
	{
		log_chat_text(chat);
	}

	LLColor4 color;
	if (log_to_file)
	{
		color = get_text_color(chat);
	}
	else
	{
		color = LLColor4::grey;	// Recap from log file.
	}

	if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
	{
		LLFloaterScriptDebug::addScriptLine(chat.mText, chat.mFromName,
											color, chat.mFromID);
		if (!gSavedSettings.getBool("ScriptErrorsAsChat"))
		{
			return;
		}
	}

	// Could flash the chat button in the status bar here. JC

	self->mHistoryWithoutMutes->setParseHTML(true);
	self->mHistoryWithMutes->setParseHTML(true);

	if (!chat.mMuted)
	{
		add_timestamped_line(self->mHistoryWithoutMutes, chat, color);
		add_timestamped_line(self->mHistoryWithMutes, chat, color);
	}
	else
	{
		// Desaturate muted chat
		LLColor4 muted_color = lerp(color, LLColor4::grey, 0.5f);
		add_timestamped_line(self->mHistoryWithMutes, chat, muted_color);
	}

	// Add objects as transient speakers that can be muted
	if (chat.mSourceType == CHAT_SOURCE_OBJECT)
	{
		self->mSpeakerPanel->setSpeaker(chat.mFromID, chat.mFromName,
										LLSpeaker::STATUS_NOT_IN_CHANNEL,
										LLSpeaker::SPEAKER_OBJECT,
										chat.mOwnerID);
	}

	// Start tab flashing on incoming text from other users (ignoring system
	// text, etc)
	if (!self->isInVisibleChain() && chat.mSourceType == CHAT_SOURCE_AGENT)
	{
		LLFloaterChatterBox::getInstance()->setFloaterFlashing(self, true);
	}
}

//static
void LLFloaterChat::setHistoryCursorAndScrollToEnd()
{
	LLFloaterChat* self = LLFloaterChat::getInstance(LLSD());
	if (!self) return;

	if (self->mHistoryWithoutMutes)
	{
		self->mHistoryWithoutMutes->setCursorAndScrollToEnd();
	}
	if (self->mHistoryWithMutes)
	{
		 self->mHistoryWithMutes->setCursorAndScrollToEnd();
	}
}

//static
void LLFloaterChat::onClickMute(void* data)
{
	LLFloaterChat* self = (LLFloaterChat*)data;

	LLComboBox*	chatter_combo = self->getChild<LLComboBox>("chatter combobox");

	const std::string& name = chatter_combo->getSimple();
	LLUUID id = chatter_combo->getCurrentID();

	if (name.empty()) return;

	LLMute mute(id);
	mute.setFromDisplayName(name);
	if (LLMuteList::add(mute))
	{
		LLFloaterMute::selectMute(mute.mID);
	}
}

//static
void LLFloaterChat::onClickToggleShowMute(LLUICtrl* ctrl, void* data)
{
	LLFloaterChat* self = (LLFloaterChat*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!check || !self || !self->mHistoryWithoutMutes ||
		!self->mHistoryWithMutes)
	{
		return;
	}

	if (check->get())
	{
		self->mHistoryWithoutMutes->setVisible(false);
		self->mHistoryWithMutes->setVisible(true);
		self->mHistoryWithMutes->setCursorAndScrollToEnd();
	}
	else
	{
		self->mHistoryWithMutes->setVisible(false);
		self->mHistoryWithoutMutes->setVisible(true);
		self->mHistoryWithoutMutes->setCursorAndScrollToEnd();
	}
}

// Put a line of chat in all the right places
//static
void LLFloaterChat::addChat(LLChat& chat, bool from_im, bool local_agent)
{
	LLFloaterChat* self = LLFloaterChat::getInstance(LLSD());
	if (!self) return;

//MK
	if (gRLenabled && chat.mText == "")
	{
		// In case crunchEmote() returned an empty string, just abort.
		return;
	}
//mk

	LLColor4 text_color = get_text_color(chat);

	bool no_script_debug = chat.mChatType == CHAT_TYPE_DEBUG_MSG &&
						   !gSavedSettings.getBool("ScriptErrorsAsChat");

	if (!no_script_debug && !local_agent && gConsolep && !chat.mMuted)
	{
		if (chat.mSourceType == CHAT_SOURCE_SYSTEM)
		{
			text_color = gSavedSettings.getColor("SystemChatColor");
		}
		else if (from_im)
		{
			text_color = gSavedSettings.getColor("IMChatColor");
		}
		// We display anything if it is not an IM. If it's an IM, check pref.
		if (!from_im || gSavedSettings.getBool("IMInChatConsole"))
		{
			gConsolep->addConsoleLine(chat.mText, text_color);
		}
	}

	static LLCachedControl<bool> log_im(gSavedPerAccountSettings, "LogChatIM");
	if (from_im && log_im)
	{
		log_chat_text(chat);
	}

	if (from_im)
	{
		if (gSavedSettings.getBool("IMInChatHistory"))
		{
			addChatHistory(chat, false);
		}
	}
	else
	{
		addChatHistory(chat, true);
	}

	resolveSLURLs(chat);
}

//static
void LLFloaterChat::resolveSLURLs(const LLChat& chat)
{
	LLFloaterChat* self = LLFloaterChat::findInstance(LLSD());
	if (!self) return;

	// SLURLs resolving: fetch the Ids associated with avatar/group/experience
	// name SLURLs present in the text.
	uuid_list_t agent_ids = LLSLURL::findSLURLs(chat.mText);
	if (agent_ids.empty()) return;

	// Add to the existing list of pending Ids
	self->mPendingIds.insert(agent_ids.begin(), agent_ids.end());

	// Launch the SLURLs resolution. Note that the substituteSLURL() callback
	// will be invoked immediately for names already in cache. That's why we
	// needed to push the untranslated SLURLs in the chat first (together with
	// the fact that doing so, gets the SLURLs auto-parsed and puts a link
	// segment on them in the text editor, segment link that will be preserved
	// when the SLURL will be replaced with the corresponding name).
	LLSLURL::resolveSLURLs();
}

//static
void LLFloaterChat::substituteSLURL(const LLUUID& id, const std::string& slurl,
									const std::string& substitute)
{
	LLFloaterChat* self = LLFloaterChat::findInstance(LLSD());
	if (self && self->mPendingIds.count(id))
	{
		self->mHistoryWithoutMutes->replaceTextAll(slurl, substitute, true);
		self->mHistoryWithoutMutes->setEnabled(false);
		self->mHistoryWithMutes->replaceTextAll(slurl, substitute, true);
		self->mHistoryWithMutes->setEnabled(false);
		if (gConsolep)
		{
			gConsolep->replaceAllText(slurl, substitute, true);
		}
	}
}

//static
void LLFloaterChat::substitutionDone(const LLUUID& id)
{
	LLFloaterChat* self = LLFloaterChat::findInstance(LLSD());
	if (self)
	{
		self->mPendingIds.erase(id);
	}
}

//static
void LLFloaterChat::loadHistory()
{
	LLLogChat::loadHistory(LLStringUtil::null, &chatFromLog,
						   (void*)LLFloaterChat::getInstance(LLSD()));
}

//static
void LLFloaterChat::chatFromLog(S32 type, const LLSD& data, void* userdata)
{
	if (type == LLLogChat::LOG_LINE)
	{
		LLChat chat;
		chat.mText = data["line"].asString();
		addChatHistory(chat, false);
	}
}

//static
void* LLFloaterChat::createSpeakersPanel(void* data)
{
	LLFloaterChat* self = (LLFloaterChat*)data;
	self->mSpeakerPanel =
		new LLPanelActiveSpeakers(LLLocalSpeakerMgr::getInstance(), true);
	return self->mSpeakerPanel;
}

//static
void* LLFloaterChat::createChatPanel(void* data)
{
	LLFloaterChat* self = (LLFloaterChat*)data;
	self->mChatBarPanel = new LLChatBar("floating_chat_bar");
	return self->mChatBarPanel;
}

//static
void LLFloaterChat::onClickToggleActiveSpeakers(void* userdata)
{
	LLFloaterChat* self = (LLFloaterChat*)userdata;
//MK
	if (gRLenabled && gRLInterface.mContainsShownames)
	{
		if (!self->mSpeakerPanel->getVisible()) return;
	}
//mk
	self->mSpeakerPanel->setVisible(!self->mSpeakerPanel->getVisible());
}

//static
bool LLFloaterChat::visible(LLFloater* instance, const LLSD& key)
{
	return VisibilityPolicy<LLFloater>::visible(instance, key);
}

//static
void LLFloaterChat::show(LLFloater* instance, const LLSD& key)
{
	VisibilityPolicy<LLFloater>::show(instance, key);
}

//static
void LLFloaterChat::hide(LLFloater* instance, const LLSD& key)
{
	if (instance->getHost())
	{
		LLFloaterChatterBox::hideInstance();
	}
	else
	{
		VisibilityPolicy<LLFloater>::hide(instance, key);
	}
}

//static
void LLFloaterChat::focus()
{
	LLFloaterChat* self = (LLFloaterChat*)findInstance();
	if (self)
	{
		self->setFocus(true);
	}
}

//static
bool LLFloaterChat::isFocused()
{
	LLFloaterChat* self = (LLFloaterChat*)findInstance();
	return self && self->mFocused;
}

//static
bool LLFloaterChat::isOwnNameInText(const std::string& text_line)
{
	if (!isAgentAvatarValid())
	{
		return false;
	}
	const std::string separators(" .,;:'!?*-()[]\"");
	bool flag;
	char before, after;
	size_t index = 0, larger_index, length = 0;
	std::set<std::string>::iterator it;
	std::string name;
	std::string text = " " + text_line + " ";
	LLStringUtil::toLower(text);

	if (make_words_list())
	{
		name = "Highlights words list changed to: ";
		flag = false;
		for (it = sHighlightWords.begin(); it != sHighlightWords.end(); ++it)
		{
			if (flag)
			{
				name += ", ";
			}
			name += *it;
			flag = true;
		}
		llinfos << name << llendl;
	}

	do
	{
		flag = false;
		larger_index = 0;
		for (it = sHighlightWords.begin(); it != sHighlightWords.end(); ++it)
		{
			name = *it;
			index = text.find(name);
			if (index != std::string::npos)
			{
				flag = true;
				before = text[index - 1];
				after = text[index + name.length()];
				if (separators.find(before) != std::string::npos &&
					separators.find(after) != std::string::npos)
				{
					return true;
				}
				if (index >= larger_index)
				{
					larger_index = index;
					length = name.length();
				}
			}
		}
		if (flag)
		{
			text = " " + text.substr(index + length);
		}
	}
	while (flag);

	return false;
}
