 /**
 * @file llaudioengine.cpp
 * @brief implementation of LLAudioEngine class abstracting the Open
 * AL audio support
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#include "llaudioengine.h"

#include "llaudiodecodemgr.h"
#include "llassetstorage.h"
#include "lldir.h"
#include "llfilesystem.h"
#include "llmath.h"
#include "llstreamingaudio.h"

// Necessary for grabbing sounds from sim (implemented in viewer)
extern void request_sound(const LLUUID& sound_guid);

LLAudioEngine* gAudiop = NULL;

//static
uuid_list_t LLAudioData::sBlockedSounds;

//
// LLAudioEngine implementation
//

LLAudioEngine::LLAudioEngine()
:	mSourcesUpdated(false),
	mListenerp(NULL)
{
	setDefaults();
}

LLStreamingAudioInterface* LLAudioEngine::getStreamingAudioImpl()
{
	return mStreamingAudioImpl;
}

void LLAudioEngine::setStreamingAudioImpl(LLStreamingAudioInterface* impl)
{
	mStreamingAudioImpl = impl;
}

void LLAudioEngine::setDefaults()
{
	mMaxWindGain = 1.f;

	if (mListenerp)
	{
		delete mListenerp;
		mListenerp = NULL;
	}

	mMuted = false;
	mUserData = NULL;

	mLastStatus = 0;

	mEnableWind = false;

	for (S32 i = 0; i < MAX_AUDIO_CHANNELS; ++i)
	{
		mChannels[i] = NULL;
	}
	for (S32 i = 0; i < MAX_AUDIO_BUFFERS; ++i)
	{
		mBuffers[i] = NULL;
	}

	mMasterGain = 1.f;
	// Setting mInternalGain to an out of range value fixes the issue reported
	// in STORM-830. There is an edge case in setMasterGain during startup
	// which prevents setInternalGain from being called if the master volume
	// setting and mInternalGain both equal 0, so using -1 forces the if
	// statement in setMasterGain to execute when the viewer starts up.
	mInternalGain = -1.f;
	mNextWindUpdate = 0.f;

	mStreamingAudioImpl = NULL;

	for (U32 i = 0; i < AUDIO_TYPE_COUNT; ++i)
	{
		mSecondaryGain[i] = 1.0f;
	}
}

//virtual
bool LLAudioEngine::init(void* userdata)
{
	setDefaults();

	mUserData = userdata;

	allocateListener();

	if (!gAudioDecodeMgrp)
	{
		// Initialize the decode manager
		gAudioDecodeMgrp = new LLAudioDecodeMgr;
	}

	llinfos << "Audio engine successfully created with " << MAX_AUDIO_CHANNELS
			<< " channels." << llendl;

	return true;
}

//virtual
void LLAudioEngine::shutdown()
{
	// Clean up decode manager
	delete gAudioDecodeMgrp;
	gAudioDecodeMgrp = NULL;

	// Clean up wind source
	cleanupWind();

	// Clean up audio sources
	for (source_map_t::iterator it = mAllSources.begin(),
								end = mAllSources.end();
		 it != end; ++it)
	{
		delete it->second;
	}

	// Clean up audio data
	for (data_map_t::iterator it = mAllData.begin(), end = mAllData.end();
		 it != end; ++it)
	{
		delete it->second;
	}

	// Clean up channels
	for (S32 i = 0; i < MAX_AUDIO_CHANNELS; ++i)
	{
		delete mChannels[i];
		mChannels[i] = NULL;
	}

	for (S32 i = 0; i < MAX_AUDIO_BUFFERS; ++i)
	{
		delete mBuffers[i];
		mBuffers[i] = NULL;
	}

	mSourcesUpdated = true;
}

//virtual
void LLAudioEngine::startInternetStream(const std::string& url)
{
	if (mStreamingAudioImpl)
	{
		mStreamingAudioImpl->start(url);
	}
}

//virtual
void LLAudioEngine::stopInternetStream()
{
	if (mStreamingAudioImpl)
	{
		mStreamingAudioImpl->stop();
	}
}

//virtual
void LLAudioEngine::pauseInternetStream(S32 pause)
{
	if (mStreamingAudioImpl)
	{
		mStreamingAudioImpl->pause(pause);
	}
}

//virtual
void LLAudioEngine::updateInternetStream()
{
	if (mStreamingAudioImpl)
	{
		mStreamingAudioImpl->update();
	}
}

//virtual
int LLAudioEngine::isInternetStreamPlaying()
{
	return mStreamingAudioImpl ? mStreamingAudioImpl->isPlaying() : 0;
}

//virtual
void LLAudioEngine::setInternetStreamGain(F32 vol)
{
	if (mStreamingAudioImpl)
	{
		mStreamingAudioImpl->setGain(vol);
	}
}

//virtual
std::string LLAudioEngine::getInternetStreamURL()
{
	return mStreamingAudioImpl ? mStreamingAudioImpl->getURL() : std::string();
}

//virtual
void LLAudioEngine::updateChannels()
{
	for (S32 i = 0; i < MAX_AUDIO_CHANNELS; ++i)
	{
		if (mChannels[i])
		{
			mChannels[i]->updateBuffer();
			mChannels[i]->update3DPosition();
			mChannels[i]->updateLoop();
		}
	}
}

//virtual
void LLAudioEngine::idle()
{
	// "Update" all of our audio sources, clean up dead ones. Primarily does
	// position updating, cleanup of unused audio sources. Also does
	// regeneration of the current priority of each audio source.

	for (S32 i = 0; i < MAX_AUDIO_BUFFERS; ++i)
	{
		if (mBuffers[i])
		{
			mBuffers[i]->mInUse = false;
		}
	}

	// Maximum priority source without a channel
	LLAudioSource* max_sourcep = NULL;
	F32 max_priority = -1.f;
	for (source_map_t::iterator iter = mAllSources.begin(),
								end = mAllSources.end();
		 iter != end; )
	{
		source_map_t::iterator curiter = iter++;
		LLAudioSource* sourcep = curiter->second;
		if (!sourcep)	// Paranoia
		{
			llwarns << "NULL source found, removing." << llendl;
			mAllSources.hmap_erase(curiter);
			mSourcesUpdated = true;
			continue;
		}

		// Update this source
		sourcep->update();
		sourcep->updatePriority();

		if (sourcep->isDone())
		{
			// The source is done playing, clean it up.
			delete sourcep;
			mAllSources.hmap_erase(curiter);
			mSourcesUpdated = true;
			continue;
		}

		if (!sourcep->isMuted() && !sourcep->getChannel() &&
			sourcep->getCurrentBuffer())
		{
			// We could potentially play this sound if its priority is high
			// enough.
			if (sourcep->getPriority() > max_priority)
			{
				max_priority = sourcep->getPriority();
				max_sourcep = sourcep;
			}
		}
	}

	// Now, do priority-based organization of audio sources. All channels used,
	// check priorities. Find channel with lowest priority.
	if (max_sourcep)
	{
		LLAudioChannel* channelp = getFreeChannel(max_priority);
		if (channelp)
		{
			max_sourcep->setChannel(channelp);
			channelp->setSource(max_sourcep);
			if (max_sourcep->isSyncSlave())
			{
				// A sync slave, it does not start playing until it is synced
				// up with the master. Flag this channel as waiting for sync
				// and return true.
				channelp->setWaiting(true);
			}
			else
			{
				channelp->setWaiting(false);
				if (channelp->mCurrentBufferp)
				{
					channelp->play();
				}
			}
		}
	}

	// Do this BEFORE we update the channels. Update the channels to sync up
	// with any changes that the source made, such as changing what sound was
	// playing.
	updateChannels();

	// Update queued sounds (switch to next queued data if the current has
	// finished playing)
	for (source_map_t::iterator iter = mAllSources.begin(),
								end = mAllSources.end();
		 iter != end; ++iter)
	{
		// This is lame, instead of this I could actually iterate through all
		// the sources attached to each channel, since only those with active
		// channels can have anything interesting happen with their queue ?
		// (Maybe not true)
		LLAudioSource* sourcep = iter->second;
		if (!sourcep || !sourcep->mQueuedDatap || sourcep->isMuted())
		{
			// Muted, or nothing queued, so we do not care.
			continue;
		}

		LLAudioChannel* channelp = sourcep->getChannel();
		if (!channelp)
		{
			// This sound is not playing, so we just process move the queue
			sourcep->mCurrentDatap = sourcep->mQueuedDatap;
			sourcep->mQueuedDatap = NULL;

			// Reset the timer so the source does not die.
			sourcep->mAgeTimer.reset();
			// Make sure we have the buffer set up if we just decoded the data
			if (sourcep->mCurrentDatap)
			{
				updateBufferForData(sourcep->mCurrentDatap);
			}

			// Actually play the associated data.
			sourcep->setupChannel();
			channelp = sourcep->getChannel();
			if (channelp)
			{
				channelp->updateBuffer();
				if (channelp->mCurrentBufferp)
				{
					channelp->play();
				}
			}
			continue;
		}
		else
		{
			// Check to see if the current sound is done playing, or looped.
			if (!channelp->isPlaying())
			{
				sourcep->mCurrentDatap = sourcep->mQueuedDatap;
				sourcep->mQueuedDatap = NULL;

				// Reset the timer so the source does not die.
				sourcep->mAgeTimer.reset();

				// Make sure we have the buffer set up if we just decoded the data
				if (sourcep->mCurrentDatap)
				{
					updateBufferForData(sourcep->mCurrentDatap);
				}

				// Actually play the associated data.
				sourcep->setupChannel();
				channelp->updateBuffer();
				if (channelp->mCurrentBufferp)
				{
					channelp->play();
				}
			}
			else if (sourcep->isLoop())
			{
				// If it is a loop, we need to check and see if we are done
				// with it.
				if (channelp->mLoopedThisFrame)
				{
					sourcep->mCurrentDatap = sourcep->mQueuedDatap;
					sourcep->mQueuedDatap = NULL;

					// Actually, should do a time sync so if we are a loop
					// master/slave we do not drift away.
					sourcep->setupChannel();
					channelp = sourcep->getChannel();
					if (channelp && channelp->mCurrentBufferp)
					{
						channelp->play();
					}
				}
			}
		}
	}

	// Lame, update the channels AGAIN.
	// Update the channels to sync up with any changes that the source made,
	// such as changing what sound was playing.
	updateChannels();

	// *HACK: for now, just use a global sync master;
	LLAudioSource* sync_masterp = NULL;
	LLAudioChannel* master_channelp = NULL;
	F32 max_sm_priority = -1.f;
	for (source_map_t::iterator iter = mAllSources.begin(),
								end = mAllSources.end();
		 iter != end; ++iter)
	{
		LLAudioSource* sourcep = iter->second;
		if (sourcep->isMuted())
		{
			continue;
		}
		if (sourcep->isSyncMaster())
		{
			if (sourcep->getPriority() > max_sm_priority)
			{
				sync_masterp = sourcep;
				master_channelp = sync_masterp->getChannel();
				max_sm_priority = sourcep->getPriority();
			}
		}
	}

	if (master_channelp && master_channelp->mLoopedThisFrame)
	{
		// Synchronize loop slaves with their masters. Update queued sounds
		// (switch to next queued data if the current has finished playing).
		for (source_map_t::iterator iter = mAllSources.begin(),
									end = mAllSources.end();
			 iter != end; ++iter)
		{
			LLAudioSource* sourcep = iter->second;

			if (!sourcep->isSyncSlave())
			{
				// Not a loop slave, we do not need to do anything
				continue;
			}

			LLAudioChannel* channelp = sourcep->getChannel();
			if (!channelp)
			{
				// Not playing, do not need to bother.
				continue;
			}

			if (!channelp->isPlaying())
			{
				// Now we need to check if our loop master has just looped, and
				// start playback if that is the case.
				if (sync_masterp->getChannel())
				{
					channelp->playSynced(master_channelp);
					channelp->setWaiting(false);
				}
			}
		}
	}

	// Sync up everything that the audio engine needs done.
	commitDeferredChanges();

	// Flush unused buffers that are stale enough
	for (S32 i = 0; i < MAX_AUDIO_BUFFERS; ++i)
	{
		LLAudioBuffer* buffer = mBuffers[i];
		if (buffer && !buffer->mInUse &&
			buffer->mLastUseTimer.getElapsedTimeF32() > 30.f)
		{
			LL_DEBUGS("Audio") << "Flushing unused buffer #" << i << LL_ENDL;
			if (buffer->mAudioDatap)	// Paranoia
			{
				buffer->mAudioDatap->mBufferp = NULL;
			}
			delete buffer;
			mBuffers[i] = NULL;
		}
	}

	// Clear all of the looped flags for the channels
	for (S32 i = 0; i < MAX_AUDIO_CHANNELS; ++i)
	{
		LLAudioChannel* channel = mChannels[i];
		if (channel)
		{
			channel->mLoopedThisFrame = false;
		}
	}

	// Decode audio files
	if (gAudioDecodeMgrp)
	{
		gAudioDecodeMgrp->processQueue();
	}

	// Call this every frame, just in case we somehow missed picking it up in
	// all the places that can add or request new data.
	startNextTransfer();

	updateInternetStream();
}

bool LLAudioEngine::updateBufferForData(LLAudioData* adp,
										const LLUUID& audio_id)
{
	if (!adp)
	{
		LL_DEBUGS("Audio") << "No audio data; cannot process " << audio_id
						   << LL_ENDL;
		return false;
	}

	// Update the audio buffer first: load a sound if we have it. Note that
	// this could potentially cause us to waste time updating buffers for
	// sounds that actually are not playing, although this should be mitigated
	// by the fact that we limit the number of buffers, and we flush buffers
	// based on priority.
	if (adp->getBuffer())
	{
		LL_DEBUGS("Audio") << "A buffer already exists for " << audio_id
						   << LL_ENDL;
		return true;
	}
	if (adp->hasDecodedData())
	{
		LL_DEBUGS("Audio") << "Loading audio data for " << audio_id
						   << LL_ENDL;
		return adp->load();
	}

	return adp->hasLocalData() && audio_id.notNull() && gAudioDecodeMgrp &&
		   gAudioDecodeMgrp->addDecodeRequest(audio_id);
}

//virtual
void LLAudioEngine::enableWind(bool enable)
{
	if (enable && !mEnableWind)
	{
		mEnableWind = initWind();
		if (mEnableWind)
		{
			llinfos << "Wind audio enabled." << llendl;
		}
	}
	else if (mEnableWind && !enable)
	{
		mEnableWind = false;
		cleanupWind();
		llinfos << "Wind audio disabled." << llendl;
	}
}

LLAudioBuffer* LLAudioEngine::getFreeBuffer()
{
	for (S32 i = 0; i < MAX_AUDIO_BUFFERS; ++i)
	{
		if (!mBuffers[i])
		{
			mBuffers[i] = createBuffer();
			return mBuffers[i];
		}
	}

	// Grab the oldest unused buffer
	F32 max_age = -1.f;
	S32 buffer_id = -1;
	for (S32 i = 0; i < MAX_AUDIO_BUFFERS; ++i)
	{
		LLAudioBuffer* buffer = mBuffers[i];
		if (buffer && !buffer->mInUse &&
			buffer->mLastUseTimer.getElapsedTimeF32() > max_age)
		{
			max_age = buffer->mLastUseTimer.getElapsedTimeF32();
			buffer_id = i;
		}
	}

	if (buffer_id >= 0)
	{
		// Do not spam us with such messages...
		llinfos_once << "Taking over unused buffer " << buffer_id << llendl;

		LLAudioBuffer* buffer = mBuffers[buffer_id];
		if (buffer->mAudioDatap)	// Paranoia
		{
			buffer->mAudioDatap->mBufferp = NULL;
		}
		delete buffer;
		buffer = createBuffer();
		mBuffers[buffer_id] = buffer;
		return buffer;
	}
	return NULL;
}

LLAudioChannel* LLAudioEngine::getFreeChannel(F32 priority)
{
	for (S32 i = 0; i < MAX_AUDIO_CHANNELS; ++i)
	{
		if (mChannels[i])
		{
			// Channel is allocated but not playing right now, use it.
			if (!mChannels[i]->isPlaying() && !mChannels[i]->isWaiting())
			{
				mChannels[i]->cleanup();
				if (mChannels[i]->getSource())
				{
					mChannels[i]->getSource()->setChannel(NULL);
				}
				return mChannels[i];
			}
		}
		else
		{
			// No channel allocated here, use it.
			mChannels[i] = createChannel();
			return mChannels[i];
		}
	}

	// All channels used, check priorities.
	// Find channel with lowest priority and see if we want to replace it.
	F32 min_priority = 10000.f;
	LLAudioChannel* min_channelp = NULL;

	for (S32 i = 0; i < MAX_AUDIO_CHANNELS; ++i)
	{
		LLAudioChannel* channelp = mChannels[i];
		LLAudioSource* sourcep = channelp->getSource();
		if (sourcep && sourcep->getPriority() < min_priority)
		{
			min_channelp = channelp;
			min_priority = sourcep->getPriority();
		}
	}

	if (min_priority > priority || !min_channelp)
	{
		// All playing channels have higher priority, return.
		return NULL;
	}

	// Flush the minimum priority channel, and return it.
	min_channelp->cleanup();
	min_channelp->getSource()->setChannel(NULL);
	return min_channelp;
}

void LLAudioEngine::cleanupBuffer(LLAudioBuffer* bufferp)
{
	for (S32 i = 0; i < MAX_AUDIO_BUFFERS; ++i)
	{
		if (mBuffers[i] == bufferp)
		{
			delete mBuffers[i];
			mBuffers[i] = NULL;
		}
	}
}

bool LLAudioEngine::preloadSound(const LLUUID& id)
{
	// We do not care about the return value, this is just to make sure that we
	// have an entry, which will mean that the audio engine knows about this:
	getAudioData(id);

	if (gAudioDecodeMgrp && gAudioDecodeMgrp->addDecodeRequest(id))
	{
		// This means that we do have a local copy, and we are working on
		// decoding it.
		return true;
	}

	LL_DEBUGS("Audio") << "Used internal preload for non-local sound"
					   << LL_ENDL;
	return false;
}

//virtual
bool LLAudioEngine::isWindEnabled()
{
	return mEnableWind;
}

void LLAudioEngine::setMuted(bool muted)
{
	if (muted != mMuted)
	{
		mMuted = muted;
		setMasterGain(mMasterGain);
	}
}

void LLAudioEngine::setMasterGain(F32 gain)
{
	mMasterGain = gain;
	F32 internal_gain = getMuted() ? 0.f : gain;
	if (internal_gain != mInternalGain)
	{
		mInternalGain = internal_gain;
		setInternalGain(mInternalGain);
	}
}

F32 LLAudioEngine::getMasterGain()
{
	return mMasterGain;
}

void LLAudioEngine::setSecondaryGain(S32 type, F32 gain)
{
	llassert(type < LLAudioEngine::AUDIO_TYPE_COUNT);

	mSecondaryGain[type] = gain;
}

F32 LLAudioEngine::getSecondaryGain(S32 type)
{
	return mSecondaryGain[type];
}

F32 LLAudioEngine::getInternetStreamGain()
{
	return mStreamingAudioImpl ? mStreamingAudioImpl->getGain() : 1.f;
}

//virtual
void LLAudioEngine::setMaxWindGain(F32 gain)
{
	mMaxWindGain = gain;
}

F32 LLAudioEngine::mapWindVecToGain(LLVector3 wind_vec)
{
	F32 gain = wind_vec.length();
	if (gain)
	{
		if (gain > 20.f)
		{
			gain = 20.f;
		}
		gain *= 0.05f; // (1/20)
	}

	return gain;
}

F32 LLAudioEngine::mapWindVecToPitch(LLVector3 wind_vec)
{
	// Wind frame is in listener-relative coordinates
	LLVector3 norm_wind = wind_vec;
	norm_wind.normalize();

	LLVector3 listen_right(1.f, 0.f, 0.f);

	// Measure angle between wind vec and listener right axis (on 0,PI)
	F32 theta = acosf(norm_wind * listen_right);

	// Put it on 0, 1
	theta *= 1.f / F_PI;

	// Put it on [0, 0.5, 0]
	if (theta > 0.5f)
	{
		theta = 1.f - theta;
	}
	else if (theta < 0.f)
	{
		theta = 0.f;
	}

	return theta;
}

F32 LLAudioEngine::mapWindVecToPan(LLVector3 wind_vec)
{
	// Wind frame is in listener-relative coordinates
	LLVector3 norm_wind = wind_vec;
	norm_wind.normalize();

	LLVector3 listen_right(1.f, 0.f, 0.f);

	// Measure angle between wind vec and listener right axis (on [0, PI])
	F32 theta = acosf(norm_wind * listen_right);

	// Put it on 0, 1
	theta *= 1.f / F_PI;

	return theta;
}

void LLAudioEngine::triggerSound(const LLUUID& audio_id,
								 const LLUUID& owner_id,
								 F32 gain, S32 type,
								 const LLVector3d& pos_global)
{
	if (type == AUDIO_TYPE_UI)
	{
		mUISounds.emplace(audio_id);
	}

	if (mMuted || gain < 0.0001f)
	{
		return;
	}

	// Create a new source (since this cannot be associated with an existing
	// source.
	LLUUID source_id;
	source_id.generate();

	LLAudioSource* asp = new LLAudioSource(source_id, owner_id, gain, type);
	addAudioSource(asp);
	if (pos_global.isExactlyZero())
	{
		asp->setAmbient(true);
	}
	else
	{
		asp->setPositionGlobal(pos_global);
	}
	asp->updatePriority();
	asp->play(audio_id);
}

//virtual
void LLAudioEngine::setListenerPos(LLVector3 aVec)
{
	if (mListenerp)
	{
		mListenerp->setPosition(aVec);
	}
}

//virtual
LLVector3 LLAudioEngine::getListenerPos()
{
	if (mListenerp)
	{
		return mListenerp->getPosition();
	}
	else
	{
		return LLVector3::zero;
	}
}

//virtual
void LLAudioEngine::setListenerVelocity(LLVector3 aVec)
{
	if (mListenerp)
	{
		mListenerp->setVelocity(aVec);
	}
}

//virtual
void LLAudioEngine::translateListener(LLVector3 aVec)
{
	if (mListenerp)
	{
		mListenerp->translate(aVec);
	}
}

//virtual
void LLAudioEngine::orientListener(LLVector3 up, LLVector3 at)
{
	if (mListenerp)
	{
		mListenerp->orient(up, at);
	}
}

//virtual
void LLAudioEngine::setListener(LLVector3 pos, LLVector3 vel, LLVector3 up,
								LLVector3 at)
{
	if (mListenerp)
	{
		mListenerp->set(pos, vel, up, at);
	}
}

//virtual
void LLAudioEngine::setDopplerFactor(F32 factor)
{
	if (mListenerp)
	{
		mListenerp->setDopplerFactor(factor);
	}
}

//virtual
F32 LLAudioEngine::getDopplerFactor()
{
	return mListenerp ? mListenerp->getDopplerFactor() : 0.f;
}

//virtual
void LLAudioEngine::setRolloffFactor(F32 factor)
{
	if (mListenerp)
	{
		mListenerp->setRolloffFactor(factor);
	}
}

//virtual
F32 LLAudioEngine::getRolloffFactor()
{
	return mListenerp ? mListenerp->getRolloffFactor() : 0.f;
}

void LLAudioEngine::commitDeferredChanges()
{
	if (mListenerp)
	{
		mListenerp->commitDeferredChanges();
	}
}

LLAudioSource* LLAudioEngine::findAudioSource(const LLUUID& source_id)
{
	source_map_t::iterator iter = mAllSources.find(source_id);
	if (iter == mAllSources.end())
	{
		return NULL;
	}
	return iter->second;
}

LLAudioData* LLAudioEngine::getAudioData(const LLUUID& audio_id)
{
	data_map_t::iterator iter= mAllData.find(audio_id);
	if (iter == mAllData.end())
	{
		// Create the new audio data
		LLAudioData* adp = new LLAudioData(audio_id);
		mAllData.emplace(audio_id, adp);
		return adp;
	}
	return iter->second;
}

void LLAudioEngine::addAudioSource(LLAudioSource* asp)
{
	mSourcesUpdated = true;
	mAllSources.emplace(asp->getID(), asp);
}

void LLAudioEngine::cleanupAudioSource(LLAudioSource* asp)
{
	source_map_t::iterator iter = mAllSources.find(asp->getID());
	if (iter == mAllSources.end())
	{
		llwarns << "Cleaning up unknown audio source !" << llendl;
		return;
	}
	delete asp;
	mSourcesUpdated = true;
	mAllSources.hmap_erase(iter);
}

//static
#if LL_SEARCH_UI_SOUNDS_IN_SKINS
bool LLAudioEngine::getUISoundFile(const LLUUID& id, std::string& sound_file,
								   bool* in_user_settings)
#else
bool LLAudioEngine::getUISoundFile(const LLUUID& id, std::string& sound_file)
#endif
{
	if (!gDirUtilp)
	{
#if LL_SEARCH_UI_SOUNDS_IN_SKINS
		if (in_user_settings)
		{
			*in_user_settings = false;
		}
#endif
		return false;
	}

	std::string filename = id.asString() + ".dsf";

	// Search first in the user's account LL_PATH_USER_SETTINGS/ui_sounds/
	// directory.
	sound_file = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
												"ui_sounds", filename);
	if (LLFile::isfile(sound_file))
	{
#if LL_SEARCH_UI_SOUNDS_IN_SKINS
		if (in_user_settings)
		{
			*in_user_settings = true;
		}
#endif
		return true;
	}

#if LL_SEARCH_UI_SOUNDS_IN_SKINS
	// No such sound in user settings...
	if (in_user_settings)
	{
		*in_user_settings = false;
	}
#endif

	// Then search in the viewer installation LL_PATH_SKINS/default/sounds/
	// sub-directory.
	sound_file = gDirUtilp->getExpandedFilename(LL_PATH_SKINS, "default",
												"sounds", filename);
	if (LLFile::isfile(sound_file))
	{
		return true;
	}

	// No pre-decoded sound file available
	sound_file.clear();
	return false;
}

bool LLAudioEngine::hasDecodedFile(const LLUUID& id)
{
	if (!gDirUtilp) return false;

	std::string sound_file;

	// If it is an UI sound, search among pre-decoded UI sound files
	if (isUISound(id) && getUISoundFile(id, sound_file))
	{
		return true;
	}

	// Search among cached sound files
	sound_file = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
												id.asString()) + ".dsf";
	return LLFile::isfile(sound_file);
}

bool LLAudioEngine::hasLocalFile(const LLUUID& id)
{
	// See if it is in the cache.
	return LLFileSystem::getExists(id);
}

void LLAudioEngine::startNextTransfer()
{
	if (mCurrentTransfer.notNull() || getMuted())
	{
		return;
	}

	// Get the ID for the next asset that we want to transfer. Pick one in the
	// following order:
	LLUUID asset_id;
	LLAudioSource* asp = NULL;
	LLAudioData* adp = NULL;

	// Check all channels for currently playing sounds.
	F32 max_pri = -1.f;
	for (S32 i = 0; i < MAX_AUDIO_CHANNELS; ++i)
	{
		if (!mChannels[i])
		{
			continue;
		}

		asp = mChannels[i]->getSource();
		if (!asp)
		{
			continue;
		}
		if (asp->getPriority() <= max_pri)
		{
			continue;
		}

		if (asp->getPriority() <= max_pri)
		{
			continue;
		}

		adp = asp->getCurrentData();
		if (!adp)
		{
			continue;
		}

		if (!adp->hasLocalData() && !adp->hasDecodeFailed())
		{
			asset_id = adp->getID();
			max_pri = asp->getPriority();
		}
	}

	// Check all channels for currently queued sounds.
	if (asset_id.isNull())
	{
		max_pri = -1.f;
		for (S32 i = 0; i < MAX_AUDIO_CHANNELS; ++i)
		{
			if (!mChannels[i])
			{
				continue;
			}

			asp = mChannels[i]->getSource();
			if (!asp)
			{
				continue;
			}

			if (asp->getPriority() <= max_pri)
			{
				continue;
			}

			adp = asp->getQueuedData();
			if (!adp)
			{
				continue;
			}

			if (!adp->hasLocalData() && !adp->hasDecodeFailed())
			{
				asset_id = adp->getID();
				max_pri = asp->getPriority();
			}
		}
	}

	// Check all live channels for other sounds (preloads).
	if (asset_id.isNull())
	{
		max_pri = -1.f;
		for (S32 i = 0; i < MAX_AUDIO_CHANNELS; ++i)
		{
			if (!mChannels[i])
			{
				continue;
			}

			asp = mChannels[i]->getSource();
			if (!asp)
			{
				continue;
			}

			if (asp->getPriority() <= max_pri)
			{
				continue;
			}

			for (data_map_t::iterator it = asp->mPreloadMap.begin(),
									  end = asp->mPreloadMap.end();
				 it != end; ++it)
			{
				adp = it->second;
				if (adp && !adp->hasLocalData() && !adp->hasDecodeFailed())
				{
					asset_id = adp->getID();
					max_pri = asp->getPriority();
				}
			}
		}
	}

	// Check all sources
	if (asset_id.isNull())
	{
		max_pri = -1.f;
		for (source_map_t::iterator sit = mAllSources.begin(),
									send = mAllSources.end();
			 sit != send; ++sit)
		{
			asp = sit->second;
			if (!asp || asp->getPriority() <= max_pri)
			{
				continue;
			}

			adp = asp->getCurrentData();
			if (adp && !adp->hasLocalData() && !adp->hasDecodeFailed())
			{
				asset_id = adp->getID();
				max_pri = asp->getPriority();
				continue;
			}

			adp = asp->getQueuedData();
			if (adp && !adp->hasLocalData() && !adp->hasDecodeFailed())
			{
				asset_id = adp->getID();
				max_pri = asp->getPriority();
				continue;
			}

			for (data_map_t::iterator dit = asp->mPreloadMap.begin(),
									  dend = asp->mPreloadMap.end();
				 dit != dend; ++dit)
			{
				adp = dit->second;
				if (adp && !adp->hasLocalData() && !adp->hasDecodeFailed())
				{
					asset_id = adp->getID();
					max_pri = asp->getPriority();
					break;
				}
			}
		}
	}

	if (asset_id.notNull())
	{
		LL_DEBUGS("Audio") << "Getting asset data for: " << asset_id
						   << LL_ENDL;
		if (!gAssetStoragep)
		{
			llwarns << "No asset storage system. Transfer for " << asset_id
					<< " aborted." << llendl;
			return;
		}
		mCurrentTransfer = asset_id;
		mCurrentTransferTimer.reset();
		gAssetStoragep->getAssetData(asset_id, LLAssetType::AT_SOUND,
									 assetCallback, NULL);
	}
}

//static
void LLAudioEngine::assetCallback(const LLUUID& id, LLAssetType::EType,
								  void*, S32 result_code, LLExtStat)
{
	if (!gAudiop)
	{
		llwarns_once << "Audio engine instance does not exist" << llendl;
		return;
	}

	if (!gAudioDecodeMgrp)
	{
		llwarns_once << "Audio decode manager instance does not exist"
					 << llendl;
		return;
	}

	if (result_code)
	{
		llwarns << "Error in audio file transfer: "
				<< LLAssetStorage::getErrorString(result_code) << " ("
				<< result_code << ")" << llendl;
		// Need to mark data as bad to avoid constant re-requests.
		LLAudioData* adp = gAudiop->getAudioData(id);
		if (adp)
        {
			adp->setHasDecodeFailed(true);
			adp->setHasLocalData(false);
			adp->setHasDecodedData(false);
			adp->setHasCompletedDecode(true);
		}
	}
	else
	{
		LLAudioData* adp = gAudiop->getAudioData(id);
		if (!adp)
        {
			// This should never happen
			llwarns << "Got asset callback without audio data for " << id
					<< llendl;
			// Make sure that corrupted sound will never be requested again
			LLAudioData::blockSound(id);
        }
		else
		{
			adp->setHasDecodeFailed(false);
		    adp->setHasLocalData(true);
		    gAudioDecodeMgrp->addDecodeRequest(id);
		}
	}
	gAudiop->mCurrentTransfer.setNull();
	gAudiop->startNextTransfer();
}

//
// LLAudioSource implementation
//

LLAudioSource::LLAudioSource(const LLUUID& id, const LLUUID& owner_id,
							 F32 gain, S32 type)
:	mID(id),
	mOwnerID(owner_id),
	mPriority(0.f),
	mGain(gain),
	mSourceMuted(false),
	mAmbient(false),
	mLoop(false),
	mSyncMaster(false),
	mSyncSlave(false),
	mQueueSounds(false),
	mPlayedOnce(false),
	mCorrupted(false),
	mType(type),
	mChannelp(NULL),
	mCurrentDatap(NULL),
	mQueuedDatap(NULL)
{
}

LLAudioSource::~LLAudioSource()
{
	if (mChannelp)
	{
		// Stop playback of this sound
		mChannelp->setSource(NULL);
		mChannelp = NULL;
	}
}

void LLAudioSource::setChannel(LLAudioChannel* channelp)
{
	if (channelp == mChannelp)
	{
		return;
	}

	mChannelp = channelp;
}

void LLAudioSource::update()
{
	if (!mCorrupted && !getCurrentBuffer())
	{
		// *HACK: try and load the sound. Will do this as a callback on
		// decode later.
		LLAudioData* adp = getCurrentData();
		if (adp)
		{
			if (adp->getBuffer())
			{
				LL_DEBUGS("Audio") << "Buffer exists for " << adp->getID()
								   << " - Playing it." << LL_ENDL;
				play(adp->getID());
			}
			else if (adp->hasDecodedData() && !adp->hasWAVLoadFailed())
			{
				LL_DEBUGS("Audio") << "Attempting to load " << adp->getID()
								   << LL_ENDL;
				if (adp->load())
				{
					LL_DEBUGS("Audio") << "Playing " << adp->getID()
									   << LL_ENDL;
					play(adp->getID());
				}
				else
				{
					LL_DEBUGS("Audio") << "Load failed for " << adp->getID()
									   << LL_ENDL;
				}
			}
			// Only mark corrupted after decode is done
			else if (adp->hasCompletedDecode() && adp->hasDecodeFailed())
			{
				llwarns << "Marking corrupted sound: " << adp->getID()
						<< llendl;
				mCorrupted = true;
			}
		}
	}
}

void LLAudioSource::updatePriority()
{
	if (isAmbient())
	{
		mPriority = 1.f;
	}
	else if (isMuted())
	{
		mPriority = 0.f;
	}
	else
	{
		// Priority is based on distance
		LLVector3 dist_vec(getPositionGlobal());
		if (gAudiop)
		{
			dist_vec -= gAudiop->getListenerPos();
		}
		F32 dist_squared = llmax(1.f, dist_vec.lengthSquared());

		mPriority = mGain / dist_squared;
	}
}

bool LLAudioSource::setupChannel()
{
	if (!gAudiop)
	{
		llwarns_once << "Audio engine instance does not exist" << llendl;
		return false;
	}

	LLAudioData* adp = getCurrentData();
	if (!adp || !adp->getBuffer())
	{
		// We are not ready to play back the sound yet, so do not try and
		// allocate a channel for it.
		LL_DEBUGS("Audio") << "Aborting, no buffer" << LL_ENDL;
		return false;
	}

	if (!mChannelp)
	{
		// Update the priority, in case we need to push out another channel.
		updatePriority();

		setChannel(gAudiop->getFreeChannel(getPriority()));
	}

	if (!mChannelp)
	{
		// We do not have any free channels; we should reprioritize.
		// For now, just do not play the sound.
		LL_DEBUGS("Audio") << "Aborting, no free channels" << LL_ENDL;
		return false;
	}

	mChannelp->setSource(this);
	return true;
}

bool LLAudioSource::play(const LLUUID& audio_id)
{
	// Special abuse of play(): do not play a sound, but kill it.
	if (audio_id.isNull())
	{
		if (getChannel())
		{
			LL_DEBUGS("Audio") << "Killing current sound." << LL_ENDL;
			getChannel()->setSource(NULL);
			setChannel(NULL);
			if (!isMuted())
			{
				mCurrentDatap = NULL;
			}
		}
		return false;
	}
	LL_DEBUGS("Audio") << "Request to play " << audio_id << LL_ENDL;

	// Reset our age timeout if someone attempts to play the source.
	mAgeTimer.reset();

	if (!gAudiop)
	{
		llwarns_once << "Audio engine instance does not exist" << llendl;
		return false;
	}

	LLAudioData* adp = gAudiop->getAudioData(audio_id);
	addAudioData(adp);

	if (isMuted())
	{
		LL_DEBUGS("Audio") << "Denied playing muted sound " << audio_id
						   << LL_ENDL;
		return false;
	}

	bool has_buffer = gAudiop->updateBufferForData(adp, audio_id);
	if (!has_buffer)
	{
		LL_DEBUGS("Audio") << "No buffer available to play sound " << audio_id
						   << LL_ENDL;
		// Do not bother trying to set up a channel or anything, we do not have
		// an audio buffer.
		return false;
	}

	if (!setupChannel())
	{
		LL_DEBUGS("Audio") << "Failed to setup channel to play sound "
						   << audio_id << LL_ENDL;
		return false;
	}

	if (isSyncSlave())
	{
		LL_DEBUGS("Audio") << "Waiting for sync to play sound " << audio_id
						   << LL_ENDL;
		// A sync slave, it does not start playing until it is synced up with
		// the master. Flag this channel as waiting for sync, and return true.
		getChannel()->setWaiting(true);
		return true;
	}

	LLAudioChannel* channelp = getChannel();
	if (channelp && channelp->mCurrentBufferp)
	{
		LL_DEBUGS("Audio") << "Playing sound " << audio_id << LL_ENDL;
		channelp->play();
		return true;
	}

	llwarns << "Cannot get the channel for " << audio_id << llendl;
	return false;
}

void LLAudioSource::stop()
{
	play(LLUUID::null);
	// Always reset data if something wants us to stop
	mCurrentDatap = NULL;
}

bool LLAudioSource::isDone() const
{
	constexpr F32 MAX_AGE = 60.f;
	constexpr F32 MAX_UNPLAYED_AGE = 15.f;
	constexpr F32 MAX_MUTED_AGE = 11.f;

	if (isLoop())
	{
		// Looped sources never die on their own.
		return false;
	}

	if (hasPendingPreloads())
	{
		return false;
	}

	if (mQueuedDatap)
	{
		// Do not kill this sound if we have got something queued up to play.
		return false;
	}

	F32 elapsed = mAgeTimer.getElapsedTimeF32();

	// This is a single-play source
	if (!mChannelp)
	{
		if (mPlayedOnce ||
			elapsed > (isMuted() ? MAX_MUTED_AGE : MAX_UNPLAYED_AGE))
		{
			// We do not have a channel assigned, and it has been over 15
			// seconds since we tried to play it. Do not bother.
			LL_DEBUGS("Audio") << "No channel assigned, source is done"
							   << LL_ENDL;
			return true;
		}
		else
		{
			return false;
		}
	}

	if (mChannelp->isPlaying())
	{
		if (elapsed > MAX_AGE)
		{
			// Arbitarily cut off non-looped sounds when they are old.
			return true;
		}
		else
		{
			// Sound is still playing and we have not timed out, do not kill it
			return false;
		}
	}

	if (mPlayedOnce || elapsed > MAX_UNPLAYED_AGE)
	{
		// The sound is not playing back after 15 seconds or we are already
		// done playing it, kill it.
		return true;
	}

	return false;
}

void LLAudioSource::addAudioData(LLAudioData* adp, bool set_current)
{
	// Only handle a single piece of audio data associated with a source right
	// now, until I implement prefetch.

	if (!gAudiop)
	{
		llwarns_once << "Audio engine instance does not exist" << llendl;
		return;
	}

	if (set_current)
	{
		mPlayedSounds.emplace(adp->getID());
		gAudiop->setSourcesUpdated();

		if (!mCurrentDatap)
		{
			mCurrentDatap = adp;
			if (mChannelp)
			{
				mChannelp->updateBuffer();
				if (mChannelp->mCurrentBufferp)
				{
					mChannelp->play();
				}
			}

			// Make sure the audio engine knows that we want to request this
			// sound.
			gAudiop->startNextTransfer();
			return;
		}
		else if (mQueueSounds)
		{
			// If we have current data, and we are queuing, put the object
			// onto the queue.
			if (mQueuedDatap)
			{
				// We only queue one sound at a time, and it is a FIFO.
				// Do not put it onto the queue.
				return;
			}

			if (adp == mCurrentDatap && isLoop())
			{
				// No point in queueing the same sound if we are looping.
				return;
			}
			mQueuedDatap = adp;

			// Make sure the audio engine knows that we want to request this
			// sound.
			gAudiop->startNextTransfer();
		}
		else if (mCurrentDatap != adp)
		{
			// Right now, if we are currently playing this sound in a channel,
			// we update the buffer associated with the channel and play it.
			// This may not be the correct behavior.
			mCurrentDatap = adp;
			if (mChannelp)
			{
				mChannelp->updateBuffer();
				if (mChannelp->mCurrentBufferp)
				{
					mChannelp->play();
				}
			}
			// Make sure the audio engine knows that we want to request
			// this sound.
			gAudiop->startNextTransfer();
		}
	}
	else
	{
		// Add it to the preload list.
		mPreloadMap.emplace(adp->getID(), adp);
		gAudiop->startNextTransfer();
	}
}

bool LLAudioSource::hasPendingPreloads() const
{
	// Check to see if we have got any preloads on deck for this source
	for (data_map_t::const_iterator iter = mPreloadMap.begin(),
									end = mPreloadMap.end();
		 iter != end; ++iter)
	{
		LLAudioData* adp = iter->second;
		// Note: a bad UUID will forever be !hasDecodedData() but also
		// hasDecodeFailed(), hence the check for it
		if (adp && !adp->hasDecodedData() && !adp->hasDecodeFailed())
		{
			// This source is still waiting for a preload
			return true;
		}
	}

	return false;
}

LLAudioBuffer* LLAudioSource::getCurrentBuffer()
{
	return mCurrentDatap ? mCurrentDatap->getBuffer() : (LLAudioBuffer*)NULL;
}

//
// LLAudioChannel implementation
//

LLAudioChannel::LLAudioChannel()
:	mCurrentSourcep(NULL),
	mCurrentBufferp(NULL),
	mLoopedThisFrame(false),
	mWaiting(false),
	mSecondaryGain(1.f)
{
}

LLAudioChannel::~LLAudioChannel()
{
	// Need to disconnect any sources which are using this channel.
	if (mCurrentSourcep)
	{
		mCurrentSourcep->setChannel(NULL);
	}
	mCurrentBufferp = NULL;
}

void LLAudioChannel::setSource(LLAudioSource* sourcep)
{
	if (!sourcep)
	{
		// Clearing the source for this channel, do not need to do anything.
		cleanup();
		mCurrentSourcep = NULL;
		mWaiting = false;
		return;
	}

	mCurrentSourcep = sourcep;

	updateBuffer();
	update3DPosition();
}

bool LLAudioChannel::updateBuffer()
{
	if (!gAudiop)
	{
		llwarns_once << "Audio engine instance does not exist" << llendl;
		return false;
	}

	if (!mCurrentSourcep)
	{
		// This channel is not associated with any source, nothing
		// to be updated
		return false;
	}

	// Initialize the channel's gain setting for this sound.
	if (gAudiop)
	{
		setSecondaryGain(gAudiop->getSecondaryGain(mCurrentSourcep->getType()));
	}

	LLAudioBuffer* bufferp = mCurrentSourcep->getCurrentBuffer();
	if (bufferp == mCurrentBufferp)
	{
		if (bufferp)
		{
			// The source has not changed what buffer it is playing
			bufferp->mLastUseTimer.reset();
			bufferp->mInUse = true;
		}
		return false;
	}

	// The source changed what buffer it is playing. We need to clean up the
	// existing channel
	cleanup();

	mCurrentBufferp = bufferp;
	if (bufferp)
	{
		bufferp->mLastUseTimer.reset();
		bufferp->mInUse = true;
	}

	if (!mCurrentBufferp)
	{
		// There's no new buffer to be played, so we just abort.
		return false;
	}

	return true;
}

//
// LLAudioData implementation
//

LLAudioData::LLAudioData(const LLUUID& id)
:	mID(id),
	mBufferp(NULL),
	mHasLocalData(false),
	mHasDecodedData(false),
	mHasCompletedDecode(false),
	mHasDecodeFailed(false),
	mHasWAVLoadFailed(true)
{
	if (id.isNull())
	{
		// This is a null sound.
		return;
	}

	if (!gAudiop)
	{
		llwarns_once << "Audio engine instance does not exist" << llendl;
		return;
	}

	if (gAudiop->hasDecodedFile(id))
	{
		// Already have a decoded version, do not need to decode it.
		mHasLocalData = true;
		mHasDecodedData = true;
		mHasCompletedDecode = true;
	}
	else if (gAssetStoragep &&
			 gAssetStoragep->hasLocalAsset(id, LLAssetType::AT_SOUND))
	{
		mHasLocalData = true;
	}
}

bool LLAudioData::load()
{
	// For now, just assume we are going to use one buffer per audiodata.
	if (mBufferp)
	{
		// We already have this sound in a buffer, do not do anything.
		llinfos << "Already have a buffer for this sound, do not bothering to load."
				<< llendl;
		mHasWAVLoadFailed = false;
		return true;
	}

	if (!gAudiop)
	{
		llwarns_once << "Audio engine instance does not exist" << llendl;
		mHasWAVLoadFailed = true;
		return false;
	}

	if (!gDirUtilp)
	{
		llwarns_once << "gDirUtilp is NULL !" << llendl;
		return false;
	}

	mBufferp = gAudiop->getFreeBuffer();
	if (!mBufferp)
	{
		// No free buffers, abort.
		llinfos_sparse << "Not able to allocate a new audio buffer, aborting."
					   << llendl;
		mHasWAVLoadFailed = true;
		return false;
	}

	std::string sound_file;
	if (!gAudiop->isUISound(mID) ||
		!LLAudioEngine::getUISoundFile(mID, sound_file))
	{
		// Not a pre-decoded UI sound file, then it will go to the cache.
		sound_file = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
													mID.asString()) + ".dsf";
	}

	mHasWAVLoadFailed = !mBufferp->loadWAV(sound_file);
	if (mHasWAVLoadFailed)
	{
		// Hrm.  Right now, let's unset the buffer, since it is empty.
		gAudiop->cleanupBuffer(mBufferp);
		mBufferp = NULL;
		if (!LLFile::isfile(sound_file))
		{
			// And preload it again.
			gAudiop->preloadSound(mID);
		}
		return false;
	}
	mBufferp->mAudioDatap = this;

	return true;
}

//static
void LLAudioData::blockSound(const LLUUID& id, bool block)
{
	bool blocked = sBlockedSounds.count(id) != 0;
	if (block && !blocked)
	{
		sBlockedSounds.emplace(id);
	}
	else if (blocked && !block)
	{
		sBlockedSounds.erase(id);
	}
}
