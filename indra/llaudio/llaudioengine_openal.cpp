/**
 * @file audioengine_openal.cpp
 * @brief implementation of audio engine using OpenAL
 * support as a OpenAL 3D implementation
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

#include "linden_common.h"

#include "llaudioengine_openal.h"

#include "lllistener_openal.h"

constexpr S32 MAX_NUM_WIND_BUFFERS = 80;
constexpr F32 WIND_BUFFER_SIZE_SEC = 0.05f; // 1/20th sec

LLAudioEngine_OpenAL::LLAudioEngine_OpenAL()
:	mWindGen(NULL),
	mWindBuf(NULL),
	mWindBufFreq(0),
	mWindBufSamples(0),
	mWindBufBytes(0),
	mWindSource(AL_NONE),
	mNumEmptyWindALBuffers(MAX_NUM_WIND_BUFFERS)
{
}

//virtual
bool LLAudioEngine_OpenAL::init(void* userdata)
{
	mWindGen = NULL;
	mAudioDevice.clear();

	LLAudioEngine::init(userdata);

	if (!alutInit(NULL, NULL))
	{
		llwarns << "LLAudioEngine_OpenAL::init() ALUT initialization failed: "
				<< alutGetErrorString(alutGetError()) << llendl;
		return false;
	}

	llinfos << "OpenAL successfully initialized" << llendl;
	llinfos << "OpenAL version: " << ll_safe_string(alGetString(AL_VERSION))
			<< llendl;
	llinfos << "OpenAL vendor: " << ll_safe_string(alGetString(AL_VENDOR))
			<< llendl;
	llinfos << "OpenAL renderer: " << ll_safe_string(alGetString(AL_RENDERER))
			<< llendl;

	ALint major = alutGetMajorVersion ();
	ALint minor = alutGetMinorVersion ();
	llinfos << "ALUT version: " << major << "." << minor << llendl;

	ALCdevice* device = alcGetContextsDevice(alcGetCurrentContext());

	alcGetIntegerv(device, ALC_MAJOR_VERSION, 1, &major);
	alcGetIntegerv(device, ALC_MINOR_VERSION, 1, &minor);
	llinfos << "ALC version: " << major << "." << minor << llendl;

	mAudioDevice = ll_safe_string(alcGetString(device,
											   ALC_DEFAULT_DEVICE_SPECIFIER));
	llinfos << "ALC default device: " << mAudioDevice << llendl;

	return true;
}

//virtual
std::string LLAudioEngine_OpenAL::getDriverName(bool verbose)
{
	std::string result = "OpenAL";
	if (!verbose)
	{
		return result;
	}

	result += " v" + ll_safe_string(alGetString(AL_VERSION));
	result += " (" + ll_safe_string(alGetString(AL_RENDERER));
	if (!mAudioDevice.empty())
	{
		result += ": " + mAudioDevice;
	}
	result += ")";

	return result;
}

//virtual
void LLAudioEngine_OpenAL::allocateListener()
{
	mListenerp = (LLListener*)new LLListener_OpenAL();
	if (!mListenerp)
	{
		llwarns << "Listener creation failed" << llendl;
	}
}

//virtual
void LLAudioEngine_OpenAL::shutdown()
{
	llinfos << "Shutting down the audio engine..." << llendl;
	LLAudioEngine::shutdown();

	if (!alutExit())
	{
		llwarns << "ALUT shutdown failed: "
				<< alutGetErrorString(alutGetError()) << llendl;
	}

	llinfos << "OpenAL successfully shut down" << llendl;

	delete mListenerp;
	mListenerp = NULL;
}

LLAudioBuffer* LLAudioEngine_OpenAL::createBuffer()
{
	return new LLAudioBufferOpenAL();
}

LLAudioChannel* LLAudioEngine_OpenAL::createChannel()
{
	return new LLAudioChannelOpenAL();
}

void LLAudioEngine_OpenAL::setInternalGain(F32 gain)
{
	alListenerf(AL_GAIN, gain);
}

LLAudioChannelOpenAL::LLAudioChannelOpenAL()
:	mALSource(AL_NONE),
	mLastSamplePos(0)
{
	alGenSources(1, &mALSource);
}

LLAudioChannelOpenAL::~LLAudioChannelOpenAL()
{
	cleanup();
	alDeleteSources(1, &mALSource);
}

void LLAudioChannelOpenAL::cleanup()
{
	alSourceStop(mALSource);
	alSourcei(mALSource, AL_BUFFER, 0);
	mCurrentBufferp = NULL;
}

void LLAudioChannelOpenAL::play()
{
	if (mALSource == AL_NONE)
	{
		llwarns << "Playing without a mALSource, aborting" << llendl;
		return;
	}

	if (!isPlaying())
	{
		alSourcePlay(mALSource);
		getSource()->setPlayedOnce(true);
	}
}

void LLAudioChannelOpenAL::playSynced(LLAudioChannel* channelp)
{
	if (channelp)
	{
		LLAudioChannelOpenAL *masterchannelp = (LLAudioChannelOpenAL*)channelp;
		if (mALSource != AL_NONE && masterchannelp->mALSource != AL_NONE)
		{
			// we have channels allocated to master and slave
			ALfloat master_offset;
			alGetSourcef(masterchannelp->mALSource, AL_SEC_OFFSET,
						 &master_offset);

			llinfos << "Syncing with master at " << master_offset << "s"
					<< llendl;
			// *TODO: detect when this fails, maybe use AL_SAMPLE_
			alSourcef(mALSource, AL_SEC_OFFSET, master_offset);
		}
	}
	play();
}

bool LLAudioChannelOpenAL::isPlaying()
{
	if (mALSource != AL_NONE)
	{
		ALint state;
		alGetSourcei(mALSource, AL_SOURCE_STATE, &state);
		if (state == AL_PLAYING)
		{
			return true;
		}
	}

	return false;
}

bool LLAudioChannelOpenAL::updateBuffer()
{
	if (LLAudioChannel::updateBuffer())
	{
		// Base class update returned true, which means that we need to
		// actually set up the source for a different buffer.
		LLAudioBufferOpenAL* bufferp;
		bufferp = (LLAudioBufferOpenAL*)mCurrentSourcep->getCurrentBuffer();
		if (!bufferp)
		{
			llwarns << "No current buffer !" << llendl;
			return false;
		}
		ALuint buffer = bufferp->getBuffer();
		alSourcei(mALSource, AL_BUFFER, buffer);
		mLastSamplePos = 0;
	}

	if (mCurrentSourcep && gAudiop && gAudiop->mListenerp)
	{
		alSourcef(mALSource, AL_GAIN,
				  mCurrentSourcep->getGain() * getSecondaryGain());
		alSourcei(mALSource, AL_LOOPING,
				  mCurrentSourcep->isLoop() ? AL_TRUE : AL_FALSE);
		alSourcef(mALSource, AL_ROLLOFF_FACTOR,
				  gAudiop->mListenerp->getRolloffFactor());
	}

	return true;
}

void LLAudioChannelOpenAL::updateLoop()
{
	if (mALSource == AL_NONE)
	{
		return;
	}

	// Hack: we keep track of whether we looped or not by seeing when the
	// sample position looks like it's going backwards.  Not reliable; may
	// yield false negatives.
	ALint cur_pos;
	alGetSourcei(mALSource, AL_SAMPLE_OFFSET, &cur_pos);
	if (cur_pos < mLastSamplePos)
	{
		mLoopedThisFrame = true;
	}
	mLastSamplePos = cur_pos;
}

void LLAudioChannelOpenAL::update3DPosition()
{
	if (!mCurrentSourcep)
	{
		return;
	}
	if (mCurrentSourcep->isAmbient())
	{
		alSource3f(mALSource, AL_POSITION, 0.0, 0.0, 0.0);
		alSource3f(mALSource, AL_VELOCITY, 0.0, 0.0, 0.0);
		alSourcei(mALSource, AL_SOURCE_RELATIVE, AL_TRUE);
	}
	else
	{
		LLVector3 float_pos(mCurrentSourcep->getPositionGlobal());
		alSourcefv(mALSource, AL_POSITION, float_pos.mV);
		alSourcefv(mALSource, AL_VELOCITY, mCurrentSourcep->getVelocity().mV);
		alSourcei(mALSource, AL_SOURCE_RELATIVE, AL_FALSE);
	}

	alSourcef(mALSource, AL_GAIN,
			  mCurrentSourcep->getGain() * getSecondaryGain());
}

LLAudioBufferOpenAL::LLAudioBufferOpenAL()
{
	mALBuffer = AL_NONE;
}

LLAudioBufferOpenAL::~LLAudioBufferOpenAL()
{
	cleanup();
}

void LLAudioBufferOpenAL::cleanup()
{
	if (mALBuffer != AL_NONE)
	{
		alGetError();
		alDeleteBuffers(1, &mALBuffer);
		ALenum error = alGetError();
		if (error != AL_NO_ERROR)
		{
			llwarns << "Error: " << error << " possible memory leak hit"
					<< llendl;
		}
		mALBuffer = AL_NONE;
	}
}

bool LLAudioBufferOpenAL::loadWAV(const std::string& filename)
{
	if (filename.empty())
	{
		// invalid filename, abort.
		return false;
	}

	cleanup();
	mALBuffer = alutCreateBufferFromFile(filename.c_str());
	if (mALBuffer == AL_NONE)
	{
		ALenum error = alutGetError();
		if (LLFile::isfile(filename))
		{
			llwarns << "Error loading: " << filename << " - "
					<< alutGetErrorString(error) << llendl;

			// If we EVER want to load wav files provided by end users, we need
			// to rethink this !  File is probably corrupt - remove it.
			LLFile::remove(filename);
		}
		else
		{
			// It's common for the file to not actually exist.
			LL_DEBUGS("OpenAL") << "Error loading: " << filename
								<< " - " << alutGetErrorString(error)
								<< LL_ENDL;
		}
		return false;
	}

	return true;
}

U32 LLAudioBufferOpenAL::getLength()
{
	if (mALBuffer == AL_NONE)
	{
		return 0;
	}
	ALint length;
	alGetBufferi(mALBuffer, AL_SIZE, &length);
	return length / 2; // convert size in bytes to size in (16-bit) samples
}

bool LLAudioEngine_OpenAL::initWind()
{
	ALenum error;

	mNumEmptyWindALBuffers = MAX_NUM_WIND_BUFFERS;

	alGetError(); /* clear error */

	alGenSources(1,&mWindSource);

	if ((error = alGetError()) != AL_NO_ERROR)
	{
		llwarns << "Error creating wind sources: " << error << llendl;
	}

	mWindGen = new LLWindGen<WIND_SAMPLE_T>;

	mWindBufFreq = mWindGen->getInputSamplingRate();
	mWindBufSamples = llceil(mWindBufFreq * WIND_BUFFER_SIZE_SEC);
	mWindBufBytes = mWindBufSamples * 2 /*stereo*/ * sizeof(WIND_SAMPLE_T);

	mWindBuf = new WIND_SAMPLE_T[mWindBufSamples * 2 /*stereo*/];
	if (!mWindBuf)
	{
		llwarns << "Error creating wind memory buffer" << llendl;
		llassert(false);
		return false;
	}

	return true;
}

void LLAudioEngine_OpenAL::cleanupWind()
{
	if (mWindSource != AL_NONE)
	{
		// Detach and delete all outstanding buffers on the wind source
		alSourceStop(mWindSource);
		ALint processed;
		alGetSourcei(mWindSource, AL_BUFFERS_PROCESSED, &processed);
		while (processed--)
		{
			ALuint buffer = AL_NONE;
			alSourceUnqueueBuffers(mWindSource, 1, &buffer);
			alDeleteBuffers(1, &buffer);
		}

		// Delete the wind source itself
		alDeleteSources(1, &mWindSource);

		mWindSource = AL_NONE;
	}

	delete[] mWindBuf;
	mWindBuf = NULL;

	delete mWindGen;
	mWindGen = NULL;
}

void LLAudioEngine_OpenAL::updateWind(LLVector3 wind_vec, F32 camera_altitude)
{
	if (!mEnableWind || !mWindBuf)
	{
		return;
	}

	if (mWindUpdateTimer.checkExpirationAndReset(LL_WIND_UPDATE_INTERVAL))
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

		alSourcei(mWindSource, AL_LOOPING, AL_FALSE);
		alSource3f(mWindSource, AL_POSITION, 0.f, 0.f, 0.f);
		alSource3f(mWindSource, AL_VELOCITY, 0.f, 0.f, 0.f);
		alSourcef(mWindSource, AL_ROLLOFF_FACTOR, 0.f);
		alSourcei(mWindSource, AL_SOURCE_RELATIVE, AL_TRUE);
	}

	// Let's make a wind buffer now

	ALint processed, queued, unprocessed;
	alGetSourcei(mWindSource, AL_BUFFERS_PROCESSED, &processed);
	alGetSourcei(mWindSource, AL_BUFFERS_QUEUED, &queued);
	unprocessed = queued - processed;

	// Ensure that there are always at least 3x as many filled buffers queued
	// as we managed to empty since last time.
	mNumEmptyWindALBuffers =
		llmin(mNumEmptyWindALBuffers + processed * 3 - unprocessed,
			  MAX_NUM_WIND_BUFFERS - unprocessed);
	mNumEmptyWindALBuffers = llmax(mNumEmptyWindALBuffers, 0);

	while (processed--) // unqueue old buffers
	{
		ALuint buffer;
		ALenum error;
		alGetError(); /* clear error */
		alSourceUnqueueBuffers(mWindSource, 1, &buffer);
		error = alGetError();
		if (error != AL_NO_ERROR)
		{
			llwarns << "Error swapping (unqueuing) buffers" << llendl;
		}
		else
		{
			alDeleteBuffers(1, &buffer);
		}
	}

	unprocessed += mNumEmptyWindALBuffers;
	while (mNumEmptyWindALBuffers > 0) // fill+queue new buffers
	{
		alGetError(); /* clear error */

		ALuint buffer;
		alGenBuffers(1,&buffer);
		ALenum error = alGetError();
		if (error != AL_NO_ERROR)
		{
			llwarns << "Error creating wind buffer: " << error << llendl;
			break;
		}

		alBufferData(buffer, AL_FORMAT_STEREO16,
					 mWindGen->windGenerate(mWindBuf, mWindBufSamples),
					 mWindBufBytes, mWindBufFreq);
		error = alGetError();
		if (error != AL_NO_ERROR)
		{
			llwarns << "Error swapping (bufferdata) buffers" << llendl;
		}

		alSourceQueueBuffers(mWindSource, 1, &buffer);
		error = alGetError();
		if (error != AL_NO_ERROR)
		{
			llwarns << "Error swapping (queuing) buffers" << llendl;
		}

		--mNumEmptyWindALBuffers;
	}

	ALint playing;
	alGetSourcei(mWindSource, AL_SOURCE_STATE, &playing);
	if (playing != AL_PLAYING)
	{
		alSourcePlay(mWindSource);

		LL_DEBUGS("OpenAL") << "Wind had stopped (probably ran out of buffers) restarting: "
							<< (unprocessed + mNumEmptyWindALBuffers)
							<< " now queued." << LL_ENDL;
	}
}
