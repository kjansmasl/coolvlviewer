/**
 * @file llmediaremotectrl.cpp
 * @brief A remote control for media (video and music)
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

#include "llmediaremotectrl.h"

#include "llaudioengine.h"
#include "llbutton.h"
#include "lliconctrl.h"
#include "llmimetypes.h"
#include "llparcel.h"
#include "llstreamingaudio.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llfloaternearbymedia.h"
#include "lloverlaybar.h"
#include "llpanelaudiovolume.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"

static const std::string LL_MEDIA_REMOTE_CTRL_TAG = "media_remote";
static LLRegisterWidget<LLMediaRemoteCtrl> r(LL_MEDIA_REMOTE_CTRL_TAG);

void nearby_media_toggle(void* dummy)
{
	LLFloaterNearByMedia::toggleInstance();
}

LLMediaRemoteCtrl::LLMediaRemoteCtrl(const std::string& name,
									 const LLRect& rect,
									 const std::string& xml_file,
									 const ERemoteType type)
:	LLPanel(name, rect, false),
	mType(type),
	mIcon(NULL),
	mPlay(NULL),
	mPause(NULL),
	mStop(NULL)
{
	setIsChrome(true);
	setFocusRoot(true);

	LLUICtrlFactory::getInstance()->buildPanel(this, xml_file);
}

bool LLMediaRemoteCtrl::postBuild()
{
	if (mType == REMOTE_PARCEL_MEDIA)
	{
		mIcon = getChild<LLIconCtrl>("media_icon");
		mIconToolTip = mIcon->getToolTip();

		mPlay = getChild<LLButton>("media_play");
		mPlay->setClickedCallback(LLViewerParcelMedia::play, this);

		mPause = getChild<LLButton>("media_pause");
		mPause->setClickedCallback(LLViewerParcelMedia::pause, this);

		mStop = getChild<LLButton>("media_stop");
		mStop->setClickedCallback(LLViewerParcelMedia::stop, this);
	}
	else if (mType == REMOTE_SHARED_MEDIA)
	{
		childSetAction("media_list", nearby_media_toggle, this);

		// The "play" button pointer is used for the button enabling all nearby
		// media.
		mPlay = getChild<LLButton>("media_play");
		mPlay->setClickedCallback(LLViewerMedia::sharedMediaEnable, this);

		// The "stop" button pointer is used for the button disabling all
		// nearby media.
		mStop = getChild<LLButton>("media_stop");
		mStop->setClickedCallback(LLViewerMedia::sharedMediaDisable, this);
	}
	else if (mType == REMOTE_PARCEL_MUSIC)
	{
		mIcon = getChild<LLIconCtrl>("music_icon");
		mIconToolTip = mIcon->getToolTip();

		mPlay = getChild<LLButton>("music_play");
		mPlay->setClickedCallback(LLViewerParcelMedia::playMusic, this);

		mPause = getChild<LLButton>("music_pause");
		mPause->setClickedCallback(LLViewerParcelMedia::pauseMusic, this);

		mStop = getChild<LLButton>("music_stop");
		mStop->setClickedCallback(LLViewerParcelMedia::stopMusic, this);
	}
	else if (mType == REMOTE_MASTER_VOLUME)
	{
		childSetAction("volume", LLOverlayBar::toggleAudioVolumeFloater, this);
		//childSetControlName("volume", "ShowAudioVolume");	// Set in XML file
	}

	return true;
}

void LLMediaRemoteCtrl::draw()
{
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();

	static LLCachedControl<LLColor4U> icon_disabled_color(gColors,
														  "IconDisabledColor");
	static LLCachedControl<LLColor4U> icon_enabled_color(gColors,
														 "IconEnabledColor");
	static LLCachedControl<bool> enable_streaming_media(gSavedSettings,
														"EnableStreamingMedia");

	if (mType == REMOTE_PARCEL_MEDIA)
	{
		bool media_play_enabled = false;
		bool media_stop_enabled = false;
		bool media_show_pause = false;

		LLColor4 media_icon_color = LLColor4(icon_disabled_color);
		std::string media_type = LLMIMETypes::getDefaultMimeType();
		std::string media_url;

		if (enable_streaming_media && parcel &&
			LLViewerParcelMedia::hasParcelMedia())
		{
			media_play_enabled = true;
			media_icon_color = LLColor4(icon_enabled_color);
			media_type = parcel->getMediaType();
			media_url = parcel->getMediaURL();

			LLViewerMediaImpl::EMediaStatus status = LLViewerParcelMedia::getStatus();
			switch (status)
			{
				case LLViewerMediaImpl::MEDIA_NONE:
					media_show_pause = false;
					media_stop_enabled = false;
					break;
				case LLViewerMediaImpl::MEDIA_LOADING:
				case LLViewerMediaImpl::MEDIA_LOADED:
				case LLViewerMediaImpl::MEDIA_PLAYING:
					// HACK: only show the pause button for movie types
					media_show_pause = LLMIMETypes::widgetType(parcel->getMediaType()) == "movie";
					media_stop_enabled = true;
					media_play_enabled = false;
					break;
				case LLViewerMediaImpl::MEDIA_PAUSED:
					media_show_pause = false;
					media_stop_enabled = true;
					break;
				default:
					// inherit defaults above
					break;
			}
		}

		mPlay->setEnabled(media_play_enabled);
		mPlay->setVisible(!media_show_pause);
		mStop->setEnabled(media_stop_enabled);
		mPause->setEnabled(media_show_pause);
		mPause->setVisible(media_show_pause);

		const std::string media_icon_name = LLMIMETypes::findIcon(media_type);
		if (mIcon)
		{
			if (!media_icon_name.empty())
			{
				mIcon->setImage(media_icon_name);
			}
			mIcon->setColor(media_icon_color);
			if (media_url.empty())
			{
				media_url = mIconToolTip;
			}
			else
			{
				media_url = mIconToolTip + " (" + media_url + ")";
			}
			mIcon->setToolTip(media_url);
		}
	}
	else if (mType == REMOTE_SHARED_MEDIA)
	{
		static LLCachedControl<bool> enable_shared_media(gSavedSettings,
														 "PrimMediaMasterEnabled");
		bool show = enable_streaming_media && enable_shared_media;
		mPlay->setEnabled(show && LLViewerMedia::isAnyMediaDisabled());
		mStop->setEnabled(show && LLViewerMedia::isAnyMediaEnabled());
	}
	else if (mType == REMOTE_PARCEL_MUSIC)
	{
		bool music_play_enabled = false;
		bool music_stop_enabled = false;
		bool music_show_pause = false;

		static LLCachedControl<bool> audio_streaming_music(gSavedSettings,
														   "EnableStreamingMusic");

		static std::string artist_str = "\n" + getString("artist_string") +
										": ";
		static std::string title_str = "\n" + getString("title_string") + ": ";

		LLColor4 music_icon_color = LLColor4(icon_disabled_color);
		std::string music_url;

		if (gAudiop && audio_streaming_music && parcel &&
			LLViewerParcelMedia::hasParcelAudio())
		{
			music_icon_color = LLColor4(icon_enabled_color);
			music_url = parcel->getMusicURL();
			music_play_enabled = true;
			music_show_pause = LLViewerParcelMedia::parcelMusicPlaying();
			music_stop_enabled = !LLViewerParcelMedia::parcelMusicStopped();
		}

		mPlay->setEnabled(music_play_enabled);
		mPlay->setVisible(!music_show_pause);
		mStop->setEnabled(music_stop_enabled);
		mPause->setEnabled(music_show_pause);
		mPause->setVisible(music_show_pause);

		if (mIcon)
		{
			mIcon->setColor(music_icon_color);

			std::string tool_tip = mIconToolTip;
			if (!music_url.empty())
			{
				tool_tip += " (" + music_url + ")";
				if (mCachedURL != music_url)
				{
					mCachedURL = music_url;
					mCachedMetaData.clear();
				}
			}
			if (music_show_pause && gAudiop)
			{
				LLStreamingAudioInterface* stream = gAudiop->getStreamingAudioImpl();
				if (stream)
				{
					if (stream->newMetaData())
					{
						mCachedMetaData.clear();
						std::string temp = stream->getArtist();
						if (!temp.empty())
						{
							mCachedMetaData += artist_str + temp;
						}
						temp = stream->getTitle();
						if (!temp.empty())
						{
							mCachedMetaData += title_str + temp;
						}
						stream->gotMetaData();
						static LLCachedControl<bool> notify_stream_changes(gSavedSettings,
																		   "NotifyStreamChanges");
						if (notify_stream_changes && !mCachedMetaData.empty())
						{
							LLSD args;
							args["STREAM_DATA"] = mCachedMetaData;
							gNotifications.add("StreamChanged", args);
						}
					}
				}
				else
				{
					mCachedMetaData.clear();
				}
				tool_tip += mCachedMetaData;
			}
			mIcon->setToolTip(tool_tip);
		}
	}

	LLPanel::draw();
}
