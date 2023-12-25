/**
 * @file llmotion.h
 * @brief Implementation of LLMotion class.
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

#ifndef LL_LLMOTION_H
#define LL_LLMOTION_H

#include <string>

#include "llerror.h"
#include "llpose.h"
#include "lluuid.h"

class LLCharacter;
class LLKeyframeMotion;

class LLMotion
{
	friend class LLMotionController;

protected:
	LOG_CLASS(LLMotion);

public:
	enum LLMotionBlendType
	{
		NORMAL_BLEND,
		ADDITIVE_BLEND
	};

	enum LLMotionInitStatus
	{
		STATUS_FAILURE,
		STATUS_SUCCESS,
		STATUS_HOLD
	};

	LLMotion(const LLUUID& id);
	virtual ~LLMotion() = default;

	LL_INLINE virtual LLKeyframeMotion* asKeyframeMotion()
	{
		return NULL;
	}

	// Gets the name of this instance
	LL_INLINE const std::string& getName() const			{ return mName; }

	// Sets the name of this instance
	LL_INLINE void setName(const std::string& name)			{ mName = name; }

	LL_INLINE const LLUUID& getID() const					{ return mID; }

	// Returns the pose associated with the current state of this motion
	LL_INLINE virtual LLPose* getPose()						{ return &mPose;}

	void fadeOut();

	void fadeIn();

	LL_INLINE F32 getFadeWeight() const						{ return mFadeWeight; }

	LL_INLINE F32 getStopTime() const						{ return mStopTimestamp; }

	virtual void setStopTime(F32 time);

	LL_INLINE bool isStopped() const						{ return mStopped; }

	LL_INLINE void setStopped(bool stopped)					{ mStopped = stopped; }

	bool isBlending() const;

	LL_INLINE virtual bool needsUpdate() const				{ return isBlending(); }

	// Activation methods.
	// It is OK for other classes to activate a motion, but only the controller
	// can deactivate it. Thus, if mActive == true, the motion *may* be on the
	// controllers active list, but if mActive == false, the motion is
	// guaranteed not to be on the active list.

	void activate(F32 time);

	LL_INLINE bool isActive() const							{ return mActive; }

protected:
	// Used by LLMotionController only
	void deactivate();

public:
	// Motions must specify whether or not they loop
	virtual bool getLoop() = 0;

	// Motions must report their total duration
	virtual F32 getDuration() = 0;

	// Motions must report their "ease in" duration
	virtual F32 getEaseInDuration() = 0;

	// Motions must report their "ease out" duration.
	virtual F32 getEaseOutDuration() = 0;

	// Motions must report their priority level
	virtual LLJoint::JointPriority getPriority() = 0;

	// Motions must report their blend type
	virtual LLMotionBlendType getBlendType() = 0;

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage.
	virtual F32 getMinPixelArea() = 0;

	// Run-time (post constructor) initialization, called after parameters have
	// been set. Must return true to indicate success and be available for
	// activation.
	virtual LLMotionInitStatus onInitialize(LLCharacter* character) = 0;

	// Called per time step. Must return true while it is active, and must
	// return false when the motion is completed.
	virtual bool onUpdate(F32 active_time, U8* joint_mask) = 0;

	// Called when a motion is deactivated.
	virtual void onDeactivate() = 0;

	// Can we crossfade this motion with a new instance when restarted ?
	// Should ultimately always be true, but lack of emote blending etc
	// requires this
	LL_INLINE virtual bool canDeprecate()					{ return true; }

	// Optional callback routine called when animation deactivated.
	void setDeactivateCallback(void (*cb)(void*), void* userdata);

	// Expose enabled status so the effects of given motion can be turned on
	// or off independently of their active state (only used and overridden so
	// far by LLHeadRotMotion).
	LL_INLINE virtual void enable()							{}
	LL_INLINE virtual void disable()						{}
	LL_INLINE virtual bool isEnabled() const				{ return true; }

protected:
	// Called when a motion is activated. Must return true to indicate success,
	// or else it will be deactivated.
	virtual bool onActivate() = 0;

	void addJointState(const LLPointer<LLJointState>& jointState);

protected:
	LLPose		mPose;

	// These are set implicitly by the motion controller and may be referenced
	// (read only) in the above handlers.
	std::string	mName;
	LLUUID		mID;

	F32			mActivationTimestamp;	// Time when motion was activated
	F32			mStopTimestamp;			// Time when motion was told to stop
	// Time when sim should be told to stop this motion
	F32			mSendStopTimestamp;

	// Blend weight at beginning of stop motion phase
	F32			mResidualWeight;
	F32			mFadeWeight;			// For fading in and out based on LOD

	void		(*mDeactivateCallback)(void*);
	void*		mDeactivateCallbackUserData;

	// Signature of which joints are animated at what priority
	U8			mJointSignature[3][LL_CHARACTER_MAX_ANIMATED_JOINTS];

	bool		mStopped;	// Motion has been stopped;
	// Motion is on active list (can be stopped or not stopped)
	bool		mActive;
};

class LLNullMotion final : public LLMotion
{
public:
	LLNullMotion(const LLUUID& id)
	:	LLMotion(id)
	{
	}

	static LLMotion* create(const LLUUID& id)				{ return new LLNullMotion(id); }

	// Motions must specify whether or not they loop
	LL_INLINE bool getLoop() override						{ return true; }

	// Motions must report their total duration
	LL_INLINE F32 getDuration() override					{ return 1.f; }

	// Motions must report their "ease in" duration
	LL_INLINE F32 getEaseInDuration() override				{ return 0.f; }

	// Motions must report their "ease out" duration.
	LL_INLINE F32 getEaseOutDuration() override				{ return 0.f; }

	// Motions must report their priority level
	LL_INLINE LLJoint::JointPriority getPriority() override	{ return LLJoint::HIGH_PRIORITY; }

	// Motions must report their blend type
	LL_INLINE LLMotionBlendType getBlendType() override		{ return NORMAL_BLEND; }

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage.
	LL_INLINE F32 getMinPixelArea() override				{ return 0.f; }

	// Run-time (post constructor) initialization, called after parameters have
	// been set. Must return true to indicate success and be available for
	// activation.
	LL_INLINE LLMotionInitStatus onInitialize(LLCharacter*) override
	{
		return STATUS_SUCCESS;
	}

	// Called when a motion is activated. Must return true to indicate success,
	// or else it will be deactivated.
	LL_INLINE bool onActivate() override					{ return true; }

	// Called per time step. Must return true while it is active, and must
	// return false when the motion is completed.
	LL_INLINE bool onUpdate(F32, U8*) override				{ return true; }

	// Called when a motion is deactivated
	LL_INLINE void onDeactivate() override					{}
};

#endif // LL_LLMOTION_H
