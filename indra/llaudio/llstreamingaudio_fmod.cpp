/**
 * @file llstreamingaudio_fmod.cpp
 * @brief LLStreamingAudio_FMOD implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 *
 * Copyright (c) 2009, Linden Research, Inc.
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

#include "linden_common.h"

#include "fmod.hpp"
#include "fmod_errors.h"

#include "llstreamingaudio_fmod.h"

#include "llmath.h"
#include "llstring.h"
#include "lltimer.h"

#include "llaudioengine_fmod.h"		// For checkFMerr()

constexpr U32 ESTIMATED_BIT_RATE = 128;		// in kbps
constexpr U32 BYTES_PER_KBIT = 1024 / 8;
// Seconds before force-releasing streams:
constexpr F32 FORCE_RELEASE_DELAY = 3.f;

//---------------------------------------------------------------------------
// LLAudioStreamManagerFMOD implementation
// Manager of possibly-multiple Internet audio streams
//---------------------------------------------------------------------------

class LLAudioStreamManagerFMOD
{
public:
	LLAudioStreamManagerFMOD(FMOD::System* system, FMOD::ChannelGroup* group,
							 const std::string& url);

	FMOD::Channel* startStream();

	// Returns true if the stream was successfully stopped:
	bool releaseStream(bool force = false);

	bool ready();

	LL_INLINE const std::string& getURL()		{ return mInternetStreamURL; }

	FMOD_OPENSTATE getOpenState(unsigned int* pctbuffered = NULL,
								bool* starving = NULL, bool* diskbusy = NULL);

protected:
	FMOD::System*		mSystem;
	FMOD::ChannelGroup*	mChannelGroup;
	FMOD::Channel*		mStreamChannel;
	FMOD::Sound*		mInternetStream;
	std::string			mInternetStreamURL;
	F32					mFirstReleaseAttempt;
	bool				mReady;
};

LLAudioStreamManagerFMOD::LLAudioStreamManagerFMOD(FMOD::System* system,
												   FMOD::ChannelGroup* group,
												   const std::string& url)
:	mSystem(system),
	mChannelGroup(group),
	mStreamChannel(NULL),
	mInternetStream(NULL),
	mFirstReleaseAttempt(0.f),
	mReady(false)
{
	mInternetStreamURL = url;

	constexpr FMOD_MODE mode = FMOD_2D | FMOD_NONBLOCKING | FMOD_IGNORETAGS;
	FMOD_RESULT result = mSystem->createStream(url.c_str(), mode, NULL,
											   &mInternetStream);
	mReady = result == FMOD_OK;
	if (!mReady || !mInternetStream)
	{
		llwarns << "Could not open fmod stream " << url << " - Error: "
				<< FMOD_ErrorString(result) << llendl;
	}
}

FMOD::Channel* LLAudioStreamManagerFMOD::startStream()
{
	if (!mSystem)	// Paranoia
	{
		llwarns << "mSystem is NULL !" << llendl;
		return NULL;
	}

	// We need a live and opened stream before we try and play it.
	if (!mInternetStream || getOpenState() != FMOD_OPENSTATE_READY)
	{
		llwarns << "No Internet stream to start playing !" << llendl;
		return NULL;
	}

	if (mStreamChannel)
	{
		// We already have a channel for this stream.
		LL_DEBUGS("FMOD") << "We already have a stream for channel: "
						  << std::hex << mStreamChannel << std::dec << LL_ENDL;
		return mStreamChannel;
	}

	LL_DEBUGS("FMOD") << "Starting stream..." << LL_ENDL;
	if (!checkFMerr(mSystem->playSound(mInternetStream, mChannelGroup, true,
											 &mStreamChannel),
								 "FMOD::System::playSound"))
	{
		LL_DEBUGS("FMOD") << "Stream started." << LL_ENDL;
	}

	return mStreamChannel;
}

bool LLAudioStreamManagerFMOD::releaseStream(bool force)
{
	if (mInternetStream)
	{
		bool timed_out = false;
		if (mFirstReleaseAttempt == 0.f)
		{
			mFirstReleaseAttempt = (F32)LLTimer::getElapsedSeconds();
		}
		else if ((F32)LLTimer::getElapsedSeconds() -
				 mFirstReleaseAttempt >= FORCE_RELEASE_DELAY)
		{
			LL_DEBUGS("FMOD") << "Stopped stream " << mInternetStreamURL
							  << " timed out." << LL_ENDL;
			timed_out = true;
		}

		FMOD_OPENSTATE state = getOpenState();

		if (!timed_out && !force && state != FMOD_OPENSTATE_READY &&
			state != FMOD_OPENSTATE_ERROR)
		{
			LL_DEBUGS("FMOD") << "Stream " << mInternetStreamURL
							  << " not yet ready for release. State is: "
							  << (S32)state  << " - Delaying." << LL_ENDL;
			return false;
		}
		LL_DEBUGS("FMOD") << "Attempting to release stream "
						  << mInternetStreamURL << " (current state is: "
						  << (S32)state << ")..." << LL_ENDL;
		if (force || timed_out)
		{
			llwarns << "Failed to release stream: " << mInternetStreamURL
					<< " - Force-closing it." << llendl;
		}
		else if (mInternetStream->release() == FMOD_OK)
		{
			LL_DEBUGS("FMOD") << "Stream " << mInternetStreamURL
							  << " released." << LL_ENDL;
		}
		else
		{
			LL_DEBUGS("FMOD") << "Failed to release stream: "
							  << mInternetStreamURL << " - Delaying."
							  << LL_ENDL;
			return false;
		}

		mStreamChannel = NULL;
		mInternetStream = NULL;
	}

	return true;
}

FMOD_OPENSTATE LLAudioStreamManagerFMOD::getOpenState(unsigned int* pctbuffered,
													  bool* starving,
													  bool* diskbusy)
{
	FMOD_OPENSTATE state = FMOD_OPENSTATE_ERROR;
	if (mInternetStream)
	{
		mInternetStream->getOpenState(&state, pctbuffered, starving, diskbusy);
	}
	return state;
}

//---------------------------------------------------------------------------
// Internet Streaming
//---------------------------------------------------------------------------
LLStreamingAudio_FMOD::LLStreamingAudio_FMOD(FMOD::System* system)
:	mSystem(system),
	mBufferMilliSeconds(10000U),
	mCurrentInternetStreamp(NULL),
	mFMODInternetStreamChannelp(NULL),
	mGain(1.0f),
	mArtist(),
	mTitle(),
	mLastStarved(0.f),
	mPendingStart(false),
	mNewMetaData(false)
{
	// Audio to buffer size for the audio card. mBufferMilliSeconds must be
	// larger than the usual Second Life frame stutter time.
	U32 size = ESTIMATED_BIT_RATE * BYTES_PER_KBIT * mBufferMilliSeconds /
			   1000U; // in seconds
	checkFMerr(mSystem->setStreamBufferSize(size, FMOD_TIMEUNIT_RAWBYTES),
			   "FMOD::System::setStreamBufferSize");

	checkFMerr(mSystem->createChannelGroup("stream", &mStreamGroup),
			   "FMOD::System::createChannelGroup");
}

LLStreamingAudio_FMOD::~LLStreamingAudio_FMOD()
{
	// Stop streaming audio if needed
	stop();
	mURL.clear();

	F32 start = (F32)LLTimer::getElapsedSeconds();
	while (!releaseDeadStreams((F32)LLTimer::getElapsedSeconds() - start >=
							   FORCE_RELEASE_DELAY)) ;

	if (mStreamGroup)
	{
		checkFMerr(mStreamGroup->release(), "FMOD::ChannelGroup::release");
		mStreamGroup = NULL;
	}
}

void LLStreamingAudio_FMOD::setBufferSizes(U32 streambuffertime,
												 U32 decodebuffertime)
{
	if (mSystem)	// Paranoia
	{
		mBufferMilliSeconds = llmin(streambuffertime, 3000U);	// in ms
		decodebuffertime = llmin(decodebuffertime, 500U);		// in ms
		U32 size = ESTIMATED_BIT_RATE * BYTES_PER_KBIT * mBufferMilliSeconds /
				   1000U; // in seconds
		checkFMerr(mSystem->setStreamBufferSize(size, FMOD_TIMEUNIT_RAWBYTES),
				   "FMOD::System::setStreamBufferSize");
		FMOD_ADVANCEDSETTINGS settings;
		memset(&settings, 0, sizeof(settings));
		settings.cbSize = sizeof(settings);
		settings.defaultDecodeBufferSize = decodebuffertime;
		checkFMerr(mSystem->setAdvancedSettings(&settings),
				   "FMOD::System::setAdvancedSettings");
	}
}

void LLStreamingAudio_FMOD::start(const std::string& url)
{
	// "stop" stream if needed, but don't clear url, etc in case
	// url == mInternetStreamURL
	stop();

	if (url.empty())
	{
		llinfos << "Set Internet stream to none." << llendl;
		mURL.clear();
		mPendingStart = false;
	}
	else
	{
		mURL = url;
		mPendingStart = true;
	}
}

bool LLStreamingAudio_FMOD::releaseDeadStreams(bool force)
{
	for (std::list<LLAudioStreamManagerFMOD*>::iterator
			iter = mDeadStreams.begin();
		 iter != mDeadStreams.end(); )
	{
		LLAudioStreamManagerFMOD* streamp = *iter;
		if (!streamp)	// Paranoia
		{
			llwarns << "Found a NULL stream in dead streams list !  Removing."
					<< llendl;
			iter = mDeadStreams.erase(iter);
		}
		else if (streamp->releaseStream(force))
		{
			llinfos << "Closed dead stream: " << streamp->getURL() << llendl;
			delete streamp;
			iter = mDeadStreams.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	return mDeadStreams.empty();
}

void LLStreamingAudio_FMOD::update()
{
	// Kill dead Internet streams
	if (!releaseDeadStreams(false))
	{
		return;
	}

	if (mPendingStart && mSystem && mStreamGroup)
	{
		llinfos << "Starting Internet stream: " << mURL << llendl;
		mCurrentInternetStreamp = new LLAudioStreamManagerFMOD(mSystem,
															   mStreamGroup,
															   mURL);
		mPendingStart = false;
	}

	// Do not do anything if there is no stream playing
	if (!mCurrentInternetStreamp)
	{
		return;
	}

	unsigned int progress;
	bool starving;
	bool diskbusy;
	FMOD_OPENSTATE open_state =
		mCurrentInternetStreamp->getOpenState(&progress, &starving, &diskbusy);
	if (open_state == FMOD_OPENSTATE_READY)
	{
		// Stream is live. Start it if it is ready.
		if (!mFMODInternetStreamChannelp &&
			(mFMODInternetStreamChannelp = mCurrentInternetStreamp->startStream()))
		{
			LL_DEBUGS("FMOD") << "Stream " << mCurrentInternetStreamp->getURL()
							  << " is live, starting it." << LL_ENDL;
			// Reset volume to previously set volume
			setGain(getGain());
			mFMODInternetStreamChannelp->setPaused(false);
			mFMODInternetStreamChannelp->setMute(false);
			mLastStarved = 0.f;
			LL_DEBUGS("FMOD") << "Stream started." << LL_ENDL;
		}
	}
	else if (open_state == FMOD_OPENSTATE_ERROR)
	{
		LL_DEBUGS("FMOD") << "Stream '" << mCurrentInternetStreamp->getURL()
						  << "' reports an error, stopping it." << LL_ENDL;
		stop();
		LL_DEBUGS("FMOD") << "Stream stopped." << LL_ENDL;
		return;
	}

	if (mFMODInternetStreamChannelp)
	{
		FMOD::Sound* sound = NULL;

		if (mFMODInternetStreamChannelp->getCurrentSound(&sound) == FMOD_OK &&
			sound)
		{
			FMOD_TAG tag;
			S32 tagcount, dirtytagcount;

			if (sound->getNumTags(&tagcount, &dirtytagcount) == FMOD_OK &&
				dirtytagcount)
			{
				S32 count = llclamp(tagcount, 0, 1024);	// Paranoia
				if (count != tagcount)
				{
					llwarns << "Bogus tag count: " << tagcount
							<< " - Clamped to: " << count << llendl;
				}
				for (S32 i = 0; i < count; ++i)
				{
					if (sound->getTag(NULL, i, &tag) != FMOD_OK)
					{
						continue;
					}

					std::string token = tag.name;
					LLStringUtil::toLower(token);

					LL_DEBUGS("FMOD") << "Stream tag name: " << token
									  << " - type: " << tag.type
									  << " - data type: " << tag.datatype
									  << LL_ENDL;

					if (tag.type == FMOD_TAGTYPE_FMOD)
					{
						if (token == "sample rate change")
						{
							llinfos << "Stream forced changing sample rate to "
									<< *((float*)tag.data) << llendl;
							mFMODInternetStreamChannelp->setFrequency(*((float*)tag.data));
						}
					}
					else if (tag.type == FMOD_TAGTYPE_ASF ||
							 tag.datatype == FMOD_TAGDATATYPE_STRING)
					{
						if (token == "title" || token == "tit2")
						{
							std::string tmp;
							tmp.assign((char*)tag.data);
							if (mTitle != tmp)
							{
								mTitle = tmp;
								mNewMetaData = true;
							}
						}
						else if (token == "artist" || token == "tpe1" ||
								 token == "wm/albumtitle")
						{
							std::string tmp;
							tmp.assign((char*)tag.data);
							if (mArtist != tmp)
							{
								mArtist = tmp;
								mNewMetaData = true;
							}
						}
					}
				}
			}

			if (starving)
			{
				bool paused = false;
				mFMODInternetStreamChannelp->getPaused(&paused);
				if (!paused && mLastStarved != 0.f)
				{
					llinfos << "Stream starvation detected, muting stream audio until it clears."
							<< llendl;
					LL_DEBUGS("FMOD") << "diskbusy = " << diskbusy
									  << " - progress = " << progress
									  << LL_ENDL;
					mFMODInternetStreamChannelp->setMute(true);
				}
				mLastStarved = (F32)LLTimer::getElapsedSeconds();
			}
			else if (mLastStarved != 0.f && progress > 50)
			{
				// more than 50% of the buffer is full, resume music playing
				F32 buffer_fill_time = ((F32)LLTimer::getElapsedSeconds() -
										mLastStarved) * 100.f / (F32)progress;
				F32 buffer_size_seconds = (F32)mBufferMilliSeconds / 1000.f;
				if (buffer_fill_time > buffer_size_seconds)
				{
					llwarns << "Starvation state cleared, resuming streaming music playing but new starvations will likely occur (time required to fill the buffer = "
							<< buffer_fill_time
							<< " - buffer size in seconds = "
							<< buffer_size_seconds << ")." << llendl;
				}
				else
				{
					llinfos << "Starvation state cleared, resuming streaming music playing."
							<< llendl;
				}
				mLastStarved = 0.f;
				mFMODInternetStreamChannelp->setMute(false);
			}
		}
	}
}

void LLStreamingAudio_FMOD::stop()
{
	mLastStarved = 0.f;
	mNewMetaData = false;
	mArtist.clear();
	mTitle.clear();

	if (mFMODInternetStreamChannelp)
	{
		LL_DEBUGS("FMOD") << "Stopping stream..." << LL_ENDL;
		checkFMerr(mFMODInternetStreamChannelp->setPaused(true),
				   "FMOD::Channel::setPaused");
		checkFMerr(mFMODInternetStreamChannelp->setPriority(0),
				   "FMOD::Channel::setPriority");
		mFMODInternetStreamChannelp = NULL;
	}

	if (mCurrentInternetStreamp)
	{
		if (mCurrentInternetStreamp->releaseStream())
		{
			llinfos << "Released Internet stream: "
					<< mCurrentInternetStreamp->getURL() << llendl;
			delete mCurrentInternetStreamp;
		}
		else
		{
			llinfos << "Pushing Internet stream to dead list: "
					<< mCurrentInternetStreamp->getURL() << llendl;
			mDeadStreams.push_back(mCurrentInternetStreamp);
		}
		mCurrentInternetStreamp = NULL;
	}
}

void LLStreamingAudio_FMOD::pause(S32 pauseopt)
{
	LL_DEBUGS("FMOD") << "called with pauseopt = " << pauseopt << LL_ENDL;
	if (pauseopt < 0)
	{
		pauseopt = mCurrentInternetStreamp ? 1 : 0;
		LL_DEBUGS("FMOD") << "pauseopt < 0 -> " << pauseopt << LL_ENDL;
	}

	if (pauseopt)
	{
		if (mCurrentInternetStreamp)
		{
			LL_DEBUGS("FMOD") << "Stopping stream" << LL_ENDL;
			stop();
		}
	}
	else
	{
		LL_DEBUGS("FMOD") << "Starting stream" << LL_ENDL;
		start(getURL());
	}
}

// A stream is "playing" if it has been requested to start. That does not
// necessarily mean audio is coming out of the speakers.
S32 LLStreamingAudio_FMOD::isPlaying()
{
	if (mCurrentInternetStreamp)
	{
		return 1; // Active and playing
	}
	else if (!mURL.empty())
	{
		return 2; // "Paused"
	}
	else
	{
		return 0;
	}
}

void LLStreamingAudio_FMOD::setGain(F32 vol)
{
	mGain = vol;

	if (mFMODInternetStreamChannelp)
	{
		vol = llclamp(vol, 0.f, 1.f);
		mFMODInternetStreamChannelp->setVolume(vol);
	}
}
