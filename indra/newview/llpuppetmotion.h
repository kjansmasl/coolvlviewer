/**
 * @file llpuppetmotion.h
 * @brief Declaration of the LLPuppetMotion class.
 *
 * $LicenseInfo:firstyear=2021&license=viewergpl$
 *
 * Copyright (c) 2021-2022, Linden Research, Inc.
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

#ifndef LL_LLPUPPETMOTION_H
#define LL_LLPUPPETMOTION_H

#include <deque>
#include <map>
#include <vector>

#include "lldatapacker.h"
#include "llframetimer.h"
#include "llik.h"
#include "llmotion.h"
#include "llpointer.h"
#include "llquaternion.h"

class LLMessageSystem;
class LLViewerRegion;

///////////////////////////////////////////////////////////////////////////////
// LLPuppet*Event classes (reside in llpuppetevent.h in LL's viewer sources).
// While they are only used internally by LLPuppetMotion, their declarations
// need to be placed here (instead of just in llpuppetmotion.cpp) to prevent
// forward declaration compilation errors in LLPuppetMotion with gcc... HB
///////////////////////////////////////////////////////////////////////////////

// Information about an expression event that we want to broadcast
class LLPuppetJointEvent
{
protected:
	LOG_CLASS(LLPuppetJointEvent);

public:
	enum E_REFERENCE_FRAME
	{
		ROOT_FRAME = 0,
		PARENT_FRAME = 1
	};

	LL_INLINE LLPuppetJointEvent()
	:	mJointID(-1),
		mRefFrame(ROOT_FRAME),
		mMask(0x0),
		mRequestID(-1)
	{
	}

	LL_INLINE void setReferenceFrame(S32 frame)	{ mRefFrame = frame; }

	LL_INLINE void setRotation(const LLQuaternion& rotation)
	{
		mRotation = rotation;
		mRotation.normalize();
		mMask |= mRefFrame == PARENT_FRAME ? LLIK::CONFIG_FLAG_LOCAL_ROT
										   : LLIK::CONFIG_FLAG_TARGET_ROT;
	}

	LL_INLINE void setPosition(const LLVector3& position)
	{
		mPosition = position;
		mMask |= mRefFrame == PARENT_FRAME ? LLIK::CONFIG_FLAG_LOCAL_POS
										   : LLIK::CONFIG_FLAG_TARGET_POS;
	}

	LL_INLINE void setScale(const LLVector3& scale)
	{
		mScale = scale;
		mMask |= LLIK::CONFIG_FLAG_LOCAL_SCALE;
	}

	LL_INLINE void disableConstraint()
	{
		mMask |= LLIK::CONFIG_FLAG_DISABLE_CONSTRAINT;
	}

	LL_INLINE void enableReporting(S32 reqid)
	{
		mMask |= LLIK::CONFIG_FLAG_ENABLE_REPORTING;
		mRequestID = reqid;
	}

	LL_INLINE S32 getRequestID() const			{ return mRequestID; }

	LL_INLINE void setJointID(S32 id)			{ mJointID = S16(id); }
	LL_INLINE S16 getJointID() const			{ return mJointID; }
	LL_INLINE LLQuaternion getRotation() const	{ return mRotation; }
	LL_INLINE LLVector3 getPosition() const		{ return mPosition; }
	LL_INLINE LLVector3 getScale() const		{ return mScale; }

	size_t getSize() const;
	size_t pack(U8* wptr) const;
	size_t unpack(U8* wptr);

	void interpolate(F32 del, const LLPuppetJointEvent& a,
					 const LLPuppetJointEvent& b);

	LL_INLINE bool isEmpty() const				{ return mMask == 0; }
	LL_INLINE U8 getMask() const				{ return mMask; }

private:
	LLQuaternion	mRotation;
	LLVector3		mPosition;
	LLVector3		mScale;
	S32				mRefFrame;
	S32				mRequestID;		// Used for reporting.
	U16				mJointID;
	U8				mMask;
};

// An event is snapshot at mTimestamp (msec from start) with 1 or more joints
// that have moved or rotated. These snapshots along with the time delta are
// used to reconstruct the animation on the receiving clients.
class LLPuppetEvent
{
protected:
	LOG_CLASS(LLPuppetEvent);

public:
	typedef std::deque<LLPuppetJointEvent> joint_deq_t;

	LL_INLINE LLPuppetEvent()
	:	mTimestamp(0)
	{
	}

	LL_INLINE void addJointEvent(const LLPuppetJointEvent& joint_event)
	{
		mJointEvents.emplace_back(joint_event);
	}

	bool pack(LLDataPackerBinaryBuffer& buffer, S32& out_num_joints);
	bool unpack(LLDataPackerBinaryBuffer& mesgsys);

	// For outbound LLPuppetEvents
	LL_INLINE void updateTimestamp()
	{
		mTimestamp = (S32)(LLFrameTimer::getElapsedSeconds() * 1000.0);
	}

	// For inbound LLPuppetEvents we compute a localized timestamp and slam it
	LL_INLINE void setTimestamp(S32 timestamp)	{ mTimestamp = timestamp; }

	LL_INLINE S32 getTimestamp() const			{ return mTimestamp; }
	LL_INLINE U32 getNumJoints() const			{ return mJointEvents.size(); }
	S32 getMinEventSize() const;

public:
	joint_deq_t	mJointEvents;

private:
	S32			mTimestamp;	// In milliseconds
};

///////////////////////////////////////////////////////////////////////////////
// LLPuppetMotion class
///////////////////////////////////////////////////////////////////////////////

class LLPuppetMotion final : public LLMotion
{
protected:
	LOG_CLASS(LLPuppetMotion);

public:
	typedef std::map<S16, LLPointer<LLJointState> > state_map_t;
	typedef std::vector<S16> jointid_vec_t;
	typedef std::deque<LLPuppetEvent> update_deq_t;
	typedef std::vector<LLPuppetJointEvent> joint_events_t;
	using ik_map_t = LLIK::Solver::joint_config_map_t;

	LLPuppetMotion(const LLUUID& id);
	virtual ~LLPuppetMotion() = default;

	class JointStateExpiry
	{
	public:
		LL_INLINE JointStateExpiry()
		:	mExpiry(S32_MAX)
		{
		}

		LL_INLINE JointStateExpiry(LLPointer<LLJointState> state, S32 expiry)
		:	mState(state),
			mExpiry(expiry)
		{
		}

	public:
		LLPointer<LLJointState>	mState;
		S32						mExpiry;
	};

	typedef std::deque<std::pair<S32, LLPuppetJointEvent> > event_queue_t;
	class DelayedEventQueue
	{
	public:
		LL_INLINE DelayedEventQueue()
		:	mLastRemoteTimestamp(-1),
			mEventPeriod(100.f),
			mEventJitter(50.f)
		{
		}

		void addEvent(S32 remote_timestamp, S32 local_timestamp,
					  const LLPuppetJointEvent& event);

		LL_INLINE event_queue_t& getEventQueue()		{ return mQueue; }

	private:
		event_queue_t	mQueue;
		S32				mLastRemoteTimestamp;	// In milliseconds
		// EventPeriod and Jitter are dynamically updated but we start with
		// these optimistic guesses
		F32				mEventPeriod;			// In milliseconds
		F32				mEventJitter;			// In milliseconds
	};
	typedef std::map<S16, DelayedEventQueue> evqueues_map_t;

	void collectJoints(LLJoint* jointp);
	void addExpressionEvent(const LLPuppetJointEvent& event);
	void queueOutgoingEvent(const LLPuppetEvent& event);
	void unpackEvents(LLMessageSystem* mesgsys, int blocknum);
	void clearAll();
	void reportRootRelativePosition(S16 joint_id, S32 request_id);

	// Methods to support MotionController and MotionRegistry
	static void requestPuppetryStatus(LLViewerRegion* regionp);
	static void requestPuppetryStatusCoro(const std::string& capurl);

	// Static constructor: all subclasses must implement such a function and
	// register it.
	LL_INLINE static LLMotion* create(const LLUUID& id)
	{
		return new LLPuppetMotion(id);
	}

	// LLMotion overrides.

	// Motions must specify whether or not they loop
	LL_INLINE bool getLoop() override					{ return false; }

	// Motions must report their total duration
	LL_INLINE F32 getDuration() override				{ return 0.f; }

	// Motions must report their "ease in" duration
	LL_INLINE F32 getEaseInDuration() override			{ return 1.f; }
	// Motions must report their "ease out" duration.
	LL_INLINE F32 getEaseOutDuration() override			{ return 1.f; }

	// Motions must report their priority
	// Note: LLMotion::getPriority() is only used to delegate motion-wide
	// priority to LLJointStates added to mPose in LLMotion::addJointState()...
	// when they have LLJoint::USE_MOTION_PRIORITY.
	LL_INLINE LLJoint::JointPriority getPriority() override
	{
		return mMotionPriority;
	}

	LL_INLINE void setPriority(LLJoint::JointPriority priority)
	{
		mMotionPriority = priority;
	}

	virtual LLMotionBlendType getBlendType() override	{ return NORMAL_BLEND; }

	// Called to determine when a motion should be activated/deactivated based
	// on avatar pixel coverage.
	LL_INLINE F32 getMinPixelArea() override
	{
		return 500.f;
	}

	// Run-time (post constructor) initialization, called after parameters have
	// been set. Must return success status and be available for activation.
	LLMotionInitStatus onInitialize(LLCharacter* charp) override;

	// LLMotionController calls this when it adds this motion to its active
	// list. As of 2022.04.21 the return value is never checked.
	bool onActivate() override;

	// LLMotionController calls this when it removes this motion from its
	// active list.
	void onDeactivate() override;

	// Called per time step. Must return true while it is active, and must
	// return false when the motion is completed.
	bool onUpdate(F32 time, U8* joint_mask) override;

	LL_INLINE bool canDeprecate() override				{ return false; }

	bool needsUpdate() const override;

	// End of overrides

	void addJointToSkeletonData(LLSD& skeleton_sd, LLJoint* joint,
								const LLVector3& parent_rel_pos,
								const LLVector3& tip_rel_end_pos);
	LLSD getSkeletonData();
	void updateSkeletonGeometry();

	LL_INLINE static bool enabled()
	{
		return sIsPuppetryEnabled;
	}

	// Called from llviewercontrol.cpp, on updates to the "PuppetryAllowed"
	// debug setting. HB
	static void updatePuppetryEnabling();

private:
	static void setPuppetryEnabled(bool enabled, size_t event_size);

	void measureArmSpan();

	void queueEvent(const LLPuppetEvent& event);
	void applyEvent(const LLPuppetJointEvent& event, U64 now,
					ik_map_t& configs);
	void applyBroadcastEvent(const LLPuppetJointEvent& event, S32 now);
	void packEvents();
	void pumpOutgoingEvents();
	void solveIKAndHarvestResults(ik_map_t& configs, S32 now);
	void updateFromExpression(S32 now);
	void updateFromBroadcast(S32 now);
	void rememberPosedJoint(S16 joint_id, LLPointer<LLJointState> joint_state,
							S32 now);

private:
	LLFrameTimer			mBroadcastTimer;	// When to broadcast events.
	LLFrameTimer			mPlaybackTimer;		// Playback what was broadcast

	state_map_t				mJointStates;		// Joints known to IK
	evqueues_map_t			mEventQueues;

	// Recently animated joints and their expiries
	typedef std::map<S16, JointStateExpiry> expiries_map_t;
	expiries_map_t			mJointStateExpiries;

	update_deq_t			mOutgoingEvents;	// LLPuppetEvents to broadcast.

	typedef std::vector<LLPointer<LLJointState> > jointstate_vec_t;
	jointstate_vec_t		mJointsToRemoveFromPose;

	joint_events_t			mExpressionEvents;

	LLIK::Solver			mIKSolver;

	LLJoint::JointPriority	mMotionPriority;

	S32						mNextJointStateExpiry;

	F32						mRemoteToLocalClockOffset; // In milliseconds
	F32						mArmSpan;

	bool					mIsSelf;

	// Simulator reported maximum size for a puppetry event.
	static size_t			sPuppeteerEventMaxSize;
	// true when puppetry is enabled on the simulator and not disabled by the
	// user.
	static bool				sIsPuppetryEnabled;
};

#endif
