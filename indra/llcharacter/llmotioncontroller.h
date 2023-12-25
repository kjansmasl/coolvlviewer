/**
 * @file llmotioncontroller.h
 * @brief Implementation of LLMotionController class.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLMOTIONCONTROLLER_H
#define LL_LLMOTIONCONTROLLER_H

#include "hbfastmap.h"
#include "llmotion.h"
#include "llpose.h"
#include "llframetimer.h"
#include "llstring.h"
#include "lluuid.h"

class LLCharacter;

typedef LLMotion*(*LLMotionConstructor)(const LLUUID& id);

class LLMotionRegistry final
{
public:
	~LLMotionRegistry();

	// Adds motion classes to the registry, returns true on success
	bool registerMotion(const LLUUID& id, LLMotionConstructor create);

	// Creates a new instance of a named motion, returns NULL motion is not
	// registered
	LLMotion* createMotion(const LLUUID& id);

	// Initialization of motion failed, do not try to create this motion again
	void markBad(const LLUUID& id);

protected:
	typedef fast_hmap<LLUUID, LLMotionConstructor> motion_map_t;
	motion_map_t mMotionTable;
};

class LLMotionController
{
protected:
	LOG_CLASS(LLMotionController);

public:
	typedef std::list<LLMotion*> motion_list_t;
	typedef fast_hset<LLMotion*> motion_set_t;

public:
	LLMotionController();
	virtual ~LLMotionController();

	// Sets associated character. This must be called exactly once by the
	// containing character class. This is generally done in the character
	LL_INLINE void setCharacter(LLCharacter* ch)	{ mCharacter = ch; }

	// Registers a motion with the controller (actually just forwards call to
	// motion registry). Returns true on success.
	bool registerMotion(const LLUUID& id, LLMotionConstructor create);

	// Creates a motion from the registry
	LLMotion* createMotion(const LLUUID& id);

	// Unregisters a motion with the controller (actually just forwards call to
	// motion registry). Returns true on success.
	void removeMotion(const LLUUID& id);

	// Starts motion playing the specified motion. Returns true on success.
	bool startMotion(const LLUUID& id, F32 start_offset);

	// Stops a playing motion; in reality, it begins the ease out transition
	// phase. Returns true on success.
	bool stopMotionLocally(const LLUUID& id, bool stop_now);

	// Moves motions from loading to loaded
	void updateLoadingMotions();

	// Updates motions. Invokes the update handlers for each active motion.
	// Activates sequenced motions and deactivates terminated motions.
	void updateMotions(bool force_update = false);
	// minimal update (e.g. while hidden)
	void updateMotionsMinimal();
	// NOTE: call updateMotion() or updateMotionsMinimal() every frame

	// Flushes motions and releases all motion instances.
	void flushAllMotions();

	// Flush is a liar.
	void deactivateAllMotions();

	// Pauses and continue all motions
	void pauseAllMotions();
	LL_INLINE void unpauseAllMotions()				{ mPaused = false; }
	LL_INLINE bool isPaused() const					{ return mPaused; }
	// Returns true when paused for more than 2 frames
	bool isReallyPaused() const;

	void setTimeStep(F32 step);
	LL_INLINE F32 getTimeStep() const				{ return mTimeStep; }

	LL_INLINE void setTimeFactor(F32 factor)		{ mTimeFactor = factor; }
	LL_INLINE F32 getTimeFactor() const				{ return mTimeFactor; }

	LL_INLINE F32 getAnimTime() const				{ return mAnimTime; }

	LL_INLINE motion_list_t& getActiveMotions()		{ return mActiveMotions; }

	void incMotionCounts(S32& num_motions, S32& num_loading_motions,
						 S32& num_loaded_motions, S32& num_active_motions,
						 S32& num_deprecated_motions);

	static void initClass();
	static void dumpStats();

	LL_INLINE bool isMotionActive(LLMotion* m)		{ return m && m->isActive(); }
	LL_INLINE bool isMotionLoading(LLMotion* m)		{ return mLoadingMotions.count(m) != 0; }
	LLMotion* findMotion(const LLUUID& id) const;

	LL_INLINE const LLFrameTimer& getFrameTimer()	{ return mTimer; }

	LL_INLINE static F32 getTimeFactorMultiplier()	{ return sTimeFactorMultiplier; }

	LL_INLINE static void setTimeFactorMultiplier(F32 factor)
	{
		sTimeFactorMultiplier = factor;
	}

protected:
	// Internal operations act on motion instances directly as there can be
	// duplicate motions per Id during blending overlap
	void deleteAllMotions();
	bool activateMotionInstance(LLMotion* motion, F32 time);
	bool deactivateMotionInstance(LLMotion* motion);
	void deprecateMotionInstance(LLMotion* motion);
	bool stopMotionInstance(LLMotion* motion, bool stop_now);
	void removeMotionInstance(LLMotion* motion);
	void updateRegularMotions();
	void updateAdditiveMotions();
	void resetJointSignatures();
	void updateMotionsByType(LLMotion::LLMotionBlendType motion_type);
	void updateIdleMotion(LLMotion* motionp);
	void updateIdleActiveMotions();
	void purgeExcessMotions();
	void deactivateStoppedMotions();

public:
	F32					mTimeFactorMultiplier;

protected:
	F32					mTimeFactor;
	LLPoseBlender		mPoseBlender;

	LLCharacter*		mCharacter;

	// Life cycle of an animation:
	//
	// Animations are instantiated and immediately put in the mAllMotions map
	// for their entire lifetime. If the animations depend on any asset data,
	// the appropriate data is fetched from the data server, and the animation
	// is put on the mLoadingMotions list. Once an animations is loaded, it
	// will be initialized and put on the mLoadedMotions list. Any animation
	// that is currently playing also sits in the mActiveMotions list.

	typedef fast_hmap<LLUUID, LLMotion*> motion_map_t;
	motion_map_t		mAllMotions;

	motion_set_t		mLoadingMotions;
	motion_set_t		mLoadedMotions;
	motion_list_t		mActiveMotions;
	motion_set_t		mDeprecatedMotions;

	LLFrameTimer		mTimer;
	F32					mPrevTimerElapsed;
	F32					mAnimTime;
	F32					mLastTime;
	bool				mHasRunOnce;
	bool				mPaused;
	U32					mPausedFrame;
	F32					mTimeStep;
	S32					mTimeStepCount;
	F32					mLastInterp;

	U8					mJointSignature[2][LL_CHARACTER_MAX_ANIMATED_JOINTS];

	// Value to use for initialization of mTimeFactorMultiplier
	static F32			sTimeFactorMultiplier;
	static LLMotionRegistry	sRegistry;

private:
	static uuid_vec_t sMotionsToKill;
};

//-----------------------------------------------------------------------------
// Class declaractions
//-----------------------------------------------------------------------------
#include "llcharacter.h"

#endif // LL_LLMOTIONCONTROLLER_H
