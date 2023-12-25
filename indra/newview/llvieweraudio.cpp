/**
 * @file llvieweraudio.cpp
 * @brief Audio functions that used to be in viewer.cpp
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

#include "llviewerprecompiledheaders.h"

#include <stack>

#include "llvieweraudio.h"

#include "llaudioengine.h"
#include "lldir.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llstartup.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerwindow.h"
#include "llvoiceclient.h"

static void get_ui_sounds_list(std::stack<std::string>& ui_sounds)
{
	ui_sounds.emplace("UISndAlert");
	ui_sounds.emplace("UISndBadKeystroke");
	ui_sounds.emplace("UISndClick");
	ui_sounds.emplace("UISndClickRelease");
	ui_sounds.emplace("UISndHealthReductionF");
	ui_sounds.emplace("UISndHealthReductionM");
	ui_sounds.emplace("UISndInvalidOp");
	ui_sounds.emplace("UISndMoneyChangeDown");
	ui_sounds.emplace("UISndMoneyChangeUp");
	ui_sounds.emplace("UISndNewIncomingIMSession");
	ui_sounds.emplace("UISndObjectCreate");
	ui_sounds.emplace("UISndObjectDelete");
	ui_sounds.emplace("UISndObjectRezIn");
	ui_sounds.emplace("UISndObjectRezOut");
	ui_sounds.emplace("UISndPieMenuAppear");
	ui_sounds.emplace("UISndPieMenuHide");
	// UISndPieMenuSliceHighlight = d9f73cf8-17b4-6f7a-1565-7951226c305d
	// Also exists (same sound, different UUID) as:
	// f6ba9816-dcaf-f755-7b67-51b31b6233e5
	// 7aff2265-d05b-8b72-63c7-dbf96dc2f21f
	// 09b2184e-8601-44e2-afbb-ce37434b8ba1
	// bbe4c7fc-7044-b05e-7b89-36924a67593c
	// d166039b-b4f5-c2ec-4911-c85c727b016c
	// 242af82b-43c2-9a3b-e108-3b0c7e384981
	// c1f334fb-a5be-8fe7-22b3-29631c21cf0b
	ui_sounds.emplace("UISndPieMenuSliceHighlight");
	ui_sounds.emplace("UISndSnapshot");
	ui_sounds.emplace("UISndStartIM");
	ui_sounds.emplace("UISndTeleportOut");
	ui_sounds.emplace("UISndTyping");
	ui_sounds.emplace("UISndWindowClose");
	ui_sounds.emplace("UISndWindowOpen");
}

std::string get_valid_sounds()
{
	std::stack<std::string> ui_sounds;
	get_ui_sounds_list(ui_sounds);

	std::string name, sound_list;
	while (!ui_sounds.empty())
 	{
		sound_list += ";" + ui_sounds.top();
		ui_sounds.pop();
	}

	return sound_list + ";";
}

void audio_preload_ui_sounds(bool force_decode)
{
	if (!gAudiop)
	{
		llwarns << "Audio Engine not initialized. Could not preload the UI sounds."
				<< llendl;
		return;
	}

	std::stack<std::string> ui_sounds;
	get_ui_sounds_list(ui_sounds);

	F32 audio_level = gSavedSettings.getF32("AudioLevelUI") *
					  gSavedSettings.getF32("AudioLevelMaster");
	if (!force_decode || audio_level == 0.f ||
		gSavedSettings.getBool("MuteAudio") ||
		gSavedSettings.getBool("MuteUI"))
	{
		audio_level = 0.f;
		if (force_decode)
		{
			llwarns << "UI muted: cannot force-decode UI sounds." << llendl;
		}
	}
	else
	{
		// Normalize to 25% combined volume, or the highest possible volume
		// if 25% can't be reached.
		audio_level = 0.25f / audio_level;
		if (audio_level > 1.f)
		{
			audio_level = 1.f;
		}
	}

	LLUUID uuid;
	std::string sound_file;
	while (!ui_sounds.empty())
 	{
		uuid.set(gSavedSettings.getString(ui_sounds.top().c_str()));
		ui_sounds.pop();
		if (uuid.isNull()) continue;

		if (!LLAudioEngine::getUISoundFile(uuid, sound_file))
		{
			// This sound is not part of the pre-decoded UI sounds and must be
			// fetched. Make sure they are at least pre-fetched.
			gAudiop->preloadSound(uuid);
			if (audio_level > 0.f)
			{
				// Try to force-decode them (will depend on actual audio level)
				// by playing them.
				gAudiop->triggerSound(uuid, gAgentID, audio_level,
									  LLAudioEngine::AUDIO_TYPE_UI);
			}
		}
	}
}

void copy_pre_decoded_ui_sounds(void*)
{
	if (!gDirUtilp) return;

	std::string ui_sounds_dir =
		gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "ui_sounds");
	LLFile::mkdir(ui_sounds_dir);
	ui_sounds_dir += LL_DIR_DELIM_STR;

	std::stack<std::string> ui_sounds;
	get_ui_sounds_list(ui_sounds);
	LLUUID uuid;
	std::string filename, sound_file;
	bool missing = false;
	while (!ui_sounds.empty())
 	{
		bool copy = false;
		uuid.set(gSavedSettings.getString(ui_sounds.top().c_str()));
		ui_sounds.pop();
		if (uuid.isNull()) continue;

		filename = uuid.asString() + ".dsf";

#if LL_SEARCH_UI_SOUNDS_IN_SKINS
		bool in_user_settings = false;
		if (LLAudioEngine::getUISoundFile(uuid, sound_file,
										  &in_user_settings))
		{
			// This pre-decoded sound file exists: let's see where:
			if (in_user_settings)
			{
				llinfos << "Decoded sound file '" << filename
						<< "' already present in '" << ui_sounds_dir << "'"
						<< llendl;
			}
			else
			{
				copy = true;
			}
		}
		else
		{
			// Search among cached sound files
			sound_file = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														filename);
			copy = LLFile::exists(sound_file);
			if (!copy)
			{
				llwarns << "UI sound file '" << filename << "' not found."
						<< llendl;
				missing = true;
			}
		}
#else
		if (LLAudioEngine::getUISoundFile(uuid, sound_file))
		{
			llinfos << "Decoded sound file '" << filename
					<< "' already present in '" << ui_sounds_dir << "'"
					<< llendl;
			continue;
		}

		// Then search in the viewer installation LL_PATH_SKINS/default/sounds/
		// sub-directory (old location, no more used).
		sound_file = gDirUtilp->getExpandedFilename(LL_PATH_SKINS, "default",
													"sounds", filename);
		if (LLFile::exists(sound_file))
		{
			copy = true;
		}
		else
		{
			// Finally, search among cached sound files
			sound_file = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														filename);
			copy = LLFile::exists(sound_file);
			if (!copy)
			{
				llwarns << "UI sound file '" << filename << "' not found."
						<< llendl;
				missing = true;
			}
		}
#endif
		if (copy)
		{
			llinfos << "Copying decoded sound file '" << filename << "' into '"
					<< ui_sounds_dir << "'" << llendl;
			LLFile::copy(sound_file, ui_sounds_dir + filename);
		}
	}
	if (missing)
	{
		gNotifications.add("SomeUISoundsMissing");
	}
	else
	{
		gNotifications.add("AllUISoundsSaved");
	}
}

void clear_pre_decoded_ui_sounds()
{
	if (!gDirUtilp) return;
	
	std::stack<std::string> ui_sounds;
	get_ui_sounds_list(ui_sounds);

	LLUUID uuid;
	std::string sound_file;
	while (!ui_sounds.empty())
 	{
		uuid.set(gSavedSettings.getString(ui_sounds.top().c_str()));
		ui_sounds.pop();
		if (uuid.isNull()) continue;

		// Search in the user's account LL_PATH_USER_SETTINGS/ui_sounds/
		// directory.
		sound_file = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
													"ui_sounds",
													uuid.asString()) + ".dsf";
		if (LLFile::exists(sound_file))
		{
			llinfos << "Removing pre-decoded UI sound file: " << sound_file
					<< llendl;
			LLFile::remove(sound_file);
		}
	}
}

void init_audio()
{
	// Clear the saved pre-decoded UI sounds from user settings if it was
	// requested in previous session (i.e. before relog); on the condition we
	// are the only running instance of our viewer !
	if (gSavedSettings.getBool("ClearSavedUISounds") &&
		!gAppViewerp->isSecondInstanceSiblingViewer())
	{
		gSavedSettings.setBool("ClearSavedUISounds", false);
		clear_pre_decoded_ui_sounds();
	}

	if (!gAudiop) return;

	setup_audio_listener();

	// Load up our initial set of sounds so they are ready to be played
	if (!gSavedSettings.getBool("NoPreload"))
	{
		audio_preload_ui_sounds();
	}

	audio_update_volume(true);
}

void setup_audio_listener()
{
	LLVector3d lpos_global = gAgent.getCameraPositionGlobal();
	LLVector3 lpos_global_f;
	lpos_global_f.set(lpos_global);
	gAudiop->setListener(lpos_global_f,
#if 0					 // *FIXME: need to replace this with smoothed velocity
						 gViewerCamera.getVelocity(),
#else
						 LLVector3::zero,
#endif
						 gViewerCamera.getUpAxis(),
						 gViewerCamera.getAtAxis());
}

// A callback set in LLAppViewer::init()
void ui_audio_callback(const LLUUID& uuid)
{
	if (gAudiop)
	{
		if (!LLStartUp::isLoggedIn())
		{
			// If we are not yet connected, we can only play pre-decoded UI
			// sounds, if any. Else we get a sound loading failure and the
			// viewer will never retry and load that sound for the rest of the
			// session !
			std::string sound_file;
			if (!LLAudioEngine::getUISoundFile(uuid, sound_file))
			{
				return;
			}
		}

		gAudiop->triggerSound(uuid, gAgentID, 1.f,
							  LLAudioEngine::AUDIO_TYPE_UI);
	}
}

void audio_update_volume(bool force_update)
{
	static LLCachedControl<bool> sMuteAudio(gSavedSettings, "MuteAudio");
	static LLCachedControl<bool> sMuteAmbient(gSavedSettings, "MuteAmbient");
	static LLCachedControl<bool> sMuteSounds(gSavedSettings, "MuteSounds");
	static LLCachedControl<bool> sMuteUI(gSavedSettings, "MuteUI");
	static LLCachedControl<bool> sMuteMusic(gSavedSettings, "MuteMusic");
	static LLCachedControl<bool> sMuteMedia(gSavedSettings, "MuteMedia");
	static LLCachedControl<bool> sMuteVoice(gSavedSettings, "MuteVoice");
	static LLCachedControl<bool> sMuteWhenMinimized(gSavedSettings, "MuteWhenMinimized");
	static LLCachedControl<bool> sDisableWindAudio(gSavedSettings, "DisableWindAudio");
	static LLCachedControl<F32> sAudioLevelMaster(gSavedSettings, "AudioLevelMaster");
	static LLCachedControl<F32> sAudioLevelAmbient(gSavedSettings, "AudioLevelAmbient");
	static LLCachedControl<F32> sAudioLevelUI(gSavedSettings, "AudioLevelUI");
	static LLCachedControl<F32> sAudioLevelSFX(gSavedSettings, "AudioLevelSFX");
	static LLCachedControl<F32> sAudioLevelMusic(gSavedSettings, "AudioLevelMusic");
	static LLCachedControl<F32> sAudioLevelMedia(gSavedSettings, "AudioLevelMedia");
	static LLCachedControl<F32> sAudioLevelVoice(gSavedSettings, "AudioLevelVoice");
	static LLCachedControl<F32> sAudioLevelMic(gSavedSettings, "AudioLevelMic");
	static LLCachedControl<F32> sAudioLevelDoppler(gSavedSettings, "AudioLevelDoppler");
	static LLCachedControl<F32> sAudioLevelRolloff(gSavedSettings, "AudioLevelRolloff");
	static LLCachedControl<F32> sAudioLevelUnderwaterRolloff(gSavedSettings,
															 "AudioLevelUnderwaterRolloff");

	bool mute = sMuteAudio;
	if (sMuteWhenMinimized && gViewerWindowp && !gViewerWindowp->getActive())
	{
		mute = true;
	}
	F32 mute_volume = mute ? 0.f : 1.f;

	if (gAudiop)
	{
		// Sound Effects
		gAudiop->setMasterGain(sAudioLevelMaster);

		gAudiop->setDopplerFactor(sAudioLevelDoppler);
		if (gViewerCamera.cameraUnderWater())
		{
			gAudiop->setRolloffFactor(sAudioLevelUnderwaterRolloff);
		}
		else
		{
			gAudiop->setRolloffFactor(sAudioLevelRolloff);
		}
		gAudiop->setMuted(mute);

		gAudiop->enableWind(!mute && !sMuteAmbient && !sDisableWindAudio &&
							sAudioLevelMaster * sAudioLevelAmbient > 0.01f);
		if (force_update)
		{
			audio_update_wind(true);
		}

		// Handle secondary gains
		gAudiop->setSecondaryGain(LLAudioEngine::AUDIO_TYPE_SFX,
								  sMuteSounds ? 0.f : sAudioLevelSFX);
		gAudiop->setSecondaryGain(LLAudioEngine::AUDIO_TYPE_UI,
								  sMuteUI ? 0.f : sAudioLevelUI);
		gAudiop->setSecondaryGain(LLAudioEngine::AUDIO_TYPE_AMBIENT,
								  sMuteAmbient ? 0.f : sAudioLevelAmbient);

		// Streaming Music
		F32 music_volume = sAudioLevelMusic;
		music_volume = mute_volume * sAudioLevelMaster * music_volume * music_volume;
		gAudiop->setInternetStreamGain(sMuteMusic ? 0.f : music_volume);
	}

	// Streaming Media
	F32 media_volume = sAudioLevelMedia;
	media_volume = mute_volume * sAudioLevelMaster * media_volume * media_volume;
	LLViewerMedia::setVolume(sMuteMedia ? 0.f : media_volume);

	// Voice
	if (LLVoiceClient::sInitDone)
	{
		F32 voice_volume = sAudioLevelVoice;
		voice_volume = mute_volume * sAudioLevelMaster * voice_volume;
		gVoiceClient.setVoiceVolume(sMuteVoice ? 0.f : voice_volume);
		gVoiceClient.setMicGain(sMuteVoice ? 0.f : sAudioLevelMic);

		if (sMuteWhenMinimized && gViewerWindowp && !gViewerWindowp->getActive())
		{
			gVoiceClient.setMuteMic(true);
		}
		else
		{
			gVoiceClient.setMuteMic(false);
		}
	}
}

void audio_update_listener()
{
	if (!gAudiop)
	{
		return;
	}
	// Update listener position because agent has moved
	LLVector3 pos_global;
	pos_global.set(gAgent.getCameraPositionGlobal());
	// *TODO: replace gAgent.getVelocity() with smoothed velocity
	gAudiop->setListener(pos_global, gAgent.getVelocity(),
						 gViewerCamera.getUpAxis(), gViewerCamera.getAtAxis());
}

void audio_update_wind(bool force_update)
{
	static LLCachedControl<bool> sMuteAudio(gSavedSettings, "MuteAudio");
	static LLCachedControl<bool> sMuteAmbient(gSavedSettings, "MuteAmbient");
	static LLCachedControl<F32> sAudioLevelMaster(gSavedSettings, "AudioLevelMaster");
	static LLCachedControl<F32> sAudioLevelAmbient(gSavedSettings, "AudioLevelAmbient");
	static LLCachedControl<F32> sAudioLevelWind(gSavedSettings, "AudioLevelWind");
	static LLCachedControl<F32> sAudioLevelRolloff(gSavedSettings, "AudioLevelRolloff");
	static LLCachedControl<F32> sAudioLevelUnderwaterRolloff(gSavedSettings,
															 "AudioLevelUnderwaterRolloff");

	if (!gAudiop || !gAudiop->isWindEnabled())
	{
		return;
	}

	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
	{
		return;	// Probably disconnected
	}

	// Determine whether we are underwater or not
	const LLVector3& camera_pos = gAgent.getCameraPositionAgent();
	F32 camera_water_height = camera_pos.mV[VZ] - region->getWaterHeight();
	if (camera_water_height < 0.f)
	{
		gRelativeWindVec.clear();	// There is no wind underwater !
		gAudiop->updateWind(gRelativeWindVec, camera_water_height);
		return;
	}

	// This line rotates the wind vector to be listener (agent) relative.
	gRelativeWindVec =
		gAgent.getFrameAgent().rotateToLocal(gWindVec - gAgent.getVelocity());
	// Scale down the contribution of weather-simulation wind to the ambient
	// wind noise. Wind velocity averages 3.5 m/s, with gusts to 7 m/s whereas
	// steady-state avatar walk velocity is only 3.2 m/s. Without this the
	// world feels desolate on first login when you are/ standing still.
	gRelativeWindVec *= llclamp((F32)sAudioLevelWind, 0.f, 1.f);

	// Do not use the setter setMaxWindGain() because we do not want to screw
	// up the fade-in on startup by setting actual source gain outside the
	// fade-in.
	F32 master_volume = sMuteAudio ? 0.f : sAudioLevelMaster;
	F32 ambient_volume = sMuteAmbient ? 0.f : sAudioLevelAmbient;

	F32 wind_volume = master_volume * ambient_volume;
	gAudiop->mMaxWindGain = wind_volume;

	gAudiop->updateWind(gRelativeWindVec, camera_water_height);
}
