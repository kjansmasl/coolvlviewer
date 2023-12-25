/**
 * @file llpose.h
 * @brief Implementation of LLPose class.
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

#ifndef LL_LLPOSE_H
#define LL_LLPOSE_H

#include <map>
#include <string>

#include "lljointstate.h"

class LLPose
{
	friend class LLPoseBlender;

public:
	LLPose();

	// Adds a joint state in this pose
	bool addJointState(const LLPointer<LLJointState>& jstate);

	// Removes a joint state from this pose
	bool removeJointState(const LLPointer<LLJointState>& jstate);

	// Removes all joint states from this pose
	bool removeAllJointStates();

	// Sets weight for all joint states in this pose
	void setWeight(F32 weight);

	// Gets weight for this pose
	LL_INLINE F32 getWeight() const				{ return mWeight; }

	// Returns number of joint states stored in this pose
	LL_INLINE S32 getNumJointStates() const		{ return mJointMap.size(); }

	// Iterate through joint states
	LLJointState* getFirstJointState();
	LLJointState* getNextJointState();
	LLJointState* findJointState(LLJoint* joint);
	LLJointState* findJointState(U32 key);

protected:
	F32					mWeight;

	typedef std::map<U32, LLPointer<LLJointState> > joint_map;
	joint_map			mJointMap;

	typedef joint_map::iterator joint_map_iterator;
	joint_map_iterator	mListIter;
};

constexpr S32 JSB_NUM_JOINT_STATES = 6;

class LLJointStateBlender
{
public:
	LLJointStateBlender();

	void blendJointStates(bool apply_now = true);

	bool addJointState(const LLPointer<LLJointState>& joint_state,
					   S32 priority, bool additive_blend);

	void interpolate(F32 u);
	void clear();
	void resetCachedJoint();

public:
	LLJoint					mJointCache;

protected:
	LLPointer<LLJointState>	mJointStates[JSB_NUM_JOINT_STATES];
	S32						mPriorities[JSB_NUM_JOINT_STATES];
	bool					mAdditiveBlends[JSB_NUM_JOINT_STATES];
};

class LLMotion;

class LLPoseBlender
{
public:
	LLPoseBlender();
	~LLPoseBlender();

	// Requests motion joint states to be added to pose blender joint state
	// records
	bool addMotion(LLMotion* motion);

	// blends all joint states and apply to skeleton
	void blendAndApply();

	// removes all joint state blenders from last time
	void clearBlenders();

	// blends all joint states and cache results
	void blendAndCache(bool reset_cached_joints);

	// interpolates all joints towards cached values
	void interpolate(F32 u);

	LL_INLINE LLPose* getBlendedPose()			{ return &mBlendedPose; }

protected:
	S32				mNextPoseSlot;
	LLPose			mBlendedPose;

	typedef std::map<LLJoint*, LLJointStateBlender*> blender_map_t;
	blender_map_t	mJointStateBlenderPool;

	typedef std::list<LLJointStateBlender*> blender_list_t;
	blender_list_t	mActiveBlenders;
};

#endif // LL_LLPOSE_H
