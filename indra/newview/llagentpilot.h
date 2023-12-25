/**
 * @file llagentpilot.h
 * @brief LLAgentPilot class definition
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

#ifndef LL_LLAGENTPILOT_H
#define LL_LLAGENTPILOT_H

#include <vector>

#include "lltimer.h"
#include "stdtypes.h"
#include "llvector3d.h"

// NOTE: all the *AutoPilot* stuff used to be in llagent.*. Moved it here for
// coherency. This class therefore now deals with both the agent auto-pilot and
// the agent pilot recorder/player. HB

class LLAgentPilot final
{
protected:
	LOG_CLASS(LLAgentPilot);

public:
	enum EActionType
	{
		STRAIGHT,
		TURN
	};

	LLAgentPilot();

	LL_INLINE bool isActive() const			{ return mAutoPilot; }

	LL_INLINE LLVector3d getAutoPilotTargetGlobal() const
	{
		return mAutoPilotTargetGlobal;
	}

	void startAutoPilotGlobal(const LLVector3d& pos_global,
							  const std::string& behavior_name = std::string(),
							  const LLQuaternion* target_rotation = NULL,
							  void (*finish_callback)(bool, void*) = NULL,
							  void* callback_data = NULL,
							  F32 stop_distance = 0.f,
							  F32 rotation_threshold = 0.03f,
							  bool allow_flying = true);

	// Returns false if leader_id is null, or auto-pilot is currently active,
	// or leader_id is not an object currently present in the viewer objects
	// list (non-rezzed or inexistent object). Starts following the leader and
	// returns true otherwise.
	bool startFollowPilot(const LLUUID& leader_id, bool allow_flying = true,
						  F32 stop_distance = 0.f);

	void stopAutoPilot(bool user_cancel = false);

	// Auto-pilot walking action, angles in radians
	void autoPilot(F32* delta_yaw);

#if 0	// Not used
	void renderAutoPilotTarget();
#endif

	bool load(const std::string& filename);
	bool save(const std::string& filename);
	static void remove(const std::string& filename);

	LL_INLINE bool isRecording() const		{ return mRecording; }
	LL_INLINE bool isPlaying() const		{ return mPlaying; }
	LL_INLINE bool hasRecord() const		{ return !mActions.empty(); }

	bool startRecord();
	bool stopRecord();
	void addAction(enum EActionType action);

	bool startPlayback(S32 num_runs, bool allow_flying);
	bool stopPlayback();

	void updateTarget();

	// Used only in llviewermenu.cpp, for menu-triggered recorder actions
	static void beginRecord(void*);
	static void endRecord(void*);
	static void forgetRecord(void*);
	static void startPlayback(void*);
	static void stopPlayback(void*);

private:
	class Action
	{
	public:

		EActionType		mType;
		LLVector3d		mTarget;
		F64				mTime;
	};

private:
	LLVector3d			mAutoPilotTargetGlobal;
	LLVector3			mAutoPilotTargetFacing;
	std::string			mAutoPilotBehaviorName;
	LLUUID				mLeaderID;
	void				(*mAutoPilotFinishedCallback)(bool, void*);
	void*				mAutoPilotCallbackData;
	F32					mAutoPilotStopDistance;
	F32					mAutoPilotTargetDist;
	S32					mAutoPilotNoProgressFrameCount;
	F32					mAutoPilotRotationThreshold;

	LLTimer				mTimer;
	std::vector<Action>	mActions;
	S32					mCurrentAction;
	F32					mLastRecordTime;
	S32					mNumRuns;

	bool				mAutoPilot;
	bool				mAutoPilotAllowFlying;
	bool				mAutoPilotFlyOnStop;
	bool				mAutoPilotUseRotation;

	bool				mRecording;
	bool				mStarted;
	bool				mPlaying;
	bool				mAllowFlying;

public:
	// Used only in llviewermenu.cpp, for menu-triggered recorder actions
	static bool			sLoop;
	static bool			sAllowFlying;
};

extern LLAgentPilot gAgentPilot;

#endif // LL_LLAGENTPILOT_H
