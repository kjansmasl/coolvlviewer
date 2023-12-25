/**
 * @file llstreamingaudio_fmod.h
 * @author Tofu Linden
 * @brief Definition of LLStreamingAudio_FMOD implementation
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

#ifndef LL_STREAMINGAUDIO_FMOD_H
#define LL_STREAMINGAUDIO_FMOD_H

#include "stdtypes.h"

#include "llstreamingaudio.h"

// Stubs
class LLAudioStreamManagerFMOD;
namespace FMOD
{
	class System;
	class Channel;
	class ChannelGroup;
}

// Interfaces
class LLStreamingAudio_FMOD : public LLStreamingAudioInterface
{
protected:
	LOG_CLASS(LLStreamingAudio_FMOD);

public:
	LLStreamingAudio_FMOD(FMOD::System* system);
	~LLStreamingAudio_FMOD() override;

	bool supportsAdjustableBufferSizes() override		{ return true; }
	void setBufferSizes(U32 streambuffertime, U32 decodebuffertime) override;

	void start(const std::string& url) override;
	void stop() override;
	void pause(S32 pause) override;
	void update() override;
	S32 isPlaying() override;
	void setGain(F32 vol) override;
	F32 getGain() override								{ return mGain; }
	std::string getURL() override						{ return mURL; }

	bool newMetaData() override							{ return mNewMetaData; }
	void gotMetaData() override							{ mNewMetaData = false; }
	std::string getArtist() override					{ return mArtist; }
	std::string getTitle() override						{ return mTitle; }

private:
	bool releaseDeadStreams(bool force);

private:
	FMOD::System*							mSystem;
	FMOD::Channel*							mFMODInternetStreamChannelp;
	FMOD::ChannelGroup*						mStreamGroup;

	LLAudioStreamManagerFMOD*				mCurrentInternetStreamp;

	U32										mBufferMilliSeconds;

	F32										mGain;
	F32										mLastStarved;

	std::string								mURL;
	std::string								mArtist;
	std::string								mTitle;

	std::list<LLAudioStreamManagerFMOD*>	mDeadStreams;

	bool									mPendingStart;
	bool									mNewMetaData;
};

#endif // LL_STREAMINGAUDIO_FMOD_H
