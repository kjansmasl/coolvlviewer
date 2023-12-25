/**
 * @file llvoiceremotectrl.cpp
 * @brief A remote control for voice chat
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llvoiceremotectrl.h"

#include "llbutton.h"
#include "lliconctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloaterchatterbox.h"
#include "llfloateractivespeakers.h"
#include "lloverlaybar.h"
#include "llviewercontrol.h"
#include "llvoicechannel.h"
#include "llvoiceclient.h"

LLVoiceRemoteCtrl::LLVoiceRemoteCtrl (const std::string& name)
:	LLPanel(name)
{
	setIsChrome(true);

#if EXPANDED_VOICE_CTRL
	if (gSavedSettings.getBool("ShowVoiceChannelPopup"))
	{
		LLUICtrlFactory::getInstance()->buildPanel(this,
												   "panel_voice_remote_expanded.xml");
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildPanel(this,
												   "panel_voice_remote.xml");
	}
#else
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_voice_remote.xml");
#endif

	setFocusRoot(true);
}

bool LLVoiceRemoteCtrl::postBuild()
{
	mTalkBtn = getChild<LLButton>("push_to_talk");
	mTalkBtn->setClickedCallback(onBtnTalkClicked);
	mTalkBtn->setHeldDownCallback(onBtnTalkHeld);
	mTalkBtn->setMouseUpCallback(onBtnTalkReleased);

	mTalkLockBtn = getChild<LLButton>("ptt_lock");
	mTalkLockBtn->setClickedCallback(onBtnLock);
	mTalkLockBtn->setCallbackUserData(this);

	mSpeakersBtn = getChild<LLButton>("speakers_btn");
	mSpeakersBtn->setClickedCallback(onClickSpeakers);
	mSpeakersBtn->setCallbackUserData(this);

	mIcon = getChild<LLIconCtrl>("voice_volume");

#if EXPANDED_VOICE_CTRL
	childSetAction("show_channel", onClickPopupBtn, this);
	childSetAction("end_call_btn", onClickEndCall, this);

	LLTextBox* text = getChild<LLTextBox>("channel_label");
	if (text)
	{
		text->setUseEllipses(true);
	}

	childSetAction("voice_channel_bg", onClickVoiceChannel, this);
#endif
	return true;
}

void LLVoiceRemoteCtrl::draw()
{
	bool voice_active = false;
	LLVoiceChannel* channelp = LLVoiceChannel::getCurrentVoiceChannel();
	if (channelp)
	{
		voice_active = channelp->isActive();
	}
	mTalkBtn->setEnabled(voice_active);
	mTalkLockBtn->setEnabled(voice_active);

	static LLCachedControl<bool> ptt_currently_enabled(gSavedSettings,
													   "PTTCurrentlyEnabled");
	// Propagate ptt state to button display,
	if (!mTalkBtn->hasMouseCapture())
	{
		// Not in push to talk mode, or push to talk is active means currently
		// talking
		mTalkBtn->setToggleState(!ptt_currently_enabled ||
								 gVoiceClient.getUserPTTState());
	}
	mSpeakersBtn->setToggleState(LLFloaterActiveSpeakers::instanceVisible(LLSD()));
	mTalkLockBtn->setToggleState(!ptt_currently_enabled);

	static S32 last_icon_number = -1;
	S32 icon_number = 4;
	std::string talk_blip_image;
	if (gVoiceClient.getIsSpeaking(gAgentID))
	{
		F32 voice_power = gVoiceClient.getCurrentPower(gAgentID);

		if (voice_power > OVERDRIVEN_POWER_LEVEL)
		{
			talk_blip_image = "icn_voice_ptt-on-lvl3.tga";
			icon_number = 3;
		}
		else
		{
			F32 power = gVoiceClient.getCurrentPower(gAgentID);
			S32 icon_image_idx =
				llmin(2, llfloor((power / OVERDRIVEN_POWER_LEVEL) * 3.f));
			icon_number = icon_image_idx;
			switch (icon_image_idx)
			{
			case 0:
				talk_blip_image = "icn_voice_ptt-on.tga";
				break;

			case 1:
				talk_blip_image = "icn_voice_ptt-on-lvl1.tga";
				break;

			case 2:
				talk_blip_image = "icn_voice_ptt-on-lvl2.tga";
			}
		}
	}
	else
	{
		talk_blip_image = "icn_voice_ptt-off.tga";
	}

	if (icon_number != last_icon_number)
	{
		last_icon_number = icon_number;
		mIcon->setImage(talk_blip_image);
	}

	LLFloater* voice_floater =
		LLFloaterChatterBox::getInstance()->getCurrentVoiceFloater();
	std::string active_channel_name;
	if (voice_floater)
	{
		active_channel_name = voice_floater->getShortTitle();
	}

#if EXPANDED_VOICE_CTRL
	LLVoiceChannel* current_channel = LLVoiceChannel::getCurrentVoiceChannel();
	childSetEnabled("end_call_btn",
					LLVoiceClient::voiceEnabled() && current_channel &&
					current_channel->isActive() &&
					current_channel != LLVoiceChannelProximal::getInstance());

	childSetValue("channel_label", active_channel_name);
	childSetToolTip("voice_channel_bg", active_channel_name);

	if (current_channel)
	{
		LLIconCtrl* voice_channel_icon = getChild<LLIconCtrl>("voice_channel_icon");
		if (voice_channel_icon && voice_floater)
		{
			voice_channel_icon->setImage(voice_floater->getString("voice_icon"));
		}

		LLButton* voice_channel_bg = getChild<LLButton>("voice_channel_bg");
		if (voice_channel_bg)
		{
			LLColor4 bg_color;
			if (current_channel->isActive())
			{
				bg_color = lerp(LLColor4::green, LLColor4::white, 0.7f);
			}
			else if (current_channel->getState() == LLVoiceChannel::STATE_ERROR)
			{
				bg_color = lerp(LLColor4::red, LLColor4::white, 0.7f);
			}
			else // active, but not connected
			{
				bg_color = lerp(LLColor4::yellow, LLColor4::white, 0.7f);
			}
			voice_channel_bg->setImageColor(bg_color);
		}
	}

	LLButton* expand_button = getChild<LLButton>("show_channel");
	if (expand_button)
	{
		if (expand_button->getToggleState())
		{
			expand_button->setImageOverlay(std::string("arrow_down.tga"));
		}
		else
		{
			expand_button->setImageOverlay(std::string("arrow_up.tga"));
		}
	}
#endif

	LLPanel::draw();
}

//static
void LLVoiceRemoteCtrl::onBtnTalkClicked(void *user_data)
{
	// when in toggle mode, clicking talk button turns mic on/off
	if (gSavedSettings.getBool("PushToTalkToggle"))
	{
		gVoiceClient.toggleUserPTTState();
	}
}

//static
void LLVoiceRemoteCtrl::onBtnTalkHeld(void *user_data)
{
	// when not in toggle mode, holding down talk button turns on mic
	if (!gSavedSettings.getBool("PushToTalkToggle"))
	{
		gVoiceClient.setUserPTTState(true);
	}
}

//static
void LLVoiceRemoteCtrl::onBtnTalkReleased(void* user_data)
{
	// when not in toggle mode, releasing talk button turns off mic
	if (!gSavedSettings.getBool("PushToTalkToggle"))
	{
		gVoiceClient.setUserPTTState(false);
	}
}

//static
void LLVoiceRemoteCtrl::onBtnLock(void* user_data)
{
	LLVoiceRemoteCtrl* self = (LLVoiceRemoteCtrl*)user_data;
	if (self)
	{
		gSavedSettings.setBool("PTTCurrentlyEnabled",
							   !self->mTalkLockBtn->getToggleState());
	}
}

//static
void LLVoiceRemoteCtrl::onClickSpeakers(void *user_data)
{
	LLFloaterActiveSpeakers::toggleInstance(LLSD());
}

#if EXPANDED_VOICE_CTRL
//static
void LLVoiceRemoteCtrl::onClickPopupBtn(void* user_data)
{
	LLVoiceRemoteCtrl* self = (LLVoiceRemoteCtrl*)user_data;

	remotep->deleteAllChildren();
	if (gSavedSettings.getBool("ShowVoiceChannelPopup"))
	{
		LLUICtrlFactory::getInstance()->buildPanel(self,
												   "panel_voice_remote_expanded.xml");
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildPanel(self,
												   "panel_voice_remote.xml");
	}
	if (gOverlayBarp)
	{
		gOverlayBarp->setDirty();
	}
}

//static
void LLVoiceRemoteCtrl::onClickEndCall(void* user_data)
{
	LLVoiceChannel* current_channel;
	current_channel = LLVoiceChannel::getCurrentVoiceChannel();

	if (current_channel &&
		current_channel != LLVoiceChannelProximal::getInstance())
	{
		current_channel->deactivate();
	}
}

//static
void LLVoiceRemoteCtrl::onClickVoiceChannel(void* user_data)
{
	LLFloaterChatterBox::showInstance();
}
#endif
