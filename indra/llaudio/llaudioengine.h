/**
 * @file llaudioengine.h
 * @brief Definition of LLAudioEngine base class abstracting the audio support
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

#ifndef LL_AUDIOENGINE_H
#define LL_AUDIOENGINE_H

#include <list>

#include "llassettype.h"
#include "llextendedstatus.h"
#include "hbfastmap.h"
#include "llframetimer.h"
#include "lllistener.h"
#include "lltimer.h"
#include "lluuid.h"
#include "llvector3.h"
#include "llvector3d.h"

constexpr F32 LL_WIND_UPDATE_INTERVAL = 0.1f;
// How much sounds are weaker under water:
constexpr F32 LL_WIND_UNDERWATER_CENTER_FREQ = 20.f;

constexpr F32 ATTACHED_OBJECT_TIMEOUT = 5.f;
constexpr F32 DEFAULT_MIN_DISTANCE = 2.f;

// Define to 1 to allow searching for pre-decode UI sounds in the
// LL_PATH_SKINS/default/sounds/ sub-directory of the viewer installation
#define LL_SEARCH_UI_SOUNDS_IN_SKINS 0

#define MAX_AUDIO_CHANNELS 30
// Number of maximum rezzed objects with sounds + sounds without an object + UI
// sounds.
#define MAX_AUDIO_BUFFERS 100

class LLAudioBuffer;
class LLAudioChannel;
class LLAudioChannelOpenAL;
class LLAudioData;
class LLAudioSource;
class LLStreamingAudioInterface;

//
//  LLAudioEngine definition
//

class LLAudioEngine
{
	friend class LLAudioChannelOpenAL;	// Channel needs some listener methods.
	friend class HBFloaterSoundsList;	// For sounds list
	friend class LLPipeline;			// For sound beacons

protected:
	LOG_CLASS(LLAudioEngine);

public:
	enum LLAudioType
	{
		AUDIO_TYPE_NONE    = 0,
		AUDIO_TYPE_SFX     = 1,
		AUDIO_TYPE_UI      = 2,
		AUDIO_TYPE_AMBIENT = 3,
		AUDIO_TYPE_COUNT   = 4 // last
	};

	LLAudioEngine();
	virtual ~LLAudioEngine() = default;

	// Initialization/startup/shutdown
	virtual bool init(void* userdata);
	virtual std::string getDriverName(bool verbose) = 0;
	virtual void shutdown();

#if 0
	// Used by the mechanics of the engine
	virtual void processQueue(const LLUUID& sound_guid);
#endif

	virtual void setListener(LLVector3 pos,LLVector3 vel,
							 LLVector3 up,LLVector3 at);
	virtual void updateWind(LLVector3 direction,
							F32 camera_height_above_water) = 0;
	virtual void idle();
	virtual void updateChannels();

	//
	// "End user" functionality
	//
	virtual bool isWindEnabled();
	virtual void enableWind(bool state_b);

	// Use these for temporarily muting the audio system. Does not change
	// buffers, initialization, etc, but stops playing new sounds.
	void setMuted(bool muted);
	LL_INLINE bool getMuted() const					{ return mMuted; }

	F32 getMasterGain();
	void setMasterGain(F32 gain);

	F32 getSecondaryGain(S32 type);
	void setSecondaryGain(S32 type, F32 gain);

	F32 getInternetStreamGain();

	virtual void setDopplerFactor(F32 factor);
	virtual F32 getDopplerFactor();
	virtual void setRolloffFactor(F32 factor);
	virtual F32 getRolloffFactor();
	virtual void setMaxWindGain(F32 gain);

	// Methods actually related to setting up and removing sounds. Owner ID is
	// the owner of the object making the request.
	void triggerSound(const LLUUID& sound_id, const LLUUID& owner_id,
					  F32 gain, S32 type = LLAudioEngine::AUDIO_TYPE_NONE,
					  const LLVector3d& pos_global = LLVector3d::zero);

	bool preloadSound(const LLUUID& id);

	void addAudioSource(LLAudioSource* asp);
	void cleanupAudioSource(LLAudioSource* asp);

	LLAudioSource* findAudioSource(const LLUUID& source_id);
	LLAudioData* getAudioData(const LLUUID& audio_id);

	// Internet stream implementation manipulation
	LLStreamingAudioInterface* getStreamingAudioImpl();
	void setStreamingAudioImpl(LLStreamingAudioInterface* impl);

	// Internet stream methods - these will call down into the
	// *mStreamingAudioImpl if it exists:
	void startInternetStream(const std::string& url);
	void stopInternetStream();
	void pauseInternetStream(S32 pause);
	void updateInternetStream();	// Expected to be called often
	S32 isInternetStreamPlaying();
	// Use a value from 0.0 to 1.0, inclusive:
	void setInternetStreamGain(F32 vol);
	std::string getInternetStreamURL();

	// For debugging usage
	virtual LLVector3 getListenerPos();

	// Get a free buffer, or flush an existing one if you have to:
	LLAudioBuffer* getFreeBuffer();

	// Get a free channel or flush an existing one if your priority is higher:
	LLAudioChannel* getFreeChannel(F32 priority);

	void cleanupBuffer(LLAudioBuffer* bufferp);

	LL_INLINE bool isUISound(const LLUUID& id)		{ return mUISounds.count(id) != 0; }

	LL_INLINE void setSourcesUpdated()				{ mSourcesUpdated = true; }

	bool hasDecodedFile(const LLUUID& id);
	bool hasLocalFile(const LLUUID& id);

	bool updateBufferForData(LLAudioData* adp,
							 const LLUUID& audio_id = LLUUID::null);

#if LL_SEARCH_UI_SOUNDS_IN_SKINS
	// Helper function to find pre-decoded UI sound files in either the user's
	// account LL_PATH_USER_SETTINGS/ui_sounds/ directory or in the viewer
	// installation LL_PATH_SKINS/default/sounds/ sub-directory.
	// If an in_user_settings bool pointer is passed, then the corresponding
	// boolean variable is set to true whenever the sound is found in the user
	// settings directory (which got the highest priority and is searched
	// first), and set to false otherwise.
	// NOTE: no check is done in this method whether the UUID corresposnds to
	// a registered UI sound or not: this is on purpose so that the existence
	// of pre-cached sounds can be checked before their UUID got registered as
	// pertaining to an UI sound.
	static bool getUISoundFile(const LLUUID& id, std::string& sound_file,
							   bool* in_user_settings = NULL);
#else
	// Helper function to find pre-decoded UI sound files in the viewer
	// installation LL_PATH_SKINS/default/sounds/ sub-directory.
	// NOTE: no check is done in this method whether the UUID corresposnds to
	// a registered UI sound or not: this is on purpose so that the existence
	// of pre-cached sounds can be checked before their UUID got registered as
	// pertaining to an UI sound.
	static bool getUISoundFile(const LLUUID& id, std::string& sound_file);
#endif

	// Asset callback when we have retrieved a sound from the asset server.
	void startNextTransfer();
	static void assetCallback(const LLUUID& id, LLAssetType::EType, void*,
							  S32 result_code, LLExtStat);

protected:
	virtual LLAudioBuffer* createBuffer() = 0;
	virtual LLAudioChannel* createChannel() = 0;

	virtual bool initWind() = 0;
	virtual void cleanupWind() = 0;
	virtual void setInternalGain(F32 gain) = 0;

	void commitDeferredChanges();

	virtual void allocateListener() = 0;

	// Listener methods
	virtual void setListenerPos(LLVector3 vec);
	virtual void setListenerVelocity(LLVector3 vec);
	virtual void orientListener(LLVector3 up, LLVector3 at);
	virtual void translateListener(LLVector3 vec);

	F32 mapWindVecToGain(LLVector3 wind_vec);
	F32 mapWindVecToPitch(LLVector3 wind_vec);
	F32 mapWindVecToPan(LLVector3 wind_vec);

private:
	void setDefaults();

public:
	// *HACK: public to set before fade in ?
	F32					mMaxWindGain;

protected:
	LLListener*			mListenerp;

	bool				mMuted;
	void*				mUserData;

	S32					mLastStatus;

	bool				mEnableWind;

	// Audio file currently being transferred by the system:
	LLUUID mCurrentTransfer;

	LLFrameTimer		mCurrentTransferTimer;

	// A list of all audio sources that are known to the viewer at this time.
	// This is most likely a superset of the ones that we actually have audio
	// data for, or are playing back.
	typedef fast_hmap<LLUUID, LLAudioSource*> source_map_t;
	typedef fast_hmap<LLUUID, LLAudioData*> data_map_t;

	source_map_t		mAllSources;
	data_map_t			mAllData;

	uuid_list_t			mUISounds;

	LLAudioChannel*		mChannels[MAX_AUDIO_CHANNELS];

	// Buffers needs to change into a different data structure, as the number
	// of buffers that we have active should be limited by RAM usage, not
	// count.
	LLAudioBuffer*		mBuffers[MAX_AUDIO_BUFFERS];

	F32					mMasterGain;
	// Actual gain set, i.e. either mMasterGain or 0 when mMuted is true:
	F32					mInternalGain;
	F32					mSecondaryGain[AUDIO_TYPE_COUNT];

	F32					mNextWindUpdate;

	LLFrameTimer		mWindUpdateTimer;

	std::string			mAudioDevice;

private:
	LLStreamingAudioInterface* mStreamingAudioImpl;
	// *HACK: checked/reset by HBFloaterSoundsList *only* (keep it that way !)
	bool						mSourcesUpdated;
};

//
// Generic metadata about a particular piece of audio data.
// The actual data is handled by the derived LLAudioBuffer classes which are
// derived for each audio engine.
//

class LLAudioData
{
	friend class LLAudioEngine; // Severe laziness, bad.

protected:
	LOG_CLASS(LLAudioData);

public:
	LLAudioData(const LLUUID& id);
	bool load();

	LL_INLINE LLUUID getID() const					{ return mID; }
	LL_INLINE LLAudioBuffer* getBuffer() const		{ return mBufferp; }

	LL_INLINE bool hasLocalData() const				{ return mHasLocalData; }
	LL_INLINE bool hasDecodedData() const			{ return mHasDecodedData; }
	LL_INLINE bool hasCompletedDecode() const		{ return mHasCompletedDecode; }
	LL_INLINE bool hasDecodeFailed() const			{ return mHasDecodeFailed; }
	LL_INLINE bool hasWAVLoadFailed() const			{ return mHasWAVLoadFailed; }

	LL_INLINE void setHasLocalData(bool b)			{ mHasLocalData = b; }
	LL_INLINE void setHasDecodedData(bool b)		{ mHasDecodedData = b; }
	LL_INLINE void setHasCompletedDecode(bool b)	{ mHasCompletedDecode = b; }
	LL_INLINE void setHasDecodeFailed(bool b)		{ mHasDecodeFailed = b; }
	LL_INLINE void setHasWAVLoadFailed(bool b)		{ mHasWAVLoadFailed = b; }

	LL_INLINE bool isBlocked()						{ return sBlockedSounds.count(mID) != 0; }

	static void blockSound(const LLUUID& id, bool block = true);

	LL_INLINE static bool isBlockedSound(const LLUUID& id)
	{
		return sBlockedSounds.count(id) != 0;
	}

	LL_INLINE static const uuid_list_t& getBlockedSounds()
	{
		return sBlockedSounds;
	}

	LL_INLINE static void setBlockedSounds(const uuid_list_t& sounds)
	{
		sBlockedSounds = sounds;
	}

protected:
	LLUUID				mID;

	// If this data is being used by the audio system, a pointer to the buffer
	// will be set here:
	LLAudioBuffer*		mBufferp;

	bool				mHasLocalData;
	bool				mHasDecodedData;
	bool				mHasCompletedDecode;
	bool				mHasDecodeFailed;
	bool				mHasWAVLoadFailed;

	static uuid_list_t	sBlockedSounds;
};

//
// Standard audio source. Can be derived from for special sources, such as
// those attached to objects.
//

class LLAudioSource
{
	friend class LLAudioEngine;
	friend class LLAudioChannel;

protected:
	LOG_CLASS(LLAudioSource);

public:
	// owner_id is the id of the agent responsible for making this sound
	// play, for example, the owner of the object currently playing it
	LLAudioSource(const LLUUID& id, const LLUUID& owner_id, F32 gain,
				  S32 type = LLAudioEngine::AUDIO_TYPE_NONE);
	virtual ~LLAudioSource();

	virtual void update();						// Update this audio source
	void updatePriority();

	// Only used for preloading UI sounds, now.
	void preload(const LLUUID& audio_id);

	void addAudioData(LLAudioData* adp, bool set_current = TRUE);

	LL_INLINE void setAmbient(bool ambient)			{ mAmbient = ambient; }
	LL_INLINE bool isAmbient() const				{ return mAmbient; }

	LL_INLINE void setLoop(bool loop)				{ mLoop = loop; }
	LL_INLINE bool isLoop() const					{ return mLoop; }

	LL_INLINE void setSyncMaster(bool master)		{ mSyncMaster = master; }
	LL_INLINE bool isSyncMaster() const				{ return mSyncMaster; }

	LL_INLINE void setSyncSlave(bool slave)			{ mSyncSlave = slave; }
	LL_INLINE bool isSyncSlave() const				{ return mSyncSlave; }

	LL_INLINE void setQueueSounds(bool queue)		{ mQueueSounds = queue; }
	LL_INLINE bool isQueueSounds() const			{ return mQueueSounds; }

	LL_INLINE void setPlayedOnce(bool played)		{ mPlayedOnce = played; }

	LL_INLINE void setType(S32 type)				{ mType = type; }
	LL_INLINE S32 getType()							{ return mType; }

	LL_INLINE void setPositionGlobal(const LLVector3d& pos)
	{
		mPositionGlobal = pos;
	}

	LL_INLINE LLVector3d getPositionGlobal() const	{ return mPositionGlobal; }
	LL_INLINE LLVector3 getVelocity() const			{ return mVelocity; }
	LL_INLINE F32 getPriority() const				{ return mPriority; }

	// Gain should always be clamped between 0 and 1.
	LL_INLINE F32 getGain() const					{ return mGain; }
	virtual void setGain(F32 gain)					{ mGain = llclamp(gain, 0.f, 1.f); }

	LL_INLINE const LLUUID& getID() const			{ return mID; }
	LL_INLINE const LLUUID& getOwnerID() const		{ return mOwnerID; }

	bool isDone() const;

	LL_INLINE bool isMuted() const
	{
		return mSourceMuted || (mCurrentDatap && mCurrentDatap->isBlocked());
	}

	const uuid_list_t& getPlayedSoundsUUIDs() const	{ return mPlayedSounds; }

	LL_INLINE LLAudioData* getCurrentData()			{ return mCurrentDatap; }
	LL_INLINE LLAudioData* getQueuedData()			{ return mQueuedDatap; }
	LLAudioBuffer* getCurrentBuffer();

	bool setupChannel();
	// Start the audio source playing, taking mute into account to preserve the
	// previous audio_id if nessesary.
	bool play(const LLUUID& audio_id);
	// Stops the audio source and resets audio_id, even if muted.
	void stop();

	LL_INLINE bool isPlaying()						{ return mChannelp != NULL; }

	// Returns true when we have preloads that have not been done yet
	bool hasPendingPreloads() const;

protected:
	void setChannel(LLAudioChannel* channelp);
	LL_INLINE LLAudioChannel* getChannel() const	{ return mChannelp; }

protected:
	// If we are currently playing back, this is the channel that we have
	// assigned to:
	LLAudioChannel*	mChannelp;

	LLAudioData*	mCurrentDatap;
	LLAudioData*	mQueuedDatap;

	// The ID of the source is that of the object if it is attached to an
	// object. For sounds not attached to an object, this UUID is a randomly
	// generated one.
	LLUUID			mID;
	// Owner of the sound source
	LLUUID			mOwnerID;

	LLVector3d		mPositionGlobal;
	LLVector3		mVelocity;

	S32             mType;

	F32				mPriority;
	F32				mGain;

	bool			mSourceMuted;
	bool			mAmbient;
	bool			mLoop;
	bool			mSyncMaster;
	bool			mSyncSlave;
	bool			mQueueSounds;
	bool			mPlayedOnce;
	bool			mCorrupted;

	typedef fast_hmap<LLUUID, LLAudioData*> data_map_t;
	data_map_t		mPreloadMap;

	uuid_list_t		mPlayedSounds;

	LLFrameTimer	mAgeTimer;
};

//
// Base class for an audio channel, i.e. a channel which is capable of playing
// back a sound. Management of channels is done generically, methods for
// actually manipulating the channel are derived for each audio engine.
//

class LLAudioChannel
{
	friend class LLAudioEngine;
	friend class LLAudioSource;

protected:
	LOG_CLASS(LLAudioChannel);

public:
	LLAudioChannel();
	virtual ~LLAudioChannel();

	virtual void setSource(LLAudioSource* sourcep);
	LL_INLINE LLAudioSource* getSource() const		{ return mCurrentSourcep; }

	LL_INLINE void setSecondaryGain(F32 gain)		{ mSecondaryGain = gain; }
	LL_INLINE F32 getSecondaryGain()				{ return mSecondaryGain; }

protected:
	virtual void play() = 0;
	virtual void playSynced(LLAudioChannel* channelp) = 0;
	virtual void cleanup() = 0;
	virtual bool isPlaying() = 0;
	LL_INLINE void setWaiting(bool waiting)			{ mWaiting = waiting; }
	LL_INLINE bool isWaiting() const				{ return mWaiting; }

	// Check to see if the buffer associated with the source changed and update
	// if necessary:
	virtual bool updateBuffer();

	virtual void update3DPosition() = 0;
	// Update your loop/completion status, for use by queueing/syncing:
	virtual void updateLoop() = 0;

protected:
	LLAudioSource*	mCurrentSourcep;
	LLAudioBuffer*	mCurrentBufferp;
	bool			mLoopedThisFrame;
	bool			mWaiting;	// Waiting for sync.
	F32             mSecondaryGain;
};

// Basically an interface class to the engine-specific implementation of audio
// data that is ready for playback. Will likely get more complex as we decide
// to do stuff like real streaming audio.

class LLAudioBuffer
{
	friend class LLAudioEngine;
	friend class LLAudioChannel;
	friend class LLAudioData;

protected:
	LOG_CLASS(LLAudioBuffer);

public:
	virtual ~LLAudioBuffer() = default;
	virtual bool loadWAV(const std::string& filename) = 0;
	virtual U32 getLength() = 0;

protected:
	LLAudioData*	mAudioDatap;
	LLFrameTimer	mLastUseTimer;
	bool			mInUse;
};

extern LLAudioEngine* gAudiop;

#endif
