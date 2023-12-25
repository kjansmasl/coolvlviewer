/**
 * @file llnotify.cpp
 * @brief Non-blocking notification that doesn't take keyboard focus.
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

#include "llnotify.h"

#include "llalertdialog.h"
#include "llbutton.h"
#include "llgl.h"
#include "lliconctrl.h"
#include "llrender.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"
#include "llxmlnode.h"

#include "llchat.h"
#include "llfloaterchat.h"			// For addChatHistory()
#include "llgroupnotify.h"			// For getGroupNotifyBoxCount()
#include "lloverlaybar.h"			// For gOverlayBarp
#include "llpanellogin.h"			// For LLPanelLogin::isVisible()
//MK
#include "mkrlinterface.h"
//mk
#include "llslurl.h"
#include "llstatusbar.h"			// For gStatusBarp
#include "llviewercontrol.h"
#include "hbviewerautomation.h"
#include "llviewerdisplay.h"
#include "llviewertexturelist.h"

// Globals

// Instance created in LLViewerWindow::initBase()
LLNotifyBoxView* gNotifyBoxViewp = NULL;

constexpr S32 BOTTOM_PAD = VPAD * 3;
constexpr F32 ANIMATION_TIME = 0.333f;

// Static members
bool LLNotifyBox::sShowNotifications = true;
S32 LLNotifyBox::sNotifyBoxCount = 0;
S32 LLNotifyBox::sNotifyTipCount = 0;
std::map<std::string, LLNotifyBox*> LLNotifyBox::sOpenUniqueNotifyBoxes;
LLNotifyBox::name_lookup_map_t LLNotifyBox::sNameLookupMap;
LLUUID LLNotifyBox::sLastNotifyRestartId;

//---------------------------------------------------------------------------
// LLNotifyBox class
//---------------------------------------------------------------------------

//static
void LLNotifyBox::initClass()
{
	LLNotificationChannel::buildChannel("Notifications", "Visible",
										LLNotificationFilters::filterBy<std::string>(&LLNotification::getType,
																					 "notify"));
	LLNotificationChannel::buildChannel("NotificationTips", "Visible",
										LLNotificationFilters::filterBy<std::string>(&LLNotification::getType,
																					 "notifytip"));

	gNotifications.getChannel("Notifications")->connectChanged(&LLNotifyBox::onNotification);
	gNotifications.getChannel("NotificationTips")->connectChanged(&LLNotifyBox::onNotification);
}

//static
bool LLNotifyBox::onNotification(const LLSD& notify)
{
	LLNotificationPtr notifp = gNotifications.find(notify["id"].asUUID());
	if (!notifp) return false;

	std::string sigtype = notify["sigtype"].asString();

	LLNotifyBox* self = getNamedInstance(notifp->getID()).get();
	if (self && !self->isDead())
	{
		if (sigtype == "delete")
		{
			self->close();
		}
		else if (!self->mIsTip && (sigtype == "add" || sigtype == "change"))
		{
			// Bring existing notification to top
			gNotifyBoxViewp->showOnly(self);
		}
		return false;
	}

	if (sigtype == "add" || sigtype == "change")
	{
		std::string dialog_name = notifp->getName();
		bool is_script_dialog = (dialog_name == "ScriptDialog" ||
								 dialog_name == "ScriptDialogOurs");
		bool is_ours = (dialog_name == "ScriptDialogOurs" ||
						dialog_name == "ScriptTextBoxOurs" ||
						dialog_name == "ScriptQuestionOurs" ||
						dialog_name == "LoadWebPageOurs" ||
						dialog_name == "ObjectGiveItemOurs");
		self = new LLNotifyBox(notifp, is_script_dialog, is_ours);
		gNotifyBoxViewp->addChild(self);

		// To avoid piling restart notifications, we close any old one when a
		// new one arrives.
		if (sigtype == "add" &&
			(dialog_name == "RegionRestartMinutes" ||
			 dialog_name == "RegionRestartSeconds"))
		{
			closeLastNotifyRestart();
			sLastNotifyRestartId = notifp->getID();
		}

		// Added this, because these notifications are not logged otherwise. HB
		if (!is_script_dialog)
		{
			LL_DEBUGS("Notifications") << "Got notification: " << dialog_name
									   << LL_ENDL;
		}

		if (gAutomationp)
		{
			if (is_script_dialog)
			{
				std::vector<std::string> buttons;
				for (S32 i = 0, count = self->mBtnCallbackData.size();
					 i < count; ++i)
				{
					CallbackData* userdata = self->mBtnCallbackData[i];
					if (userdata)
					{
						buttons.push_back(userdata->mButtonName);
					}
				}
				gAutomationp->onScriptDialog(notifp->getID(), self->mMessage,
											 buttons);
			}
			else
			{
				gAutomationp->onNotification(dialog_name, notifp->getID(),
											 self->mMessage);
			}
		}
	}

	return false;
}

//static
void LLNotifyBox::setShowNotifications(bool show)
{
	sShowNotifications = show;
	bool is_first = show;
	bool focused = false;
	for (child_list_const_iter_t
			iter = gNotifyBoxViewp->getChildList()->begin(),
			end = gNotifyBoxViewp->getChildList()->end();
		 iter != end; ++iter)
	{
		LLView* view = dynamic_cast<LLView*>(*iter);
		if (view && view->getName() == "groupnotify")
		{
			view->setVisible(show);
			if (show && !focused)
			{
				view->setFocus(true);
				focused = true;
			}
		}
		else
		{
			LLNotifyBox* box = dynamic_cast<LLNotifyBox*>(*iter);
			if (box && !box->isTip())
			{
				box->setVisible(is_first);
				is_first = false;
				if (show && !focused)
				{
					box->setFocus(true);
					focused = true;
				}
			}
		}
	}
}

//static
void LLNotifyBox::substituteSLURL(const LLUUID& id, const std::string& slurl,
								  const std::string& substitute)
{
	if (!sNameLookupMap.count(id)) return;

	std::pair <name_lookup_map_t::iterator, name_lookup_map_t::iterator> range;
	range = sNameLookupMap.equal_range(id);
	for (name_lookup_map_t::iterator it = range.first, end = range.second;
		 it != end; ++it)
	{
		const LLUUID& notif_id = it->second;
		LLNotifyBox* boxp = getNamedInstance(notif_id).get();
		if (boxp && !boxp->isDead() && boxp->mTextEditor)
		{
			boxp->mTextEditor->replaceTextAll(slurl, substitute, true);
			boxp->mTextEditor->setEnabled(false);
		}
	}
}

//static
void LLNotifyBox::substitutionDone(const LLUUID& id)
{
	sNameLookupMap.erase(id);
}

LLNotifyBox::LLNotifyBox(LLNotificationPtr notification, bool script_dialog,
						 bool is_ours)
:	LLPanel(notification->getName(), LLRect(), BORDER_NO),
			LLEventTimer(notification->getExpiration() == LLDate() ?
						 LLDate(LLTimer::getEpochSeconds() +
								(F64)gSavedSettings.getF32("NotifyTipDuration")) :
						 notification->getExpiration()),
	LLInstanceTracker<LLNotifyBox, LLUUID>(notification->getID()),
	mNotification(notification),
	mIsTip(notification->getType() == "notifytip"),
	mAnimating(true),
	mNextBtn(NULL),
	mNumOptions(0),
	mNumButtons(0),
	mAddedDefaultBtn(false),
	mLayoutScriptDialog(script_dialog),
	mIsFromOurObject(is_ours),
	mUserInputBox(NULL),
	mTextEditor(NULL)
{
	// We will start it later if actually needed...
	mNotifyShowingTimer.stop();

	LLFontGL* fontp = LLFontGL::getFontSansSerif();

	mMessage = notification->getMessage();
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		// Hide every occurrence of the Region and Parcel names if the location
		// restriction is active
		mMessage = gRLInterface.getCensoredLocation(mMessage);
	}

	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		mMessage = gRLInterface.getCensoredMessage(mMessage);
	}
//mk

	setFocusRoot(!mIsTip);

	// Caution flag can be set explicitly by specifying it in the notification
	// payload, or it can be set implicitly if the notify xml template
	// specifies that it is a caution.
	//
	// Tip-style notifications handle 'caution' differently: they display the
	// tip in a different color.
	mIsCaution = notification->getPriority() >= NOTIFICATION_PRIORITY_HIGH;

	LLNotificationFormPtr form(notification->getForm());
	bool is_textbox = form->getElement("message").isDefined();
	mNumOptions = form->getNumElements();

	// Only animate first window, and never when showing the login panel for
	// notify tips. Also set rect appropriately.
	LLRect rect;
	if (mIsTip)
	{
		mAnimating = sNotifyTipCount <= 0 && !LLPanelLogin::isVisible();
		rect = getNotifyTipRect(mMessage, fontp);
		setFollows(FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	}
	else
	{
		mAnimating = sNotifyBoxCount <= 0 &&
					 LLGroupNotifyBox::getGroupNotifyBoxCount() <= 0;
		rect = getNotifyRect(is_textbox ? 10 : mNumOptions, script_dialog,
							 mIsCaution);
		setFollows(FOLLOWS_TOP | FOLLOWS_RIGHT);
	}
	setRect(rect);
	setBackgroundVisible(false);
	setBackgroundOpaque(true);

	const S32 top = getRect().getHeight() -
					(mIsTip ? (S32)fontp->getLineHeight() : 32);
	const S32 bottom = (S32)fontp->getLineHeight();
	S32 x = 2 * HPAD;
	S32 y = top;

	std::string icon_name;
	if (mIsTip)
	{
		// Use the tip notification icon
		icon_name = "notify_tip_icon.tga";
	}
	else if (mIsCaution)
	{
		// Use the caution notification icon
		icon_name = "notify_caution_icon.tga";
	}
	else
	{
		// Use the default notification icon
		icon_name = "notify_box_icon.tga";
	}
	LLIconCtrl* icon = new LLIconCtrl("icon", LLRect(x, y, x + 32, top - 32),
									  icon_name);
	icon->setMouseOpaque(false);
	addChild(icon);

	x += 2 * HPAD + 32;

	// Set proper background color depending on whether notify box is a caution
	// or a Lua notification, or any other notification.
	if (mIsCaution)
	{
		mBackgroundColor = gColors.getColor("NotifyCautionBoxColor");
	}
	else if (notification->getName().find("Lua") == 0)
	{
		mBackgroundColor = gColors.getColor("NotifyLuaBoxColor");
	}
	else
	{
		mBackgroundColor = gColors.getColor("NotifyBoxColor");
	}

	// Add a caution textbox at the top of a caution notification
	if (mIsCaution && !mIsTip)
	{
		S32 caution_height = ((S32)fontp->getLineHeight() * 2) + VPAD;
		LLTextBox* caution_box;
		caution_box = new LLTextBox(std::string("caution_box"),
									LLRect(x, y, getRect().getWidth() - 2,
										   caution_height),
									LLStringUtil::null, fontp, false);

		caution_box->setFontStyle(LLFontGL::BOLD);
		caution_box->setColor(gColors.getColor("NotifyCautionWarnColor"));
		caution_box->setBackgroundColor(mBackgroundColor);
		caution_box->setBorderVisible(false);
		caution_box->setWrappedText(notification->getMessage());

		addChild(caution_box);

		// Adjust the vertical position of the next control so that it appears
		// below the caution textbox
		y = y - caution_height;
	}
	else
	{
		const S32 btn_top = BOTTOM_PAD +
							(mNumOptions / 3) * (gBtnHeight + VPAD);

		// Tokenization on \n is handled by LLTextBox

		constexpr S32 MAX_LENGTH = 512 + 20 + DB_FIRST_NAME_BUF_SIZE +
								   DB_LAST_NAME_BUF_SIZE +
								   // For script dialogs: add space for title.
								   DB_INV_ITEM_NAME_BUF_SIZE;

		mTextEditor = new LLTextEditor(std::string("box"),
									   LLRect(x, y, getRect().getWidth() - 2,
											  mIsTip ? bottom : btn_top + 16),
									   MAX_LENGTH, LLStringUtil::null, fontp,
									   false);
		mTextEditor->setWordWrap(true);
		mTextEditor->setMouseOpaque(true);
		mTextEditor->setBorderVisible(false);
		mTextEditor->setHideScrollbarForShortDocs(true);
		mTextEditor->setParseHTML(true);
		mTextEditor->setPreserveSegments(true);
		// The background color of the box is manually rendered under the text
		// box, therefore we want the actual text box to be transparent :
		mTextEditor->setReadOnlyBgColor(LLColor4::transparent);
		mTextEditor->setLinkColor(gColors.getColor("NotifyLinkColor"));
		LLColor4 text_color = gColors.getColor("NotifyTextColor");
		mTextEditor->setReadOnlyFgColor(text_color);
		mTextEditor->appendColoredText(mMessage, false, false, text_color);

		mTextEditor->setEnabled(false); // makes it read-only
		// can't tab to it (may be a problem for scrolling via keyboard)
		mTextEditor->setTabStop(false);
		addChild(mTextEditor);
	}

	if (mIsTip)
	{
		if (++sNotifyTipCount <= 0)
		{
			llwarns << "A notification was mishandled. sNotifyTipCount = "
					<< sNotifyTipCount << ", resetting..." << llendl;
			sNotifyTipCount = 1;
		}
		if (!gSavedSettings.getBool("HideNotificationsInChat"))
		{
			// *TODO: Make a separate archive for these.
			LLChat chat(mMessage);
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			LLFloaterChat::addChatHistory(chat);
			LLFloaterChat::resolveSLURLs(chat);
		}
	}
	else
	{
		if (++sNotifyBoxCount <= 0)
		{
			llwarns << "A notification was mishandled. sNotifyBoxCount = "
					<< sNotifyBoxCount << ", resetting..." << llendl;
			sNotifyBoxCount = 1;
		}
		if (gStatusBarp)
		{
			gStatusBarp->setDirty();
		}

		LLRect rect(getRect().getWidth() - 26, BOTTOM_PAD + 20,
					getRect().getWidth() - 2, BOTTOM_PAD);
		mNextBtn = new LLButton("next", rect, "notify_next.png",
								"notify_next.png", NULL, onClickNext, this,
								fontp);
		mNextBtn->setScaleImage(true);
		// *TODO: Translate
		mNextBtn->setToolTip(std::string("Next notification"));
		addChild(mNextBtn);

		std::string edit_text_name, edit_text_contents;
		LLButton* btn;
		for (S32 i = 0; i < mNumOptions; ++i)
		{
			LLSD form_element = form->getElement(i);
			std::string element_type = form_element["type"].asString();
			if (element_type == "button")
			{
				btn = addButton(form_element["name"].asString(),
								form_element["text"].asString(),
								true,
								form_element["default"].asBoolean());
				if (sNotifyBoxCount > 1)
				{
					// Avoid unwanted clicks when the notify box appears over
					// an existing one while the user was clicking on the
					// latter...
					btn->setEnabled(false);
				}
			}
			else if (element_type == "input")
			{
				edit_text_contents = form_element["value"].asString();
				edit_text_name = form_element["name"].asString();
			}
		}

		if (is_textbox)
		{
			S32 button_rows = script_dialog ? 2 : 1;

			constexpr S32 row_width = 3 * 80 + 4 * HPAD;
			const S32 row_height = gBtnHeight + VPAD;
			rect.setOriginAndSize(x, BOTTOM_PAD + button_rows * row_height,
								  row_width,
								  button_rows * row_height + gBtnHeight);

			mUserInputBox = new LLTextEditor(edit_text_name, rect, 254,
											 edit_text_contents, fontp, false);
			mUserInputBox->setBorderVisible(true);
			mUserInputBox->setHideScrollbarForShortDocs(true);
			mUserInputBox->setWordWrap(true);
			mUserInputBox->setTabsToNextField(false);
			mUserInputBox->setCommitOnFocusLost(false);
			mUserInputBox->setHandleEditKeysDirectly(true);

			addChild(mUserInputBox, -1);
		}
		else
		{
			setIsChrome(true);
		}

		if (mNumButtons == 0)
		{
			btn = addButton("OK", "OK", false, true);
			if (sNotifyBoxCount > 1)
			{
				// Avoid unwanted clicks when the notify box appears over an
				// existing one while the user was clicking on the latter...
				btn->setEnabled(false);
			}
			mAddedDefaultBtn = true;
		}

		if (sNotifyBoxCount > 1)
		{
			mNotifyShowingTimer.start();
		}
	}

	if (!mTextEditor) return;

	// SLURLs resolving: fetch the Ids associated with avatar/group/experience
	// name SLURLs present in the text.
	uuid_list_t agent_ids = LLSLURL::findSLURLs(mMessage);
	if (agent_ids.empty()) return;

	// Keep track of which notification got which UUID
	const LLUUID& notif_id = notification->getID();
	for (uuid_list_t::iterator it = agent_ids.begin(), end = agent_ids.end();
		 it != end; ++it)
	{
		sNameLookupMap.emplace(*it, notif_id);
	}

	// Launch the SLURLs resolution. Note that the substituteSLURL() callback
	// will be invoked immediately for names already in cache. That is why we
	// needed to push the untranslated SLURLs in the text editor (together with
	// the fact that doing so, it gets the SLURLs auto-parsed and puts a link
	// segment on them in the text editor, segment link that will be preserved
	// when the SLURL will be replaced with the corresponding name).
	LLSLURL::resolveSLURLs();
}

// virtual
LLNotifyBox::~LLNotifyBox()
{
	if (mIsTip)
	{
		--sNotifyTipCount;
	}
	else
	{
		--sNotifyBoxCount;
		if (gStatusBarp)
		{
			gStatusBarp->setDirty();
		}
	}
	std::for_each(mBtnCallbackData.begin(), mBtnCallbackData.end(),
				  DeletePointer());
	mBtnCallbackData.clear();
}

// virtual
LLButton* LLNotifyBox::addButton(const std::string& name,
								 const std::string& label,
								 bool is_option, bool is_default)
{
	// Make caution notification buttons slightly narrower so that 3 of them
	// can fit without overlapping the "next" button
	S32 btn_width = mIsCaution ? 84 : 90;

	LLRect btn_rect;
	LLButton* btn;
	S32 btn_height = gBtnHeight;
	S32 ignore_pad = 0;
	S32 button_index = mNumButtons;
	S32 index = button_index;
	S32 x = HPAD * 4 + 32;

	static LLFontGL* default_font = LLFontGL::getFontSansSerif();
	LLFontGL* fontp = default_font;
	if (mLayoutScriptDialog)
	{
		// Add one "blank" option space, before the "Mute" and "Ignore" buttons
		index = button_index + 1;
		if (button_index == 0 || button_index == 1)
		{
			// Ignore and mute buttons are smaller
			btn_height = gBtnHeightSmall;
			ignore_pad = 10;
			static LLFontGL* small_font = LLFontGL::getFontSansSerifSmall();
			fontp = small_font;
		}
	}

	btn_rect.setOriginAndSize(x + (index % 3) * (btn_width + 2 * HPAD) +
							  ignore_pad,
							  BOTTOM_PAD + (index / 3) * (gBtnHeight + VPAD),
							  btn_width - 2 * ignore_pad, btn_height);

	CallbackData* userdata = new CallbackData;
	userdata->mSelf = this;
	userdata->mButtonName = is_option ? name : "";

	mBtnCallbackData.push_back(userdata);

	btn = new LLButton(name, btn_rect, "", onClickButton, userdata);
	btn->setLabel(label);
	btn->setFont(fontp);
	if (mIsFromOurObject && name == "client_side_mute")
	{
		// hide the Mute button for our scripted objects
		btn->setVisible(false);
	}

	if (mIsCaution)
	{
		static LLCachedControl<LLColor4U> color(gColors,
												"ButtonCautionImageColor");
		btn->setImageColor(LLColor4(color));
		btn->setDisabledImageColor(LLColor4(color));
	}

	addChild(btn, -1);

	if (is_default)
	{
		setDefaultBtn(btn);
	}

	++mNumButtons;
	return btn;
}

bool LLNotifyBox::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (mIsTip)
	{
		mNotification->respond(mNotification->getResponseTemplate(LLNotification::WITH_DEFAULT_BUTTON));
		close();
		return true;
	}

	setFocus(true);

	return LLPanel::handleMouseUp(x, y, mask);
}

// virtual
bool LLNotifyBox::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (!mIsTip)
	{
		moveToBack(true);
		return true;
	}

	return LLPanel::handleRightMouseDown(x, y, mask);
}

// virtual
void LLNotifyBox::draw()
{
	// If we are teleporting, stop the timer and restart it when the teleport
	// completes
	if (gTeleportDisplay)
	{
		mEventTimer.stop();
	}
	else if (!mEventTimer.getStarted())
	{
		mEventTimer.start();
	}

	if (!mIsTip && !sShowNotifications)
	{
		setVisible(false);
		return;
	}

	F32 display_time = mAnimateTimer.getElapsedTimeF32();

	if (mNextBtn)
	{
		mNextBtn->setVisible(sNotifyBoxCount > 1);
	}

	if (mAnimating && display_time < ANIMATION_TIME)
	{
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		LLUI::pushMatrix();

		S32 height = getRect().getHeight();
		F32 fraction = display_time / ANIMATION_TIME;
		F32 voffset = (1.f - fraction) * height;
		if (mIsTip)
		{
			voffset *= -1.f;
		}
		LLUI::translate(0.f, voffset, 0.f);

		drawBackground();

		LLPanel::draw();

		LLUI::popMatrix();

		if (mNotifyShowingTimer.getStarted())
		{
			// Do not start counting before we are done animating...
			mNotifyShowingTimer.reset();
		}
	}
	else
	{
		if (mAnimating)
		{
			mAnimating = false;
			if (!mIsTip)
			{
				// Hide everyone behind me once I am done animating
				gNotifyBoxViewp->showOnly(this);
			}
		}

		// If the time has come to enable buttons, then do so...
		static LLCachedControl<F32> enable_delay(gSavedSettings,
												 "NotifyBoxButtonsEnableDelay");
		if (mNotifyShowingTimer.getStarted() &&
			mNotifyShowingTimer.getElapsedTimeF32() >= enable_delay)
		{
			mNotifyShowingTimer.stop();
			LLView* child = getFirstChild();
			while (child)
			{
				// Only enable buttons...
				LLButton* btn = dynamic_cast<LLButton*>(child);
				if (btn)
				{
					btn->setEnabled(true);
				}
				child = findNextSibling(child);
			}
		}

		drawBackground();
		LLPanel::draw();
	}
}

void LLNotifyBox::drawBackground() const
{
	static const S32 tex_width = LLUIImage::sRoundedSquareWidth;
	static const S32 tex_height = LLUIImage::sRoundedSquareHeight;
	gGL.getTexUnit(0)->bind(LLUIImage::sRoundedSquare->getImage());

	LLColor4 bgcolor = LLColor4(mBackgroundColor);

	U32 edges = mIsTip ? ROUNDED_RECT_TOP : ROUNDED_RECT_BOTTOM;

	if (gFocusMgr.childHasKeyboardFocus(this))
	{
		constexpr S32 focus_width = 2;
		LLColor4 color = LLUI::sFloaterFocusBorderColor;
		gGL.color4fv(color.mV);
		gl_segmented_rect_2d_tex(-focus_width,
								 getRect().getHeight() + focus_width,
								 getRect().getWidth() + focus_width,
								 -focus_width,
								 tex_width, tex_height, 16, edges);
		color = LLColor4(LLUI::sColorDropShadow);
		gGL.color4fv(color.mV);
		gl_segmented_rect_2d_tex(0, getRect().getHeight(),
								 getRect().getWidth(), 0,
								 tex_width, tex_height, 16, edges);

		gGL.color4fv(bgcolor.mV);
		gl_segmented_rect_2d_tex(1, getRect().getHeight() - 1,
								 getRect().getWidth() - 1, 1,
								 tex_width, tex_height, 16, edges);
	}
	else
	{
		gGL.color4fv(bgcolor.mV);
		gl_segmented_rect_2d_tex(0, getRect().getHeight(),
								 getRect().getWidth(), 0,
								 tex_width, tex_height, 16, edges);
	}
}

void LLNotifyBox::close()
{
	bool was_tip = mIsTip;

	die();
	if (!was_tip)
	{
		LLNotifyBox* front = gNotifyBoxViewp->getFirstNontipBox();
		if (front)
		{
			gNotifyBoxViewp->showOnly(front);
			// We are assuming that close is only called by user action (for
			// non-tips), so we then give focus to the next close button
			if (front->getDefaultButton())
			{
				front->getDefaultButton()->setFocus(true);
			}
			gFocusMgr.triggerFocusFlash(); // TODO it's ugly to call this here
		}
	}
}

void LLNotifyBox::format(std::string& msg,
						 const LLStringUtil::format_map_t& args)
{
	// *TODO: translate
	LLStringUtil::format_map_t targs = args;
	targs["[SECOND_LIFE]"] = "Second Life";
	targs["[VIEWER_NAME]"] = "the Cool VL Viewer";
	LLStringUtil::format(msg, targs);
}

//virtual
bool LLNotifyBox::tick()
{
	if (mIsTip)
	{
		close();
	}
	return false;
}

void LLNotifyBox::moveToBack(bool getfocus)
{
	// Move this dialog to the back.
	gNotifyBoxViewp->sendChildToBack(this);
	if (!mIsTip && mNextBtn)
	{
		mNextBtn->setVisible(false);

		// And enable the next button on the frontmost one, if there is one
		if (gNotifyBoxViewp->getChildCount() > 0)
		{
			LLNotifyBox* front = gNotifyBoxViewp->getFirstNontipBox();
			if (front)
			{
				gNotifyBoxViewp->showOnly(front);
				if (getfocus)
				{
					// if are called from a user interaction we give focus to
					// the next next button
					if (front->mNextBtn != NULL)
					{
						front->mNextBtn->setFocus(true);
					}
					// *TODO: it is ugly to call this here
					gFocusMgr.triggerFocusFlash();
				}
			}
		}
	}
}

//static
LLRect LLNotifyBox::getNotifyRect(S32 num_options, bool script_dialog,
								  bool is_caution)
{
	// Make caution-style dialog taller to accomodate extra text, as well as
	// causing the accept/decline buttons to be drawn in a different position,
	// to help prevent "quick-click-through" of many permissions prompts.
	static LLCachedControl<S32> caution_height(gSavedSettings,
											   "PermissionsCautionNotifyBoxHeight");
	static LLCachedControl<S32> notify_height(gSavedSettings, "NotifyBoxHeight");
	static LLCachedControl<S32> script_height(gSavedSettings, "ScriptDialogHeight");
	S32 height = script_dialog ? script_height
							   : (is_caution ? caution_height : notify_height);
	if (height < 150)
	{
		height = 150;
	}

	static LLCachedControl<S32> notify_width(gSavedSettings, "NotifyBoxWidth");
	static LLCachedControl<S32> script_width(gSavedSettings, "ScriptDialogWidth");
	S32 width = script_dialog ? script_width : notify_width;
	if (width < 250)
	{
		width = 250;
	}

	const S32 top = gNotifyBoxViewp->getRect().getHeight();
	const S32 right = gNotifyBoxViewp->getRect().getWidth();
	const S32 left = right - width;

	if (num_options < 1)
	{
		num_options = 1;
	}

	// Add one "blank" option space.
	if (script_dialog)
	{
		num_options += 1;
	}

	S32 additional_lines = (num_options - 1) / 3;

	height += additional_lines * (gBtnHeight + VPAD);

	return LLRect(left, top, right, top - height);
}

//static
LLRect LLNotifyBox::getNotifyTipRect(const std::string& utf8message,
									 LLFontGL* fontp)
{
	S32 line_count = 1;
	LLWString message = utf8str_to_wstring(utf8message);
	S32 message_len = message.length();

	static LLCachedControl<S32> notify_width(gSavedSettings, "NotifyBoxWidth");
	S32 width = notify_width;
	if (width < 250)
	{
		width = 250;
	}
	// Make room for the icon area.
	const S32 text_area_width = width - HPAD * 4 - 32;

	const llwchar* wchars = message.c_str();
	const llwchar* start = wchars;
	const llwchar* end;
	S32 total_drawn = 0;
	bool done = false;

	do
	{
		++line_count;

		for (end = start; *end != 0 && *end != '\n'; ++end)
		{
		}

		if (*end == 0)
		{
			end = wchars + message_len;
			done = true;
		}

		S32 remaining = end - start;
		while (remaining)
		{
			S32 drawn = fontp->maxDrawableChars(start, (F32)text_area_width,
												remaining, true);
			if (drawn == 0)
			{
				// Draw at least one character, even if it does not all fit
				// (avoids an infinite loop).
				drawn = 1;
			}

			total_drawn += drawn;
			start += drawn;
			remaining -= drawn;

			if (total_drawn < message_len)
			{
				if (wchars[total_drawn] != '\n')
				{
					// Wrap because line was too long
					++line_count;
				}
			}
			else
			{
				done = true;
			}
		}

		++total_drawn;	// Account for '\n'
		start = ++end;
	}
	while (!done);

	S32 height = llceil((F32)(line_count + 1) * fontp->getLineHeight());
	S32 delta = 0;
	if (gOverlayBarp)	// This should always be true...
	{
		if (LLPanelLogin::isVisible())
		{
			// Display above the login panel lower background strip. The offset
			// needs to be adjusted if you change the layout in panel_login.xml
			delta = 102 - gOverlayBarp->getRect().mTop;
			height += 12;
		}
		else
		{
			height += gOverlayBarp->getRect().getHeight();
		}
	}
	constexpr S32 MIN_NOTIFY_HEIGHT = 72;
	constexpr S32 MAX_NOTIFY_HEIGHT = 600;
	height = llclamp(height + VPAD, MIN_NOTIFY_HEIGHT, MAX_NOTIFY_HEIGHT);

	const S32 right = gNotifyBoxViewp->getRect().getWidth();
	const S32 left = right - width;

	// Make sure it goes slightly offscreen
	return LLRect(left, delta + height - 1, right, delta - 1);
}

//static
void LLNotifyBox::onClickButton(void* data)
{
	CallbackData* self_and_button = (CallbackData*)data;
	if (!self_and_button) return;

	LLNotifyBox* self = self_and_button->mSelf;
	std::string button_name = self_and_button->mButtonName;

	LLSD response = self->mNotification->getResponseTemplate();
	if (!self->mAddedDefaultBtn && !button_name.empty())
	{
		response[button_name] = true;
	}
	if (self->mUserInputBox)
	{
		response[self->mUserInputBox->getName()] = self->mUserInputBox->getValue();
	}
	self->mNotification->respond(response);
}

//static
void LLNotifyBox::onClickNext(void* data)
{
	LLNotifyBox* self = static_cast<LLNotifyBox*>(data);
	if (self)
	{
		self->moveToBack(true);
	}
}

//static
void LLNotifyBox::closeLastNotifyRestart()
{
	if (sLastNotifyRestartId.notNull())
	{
		LLNotificationPtr n = gNotifications.find(sLastNotifyRestartId);
		if (n)
		{
			gNotifications.cancel(n);
		}
		sLastNotifyRestartId.setNull();
	}
}

//---------------------------------------------------------------------------
// LLNotifyBoxView class
//---------------------------------------------------------------------------

LLNotifyBoxView::LLNotifyBoxView(const std::string& name, const LLRect& rect,
								 bool mouse_opaque, U32 follows)
:	LLUICtrl(name, rect, mouse_opaque, NULL, NULL, follows)
{
}

LLNotifyBoxView::~LLNotifyBoxView()
{
	gNotifyBoxViewp = NULL;
}

LLNotifyBox* LLNotifyBoxView::getFirstNontipBox() const
{
	for (child_list_const_iter_t iter = getChildList()->begin(),
								 end = getChildList()->end();
		 iter != end; ++iter)
	{
		if (*iter && !isGroupNotifyBox(*iter))
		{
			LLNotifyBox* box = (LLNotifyBox*)(*iter);
			if (!box->isTip() && !box->isDead())
			{
				return box;
			}
		}
	}
	return NULL;
}

void LLNotifyBoxView::showOnly(LLView* view)
{
	if (view)
	{
		// assumes that the argument is actually a child
		LLNotifyBox* shown = dynamic_cast<LLNotifyBox*>(view);
		if (!shown)
		{
			return;
		}

		// make every other notification invisible
		for (child_list_const_iter_t iter = getChildList()->begin(),
									 end = getChildList()->end();
			 iter != end; ++iter)
		{
			if (*iter && !isGroupNotifyBox(*iter))
			{
				LLNotifyBox* box = (LLNotifyBox*)(*iter);
				if (box != view && box->getVisible() && !box->isTip())
				{
					box->setVisible(false);
				}
			}
		}
		shown->setVisible(true);
		sendChildToFront(shown);
	}
}

void LLNotifyBoxView::purgeMessagesMatching(const Matcher& matcher)
{
	// Make a *copy* of the child list to iterate over since we will be
	// removing items from the real list as we go.
	LLView::child_list_t notification_queue(*getChildList());
	for (LLView::child_list_iter_t iter = notification_queue.begin(),
								   end = notification_queue.end();
		 iter != end; ++iter)
	{
		if (*iter && !isGroupNotifyBox(*iter))
		{
			LLNotifyBox* notification = (LLNotifyBox*)*iter;
			if (matcher.matches(notification->getNotification()))
			{
				removeChild(notification);
				delete notification;
			}
		}
	}
}

bool LLNotifyBoxView::isGroupNotifyBox(const LLView* view) const
{
	return view && view->getName() == "groupnotify";
}
