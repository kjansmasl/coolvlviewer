/**
 * @file llaudioengine_fmod.cpp
 * @brief Implementation of LLAudioEngine class abstracting the audio support
 * as a FMOD Studio implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2014, Linden Research, Inc.
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

#include "lldir.h"
#include "llmath.h"
#include "llrand.h"
#include "llversionviewer.h"		// For LL_CHANNEL

#include "fmod.hpp"
#include "fmod_errors.h"

#include "llstreamingaudio.h"
#include "llstreamingaudio_fmod.h"

#include "llaudioengine_fmod.h"
#include "lllistener_fmod.h"

// Set to 1 to override FMOD memory management with ours
#define LL_MANAGE_FMOD_MEMORY 0

// static variables
#if LL_LINUX
bool LLAudioEngine_FMOD::sNoALSA = false;
bool LLAudioEngine_FMOD::sNoPulseAudio = false;
#endif

FMOD::ChannelGroup* LLAudioEngine_FMOD::sChannelGroups[LLAudioEngine::AUDIO_TYPE_COUNT] = { NULL };

// Helper functions and callbacks

bool checkFMerr(S32 result, const char* str)
{
	if (result == (S32)FMOD_OK)
	{
		return false;
	}
	LL_DEBUGS("FMOD") << str << " Error: "
					  << FMOD_ErrorString((FMOD_RESULT)result) << LL_ENDL;
	return true;
}

#if LL_MANAGE_FMOD_MEMORY
static void* F_CALLBACK decode_alloc(unsigned int size, FMOD_MEMORY_TYPE type,
									 const char* sourcestr)
{
	if (type & FMOD_MEMORY_STREAM_DECODE)
	{
		llinfos << "Decode buffer size: " << size << llendl;
	}
	else if (type & FMOD_MEMORY_STREAM_FILE)
	{
		llinfos << "Stream buffer size: " << size << llendl;
	}
	return ll_aligned_malloc_16(size);
}

static void* F_CALLBACK decode_realloc(void* ptr, unsigned int size,
									   FMOD_MEMORY_TYPE type,
									   const char* sourcestr)
{
	return ll_aligned_realloc_16(ptr, size, 0);
}

static void F_CALLBACK decode_dealloc(void* ptr, FMOD_MEMORY_TYPE type,
									  const char* sourcestr)
{
	ll_aligned_free_16(ptr);
}
#endif

// *NOTE: this is almost certainly being called on the mixer thread, not the
// main thread. May have implications for callees or audio engine shutdown.
FMOD_RESULT F_CALLBACK windCallback(FMOD_DSP_STATE* dsp_state,
									float* originalbuffer, float* newbuffer,
									unsigned int length, int inchannels,
									int* outchannels)
{
	// originalbuffer = fmod's original mixbuffer.
	// newbuffer = the buffer passed from the previous DSP unit.
	// length = length in samples at this mix time.

	if (!dsp_state)	// Paranoia ?... Better safe than sorry ! HB
	{
		return FMOD_ERR_DSP_NOTFOUND;	// Seems appropriate to me... HB
	}

	FMOD::DSP* thisdsp = (FMOD::DSP*)dsp_state->instance;
	if (!thisdsp)
	{
		return FMOD_ERR_DSP_NOTFOUND;	// Seems appropriate to me... HB
	}

	LLWindGen<LLAudioEngine_FMOD::MIXBUFFERFORMAT>* windgen;
	thisdsp->getUserData((void**)&windgen);
	if (windgen)
	{
		windgen->windGenerate((LLAudioEngine_FMOD::MIXBUFFERFORMAT*)newbuffer,
							  length);
	}

	return FMOD_OK;
}

//
// LLAudioEngine_FMOD implementation
//

LLAudioEngine_FMOD::LLAudioEngine_FMOD(bool enable_profiler)
:	mInited(false),
	mWindGen(NULL),
	mWindDSP(NULL),
	mSystem(NULL),
	mEnableProfiler(enable_profiler)
{
	mWindDSPDesc = new FMOD_DSP_DESCRIPTION();
}

LLAudioEngine_FMOD::~LLAudioEngine_FMOD()
{
	delete mWindDSPDesc;
	mWindDSPDesc = NULL;
}

bool LLAudioEngine_FMOD::init(void* userdata)
{
	LL_DEBUGS("AppInit") << "Initializing FMOD" << LL_ENDL;

	mAudioDevice.clear();

#if LL_MANAGE_FMOD_MEMORY
	FMOD_RESULT result = FMOD::Memory_Initialize(NULL, 0, &decode_alloc,
												 &decode_realloc,
												 &decode_dealloc,
												 FMOD_MEMORY_ALL);
	checkFMerr(result, "FMOD::Memory_Initialize");
#else
	FMOD_RESULT result;
#endif

	result = FMOD::System_Create(&mSystem);
	if (checkFMerr(result, "FMOD::System_Create"))
	{
		return false;
	}

	// Will call LLAudioEngine_FMOD::allocateListener, which needs a valid
	// mSystem pointer.
	LLAudioEngine::init(userdata);

	unsigned int version;
	result = mSystem->getVersion(&version);
	checkFMerr(result, "FMOD::System::getVersion");
	if (version < FMOD_VERSION)
	{
		llwarns << "You are using the wrong FMOD Studio version (" << version
				<< ") !  You should be using FMOD Studio " << FMOD_VERSION
				<< llendl;
	}

	// In this case, all sounds, PLUS wind and stream will be software.
	result = mSystem->setSoftwareChannels(MAX_AUDIO_CHANNELS + 2);
	checkFMerr(result, "FMOD::System::setSoftwareChannels");

	FMOD_ADVANCEDSETTINGS adv_settings;
	memset(&adv_settings, 0, sizeof(FMOD_ADVANCEDSETTINGS));
	adv_settings.cbSize = sizeof(FMOD_ADVANCEDSETTINGS);
	adv_settings.resamplerMethod = FMOD_DSP_RESAMPLER_LINEAR;
	result = mSystem->setAdvancedSettings(&adv_settings);
	checkFMerr(result, "FMOD::System::setAdvancedSettings");

	U32 fmod_flags = FMOD_INIT_NORMAL | FMOD_INIT_THREAD_UNSAFE |
					 FMOD_INIT_3D_RIGHTHANDED;
	if (mEnableProfiler)
	{
		fmod_flags |= FMOD_INIT_PROFILE_ENABLE;
		mSystem->createChannelGroup("None", &sChannelGroups[AUDIO_TYPE_NONE]);
		mSystem->createChannelGroup("SFX", &sChannelGroups[AUDIO_TYPE_SFX]);
		mSystem->createChannelGroup("UI", &sChannelGroups[AUDIO_TYPE_UI]);
		mSystem->createChannelGroup("Ambient",
									&sChannelGroups[AUDIO_TYPE_AMBIENT]);
	}

#if LL_LINUX
	bool audio_ok = false;

	if (sNoPulseAudio)
	{
		LL_DEBUGS("AppInit") << "PulseAudio audio output SKIPPED" << LL_ENDL;
	}
	else
	{
		LL_DEBUGS("AppInit") << "Trying PulseAudio audio output..." << LL_ENDL;
		if (mSystem->setOutput(FMOD_OUTPUTTYPE_PULSEAUDIO) == FMOD_OK &&
			(result = mSystem->init(MAX_AUDIO_CHANNELS + 2, fmod_flags,
									(void*)LL_CHANNEL)) == FMOD_OK)
		{
			mAudioDevice = "PulseAudio";
			audio_ok = true;
		}
		else
		{
			checkFMerr(result, "PulseAudio audio output FAILED to initialize");
		}
	}

	if (audio_ok || sNoALSA)
	{
		LL_DEBUGS("AppInit") << "ALSA audio output SKIPPED" << LL_ENDL;
	}
	else
	{
		LL_DEBUGS("AppInit") << "Trying ALSA audio output..." << LL_ENDL;
		if (mSystem->setOutput(FMOD_OUTPUTTYPE_ALSA) == FMOD_OK &&
			(result = mSystem->init(MAX_AUDIO_CHANNELS + 2, fmod_flags, 0)) == FMOD_OK)
		{
			mAudioDevice = "ALSA";
			audio_ok = true;
		}
		else
		{
			checkFMerr(result, "ALSA audio output FAILED to initialize");
		}
	}

	if (!audio_ok)
	{
		llwarns << "Overall audio init failure." << llendl;
		return false;
	}

	llinfos << mAudioDevice << " output initialized" << llendl;

	// We are interested in logging which output method we ended up with, for
	// QA purposes.
	FMOD_OUTPUTTYPE output_type;
	mSystem->getOutput(&output_type);
	switch (output_type)
	{
		case FMOD_OUTPUTTYPE_NOSOUND:
			LL_DEBUGS("AppInit") << "Audio output: NoSound" << LL_ENDL;
			break;

		case FMOD_OUTPUTTYPE_PULSEAUDIO:
			LL_DEBUGS("AppInit") << "Audio output: PulseAudio" << LL_ENDL;
			break;

		case FMOD_OUTPUTTYPE_ALSA:
			LL_DEBUGS("AppInit") << "Audio output: ALSA" << LL_ENDL;
			break;

		default:
			llinfos << "Audio output: Unknown !" << llendl;
	}
#else // LL_LINUX
	// Initialize the FMOD engine
	result = mSystem->init(MAX_AUDIO_CHANNELS + 2, fmod_flags, 0);
	if (result == FMOD_ERR_OUTPUT_CREATEBUFFER)
	{
		// Ok, the format selected is not supported by this soundcard. Switch
		// it back to stereo...
		result = mSystem->setSoftwareFormat(44100, FMOD_SPEAKERMODE_STEREO, 0);
		checkFMerr(result, "Error falling back to stereo mode");
		// ... and re-init.
		result = mSystem->init(MAX_AUDIO_CHANNELS + 2, fmod_flags, 0);
	}
	if (checkFMerr(result, "Error initializing FMOD Studio"))
	{
		return false;
	}
#endif

	// Set up our favourite FMOD-native streaming audio implementation if none
	// has already been added
	if (!getStreamingAudioImpl()) // no existing implementation added
	{
		setStreamingAudioImpl(new LLStreamingAudio_FMOD(mSystem));
	}

	LL_DEBUGS("AppInit") << "FMOD Studio initialized correctly" << LL_ENDL;

	FMOD_ADVANCEDSETTINGS adv_settings_dump = {};
	mSystem->getAdvancedSettings(&adv_settings_dump);
	LL_DEBUGS("AppInit") << "FMOD Studio resampler: "
						 << adv_settings.resamplerMethod << " bytes"
						 << LL_ENDL;

	int r_numbuffers;
	unsigned int r_bufferlength;
	mSystem->getDSPBufferSize(&r_bufferlength, &r_numbuffers);
	int r_samplerate, r_channels;
	char r_name[512];
	mSystem->getDriverInfo(0, r_name, 511, NULL, &r_samplerate, NULL, &r_channels);
	r_name[511] = '\0';

	// Optimistic default; I suspect if sample rate is 0, everything breaks:
	int latency = 100;
	if (r_samplerate != 0)
	{
		latency = (int)(1000.f * r_bufferlength * r_numbuffers / r_samplerate);
	}

	llinfos << "FMOD device: " << r_name << " with parameters: "
			<< r_samplerate << " Hz, " << r_channels << " channels - Buffers: "
			<< r_numbuffers << " * " << r_bufferlength << " bytes - Latency: "
			<< latency << "ms." << llendl;

	mInited = true;

	return true;
}

std::string LLAudioEngine_FMOD::getDriverName(bool verbose)
{
	if (!mSystem)
	{
		llwarns << "FMOD not properly initialized !" << llendl;
		return "FMODEx_NOT_INITIALIZED";
	}

	if (!verbose)
	{
		return "FMODStudio";
	}

	std::string result = "FMOD Studio";

	unsigned int version;
	if (!checkFMerr(mSystem->getVersion(&version), "FMOD::System::getVersion"))
	{
		result += llformat(" v%1x.%02x.%02x", version >> 16,
						   version >> 8 & 0x000000FF, version & 0x000000FF);
	}

	if (!mAudioDevice.empty())
	{
		result += " (" + mAudioDevice + ")";
	}

	return result;
}

void LLAudioEngine_FMOD::allocateListener()
{
	mListenerp = (LLListener*)new(std::nothrow) LLListener_FMOD(mSystem);
	if (!mListenerp)
	{
		llwarns << "Listener creation failed" << llendl;
	}
}

void LLAudioEngine_FMOD::shutdown()
{
	if (mWindDSP)
	{
		cleanupWind();
	}

	stopInternetStream();

	llinfos << "Shutting down the audio engine..." << llendl;
	LLAudioEngine::shutdown();

	if (mSystem)
	{
		llinfos << "Closing FMOD Studio" << llendl;
		mSystem->close();
		mSystem->release();
	}
	llinfos << "Done closing FMOD Studio" << llendl;

	delete mListenerp;
	mListenerp = NULL;
}

LLAudioBuffer* LLAudioEngine_FMOD::createBuffer()
{
	return new LLAudioBufferFMOD(mSystem);
}

LLAudioChannel* LLAudioEngine_FMOD::createChannel()
{
	return new LLAudioChannelFMOD(mSystem);
}

bool LLAudioEngine_FMOD::initWind()
{
	mNextWindUpdate = 0.f;

	if (!mWindDSP)
	{
		if (!mWindDSPDesc)
		{
			llwarns << "mWindDSPDesc is NULL !" << llendl;
			return false;
		}

		if (mWindGen)
		{
			LL_DEBUGS("FMOD") << "mWindGen was non-NULL. Deleting." << LL_ENDL;
			delete mWindGen;
		}

		// Set everything to zero:
		memset(mWindDSPDesc, 0, sizeof(*mWindDSPDesc));

		mWindDSPDesc->pluginsdkversion = FMOD_PLUGIN_SDK_VERSION;

		// Set name to "Wind Unit"
		strncpy(mWindDSPDesc->name, "Wind Unit", sizeof(mWindDSPDesc->name));

		mWindDSPDesc->numoutputbuffers = 1;

		// Assign the callback, which may be called from arbitrary threads
		mWindDSPDesc->read = &windCallback;

		bool error = checkFMerr(mSystem->createDSP(mWindDSPDesc, &mWindDSP),
								"FMOD::createDSP");
		if (error || !mWindDSP)
		{
			llwarns << "Failed to create the wind DSP" << llendl;
			return false;
		}

		int frequency = 44100;
		FMOD_SPEAKERMODE mode;
		if (!checkFMerr(mSystem->getSoftwareFormat(&frequency, &mode, NULL),
						"FMOD::System::getSoftwareFormat"))
		{
			mWindGen = new LLWindGen<MIXBUFFERFORMAT>((U32)frequency);
			if (!checkFMerr(mWindDSP->setUserData((void*)mWindGen),
							"FMOD::DSP::setUserData") &&
				!checkFMerr(mWindDSP->setChannelFormat(FMOD_CHANNELMASK_STEREO,
													   2, mode),
							"FMOD::DSP::setChannelFormat") &&
				!checkFMerr(mSystem->playDSP(mWindDSP, NULL, false, NULL),
							"FMOD::System::playDSP"))
			{
				return true;
			}

			llwarns << "Failed to initialize the wind DSP" << llendl;
			cleanupWind();
		}
	}

	return false;
}

void LLAudioEngine_FMOD::cleanupWind()
{
	if (mWindDSP)
	{
		FMOD::ChannelGroup* group = NULL;
		bool error = checkFMerr(mSystem->getMasterChannelGroup(&group),
								"FMOD::System::getMasterChannelGroup");
		if (!error && group)
		{
			group->removeDSP(mWindDSP);
		}
		mWindDSP->release();
		mWindDSP = NULL;
	}

	if (mWindGen)
	{
		delete mWindGen;
		mWindGen = NULL;
	}
}

void LLAudioEngine_FMOD::updateWind(LLVector3 wind_vec,
									F32 camera_height_above_water)
{
	if (mEnableWind && mWindGen &&
		mWindUpdateTimer.checkExpirationAndReset(LL_WIND_UPDATE_INTERVAL))
	{
		// Wind comes in as Linden coordinate (+X = forward, +Y = left, +Z =
		// up), so we need to convert this to the conventional orientation DS3D
		// and OpenAL use where +X = right, +Y = up, +Z = backwards
		wind_vec.set(-wind_vec.mV[1], wind_vec.mV[2], -wind_vec.mV[0]);

		mWindGen->mTargetFreq =
			80.f * powf(1.f + mapWindVecToPitch(wind_vec),
						2.5f * (mapWindVecToGain(wind_vec) + 1.f));
		mWindGen->mTargetGain = mapWindVecToGain(wind_vec) * mMaxWindGain;
		mWindGen->mTargetPanGainR = mapWindVecToPan(wind_vec);
  	}
}

void LLAudioEngine_FMOD::setInternalGain(F32 gain)
{
	if (!mInited || !mSystem)
	{
		return;
	}

	gain = llclamp(gain, 0.f, 1.f);

	FMOD::ChannelGroup* master_group;
	mSystem->getMasterChannelGroup(&master_group);
	if (!master_group)
	{
		LL_DEBUGS("FMOD") << "Could not get master group." << LL_ENDL;
		return;
	}
	master_group->setVolume(gain);

	LLStreamingAudioInterface* saimpl = getStreamingAudioImpl();
	if (saimpl)
	{
		// FMOD Studio likes its streaming audio channel gain re-asserted after
		// master volume change.
		saimpl->setGain(saimpl->getGain());
	}
}

//
// LLAudioChannelFMOD implementation
//

LLAudioChannelFMOD::LLAudioChannelFMOD(FMOD::System* system)
:	LLAudioChannel(),
	mSystemp(system),
	mChannelp(NULL),
	mLastSamplePos(0)
{
}

LLAudioChannelFMOD::~LLAudioChannelFMOD()
{
	cleanup();
}

bool LLAudioChannelFMOD::updateBuffer()
{
	FMOD_RESULT result;

	if (LLAudioChannel::updateBuffer())
	{
		// Base class update returned true, which means that we need to
		// actually set up the channel for a different buffer.

		LLAudioBufferFMOD* bufferp =
			(LLAudioBufferFMOD*)mCurrentSourcep->getCurrentBuffer();
		if (!bufferp)
		{
			llwarns << "No current buffer !" << llendl;
			mCurrentBufferp = NULL;
			return false;
		}

		// Grab the FMOD sound associated with the buffer
		FMOD::Sound* soundp = bufferp->getSound();
		if (!soundp)
		{
			// This is bad, there should ALWAYS be a sound associated with a
			// legit buffer.
			llwarns << "No FMOD sound" << llendl;
#if 1
			mCurrentBufferp = NULL;
#endif
			return false;
		}

		// Actually play the sound. Start it off paused so we can do all the
		// necessary setup.
		if (!mChannelp)
		{
			result = getSystem()->playSound(soundp, NULL, true, &mChannelp);
			checkFMerr(result, "FMOD::System::playSound");
			if (!mChannelp)
			{
				LL_DEBUGS("FMOD") << "Could not allocate a new channel"
								  << LL_ENDL;
				mCurrentBufferp = NULL;
				return false;
			}
		}
	}

	// If we have a source for the channel, we need to update its gain.
	if (mCurrentSourcep)
	{
		result = mChannelp->setVolume(getSecondaryGain() *
									  mCurrentSourcep->getGain());
		if (result != FMOD_OK)
		{
			LL_DEBUGS("FMOD") << checkFMerr(result, "FMOD::Channel::setVolume")
							  << LL_ENDL;
		}

		mChannelp->setMode(mCurrentSourcep->isLoop() ? FMOD_LOOP_NORMAL
													 : FMOD_LOOP_OFF);
	}

	return true;
}

void LLAudioChannelFMOD::update3DPosition()
{
	if (!mChannelp)
	{
		// We are not actually a live channel, i.e. we are not playing anything
		return;
	}

	LLAudioBufferFMOD* bufferp =
		(LLAudioBufferFMOD*)mCurrentBufferp;
	if (!bufferp)
	{
		// We do not have a buffer associated with us (should really have been
		// picked up by the above test).
		return;
	}

	if (mCurrentSourcep->isAmbient())
	{
		// Ambient sound, so we do not need to do any positional updates.
		set3DMode(false);
	}
	else
	{
		// Localized sound. Update the position and velocity of the sound.
		set3DMode(true);

		LLVector3 float_pos(mCurrentSourcep->getPositionGlobal());
		FMOD_RESULT result =
			mChannelp->set3DAttributes((FMOD_VECTOR*)float_pos.mV,
									   (FMOD_VECTOR*)mCurrentSourcep->getVelocity().mV);
		checkFMerr(result, "FMOD::Channel::set3DAttributes");
	}
}

void LLAudioChannelFMOD::updateLoop()
{
	if (!mChannelp)
	{
		// May want to clear up the loop/sample counters.
		return;
	}

	//
	//* HACK: we keep track of whether we looped or not by seeing when the
	// sample position looks like it's going backwards. Not reliable; may
	// yield false negatives.
	//
	unsigned int cur_pos;
	mChannelp->getPosition(&cur_pos, FMOD_TIMEUNIT_PCMBYTES);
	if ((U32)cur_pos < mLastSamplePos)
	{
		mLoopedThisFrame = true;
	}
	mLastSamplePos = cur_pos;
}

void LLAudioChannelFMOD::cleanup()
{
	if (mChannelp)
	{
		LL_DEBUGS("FMOD") << "Cleaning-up channel " << std::hex << mChannelp
						  << std::dec << LL_ENDL;
		checkFMerr(mChannelp->stop(), "FMOD::Channel::stop");
		mCurrentBufferp = NULL;
		mChannelp = NULL;
	}
}

void LLAudioChannelFMOD::play()
{
	if (!mChannelp)
	{
		llwarns << "Playing without a channelID, aborting" << llendl;
		return;
	}

	checkFMerr(mChannelp->setPaused(false), "FMOD::Channel::pause");

	getSource()->setPlayedOnce(true);

	FMOD::ChannelGroup* group =
		LLAudioEngine_FMOD::sChannelGroups[getSource()->getType()];
	if (group)
	{
		mChannelp->setChannelGroup(group);
	}
}

void LLAudioChannelFMOD::playSynced(LLAudioChannel* channelp)
{
	LLAudioChannelFMOD* fmod_channelp =
		(LLAudioChannelFMOD*)channelp;
	if (!(fmod_channelp->mChannelp && mChannelp))
	{
		// No channel allocated to both the master and the slave
		return;
	}

	unsigned int cur_pos;
	if (checkFMerr(mChannelp->getPosition(&cur_pos, FMOD_TIMEUNIT_PCMBYTES),
				   "Unable to retrieve current position"))
	{
		return;
	}

	cur_pos %= mCurrentBufferp->getLength();

	// Try to match the position of our sync master
	checkFMerr(mChannelp->setPosition(cur_pos, FMOD_TIMEUNIT_PCMBYTES),
			   "Unable to set current position");

	// Start us playing
	play();
}

bool LLAudioChannelFMOD::isPlaying()
{
	if (!mChannelp)
	{
		return false;
	}

	bool paused, playing;
	mChannelp->getPaused(&paused);
	mChannelp->isPlaying(&playing);
	return !paused && playing;
}

void LLAudioChannelFMOD::set3DMode(bool use3d)
{
	FMOD_MODE current_mode;
	if (mChannelp->getMode(&current_mode) != FMOD_OK)
	{
		return;
	}
	FMOD_MODE new_mode = current_mode;
	new_mode &= ~(use3d ? FMOD_2D : FMOD_3D);
	new_mode |= use3d ? FMOD_3D : FMOD_2D;

	if (current_mode != new_mode)
	{
		mChannelp->setMode(new_mode);
	}
}

//
// LLAudioBufferFMOD implementation
//

LLAudioBufferFMOD::LLAudioBufferFMOD(FMOD::System* system)
:	mSystemp(system),
	mSoundp(NULL)
{
}

LLAudioBufferFMOD::~LLAudioBufferFMOD()
{
	if (mSoundp)
	{
		mSoundp->release();
		mSoundp = NULL;
	}
}

bool LLAudioBufferFMOD::loadWAV(const std::string& filename)
{
	// Try to open a wav file from disk. This will eventually go away, as we
	// do not really want to block doing this.
	if (filename.empty())
	{
		// Invalid filename, abort.
		return false;
	}

	if (!LLFile::isfile(filename))
	{
		// File not found, abort.
		return false;
	}

	if (mSoundp)
	{
		// If there is already something loaded in this buffer, clean it up.
		mSoundp->release();
		mSoundp = NULL;
	}

	FMOD_MODE base_mode = FMOD_LOOP_NORMAL;
	FMOD_CREATESOUNDEXINFO exinfo;
	memset(&exinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
	exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
	// Hint to speed up loading:
	exinfo.suggestedsoundtype = FMOD_SOUND_TYPE_WAV;

	// Load up the wav file into an fmod sample
	FMOD_RESULT result =
		getSystem()->createSound(filename.c_str(), base_mode, &exinfo,
								 &mSoundp);
	if (result != FMOD_OK)
	{
		// We failed to load the file for some reason.
		llwarns << "Could not load data '" << filename << "': "
				<< FMOD_ErrorString(result) << llendl;

		// If we EVER want to load wav files provided by end users, we need
		// to rethink this!
		//
		// File is probably corrupt: remove it.
		LLFile::remove(filename);
		return false;
	}

	// Everything went well, return true
	return true;
}

U32 LLAudioBufferFMOD::getLength()
{
	unsigned int length = 0;
	if (mSoundp)
	{
		mSoundp->getLength(&length, FMOD_TIMEUNIT_PCMBYTES);
	}
	return length;
}
