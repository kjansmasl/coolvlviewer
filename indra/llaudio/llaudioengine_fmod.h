/**
 * @file llaudioengine_fmod.h
 * @brief Definition of LLAudioEngine class abstracting the audio support
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

#ifndef LL_AUDIOENGINE_FMOD_H
#define LL_AUDIOENGINE_FMOD_H

#include "llaudioengine.h"
#include "llwindgen.h"

// Stubs
class LLAudioStreamManagerFMOD;
namespace FMOD
{
	class System;
	class Channel;
	class ChannelGroup;
	class Sound;
	class DSP;
}

typedef struct FMOD_DSP_DESCRIPTION FMOD_DSP_DESCRIPTION;

// Interfaces
class LLAudioEngine_FMOD final : public LLAudioEngine
{
protected:
	LOG_CLASS(LLAudioEngine_FMOD);

public:
	LLAudioEngine_FMOD(bool enable_profiler);
	~LLAudioEngine_FMOD() override;

	// Initialization/startup/shutdown
	bool init(void* user_data) override;
	std::string getDriverName(bool verbose) override;
	void allocateListener() override;

	void shutdown() override;

	bool initWind() override;
	void cleanupWind() override;

	void updateWind(LLVector3 direction, F32 cam_height_above_water) override;

	FMOD::System* getSystem() const				{ return mSystem; }

	typedef F32 MIXBUFFERFORMAT;

protected:
	// Get a free buffer, or flush an existing one if you have to.
	LLAudioBuffer* createBuffer() override;

	// Create a new audio channel.
	LLAudioChannel* createChannel() override;

	void setInternalGain(F32 gain) override;

protected:
	FMOD::System*				mSystem;
	LLWindGen<MIXBUFFERFORMAT>*	mWindGen;
	FMOD_DSP_DESCRIPTION*		mWindDSPDesc;
	FMOD::DSP*					mWindDSP;
	bool						mInited;
	bool						mEnableProfiler;

public:
	static FMOD::ChannelGroup*	sChannelGroups[LLAudioEngine::AUDIO_TYPE_COUNT];
#if LL_LINUX
	static bool					sNoALSA;
	static bool					sNoPulseAudio;
#endif
};

class LLAudioChannelFMOD final : public LLAudioChannel
{
protected:
	LOG_CLASS(LLAudioChannelFMOD);

public:
	LLAudioChannelFMOD(FMOD::System* system);
	~LLAudioChannelFMOD() override;

protected:
	void play() override;
	void playSynced(LLAudioChannel* channelp) override;
	void cleanup() override;
	bool isPlaying() override;

	bool updateBuffer() override;
	void update3DPosition() override;
	void updateLoop() override;

	void set3DMode(bool use3d);

protected:
	FMOD::System* getSystem() const					{ return mSystemp; }

protected:
	FMOD::System*	mSystemp;
	FMOD::Channel*	mChannelp;
	U32				mLastSamplePos;
};

class LLAudioBufferFMOD final : public LLAudioBuffer
{
	friend class LLAudioChannelFMOD;

protected:
	LOG_CLASS(LLAudioBufferFMOD);

public:
	LLAudioBufferFMOD(FMOD::System* system);
	~LLAudioBufferFMOD() override;

	bool loadWAV(const std::string& filename) override;
	U32 getLength() override;

protected:
	FMOD::System* getSystem() const					{ return mSystemp; }
	FMOD::Sound* getSound() const					{ return mSoundp; }

protected:
	FMOD::System*	mSystemp;
	FMOD::Sound*	mSoundp;
};

bool checkFMerr(S32 result, const char* str);

#endif // LL_AUDIOENGINE_FMOD_H
