/**
 * @file audioengine_openal.cpp
 * @brief implementation of audio engine using OpenAL
 * support as a OpenAL 3D implementation
 *
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

#ifndef LL_AUDIOENGINE_OPENAL_H
#define LL_AUDIOENGINE_OPENAL_H

#include "llaudioengine.h"
#include "lllistener_openal.h"
#include "llwindgen.h"

class LLAudioEngine_OpenAL final : public LLAudioEngine
{
protected:
	LOG_CLASS(LLAudioEngine_OpenAL);

public:
	LLAudioEngine_OpenAL();

	bool init(void* user_data) override;
	std::string getDriverName(bool verbose) override;
	void allocateListener() override;

	void shutdown() override;

	void setInternalGain(F32 gain) override;

	LLAudioBuffer* createBuffer() override;
	LLAudioChannel* createChannel() override;

	bool initWind() override;
	void cleanupWind() override;
	void updateWind(LLVector3 direction, F32 camera_altitude) override;

private:
	void* windDSP(void *newbuffer, int length);

private:
	typedef S16 WIND_SAMPLE_T;
	LLWindGen<WIND_SAMPLE_T>*	mWindGen;
	S16*						mWindBuf;
	U32							mWindBufFreq;
	U32							mWindBufSamples;
	U32							mWindBufBytes;
	ALuint						mWindSource;
	int							mNumEmptyWindALBuffers;
};

class LLAudioChannelOpenAL final : public LLAudioChannel
{
protected:
	LOG_CLASS(LLAudioChannelOpenAL);

public:
	LLAudioChannelOpenAL();
	~LLAudioChannelOpenAL() override;

protected:
	void play() override;
	void playSynced(LLAudioChannel* channelp) override;
	void cleanup() override;
	bool isPlaying() override;

	bool updateBuffer() override;
	void update3DPosition() override;
	void updateLoop() override;

protected:
	ALuint mALSource;
	ALint mLastSamplePos;
};

class LLAudioBufferOpenAL final : public LLAudioBuffer
{
	friend class LLAudioChannelOpenAL;

protected:
	LOG_CLASS(LLAudioBufferOpenAL);

public:
	LLAudioBufferOpenAL();
	~LLAudioBufferOpenAL() override;

	bool loadWAV(const std::string& filename) override;
	U32 getLength() override;

protected:
	void cleanup();
	ALuint getBuffer()					{ return mALBuffer; }

protected:
	ALuint mALBuffer;
};

#endif
