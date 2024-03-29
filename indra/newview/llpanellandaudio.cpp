/**
 * @file llpanellandaudio.cpp
 * @brief Allows configuration of "media" for a land parcel,
 *   for example movies, web pages, and audio.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llpanellandaudio.h"

#include "llcheckboxctrl.h"
#include "lllineeditor.h"
#include "llmimetypes.h"
#include "llparcel.h"
#include "llsdutil.h"
#include "llradiogroup.h"

#include "llfloaterurlentry.h"
#include "lltexturectrl.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "roles_constants.h"

// Values for the parcel voice settings radio group
enum
{
	kRadioVoiceChatEstate = 0,
	kRadioVoiceChatPrivate = 1,
	kRadioVoiceChatDisable = 2
};

//---------------------------------------------------------------------------
// LLPanelLandAudio
//---------------------------------------------------------------------------

LLPanelLandAudio::LLPanelLandAudio(LLParcelSelectionHandle& parcel)
:	LLPanel(),
	mParcel(parcel),
	mCheckSoundLocal(NULL),
	mRadioVoiceChat(NULL),
	mMusicURLEdit(NULL)
{
}

//virtual
bool LLPanelLandAudio::postBuild()
{
	mCheckSoundLocal = getChild<LLCheckBoxCtrl>("check_sound_local");
	mCheckSoundLocal->setCommitCallback(onCommitAny);
	mCheckSoundLocal->setCallbackUserData(this);

	mRadioVoiceChat = getChild<LLRadioGroup>("parcel_voice_channel");
	mRadioVoiceChat->setCommitCallback(onCommitAny);
	mRadioVoiceChat->setCallbackUserData(this);

	mMusicURLEdit = getChild<LLLineEditor>("music_url");
	mMusicURLEdit->setCommitCallback(onCommitAny);
	mMusicURLEdit->setCallbackUserData(this);

	mCheckAVSoundAny = getChild<LLCheckBoxCtrl>("all av sound check");
	mCheckAVSoundAny->setCommitCallback(onCommitAny);
	mCheckAVSoundAny->setCallbackUserData(this);

	mCheckAVSoundGroup = getChild<LLCheckBoxCtrl>("group av sound check");
	mCheckAVSoundGroup->setCommitCallback(onCommitAny);
	mCheckAVSoundGroup->setCallbackUserData(this);

	return true;
}

//virtual
void LLPanelLandAudio::refresh()
{
	LLParcel* parcel = mParcel->getParcel();
	if (!parcel)
	{
		clearCtrls();
	}
	else	// Something selected, hooray !
	{
		// Display options
		bool can_change_media =
			LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
														 GP_LAND_CHANGE_MEDIA);

		mMusicURLEdit->setText(parcel->getMusicURL());
		mMusicURLEdit->setEnabled(can_change_media);

		mCheckSoundLocal->set(parcel->getSoundLocal());
		mCheckSoundLocal->setEnabled(can_change_media);

		if (parcel->getParcelFlagAllowVoice())
		{
			if (parcel->getParcelFlagUseEstateVoiceChannel())
			{
				mRadioVoiceChat->setSelectedIndex(kRadioVoiceChatEstate);
			}
			else
			{
				mRadioVoiceChat->setSelectedIndex(kRadioVoiceChatPrivate);
			}
		}
		else
		{
			mRadioVoiceChat->setSelectedIndex(kRadioVoiceChatDisable);
		}

		LLViewerRegion* region = gViewerParcelMgr.getSelectionRegion();
		mRadioVoiceChat->setEnabled(region && region->isVoiceEnabled() &&
									can_change_media);

		bool new_limits = parcel->getHaveNewParcelLimitData();
		bool can_change_av_sounds = new_limits &&
									LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
																				 GP_LAND_OPTIONS);
		mCheckAVSoundAny->set(parcel->getAllowAnyAVSounds() || !new_limits);
		mCheckAVSoundAny->setEnabled(can_change_av_sounds);

		// On if "Everyone" is on
		mCheckAVSoundGroup->set(!new_limits ||
								parcel->getAllowGroupAVSounds() ||
								parcel->getAllowAnyAVSounds());

		// Enabled if "Everyone" is off
		mCheckAVSoundGroup->setEnabled(can_change_av_sounds &&
									   !parcel->getAllowAnyAVSounds());
	}
}

//static
void LLPanelLandAudio::onCommitAny(LLUICtrl*, void* userdata)
{
	LLPanelLandAudio* self = (LLPanelLandAudio*)userdata;
	if (!self)
	{
		return;
	}
	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	bool sound_local = self->mCheckSoundLocal->get();
	S32 voice_setting = self->mRadioVoiceChat->getSelectedIndex();
	std::string music_url = self->mMusicURLEdit->getText();

	bool voice_enabled, voice_estate_chan;
	switch (voice_setting)
	{
		default:
		case kRadioVoiceChatEstate:
			voice_enabled = true;
			voice_estate_chan = true;
			break;

		case kRadioVoiceChatPrivate:
			voice_enabled = true;
			voice_estate_chan = false;
			break;

		case kRadioVoiceChatDisable:
			voice_enabled = false;
			voice_estate_chan = false;
			break;
	}

	bool any_av_sound = self->mCheckAVSoundAny->get();
	// If set to "Everyone" then group is checked as well:
	bool group_av_sound = true;
	if (!any_av_sound)
	{	// If "Everyone" is off, use the value from the checkbox
		group_av_sound = self->mCheckAVSoundGroup->get();
	}

	// Remove leading/trailing whitespace (common when copying/pasting)
	LLStringUtil::trim(music_url);

	// Push data into current parcel
	parcel->setParcelFlag(PF_ALLOW_VOICE_CHAT, voice_enabled);
	parcel->setParcelFlag(PF_USE_ESTATE_VOICE_CHAN, voice_estate_chan);
	parcel->setParcelFlag(PF_SOUND_LOCAL, sound_local);
	parcel->setMusicURL(music_url);
	parcel->setAllowAnyAVSounds(any_av_sound);
	parcel->setAllowGroupAVSounds(group_av_sound);

	// Send current parcel data upstream to server
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);

	// Might have changed properties, so let's redraw!
	self->refresh();
}
