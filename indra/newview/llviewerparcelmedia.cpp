/**
 * @file llviewerparcelmedia.cpp
 * @brief Handlers for multimedia on a per-parcel basis
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

#include "llviewerparcelmedia.h"

#include "llcorehttputil.h"
#include "lleventtimer.h"
#include "llmimetypes.h"
#include "llnotifications.h"
#include "llparcel.h"
#include "llpluginclassmedia.h"
#include "llstreamingaudio.h"
#include "llmessage.h"

#include "llagent.h"
#include "llfirstuse.h"
#include "llviewercontrol.h"
#include "llviewermediafocus.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"

///////////////////////////////////////////////////////////////////////////////
// LLStreamingAudio_MediaPlugins class
///////////////////////////////////////////////////////////////////////////////

class LLStreamingAudio_MediaPlugins : public LLStreamingAudioInterface
{
protected:
	LOG_CLASS(LLStreamingAudio_MediaPlugins);

public:
	LLStreamingAudio_MediaPlugins()
	:	mMediaPlugin(NULL),
		mGain(1.f)
	{
	}

	~LLStreamingAudio_MediaPlugins() override
	{
		delete mMediaPlugin;
		mMediaPlugin = NULL;
	}

	void start(const std::string& url) override;
	void stop() override;
	void pause(int pause) override;
	void update() override;
	S32 isPlaying() override;

	void setGain(F32 vol) override;
	LL_INLINE F32 getGain() override			{ return mGain; }

	LL_INLINE std::string getURL() override		{ return mURL; }

private:
	LLPluginClassMedia* initializeMedia(const std::string& media_type);

private:
	LLPluginClassMedia*	mMediaPlugin;
	std::string			mURL;
	F32					mGain;
};

void LLStreamingAudio_MediaPlugins::start(const std::string& url)
{
	if (!mMediaPlugin) // lazy-init the underlying media plugin
	{
		// Assumes that whatever media implementation supports mp3 also
		// supports vorbis.
		mMediaPlugin = initializeMedia("audio/mpeg");
		if (!mMediaPlugin)
		{
			llwarns << "Cannot start a media plugin for audio/mpeg" << llendl;
			return;
		}
		llinfos << "Media plugin for '" << url << "' is now: "
				<< mMediaPlugin->getPluginFileName() << llendl;
	}

	if (url.empty())
	{
		llinfos << "URL is empty, setting stream to NULL" << llendl;
		mURL.clear();
		mMediaPlugin->stop();
	}
	else
	{
		llinfos << "Starting internet stream: " << url << llendl;
		mURL = url;
		mMediaPlugin->loadURI(url);
		mMediaPlugin->start();
	}
}

void LLStreamingAudio_MediaPlugins::stop()
{
	if (mMediaPlugin)
	{
		llinfos << "Stopping internet stream: " << mURL << llendl;
		mMediaPlugin->stop();
	}

	mURL.clear();
}

void LLStreamingAudio_MediaPlugins::pause(int pause)
{
	if (mMediaPlugin)
	{
		if (pause)
		{
			mMediaPlugin->pause();
		}
		else
		{
			mMediaPlugin->start();
		}
	}
}

void LLStreamingAudio_MediaPlugins::update()
{
	if (mMediaPlugin)
	{
		mMediaPlugin->idle();
	}
}

S32 LLStreamingAudio_MediaPlugins::isPlaying()
{
	if (!mMediaPlugin)
	{
		return 0;
	}

	// *TODO: can probably do better than this
	if (mMediaPlugin->isPluginRunning())
	{
		return 1; // Active and playing
	}

	if (mMediaPlugin->isPluginExited())
	{
		return 0; // stopped
	}

	return 2; // paused
}

void LLStreamingAudio_MediaPlugins::setGain(F32 vol)
{
	vol = llclamp(vol, 0.f, 1.f);
	mGain = vol;

	if (mMediaPlugin)
	{
		mMediaPlugin->setVolume(vol);
	}
}

LLPluginClassMedia* LLStreamingAudio_MediaPlugins::initializeMedia(const std::string& media_type)
{
	LLPluginClassMediaOwner* owner = NULL;
	constexpr S32 default_size = 1;	// audio-only, be minimal, does not matter
	LLPluginClassMedia* media_source;
	media_source = LLViewerMediaImpl::newSourceFromMediaType(media_type, owner,
															 default_size,
															 default_size);
	if (media_source)
	{
		media_source->setLoop(false); // audio streams are not expected to loop
	}

	return media_source;
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerParcelMediaAutoPlay class
///////////////////////////////////////////////////////////////////////////////

// timer to automatically play media
class LLViewerParcelMediaAutoPlay : LLEventTimer
{
public:
	LLViewerParcelMediaAutoPlay()
	:	LLEventTimer(1),
		mPlayed(false),
		mTimeInParcel(0)
	{
	}

	bool tick() override;

	static void initClass();
	static void cleanupClass();
	static void playStarted();

private:
	S32 mLastParcelID;
	bool mPlayed;
	F32 mTimeInParcel;
};

constexpr F32 AUTOPLAY_TIME  = 5;		// How many seconds before we autoplay
// How big the texture must be (pixel area) before we autoplay
constexpr F32 AUTOPLAY_SIZE  = 24 * 24;
// How slow should the agent be moving to autoplay
constexpr F32 AUTOPLAY_SPEED = 0.1f;

static LLViewerParcelMediaAutoPlay* sAutoPlay = NULL;

//static
void LLViewerParcelMediaAutoPlay::initClass()
{
	if (!sAutoPlay)
	{
		sAutoPlay = new LLViewerParcelMediaAutoPlay;
	}
}

//static
void LLViewerParcelMediaAutoPlay::cleanupClass()
{
	if (sAutoPlay)
	{
		delete sAutoPlay;
		sAutoPlay = NULL;
	}
}

//static
void LLViewerParcelMediaAutoPlay::playStarted()
{
	if (sAutoPlay)
	{
		sAutoPlay->mPlayed = true;
	}
}

bool LLViewerParcelMediaAutoPlay::tick()
{
	static LLCachedControl<bool> auto_play(gSavedSettings,
										   "ParcelMediaAutoPlayEnable");
	if (!auto_play)
	{
		// Don't bother !
		mPlayed = false;
		return false;
	}

	LLParcel* this_parcel = gViewerParcelMgr.getAgentParcel();
	if (!this_parcel)
	{
		mPlayed = false;
		return false;
	}

	if (this_parcel->getMediaURL().empty())
	{
		// No media in this parcel
		mPlayed = false;
		return false;
	}

	const LLUUID& this_media_texture_id = this_parcel->getMediaID();
	if (this_media_texture_id.isNull())
	{
		// Bad media texture
		mPlayed = false;
		return false;
	}

	if (LLViewerParcelMedia::sMediaImpl.notNull())
	{
		// Media is already playing !
		mPlayed = true;
		return false;
	}

	S32 this_parcel_id = this_parcel->getLocalID();
	if (this_parcel_id != mLastParcelID)
	{
		// we've entered a new parcel
		mPlayed = false;
		mTimeInParcel = 0;
		mLastParcelID = this_parcel_id;
	}

	// Increase mTimeInParcel by the amount of time between ticks
	mTimeInParcel += mPeriod;
	if (mTimeInParcel < AUTOPLAY_TIME)
	{
		// we've not yet been here long enough
		mPlayed = false;
		return false;
	}

	if (!mPlayed)	// if we've never played
	{
		F32 image_size = 0;
		LLViewerTexture* image;
		image = LLViewerTextureManager::getFetchedTexture(this_media_texture_id,
														  FTT_DEFAULT,
														  false);
		if (image)
		{
			image_size = image->getMaxVirtualSize();
		}

		// If the agent is stopped (slow enough)
		if (gAgent.getVelocity().length() < AUTOPLAY_SPEED)
		{
			// and if the target texture is big enough on screen
			if (image_size > AUTOPLAY_SIZE)
			{
				LLViewerParcelMedia::playMedia(this_parcel);
				mPlayed = true;
			}
		}
	}

	return false; // continue ticking forever please.
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerParcelMedia class
///////////////////////////////////////////////////////////////////////////////

// Static Variables

S32 LLViewerParcelMedia::sParcelMusicState = 0;
S32 LLViewerParcelMedia::sMediaParcelLocalID = 0;
LLUUID LLViewerParcelMedia::sMediaRegionID;
viewer_media_t LLViewerParcelMedia::sMediaImpl;

// Local functions
bool callback_play_media(const LLSD& notification, const LLSD& response,
						 LLParcel* parcel);

//static
void LLViewerParcelMedia::initClass()
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->setHandlerFunc("ParcelMediaCommandMessage",
						processParcelMediaCommandMessage);
	msg->setHandlerFunc("ParcelMediaUpdate", processParcelMediaUpdate);
	LLViewerParcelMediaAutoPlay::initClass();
}

//static
void LLViewerParcelMedia::cleanupClass()
{
	// This needs to be destroyed before global destructor time.
	sMediaImpl = NULL;
}

//static
void LLViewerParcelMedia::registerStreamingAudioPlugin()
{
	// If the audio engine hasn't set up its own preferred handler for
	// streaming audio then set up the generic streaming audio implementation
	// which uses media plugins.
	if (gAudiop && gAudiop->getStreamingAudioImpl() == NULL)
	{
		llinfos << "Using media plugins to render streaming audio" << llendl;
		gAudiop->setStreamingAudioImpl(new LLStreamingAudio_MediaPlugins());
	}
}

//static
void LLViewerParcelMedia::update(LLParcel* parcel)
{
	if (parcel)
	{
		if (!gAgent.getRegion())
		{
			sMediaRegionID = LLUUID();
			stop();
			LL_DEBUGS("Media") << "no agent region, bailing out." << LL_ENDL;
			return;
		}

		// we're in a parcel
		S32 parcel_id = parcel->getLocalID();
		const LLUUID& region_id = gAgent.getRegion()->getRegionID();
		if (parcel_id != sMediaParcelLocalID || region_id != sMediaRegionID)
		{
			LL_DEBUGS("Media") << "New parcel, parcel id = " << parcel_id
							   << ", region id = " << region_id << LL_ENDL;
			sMediaParcelLocalID = parcel_id;
			sMediaRegionID = region_id;
		}

		std::string media_url = parcel->getMediaURL();
		const std::string& media_curr_url = parcel->getMediaCurrentURL();

		// First use warning
		if (!media_url.empty() &&
			gSavedSettings.getWarning("FirstStreamingVideo"))
		{
			gNotifications.add("ParcelCanPlayMedia", LLSD(), LLSD(),
							   boost::bind(callback_play_media, _1, _2,
										   parcel));
			return;
		}

		// If we have a current (link sharing) url, use it instead
		if (!media_curr_url.empty() &&
			parcel->getMediaType() == HTTP_CONTENT_TEXT_HTML)
		{
			media_url = media_curr_url;
		}

		LLStringUtil::trim(media_url);

		// If no parcel media is playing, nothing left to do
		if (sMediaImpl.isNull())
		{
			playMedia(parcel);
			return;
		}
		// Media is playing... has something changed ?
		else if (sMediaImpl->getMediaURL() != media_url ||
				 sMediaImpl->getMediaTextureID() != parcel->getMediaID() ||
				 sMediaImpl->getMimeType() != parcel->getMediaType())
		{
			// Only play if the media types are the same.
			if (sMediaImpl->getMimeType() == parcel->getMediaType())
			{
				playMedia(parcel);
			}
			else
			{
				stop();
			}
		}
	}
	else
	{
		stop();
	}
}

//static
void LLViewerParcelMedia::playMedia(LLParcel* parcel, bool filter)
{
	if (!parcel || parcel != gViewerParcelMgr.getAgentParcel() ||
		!gSavedSettings.getBool("EnableStreamingMedia"))
	{
		return;
	}

	std::string media_url = parcel->getMediaURL();
	LLStringUtil::trim(media_url);

	if (!media_url.empty() && gSavedSettings.getBool("MediaEnableFilter") &&
		(filter || !LLViewerMedia::allowedMedia(media_url)))
	{
		// If filtering is needed or in case media_url just changed
		// to something we did not yet approve.
		LLViewerParcelMediaAutoPlay::playStarted();
		LLViewerMedia::filterParcelMedia(parcel, 0);
		return;
	}

	std::string mime_type = parcel->getMediaType();
	const LLUUID& placeholder_texture_id = parcel->getMediaID();
	bool media_auto_scale = parcel->getMediaAutoScale();
	bool media_loop = parcel->getMediaLoop();
	S32 media_width = parcel->getMediaWidth();
	S32 media_height = parcel->getMediaHeight();

	// Debug print
	LL_DEBUGS("Media") << "Play media type: " << mime_type << ", url : "
					   << media_url << LL_ENDL;

	if (sMediaImpl)
	{
		// If the url and mime type are the same, call play again
		if (sMediaImpl->getMediaURL() == media_url &&
			sMediaImpl->getMimeType() == mime_type &&
			sMediaImpl->getMediaTextureID() == placeholder_texture_id)
		{
			LL_DEBUGS("Media") << "playing with existing url " << media_url
							   << LL_ENDL;
			sMediaImpl->play();
		}
		else
		{
			// Since the texture id is different, we need to generate a new
			// impl. Delete the old media impl first so they don't fight over
			// the texture. A new impl will be created below.
			sMediaImpl->stop();
			sMediaImpl = NULL;
		}
	}

	// Don't ever try to play if the media type is set to "none/none"
	if (stricmp(mime_type.c_str(),
				LLMIMETypes::getDefaultMimeType().c_str()) != 0)
	{
		if (!sMediaImpl)
		{
			LL_DEBUGS("Media") << "new media impl with mime type " << mime_type
							   << ", url " << media_url << LL_ENDL;

			// There is no media impl, make a new one
			sMediaImpl = LLViewerMedia::newMediaImpl(placeholder_texture_id,
													 media_width, media_height,
													 media_auto_scale,
													 media_loop);
			sMediaImpl->setIsParcelMedia(true);
			sMediaImpl->navigateTo(media_url, mime_type, true);
		}

		LLFirstUse::useMedia();

		LLViewerParcelMediaAutoPlay::playStarted();
	}
}

//static
void LLViewerParcelMedia::play(void* userdata)
{
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		LLViewerMedia::sIsUserAction = true;
		playMedia(parcel);
	}
}

//static
void LLViewerParcelMedia::stop(void* userdata)
{
	if (sMediaImpl.notNull())
	{
		// We need to remove the media HUD if it is up.
		LLViewerMediaFocus::getInstance()->clearFocus();

		// This will kill the media instance.
		sMediaImpl->stop();
		sMediaImpl = NULL;
	}
}

//static
void LLViewerParcelMedia::pause(void* userdata)
{
	if (sMediaImpl.notNull())
	{
		sMediaImpl->pause();
	}
}

//static
void LLViewerParcelMedia::start()
{
	if (sMediaImpl.notNull())
	{
		sMediaImpl->start();
		LLFirstUse::useMedia();
		LLViewerParcelMediaAutoPlay::playStarted();
	}
}

//static
void LLViewerParcelMedia::seek(F32 time)
{
	if (sMediaImpl.notNull())
	{
		sMediaImpl->seek(time);
	}
}

//static
void LLViewerParcelMedia::focus(bool focus)
{
	if (sMediaImpl.notNull())
	{
		sMediaImpl->focus(focus);
	}
}

//static
LLViewerMediaImpl::EMediaStatus LLViewerParcelMedia::getStatus()
{
	if (sMediaImpl.notNull() && sMediaImpl->hasMedia())
	{
		return sMediaImpl->getMediaPlugin()->getStatus();
	}
	return LLViewerMediaImpl::MEDIA_NONE;
}

//static
std::string LLViewerParcelMedia::getMimeType()
{
	return sMediaImpl.notNull() ? sMediaImpl->getMimeType()
								: LLMIMETypes::getDefaultMimeType();
}

//static
std::string LLViewerParcelMedia::getURL()
{
	std::string url;
	if (sMediaImpl.notNull())
	{
		url = sMediaImpl->getMediaURL();
	}
	LLStringUtil::trim(url);

	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel && stricmp(parcel->getMediaType().c_str(),
						  LLMIMETypes::getDefaultMimeType().c_str()) != 0)
	{
		if (url.empty())
		{
			url = parcel->getMediaCurrentURL();
		}

		if (url.empty())
		{
			url = parcel->getMediaURL();
		}
	}

	return url;
}

//static
std::string LLViewerParcelMedia::getParcelAudioURL()
{
	std::string music_url;
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		music_url = parcel->getMusicURL();
		LLStringUtil::trim(music_url);
	}
	return music_url;
}

//static
std::string LLViewerParcelMedia::getName()
{
	return sMediaImpl.notNull() ? sMediaImpl->getName() : "";
}

//static
void LLViewerParcelMedia::processParcelMediaCommandMessage(LLMessageSystem* msg,
														   void**)
{
	// extract the agent id
	//LLUUID agent_id;
	//msg->getUUID(agent_id);

	U32 flags;
	U32 command;
	F32 time;
	msg->getU32("CommandBlock", "Flags", flags);
	msg->getU32("CommandBlock", "Command", command);
	msg->getF32("CommandBlock", "Time", time);

	if (flags & ((1 << PARCEL_MEDIA_COMMAND_STOP) |
				 (1 << PARCEL_MEDIA_COMMAND_PAUSE) |
				 (1 << PARCEL_MEDIA_COMMAND_PLAY) |
				 (1 << PARCEL_MEDIA_COMMAND_LOOP) |
				 (1 << PARCEL_MEDIA_COMMAND_UNLOAD)))
	{
		if (command == PARCEL_MEDIA_COMMAND_STOP)
		{
			stop();
		}
		else if (command == PARCEL_MEDIA_COMMAND_PAUSE)
		{
			pause();
		}
		else if (command == PARCEL_MEDIA_COMMAND_PLAY ||
				 command == PARCEL_MEDIA_COMMAND_LOOP)
		{
			if (getStatus() == LLViewerMediaImpl::MEDIA_PAUSED)
			{
				start();
			}
			else
			{
				LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
				playMedia(parcel);
			}
		}
		else if (command == PARCEL_MEDIA_COMMAND_UNLOAD)
		{
			stop();
		}
	}

	if (flags & (1 << PARCEL_MEDIA_COMMAND_TIME))
	{
		if (sMediaImpl.isNull())
		{
			LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
			playMedia(parcel);
		}
		seek(time);
	}
}

//static
void LLViewerParcelMedia::processParcelMediaUpdate(LLMessageSystem* msg,
												   void**)
{
	LLUUID media_id;
	std::string media_url;
	std::string media_type;
	S32 media_width = 0;
	S32 media_height = 0;
	U8 temp;
	bool media_auto_scale = false;
	bool media_loop = false;

	msg->getUUID("DataBlock", "MediaID", media_id);
	char media_url_buffer[257];
	msg->getString("DataBlock", "MediaURL", 255, media_url_buffer);
	media_url = media_url_buffer;
	msg->getU8("DataBlock", "MediaAutoScale", temp);
	media_auto_scale = (bool)temp;

	if (msg->has("DataBlockExtended")) // do we have the extended data?
	{
		char media_type_buffer[257];
		msg->getString("DataBlockExtended", "MediaType", 255,
					   media_type_buffer);
		media_type = media_type_buffer;
		msg->getU8("DataBlockExtended", "MediaLoop", temp);
		media_loop = (bool)temp;
		msg->getS32("DataBlockExtended", "MediaWidth", media_width);
		msg->getS32("DataBlockExtended", "MediaHeight", media_height);
	}

	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		if (parcel->getMediaURL() != media_url ||
			parcel->getMediaType() != media_type ||
			parcel->getMediaID() != media_id ||
			parcel->getMediaWidth() != media_width ||
			parcel->getMediaHeight() != media_height ||
			parcel->getMediaAutoScale() != media_auto_scale ||
			parcel->getMediaLoop() != media_loop)
		{
			// temporarily store these new values in the parcel
			parcel->setMediaURL(media_url);
			parcel->setMediaType(media_type);
			parcel->setMediaID(media_id);
			parcel->setMediaWidth(media_width);
			parcel->setMediaHeight(media_height);
			parcel->setMediaAutoScale(media_auto_scale);
			parcel->setMediaLoop(media_loop);

			playMedia(parcel);
		}
	}
}

//static
void LLViewerParcelMedia::sendMediaNavigateMessage(const std::string& url)
{
	const std::string& cap_url =
		gAgent.getRegionCapability("ParcelNavigateMedia");
	if (cap_url.empty())
	{
		llwarns << "Cannot get ParcelNavigateMedia capability" << llendl;
		return;
	}

	// Send a navigate event to sim for link sharing
	LLSD body;
	body["agent-id"] = gAgentID;
	body["local-id"] = gViewerParcelMgr.getAgentParcel()->getLocalID();
	body["url"] = url;
	LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(cap_url, body,
														  "Media navigation sent to sim.",
														  "Failed to send media navigation to sim.");
}

// inherited from LLViewerMediaObserver
//virtual
void LLViewerParcelMedia::handleMediaEvent(LLPluginClassMedia* self,
										   EMediaEvent event)
{
	if (!self) return;

	switch (event)
	{
		case MEDIA_EVENT_DEBUG_MESSAGE:
#if 0
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_DEBUG_MESSAGE"
							   << LL_ENDL;
#endif
			break;

		case MEDIA_EVENT_CONTENT_UPDATED:
#if 0
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_CONTENT_UPDATED"
							   << LL_ENDL;
#endif
			break;

		case MEDIA_EVENT_TIME_DURATION_UPDATED:
#if 0
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_TIME_DURATION_UPDATED, time is "
							   << self->getCurrentTime() << " of "
							   << self->getDuration() << LL_ENDL;
#endif
			break;

		case MEDIA_EVENT_SIZE_CHANGED:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_SIZE_CHANGED"
							   << LL_ENDL;
			break;

		case MEDIA_EVENT_CURSOR_CHANGED:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_CURSOR_CHANGED, new cursor is "
							   << self->getCursorName() << LL_ENDL;
			break;

		case MEDIA_EVENT_NAVIGATE_BEGIN:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_NAVIGATE_BEGIN"
							   << LL_ENDL;
			break;

		case MEDIA_EVENT_NAVIGATE_COMPLETE:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_NAVIGATE_COMPLETE, result string is: "
							   << self->getNavigateResultString() << LL_ENDL;
			break;

		case MEDIA_EVENT_PROGRESS_UPDATED:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_PROGRESS_UPDATED, loading at "
							   << self->getProgressPercent() << "%" << LL_ENDL;
			break;

		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_STATUS_TEXT_CHANGED, new status text is: "
							   << self->getStatusText() << LL_ENDL;
			break;

		case MEDIA_EVENT_LOCATION_CHANGED:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_LOCATION_CHANGED, new uri is: "
							   << self->getLocation() << LL_ENDL;
			break;

		case MEDIA_EVENT_NAVIGATE_ERROR_PAGE:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_NAVIGATE_ERROR_PAGE"
							   << LL_ENDL;
			break;

		case MEDIA_EVENT_CLICK_LINK_HREF:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_CLICK_LINK_HREF, target is \""
							   << self->getClickTarget() << "\", uri is "
							   << self->getClickURL() << LL_ENDL;
			break;

		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri is "
							   << self->getClickURL() << LL_ENDL;
			break;

		case MEDIA_EVENT_PLUGIN_FAILED:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_PLUGIN_FAILED"
							   << LL_ENDL;
			break;

		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_PLUGIN_FAILED_LAUNCH"
							   << LL_ENDL;
			break;

		case MEDIA_EVENT_NAME_CHANGED:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_NAME_CHANGED"
							   << LL_ENDL;
			break;

		case MEDIA_EVENT_CLOSE_REQUEST:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_CLOSE_REQUEST"
							   << LL_ENDL;
			break;

		case MEDIA_EVENT_PICK_FILE_REQUEST:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_PICK_FILE_REQUEST"
							   << LL_ENDL;
			break;

		case MEDIA_EVENT_FILE_DOWNLOAD:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_FILE_DOWNLOAD"
							   << LL_ENDL;
			break;

		case MEDIA_EVENT_GEOMETRY_CHANGE:
			LL_DEBUGS("Media") << "Media event:  MEDIA_EVENT_GEOMETRY_CHANGE, uuid is "
							   << self->getClickUUID() << LL_ENDL;
			break;

		case MEDIA_EVENT_AUTH_REQUEST:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_AUTH_REQUEST, url "
							   << self->getAuthURL() << ", realm "
							   << self->getAuthRealm() << LL_ENDL;
			break;

		case MEDIA_EVENT_LINK_HOVERED:
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_LINK_HOVERED, hover text is: "
							   << self->getHoverText() << LL_ENDL;
	}
}

bool callback_play_media(const LLSD& notification, const LLSD& response,
						 LLParcel* parcel)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		gSavedSettings.setBool("EnableStreamingMedia", true);
		LLViewerParcelMedia::playMedia(parcel);
	}
	else
	{
		gSavedSettings.setBool("EnableStreamingMedia", false);
	}
	gSavedSettings.setWarning("FirstStreamingVideo", false);
	return false;
}

#if 0	// TODO: observer
void LLViewerParcelMediaNavigationObserver::onNavigateComplete(const EventType& event_in)
{
	std::string url = event_in.getStringValue();
	if (mCurrentURL != url && !mFromMessage)
	{
		LLViewerParcelMedia::sendMediaNavigateMessage(url);
	}
	mCurrentURL = url;
	mFromMessage = false;
}
#endif

//static
void LLViewerParcelMedia::playStreamingMusic(LLParcel* parcel, bool filter)
{
	if (!parcel || parcel != gViewerParcelMgr.getAgentParcel() ||
		!gSavedSettings.getBool("EnableStreamingMusic"))
	{
		return;
	}

	std::string music_url = parcel->getMusicURL();
	LLStringUtil::trim(music_url);
	if (!music_url.empty() && gSavedSettings.getBool("MediaEnableFilter") &&
		(filter || !LLViewerMedia::allowedMedia(music_url)))
	{
		// If filtering is needed or in case music_url just changed
		// to something we did not yet approve.
		LLViewerMedia::filterParcelMedia(parcel, 1);
	}
	else if (gAudiop)
	{
		LLStreamingAudioInterface* stream = gAudiop->getStreamingAudioImpl();
		if (stream && stream->supportsAdjustableBufferSizes())
		{
			stream->setBufferSizes(gSavedSettings.getU32("FMODStreamBufferSize"),
								   gSavedSettings.getU32("FMODDecodeBufferSize"));
		}

		gAudiop->startInternetStream(music_url);
		if (music_url.empty())
		{
			sParcelMusicState = STOPPED;
		}
		else
		{
			sParcelMusicState = PLAYING;
		}
	}
}

//static
void LLViewerParcelMedia::stopStreamingMusic()
{
	if (gAudiop)
	{
		gAudiop->stopInternetStream();
		sParcelMusicState = STOPPED;
	}
}

//static
void LLViewerParcelMedia::playMusic(void*)
{
	if (gAudiop)
	{
		LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
		if (parcel)
		{
			// this doesn't work properly when crossing parcel boundaries
			// - even when the stream is stopped, it doesn't return the right
			// thing - commenting out for now.
			//if (gAudiop->isInternetStreamPlaying() == 0)
			{
				LLViewerMedia::sIsUserAction = true;
				playStreamingMusic(parcel);
			}
		}
	}
}

//static
void LLViewerParcelMedia::pauseMusic(void*)
{
	if (gAudiop)
	{
		gAudiop->pauseInternetStream(1);
		sParcelMusicState = PAUSED;
	}
}

//static
void LLViewerParcelMedia::stopMusic(void*)
{
	stopStreamingMusic();
}
