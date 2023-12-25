/**
 * @file llkeyframemotion.cpp
 * @brief Implementation of LLKeyframeMotion class.
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

#include "linden_common.h"

#include <algorithm>

#include "llkeyframemotion.h"

#include "llanimationstates.h"
#include "llassetstorage.h"
#include "llcharacter.h"
#include "llcriticaldamp.h"
#include "lldatapacker.h"
#include "lldir.h"
#include "llfilesystem.h"
#include "llmath.h"
#include "llquantize.h"

// Static members
std::string	LLKeyframeMotion::sStaticAnimsDir;
LLKeyframeDataCache::data_map_t LLKeyframeDataCache::sKeyframeDataMap;

constexpr F32 JOINT_LENGTH_K = 0.7f;
constexpr S32 MAX_ITERATIONS = 20;
constexpr S32 MIN_ITERATIONS = 1;
constexpr S32 MIN_ITERATION_COUNT = 2;
constexpr F32 MAX_PIXEL_AREA_CONSTRAINTS = 80000.f;
constexpr F32 MIN_PIXEL_AREA_CONSTRAINTS = 1000.f;
constexpr F32 MIN_ACCELERATION_SQUARED = 0.0005f * 0.0005f;

// Normally 10, but the "clap" SL anim is bogus and got 11 constraints
constexpr S32 MAX_CONSTRAINTS = 11;

//-----------------------------------------------------------------------------
// LLKeyframeMotion::JointMotionList sub-class
//-----------------------------------------------------------------------------

LLKeyframeMotion::JointMotionList::JointMotionList()
:	mDuration(0.f),
	mLoopInPoint(0.f),
	mLoopOutPoint(0.f),
	mEaseInDuration(0.f),
	mEaseOutDuration(0.f),
	mBasePriority(LLJoint::LOW_PRIORITY),
	mMaxPriority(LLJoint::LOW_PRIORITY),
	mHandPose(LLHandMotion::HAND_POSE_SPREAD),
	mLoop(false)
{
}

LLKeyframeMotion::JointMotionList::~JointMotionList()
{
	std::for_each(mConstraints.begin(), mConstraints.end(), DeletePointer());
	mConstraints.clear();
	std::for_each(mJointMotionArray.begin(), mJointMotionArray.end(),
				  DeletePointer());
	mJointMotionArray.clear();
}

U32 LLKeyframeMotion::JointMotionList::dumpDiagInfo()
{
	S32	total_size = sizeof(JointMotionList);

	for (U32 i = 0, count = getNumJointMotions(); i < count; ++i)
	{
		JointMotion* joint_motionp = mJointMotionArray[i];

		llinfos << "\tJoint " << joint_motionp->mJointName << llendl;
		if (joint_motionp->mUsage & LLJointState::SCALE)
		{
			llinfos << "    " << joint_motionp->mScaleCurve.mNumKeys
					<< " scale keys at "
					<< joint_motionp->mScaleCurve.mNumKeys * sizeof(ScaleKey)
					<< " bytes" << llendl;

			total_size += joint_motionp->mScaleCurve.mNumKeys *
						  sizeof(ScaleKey);
		}
		if (joint_motionp->mUsage & LLJointState::ROT)
		{
			llinfos <<  "    " << joint_motionp->mRotationCurve.mNumKeys
					<< " rotation keys at "
					<< joint_motionp->mRotationCurve.mNumKeys *
					   sizeof(RotationKey) << " bytes" << llendl;

			total_size += joint_motionp->mRotationCurve.mNumKeys *
						  sizeof(RotationKey);
		}
		if (joint_motionp->mUsage & LLJointState::POS)
		{
			llinfos <<  "    " << joint_motionp->mPositionCurve.mNumKeys
					<< " position keys at "
					<< joint_motionp->mPositionCurve.mNumKeys *
					   sizeof(PositionKey) << " bytes" << llendl;

			total_size += joint_motionp->mPositionCurve.mNumKeys *
						  sizeof(PositionKey);
		}
	}
	llinfos << "Size: " << total_size << " bytes" << llendl;

	return total_size;
}

//-----------------------------------------------------------------------------
// LLKeyframeMotion::ScaleCurve sub-class
//-----------------------------------------------------------------------------

LLKeyframeMotion::ScaleCurve::ScaleCurve()
{
	mInterpolationType = LLKeyframeMotion::IT_LINEAR;
	mNumKeys = 0;
}

LLKeyframeMotion::ScaleCurve::~ScaleCurve()
{
	mKeys.clear();
	mNumKeys = 0;
}

LLVector3 LLKeyframeMotion::ScaleCurve::getValue(F32 time, F32 duration)
{
	LLVector3 value;

	if (mKeys.empty())
	{
		return value;
	}

	key_map_t::iterator right = mKeys.lower_bound(time);
	if (right == mKeys.end())
	{
		// Past last key
		value = (--right)->second.mScale;
	}
	else if (right == mKeys.begin() || right->first == time)
	{
		// Before first key or exactly on a key
		value = right->second.mScale;
	}
	else
	{
		// Between two keys
		key_map_t::iterator left = right;
		F32 index_before = (--left)->first;
		F32 index_after = right->first;
		if (index_after > index_before)
		{
			ScaleKey& scale_before = left->second;
			ScaleKey& scale_after = right->second;
			if (right == mKeys.end())
			{
				scale_after = mLoopInKey;
				index_after = duration;
			}

			F32 u = (time - index_before) / (index_after - index_before);
			value = interp(u, scale_before, scale_after);
		}
		else
		{
			llwarns << "Out of order indexes." << llendl;
			value = right->second.mScale;
		}
	}
	return value;
}

LLVector3 LLKeyframeMotion::ScaleCurve::interp(F32 u, ScaleKey& before,
											   ScaleKey& after)
{
	if (mInterpolationType == IT_STEP)
	{
		return before.mScale;
	}
	else
	{
		return lerp(before.mScale, after.mScale, u);
	}
}

//-----------------------------------------------------------------------------
// LLKeyframeMotion::RotationCurve sub-class
//-----------------------------------------------------------------------------

LLKeyframeMotion::RotationCurve::RotationCurve()
{
	mInterpolationType = LLKeyframeMotion::IT_LINEAR;
	mNumKeys = 0;
}

LLKeyframeMotion::RotationCurve::~RotationCurve()
{
	mKeys.clear();
	mNumKeys = 0;
}

LLQuaternion LLKeyframeMotion::RotationCurve::getValue(F32 time, F32 duration)
{
	if (mKeys.empty())
	{
		return LLQuaternion::DEFAULT;
	}

	LLQuaternion value;

	key_map_t::iterator right = mKeys.lower_bound(time);
	if (right == mKeys.end())
	{
		// Past last key
		value = (--right)->second.mRotation;
	}
	else if (right == mKeys.begin() || right->first == time)
	{
		// Before first key or exactly on a key
		value = right->second.mRotation;
	}
	else
	{
		// Between two keys
		key_map_t::iterator left = right;
		F32 index_before = (--left)->first;
		F32 index_after = right->first;
		if (index_after > index_before)
		{
			RotationKey& rot_before = left->second;
			RotationKey& rot_after = right->second;
			if (right == mKeys.end())
			{
				rot_after = mLoopInKey;
				index_after = duration;
			}

			F32 u = (time - index_before) / (index_after - index_before);
			value = interp(u, rot_before, rot_after);
		}
		else
		{
			llwarns << "Out of order indexes." << llendl;
			value = right->second.mRotation;
		}
	}
	return value;
}

LLQuaternion LLKeyframeMotion::RotationCurve::interp(F32 u,
													 RotationKey& before,
													 RotationKey& after)
{
	if (mInterpolationType == IT_STEP)
	{
		return before.mRotation;
	}
	else
	{
		return nlerp(u, before.mRotation, after.mRotation);
	}
}

//-----------------------------------------------------------------------------
// LLKeyframeMotion::PositionCurve sub-class
//-----------------------------------------------------------------------------

LLKeyframeMotion::PositionCurve::PositionCurve()
{
	mInterpolationType = LLKeyframeMotion::IT_LINEAR;
	mNumKeys = 0;
}

LLKeyframeMotion::PositionCurve::~PositionCurve()
{
	mKeys.clear();
	mNumKeys = 0;
}

LLVector3 LLKeyframeMotion::PositionCurve::getValue(F32 time, F32 duration)
{
	LLVector3 value;

	if (mKeys.empty())
	{
		return value;
	}

	key_map_t::iterator right = mKeys.lower_bound(time);
	if (right == mKeys.end())
	{
		// Past last key
		value = (--right)->second.mPosition;
	}
	else if (right == mKeys.begin() || right->first == time)
	{
		// Before first key or exactly on a key
		value = right->second.mPosition;
	}
	else
	{
		// Between two keys
		key_map_t::iterator left = right;
		F32 index_before = (--left)->first;
		F32 index_after = right->first;
		PositionKey& pos_before = left->second;
		PositionKey& pos_after = right->second;
		if (index_after > index_before)
		{
			if (right == mKeys.end())
			{
				pos_after = mLoopInKey;
				index_after = duration;
			}

			F32 u = (time - index_before) / (index_after - index_before);
			value = interp(u, pos_before, pos_after);
		}
		else
		{
			llwarns << "Out of order indexes." << llendl;
			value = right->second.mPosition;
		}
	}

	llassert(value.isFinite());

	return value;
}

LLVector3 LLKeyframeMotion::PositionCurve::interp(F32 u, PositionKey& before,
												  PositionKey& after)
{
	if (mInterpolationType == IT_STEP)
	{
		return before.mPosition;
	}
	return lerp(before.mPosition, after.mPosition, u);
}

//-----------------------------------------------------------------------------
// LLKeyframeMotion::JointMotion sub-class
//-----------------------------------------------------------------------------

void LLKeyframeMotion::JointMotion::update(LLJointState* joint_state, F32 time,
										   F32 duration)
{
	// This value being 0 is the cause of:
	// https://jira.lindenlab.com/browse/SL-22678
	// but I have not managed to get a stack to see how it got here. Testing
	// for 0 here will stop the crash.
	if (!joint_state)
	{
		return;
	}

	U32 usage = joint_state->getUsage();

	// Update scale component of joint state
	if ((usage & LLJointState::SCALE) && mScaleCurve.mNumKeys)
	{
		joint_state->setScale(mScaleCurve.getValue(time, duration));
	}

	// Update rotation component of joint state
	if ((usage & LLJointState::ROT) && mRotationCurve.mNumKeys)
	{
		joint_state->setRotation(mRotationCurve.getValue(time, duration));
	}

	// Update position component of joint state
	if ((usage & LLJointState::POS) && mPositionCurve.mNumKeys)
	{
		joint_state->setPosition(mPositionCurve.getValue(time, duration));
	}
}

//-----------------------------------------------------------------------------
// LLKeyframeMotion class
//-----------------------------------------------------------------------------

LLKeyframeMotion::LLKeyframeMotion(const LLUUID& id)
:	LLMotion(id),
	mCharacter(NULL),
	mJointMotionList(NULL),
	mPelvisp(NULL),
	mLastSkeletonSerialNum(0),
	mLastUpdateTime(0.f),
	mLastLoopedTime(0.f),
	mAssetStatus(ASSET_UNDEFINED)
{
}

LLKeyframeMotion::~LLKeyframeMotion()
{
	std::for_each(mConstraints.begin(), mConstraints.end(), DeletePointer());
	mConstraints.clear();
}

LLPointer<LLJointState>& LLKeyframeMotion::getJointState(U32 index)
{
	if (LL_UNLIKELY(index >= mJointStates.size()))
	{
		llerrs << "Index " << index << " out of range for motion: "
			   << getName() << " - Maximum was: " << mJointStates.size() - 1
			   << llendl;
	}
	return mJointStates[index];
}

LLJoint* LLKeyframeMotion::getJoint(U32 index)
{
	if (LL_UNLIKELY(index >= mJointStates.size()))
	{
		llwarns_once << "Index " << index << " out of range for motion: "
					 << getName() << " - Maximum is: "
					 << mJointStates.size() - 1 << llendl;
		return NULL;
	}
	return mJointStates[index]->getJoint();
}

LLMotion::LLMotionInitStatus LLKeyframeMotion::onInitialize(LLCharacter* chr)
{
	mCharacter = chr;

	// Asset already loaded ?
	switch (mAssetStatus)
	{
		case ASSET_NEEDS_FETCH:
		{
			// Request asset
			if (!gAssetStoragep)
			{
				llwarns << "No asset storage system. Aborted." << llendl;
				mAssetStatus = ASSET_FETCH_FAILED;
				return STATUS_FAILURE;
			}
			if (mID.isNull())
			{
				llwarns_once << "Attempt to fetch animation " << mName
							 << " with a null Id. Aborted." << llendl;
				mAssetStatus = ASSET_FETCH_FAILED;
				return STATUS_FAILURE;
			}
			mAssetStatus = ASSET_FETCHED;
			LLUUID* character_id = new LLUUID(mCharacter->getID());
			gAssetStoragep->getAssetData(mID, LLAssetType::AT_ANIMATION,
										 onLoadComplete, (void*)character_id,
										 false);
			return STATUS_HOLD;
		}

		case ASSET_FETCHED:
			return STATUS_HOLD;

		case ASSET_FETCH_FAILED:
			return STATUS_FAILURE;

		case ASSET_LOADED:
			return STATUS_SUCCESS;

		default:
			// We do not know what state the asset is in yet, so keep going
			// check keyframe cache first then static cache then asset request
			break;
	}

	LLKeyframeMotion::JointMotionList* joint_motion_list =
		LLKeyframeDataCache::getKeyframeData(getID());
	if (joint_motion_list)
	{
		// Motion already existed in cache, so grab it
		mJointMotionList = joint_motion_list;

		U32 count = mJointMotionList->getNumJointMotions();
		mJointStates.reserve(count);

		// Do not forget to allocate joint states. Set up joint states to point
		// to character joints.
		for (U32 i = 0; i < count; ++i)
		{
			JointMotion* joint_motion = mJointMotionList->getJointMotion(i);
			if (!joint_motion)
			{
				llwarns << "NULL joint motion found !" << llendl;
				continue;
			}

			LLJoint* joint = NULL;
			U32 joint_key = joint_motion->mJointKey;
			if (joint_key)
			{
				joint = mCharacter->getJoint(joint_key);
			}
			if (joint)
			{
				LLPointer<LLJointState> joint_state = new LLJointState;
				mJointStates.push_back(joint_state);
				joint_state->setJoint(joint);
				joint_state->setUsage(joint_motion->mUsage);
				joint_state->setPriority(joint_motion->mPriority);
			}
			else
			{
				// Add dummy joint state with no associated joint
				mJointStates.push_back(new LLJointState);
			}
		}
		mAssetStatus = ASSET_LOADED;
		setupPose();
		return STATUS_SUCCESS;
	}

	// Check that everything is properly initialized...
	if (sStaticAnimsDir.empty())
	{
		sStaticAnimsDir = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER,
														 "anims");
		sStaticAnimsDir += LL_DIR_DELIM_STR;
	}

	bool success = false;
	size_t anim_file_size = 0;
	U8* anim_data = NULL;

	// We first search in the static animation assets bundled with the viewer
	std::string filename = sStaticAnimsDir + mID.asString() + ".lla";
	llstat stat;
	if (!LLFile::stat(filename, &stat))	// If the file exists
	{
		anim_file_size = stat.st_size;
		if (anim_file_size > 0)
		{
			LLFILE* fp = LLFile::open(filename, "rb");
			if (fp)
			{
				anim_data = new (std::nothrow) U8[anim_file_size];
				if (anim_data)
				{
					success = fread(anim_data, 1,
									anim_file_size, fp) == anim_file_size;
				}
				else
				{
					LLMemory::allocationFailed();
					llwarns << "Failed to allocate data buffer (size: "
							<< anim_file_size << " bytes) for animation: " << mID
							<< llendl;
				}
			}
			LLFile::close(fp);
		}
	}

	if (success)
	{
		LL_DEBUGS("KeyFrameMotion") << "Loaded keyframe data from static anim file: "
									<< filename << LL_ENDL;
	}
	else
	// If not a valid static asset, then try the cache...
	{
		// Load named file by concatenating the character prefix with the
		// motion name. Load data into a buffer to be parsed.
		LLFileSystem* anim_file = new LLFileSystem(mID);
		if (!anim_file || !anim_file->getSize())
		{
			delete anim_file;
			// Request asset over network on next call to load
			mAssetStatus = ASSET_NEEDS_FETCH;
			return STATUS_HOLD;
		}

		anim_file_size = anim_file->getSize();
		anim_data = new (std::nothrow) U8[anim_file_size];
		if (anim_data)
		{
			success = anim_file->read(anim_data, anim_file_size);
		}
		else
		{
			LLMemory::allocationFailed();
			llwarns << "Failed to allocate data buffer (size: "
					<< anim_file_size << " bytes) for animation: " << mID
					<< llendl;
		}
		delete anim_file;
	}

	if (!success)
	{
		llwarns << "Cannot open animation file " << mID << llendl;
		mAssetStatus = ASSET_FETCH_FAILED;
		return STATUS_FAILURE;
	}

	LL_DEBUGS("KeyFrameMotion") << "Loading keyframe data for: " << getName()
								<< ":" << getID() << " (" << anim_file_size
								<< " bytes)" << LL_ENDL;

	LLDataPackerBinaryBuffer dp(anim_data, anim_file_size);
	if (!deserialize(dp, mID))
	{
		llwarns << "Failed to decode asset for animation " << getName() << ": "
				<< getID() << llendl;
		mAssetStatus = ASSET_FETCH_FAILED;
		return STATUS_FAILURE;
	}

	delete[] anim_data;

	mAssetStatus = ASSET_LOADED;
	return STATUS_SUCCESS;
}

bool LLKeyframeMotion::setupPose()
{
	if (!mJointMotionList || !mCharacter)
	{
		return false;
	}

	// Add all valid joint states to the pose
	for (U32 jm = 0, count = mJointMotionList->getNumJointMotions();
		 jm < count; ++jm)
	{
		LLPointer<LLJointState> joint_state = getJointState(jm);
		if (joint_state->getJoint())
		{
			addJointState(joint_state);
		}
	}

	// Initialize joint constraints
	for (JointMotionList::constraint_list_t::iterator
			iter = mJointMotionList->mConstraints.begin();
		 iter != mJointMotionList->mConstraints.end(); ++iter)
	{
		JointConstraintSharedData* jcsd = *iter;
		JointConstraint* constraintp = new JointConstraint(jcsd);
		initializeConstraint(constraintp);
		mConstraints.push_front(constraintp);
	}

	if (mJointMotionList->mConstraints.size())
	{
		mPelvisp = mCharacter->getJoint(LL_JOINT_KEY_PELVIS);
		if (!mPelvisp)
		{
			return false;
		}
	}

	// Setup loop keys
	setLoopIn(mJointMotionList->mLoopInPoint);
	setLoopOut(mJointMotionList->mLoopOutPoint);

	return true;
}

bool LLKeyframeMotion::onActivate()
{
	if (!mJointMotionList || !mCharacter)
	{
		return false;
	}

	// If the keyframe anim has an associated emote, trigger it.
	if (mJointMotionList->mEmoteName.length() > 0)
	{
		LLUUID emote_anim_id =
			gAnimLibrary.stringToAnimState(mJointMotionList->mEmoteName);
		// Do not start emote if already active to avoid recursion
		if (!mCharacter->isMotionActive(emote_anim_id))
		{
			mCharacter->startMotion(emote_anim_id);
		}
	}

	mLastLoopedTime = 0.f;

	return true;
}

bool LLKeyframeMotion::onUpdate(F32 time, U8* joint_mask)
{
	if (!mJointMotionList)
	{
		return false;
	}

	if (time - mLastUpdateTime < 0.f)
	{
		mLastUpdateTime = mLastLoopedTime = time;
		LL_DEBUGS("KeyFrameMotion") << "Negative time passed; time delta zeroed."
									<< LL_ENDL;
	}

	if (mJointMotionList->mLoop)
	{
		if (mJointMotionList->mDuration == 0.f)
		{
			time = 0.f;
			mLastLoopedTime = 0.f;
		}
		else if (mStopped)
		{
			mLastLoopedTime = llmin(mJointMotionList->mDuration,
									mLastLoopedTime + time - mLastUpdateTime);
		}
		else if (time > mJointMotionList->mLoopOutPoint)
		{
			if (mJointMotionList->mLoopOutPoint -
				mJointMotionList->mLoopInPoint == 0.f)
			{
				mLastLoopedTime = mJointMotionList->mLoopOutPoint;
			}
			else
			{
				mLastLoopedTime = mJointMotionList->mLoopInPoint +
								  fmod(time - mJointMotionList->mLoopOutPoint,
									   mJointMotionList->mLoopOutPoint -
									   mJointMotionList->mLoopInPoint);
			}
		}
		else
		{
			mLastLoopedTime = time;
		}
	}
	else
	{
		mLastLoopedTime = time;
	}

	applyKeyframes(mLastLoopedTime);

	applyConstraints(mLastLoopedTime, joint_mask);

	mLastUpdateTime = time;

	return mLastLoopedTime <= mJointMotionList->mDuration;
}

void LLKeyframeMotion::applyKeyframes(F32 time)
{
	if (!mJointMotionList || !mCharacter)
	{
		return;
	}

	U32 count = mJointMotionList->getNumJointMotions();
	if (count > mJointStates.size())
	{
		llwarns_once << "More joint states (" << count
					 << ") than joint motion list members ("
					 << mJointStates.size() << "). Aborting update." << llendl;
		return;
	}
	for (U32 i = 0; i < count; ++i)
	{
		JointMotion* joint_motion = mJointMotionList->getJointMotion(i);
		if (!joint_motion)
		{
			llwarns << "NULL joint motion found !" << llendl;
			continue;
		}
		joint_motion->update(mJointStates[i], time,
							 mJointMotionList->mDuration);
	}

	static const std::string hand_pose = "Hand Pose";
	static const std::string hand_pose_prio = "Hand Pose Priority";
	LLJoint::JointPriority* pose_priority  =
		(LLJoint::JointPriority*)mCharacter->getAnimationData(hand_pose_prio);
	if (pose_priority)
	{
		if (mJointMotionList->mMaxPriority >= *pose_priority)
		{
			mCharacter->setAnimationData(hand_pose,
										 &mJointMotionList->mHandPose);
			mCharacter->setAnimationData(hand_pose_prio,
										 &mJointMotionList->mMaxPriority);
		}
	}
	else
	{
		mCharacter->setAnimationData(hand_pose, &mJointMotionList->mHandPose);
		mCharacter->setAnimationData(hand_pose_prio,
									 &mJointMotionList->mMaxPriority);
	}
}

// *TODO: investigate replacing spring simulation with critically damped motion
void LLKeyframeMotion::applyConstraints(F32 time, U8* joint_mask)
{
	// Re-init constraints if skeleton has changed
	if (mCharacter->getSkeletonSerialNum() != mLastSkeletonSerialNum)
	{
		mLastSkeletonSerialNum = mCharacter->getSkeletonSerialNum();
		for (constraint_list_t::iterator iter = mConstraints.begin();
			 iter != mConstraints.end(); ++iter)
		{
			JointConstraint* constraintp = *iter;
			initializeConstraint(constraintp);
		}
	}

	// Apply constraints
	for (constraint_list_t::iterator iter = mConstraints.begin();
		 iter != mConstraints.end(); ++iter)
	{
		JointConstraint* constraintp = *iter;
		applyConstraint(constraintp, time, joint_mask);
	}
}

void LLKeyframeMotion::onDeactivate()
{
	for (constraint_list_t::iterator iter = mConstraints.begin(),
									 end = mConstraints.end();
		 iter != end; ++iter)
	{
		JointConstraint* constraintp = *iter;
		deactivateConstraint(constraintp);
	}
}

// Time is in seconds since character creation
void LLKeyframeMotion::setStopTime(F32 time)
{
	LLMotion::setStopTime(time);

	if (mJointMotionList && mJointMotionList->mLoop &&
		mJointMotionList->mLoopOutPoint != mJointMotionList->mDuration)
	{
		F32 start_loop_time = mActivationTimestamp + mJointMotionList->mLoopInPoint;
		F32 loop_fraction_time;
		if (mJointMotionList->mLoopOutPoint == mJointMotionList->mLoopInPoint)
		{
			loop_fraction_time = 0.f;
		}
		else
		{
			loop_fraction_time = fmod(time - start_loop_time,
									  mJointMotionList->mLoopOutPoint -
									  mJointMotionList->mLoopInPoint);
		}
		mStopTimestamp = llmax(time,
							   time - loop_fraction_time +
							   mJointMotionList->mDuration -
							   mJointMotionList->mLoopInPoint -
							   getEaseOutDuration());
	}
}

void LLKeyframeMotion::initializeConstraint(JointConstraint* constraint)
{
	JointConstraintSharedData* jcsd = constraint->mSharedData;

	LLVector3 source_pos =
		mCharacter->getVolumePos(jcsd->mSourceConstraintVolId,
								 jcsd->mSourceConstraintOffset);
	LLJoint* cur_joint = getJoint(jcsd->mJointStateIndices[0]);
	if (!cur_joint)
	{
		return;
	}

	F32 src_pos_offset = dist_vec(source_pos, cur_joint->getWorldPosition());

	constraint->mTotalLength = constraint->mJointLengths[0] =
		dist_vec(cur_joint->getParent()->getWorldPosition(), source_pos);

	// Grab joint lengths
	for (S32 joint_num = 1, count = jcsd->mChainLength;
		 joint_num < count; ++joint_num)
	{
		cur_joint =
			getJointState(jcsd->mJointStateIndices[joint_num])->getJoint();
		if (!cur_joint)
		{
			return;
		}
		constraint->mJointLengths[joint_num] =
			dist_vec(cur_joint->getWorldPosition(),
					 cur_joint->getParent()->getWorldPosition());
		constraint->mTotalLength += constraint->mJointLengths[joint_num];
	}

	// Store fraction of total chain length so we know how to shear the entire
	// chain towards the goal position
	for (S32 joint_num = 1, count = jcsd->mChainLength;
		 joint_num < count; ++joint_num)
	{
		constraint->mJointLengthFractions[joint_num] =
			constraint->mJointLengths[joint_num] / constraint->mTotalLength;
	}

	// Add last step in chain, from final joint to constraint position
	constraint->mTotalLength += src_pos_offset;

	constraint->mSourceVolume =
		mCharacter->findCollisionVolume(jcsd->mSourceConstraintVolId);
	constraint->mTargetVolume =
		mCharacter->findCollisionVolume(jcsd->mTargetConstraintVolId);
}

void LLKeyframeMotion::activateConstraint(JointConstraint* constraint)
{
	JointConstraintSharedData* jcsd = constraint->mSharedData;
	constraint->mActive = true;

	// Grab ground position if we need to
	if (jcsd->mConstraintTargetType == CONSTRAINT_TARGET_TYPE_GROUND)
	{
		LLVector3 source_pos =
			mCharacter->getVolumePos(jcsd->mSourceConstraintVolId,
									 jcsd->mSourceConstraintOffset);
		LLVector3 ground_pos_agent;
		mCharacter->getGround(source_pos, ground_pos_agent,
							  constraint->mGroundNorm);
		constraint->mGroundPos =
			mCharacter->getPosGlobalFromAgent(ground_pos_agent +
											  jcsd->mTargetConstraintOffset);
	}

	for (S32 joint_num = 1, count = jcsd->mChainLength;
		 joint_num < count; ++joint_num)
	{
		LLJoint* cur_joint = getJoint(jcsd->mJointStateIndices[joint_num]);
		if (!cur_joint)
		{
			return;
		}
		constraint->mPositions[joint_num] =
			(cur_joint->getWorldPosition() - mPelvisp->getWorldPosition()) *
			~mPelvisp->getWorldRotation();
	}

	constraint->mWeight = 1.f;
}

void LLKeyframeMotion::deactivateConstraint(JointConstraint* constraintp)
{
	if (constraintp->mSourceVolume)
	{
		constraintp->mSourceVolume->mUpdateXform = false;
	}

	if (constraintp->mSharedData->mConstraintTargetType !=
			CONSTRAINT_TARGET_TYPE_GROUND)
	{
		if (constraintp->mTargetVolume)
		{
			constraintp->mTargetVolume->mUpdateXform = false;
		}
	}
	constraintp->mActive = false;
}

void LLKeyframeMotion::applyConstraint(JointConstraint* constraint, F32 time,
									   U8* joint_mask)
{
	JointConstraintSharedData* jcsd = constraint->mSharedData;
	if (!jcsd) return;

	S32 chain_length = jcsd->mChainLength;
	LLVector3 positions[MAX_CHAIN_LENGTH];
	const F32* joint_lengths = constraint->mJointLengths;
	LLVector3 velocities[MAX_CHAIN_LENGTH - 1];
	LLQuaternion old_rots[MAX_CHAIN_LENGTH];
	S32 joint_num;

	if (time < jcsd->mEaseInStartTime)
	{
		return;
	}

	if (time > jcsd->mEaseOutStopTime)
	{
		if (constraint->mActive)
		{
			deactivateConstraint(constraint);
		}
		return;
	}

	if (!constraint->mActive || time < jcsd->mEaseInStopTime)
	{
		activateConstraint(constraint);
	}

	LLJoint* root_joint = getJoint(jcsd->mJointStateIndices[chain_length]);
	if (!root_joint)
	{
		return;
	}

	LLVector3 root_pos = root_joint->getWorldPosition();
	root_joint->getParent()->getWorldRotation();

	// Apply underlying keyframe animation to get nominal "kinematic" joint
	// positions
	for (joint_num = 0; joint_num <= chain_length; ++joint_num)
	{
		LLJoint* cur_joint = getJoint(jcsd->mJointStateIndices[joint_num]);
		if (!cur_joint)
		{
			return;
		}
		if (joint_mask[cur_joint->getJointNum()] >= (0xff >> (7 - getPriority())))
		{
			// Skip constraint
			return;
		}
		old_rots[joint_num] = cur_joint->getRotation();
		cur_joint->setRotation(getJointState(jcsd->mJointStateIndices[joint_num])->getRotation());
	}

	LLVector3 keyframe_source_pos =
		mCharacter->getVolumePos(jcsd->mSourceConstraintVolId,
								 jcsd->mSourceConstraintOffset);
	LLVector3 target_pos;
	switch (jcsd->mConstraintTargetType)
	{
		case CONSTRAINT_TARGET_TYPE_GROUND:
			target_pos =
				mCharacter->getPosAgentFromGlobal(constraint->mGroundPos);
			break;

		case CONSTRAINT_TARGET_TYPE_BODY:
			target_pos =
				mCharacter->getVolumePos(jcsd->mTargetConstraintVolId,
										 jcsd->mTargetConstraintOffset);
			break;

		default:
			break;
	}

	if (jcsd->mConstraintType == CONSTRAINT_TYPE_PLANE)
	{
		LLVector3 norm;
		switch (jcsd->mConstraintTargetType)
		{
			case CONSTRAINT_TARGET_TYPE_GROUND:
				norm = constraint->mGroundNorm;
				break;

			case CONSTRAINT_TARGET_TYPE_BODY:
			{
				LLJoint* target_jointp =
					mCharacter->findCollisionVolume(jcsd->mTargetConstraintVolId);
				if (target_jointp)
				{
					// *FIX: do proper normal calculation for stretched spheres
					// (inverse transpose)
					norm = target_pos - target_jointp->getWorldPosition();
				}

				if (norm.isExactlyZero())
				{
					LLJoint* source_jointp =
						mCharacter->findCollisionVolume(jcsd->mSourceConstraintVolId);
					norm = -1.f * jcsd->mSourceConstraintOffset;
					if (source_jointp)
					{
						norm = norm * source_jointp->getWorldRotation();
					}
				}
				norm.normalize();
				break;
			}

			default:
				norm.clear();
				break;
		}

		target_pos = keyframe_source_pos +
					 (norm * ((target_pos - keyframe_source_pos) * norm));
	}

	if (chain_length != 0 &&
		dist_vec_squared(root_pos, target_pos) * 0.95f >
			constraint->mTotalLength * constraint->mTotalLength)
	{
		constraint->mWeight = lerp(constraint->mWeight, 0.f,
								   LLCriticalDamp::getInterpolant(0.1f));
	}
	else
	{
		constraint->mWeight = lerp(constraint->mWeight, 1.f,
								   LLCriticalDamp::getInterpolant(0.3f));
	}

	F32 weight = 1.f;
	if (jcsd->mEaseOutStopTime != 0.f)
	{
		weight = constraint->mWeight *
				 llmin(clamp_rescale(time, jcsd->mEaseInStartTime,
									 jcsd->mEaseInStopTime, 0.f, 1.f),
					   clamp_rescale(time, jcsd->mEaseOutStartTime,
									 jcsd->mEaseOutStopTime, 1.f, 0.f));
	}

	LLVector3 source_to_target = target_pos - keyframe_source_pos;

	if (chain_length)
	{
		LLJoint* end_joint = getJoint(jcsd->mJointStateIndices[0]);
		if (!end_joint)
		{
			return;
		}
		LLQuaternion end_rot = end_joint->getWorldRotation();

		// Slam start and end of chain to the proper positions (rest of chain
		// stays put)
		positions[0] = lerp(keyframe_source_pos, target_pos, weight);
		positions[chain_length] = root_pos;

		// Grab keyframe-specified positions of joints
		for (joint_num = 1; joint_num < chain_length; ++joint_num)
		{
			LLJoint* cur_joint = getJoint(jcsd->mJointStateIndices[joint_num]);
			if (!cur_joint)
			{
				return;
			}
			LLVector3 kinematic_pos =
				cur_joint->getWorldPosition() +
				(source_to_target *
				 constraint->mJointLengthFractions[joint_num]);

			// Convert intermediate joint positions to world coordinates
			positions[joint_num] = constraint->mPositions[joint_num] *
								   mPelvisp->getWorldRotation() +
								   mPelvisp->getWorldPosition();
			F32 tc = 1.f / clamp_rescale(constraint->mFixupDistanceRMS, 0.f,
										 0.5f, 0.2f, 8.f);
			positions[joint_num] = lerp(positions[joint_num], kinematic_pos,
										LLCriticalDamp::getInterpolant(tc,
																	   false));
		}

		S32 max_iter_count =
			ll_roundp(clamp_rescale(mCharacter->getPixelArea(),
									MAX_PIXEL_AREA_CONSTRAINTS,
									MIN_PIXEL_AREA_CONSTRAINTS,
									(F32)MAX_ITERATIONS,
									(F32)MIN_ITERATIONS));
		for (S32 iteration_count = 0; iteration_count < max_iter_count;
			 ++iteration_count)
		{
			S32 num_joints_finished = 0;
			for (joint_num = 1; joint_num < chain_length; ++joint_num)
			{
				// Constraint to child
				LLVector3 acceleration =
					(positions[joint_num - 1] - positions[joint_num]) *
					(dist_vec(positions[joint_num], positions[joint_num - 1]) -
					 joint_lengths[joint_num - 1]) * JOINT_LENGTH_K;
				// Constraint to parent
				acceleration += (positions[joint_num + 1] -
								 positions[joint_num]) *
								(dist_vec(positions[joint_num + 1],
										  positions[joint_num]) -
								 joint_lengths[joint_num]) * JOINT_LENGTH_K;

				if (acceleration.lengthSquared() < MIN_ACCELERATION_SQUARED)
				{
					++num_joints_finished;
				}

				velocities[joint_num - 1] = velocities[joint_num - 1] * 0.7f;
				positions[joint_num] += velocities[joint_num - 1] +
										acceleration * 0.5f;
				velocities[joint_num - 1] += acceleration;
			}

			if (iteration_count >= MIN_ITERATION_COUNT &&
				num_joints_finished == chain_length - 1)
			{
				break;
			}
		}

		for (joint_num = chain_length; joint_num > 0; --joint_num)
		{
			LLJoint* cur_joint = getJoint(jcsd->mJointStateIndices[joint_num]);
			if (!cur_joint)
			{
				return;
			}
			LLJoint* child_joint =
				getJoint(jcsd->mJointStateIndices[joint_num - 1]);
			if (!child_joint)
			{
				return;
			}
			LLQuaternion parent_rot =
				cur_joint->getParent()->getWorldRotation();

			LLQuaternion cur_rot = cur_joint->getWorldRotation();
			LLQuaternion fixup_rot;

			LLVector3 target_at = positions[joint_num - 1] -
								  positions[joint_num];

			// At bottom of chain, use point on collision volume, not joint
			// position
			LLVector3 current_at;
			if (joint_num == 1)
			{
				current_at =
					mCharacter->getVolumePos(jcsd->mSourceConstraintVolId,
											 jcsd->mSourceConstraintOffset) -
					cur_joint->getWorldPosition();
			}
			else
			{
				current_at = child_joint->getPosition() * cur_rot;
			}
			fixup_rot.shortestArc(current_at, target_at);

			LLQuaternion tgt_rot = (cur_rot * fixup_rot) * ~parent_rot;
			if (weight != 1.f)
			{
				LLQuaternion cur_rot =
					getJointState(jcsd->mJointStateIndices[joint_num])->getRotation();
				tgt_rot = nlerp(weight, cur_rot, tgt_rot);
			}

			getJointState(jcsd->mJointStateIndices[joint_num])->setRotation(tgt_rot);
			cur_joint->setRotation(tgt_rot);
		}

		// End local rotation
		LLQuaternion end_loc_rot = end_rot *
								   ~end_joint->getParent()->getWorldRotation();

		if (weight == 1.f)
		{
			getJointState(jcsd->mJointStateIndices[0])->setRotation(end_loc_rot);
		}
		else
		{
			LLQuaternion cur_rot =
				getJointState(jcsd->mJointStateIndices[0])->getRotation();
			getJointState(jcsd->mJointStateIndices[0])->setRotation(nlerp(weight,
																		  cur_rot,
																		  end_loc_rot));
		}

		// Save simulated positions in pelvis-space and calculate total fixup
		// distance
		constraint->mFixupDistanceRMS = 0.f;
		F32 delta_time = llmax(0.02f, fabsf(time - mLastUpdateTime));
		for (joint_num = 1; joint_num < chain_length; ++joint_num)
		{
			LLVector3 new_pos =
				(positions[joint_num] - mPelvisp->getWorldPosition()) *
				~mPelvisp->getWorldRotation();
			constraint->mFixupDistanceRMS +=
				dist_vec_squared(new_pos, constraint->mPositions[joint_num]) /
				delta_time;
			constraint->mPositions[joint_num] = new_pos;
		}
		constraint->mFixupDistanceRMS *= 1.f / (constraint->mTotalLength *
										 (F32)(chain_length - 1));
		constraint->mFixupDistanceRMS = sqrtf(constraint->mFixupDistanceRMS);

		// Reset old joint rots
		for (joint_num = 0; joint_num <= chain_length; ++joint_num)
		{
			LLJoint* cur_joint = getJoint(jcsd->mJointStateIndices[joint_num]);
			if (!cur_joint)
			{
				return;
			}
			cur_joint->setRotation(old_rots[joint_num]);
		}
	}
	// Simple positional constraint (pelvis only)
	else if (getJointState(jcsd->mJointStateIndices[0])->getUsage() &
				LLJointState::POS)
	{
		LLVector3 delta = source_to_target * weight;
		LLPointer<LLJointState> cur_jt_state =
			getJointState(jcsd->mJointStateIndices[0]);
		if (cur_jt_state->getJoint() && cur_jt_state->getJoint()->getParent())
		{
			LLQuaternion parent_rot =
				cur_jt_state->getJoint()->getParent()->getWorldRotation();
			delta = delta * ~parent_rot;
			cur_jt_state->setPosition(cur_jt_state->getJoint()->getPosition() +
									  delta);
		}
	}
}

// NOTE: 'allow_invalid_joints' should be true when handling existing content,
// to avoid breakage. During upload, we should be more restrictive and reject
// such animations.
bool LLKeyframeMotion::deserialize(LLDataPacker& dp, const LLUUID& asset_id,
								   bool allow_invalid_joints)
{
	// Check version
	U16 version;
	if (!dp.unpackU16(version, "version"))
	{
		llwarns << "Cannot read version number" << llendl;
		return false;
	}
	U16 sub_version;
	if (!dp.unpackU16(sub_version, "sub_version"))
	{
		llwarns << "Cannot read sub-version number" << llendl;
		return false;
	}
	bool old_version = false;
	if (version == 0 && sub_version == 1)
	{
		old_version = true;
	}
	else if (version != KEYFRAME_MOTION_VERSION ||
			 sub_version != KEYFRAME_MOTION_SUBVERSION)
	{
		llwarns << "Bad animation version " << version << "." << sub_version
				<< llendl;
		llassert(false);
		return false;
	}

	// Get base priority
	S32 temp_priority;
	if (!dp.unpackS32(temp_priority, "base_priority"))
	{
		llwarns << "Cannot read animation base priority" << llendl;
		return false;
	}
	mJointMotionList = new LLKeyframeMotion::JointMotionList;
	mJointMotionList->mBasePriority = (LLJoint::JointPriority)temp_priority;

	if (mJointMotionList->mBasePriority >= LLJoint::ADDITIVE_PRIORITY)
	{
		mJointMotionList->mBasePriority =
			(LLJoint::JointPriority)((S32)LLJoint::ADDITIVE_PRIORITY - 1);
		mJointMotionList->mMaxPriority = mJointMotionList->mBasePriority;
	}
	else if (mJointMotionList->mBasePriority < LLJoint::USE_MOTION_PRIORITY)
	{
		llwarns << "Bad animation base priority "
				<< mJointMotionList->mBasePriority << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	// Get duration
	if (!dp.unpackF32(mJointMotionList->mDuration, "duration"))
	{
		llwarns << "Cannot read duration" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	if (mJointMotionList->mDuration > ABSOLUTE_MAX_ANIM_DURATION ||
		!llfinite(mJointMotionList->mDuration))
	{
		llwarns << "Invalid animation duration" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	// Get emote (optional)
	if (!dp.unpackString(mJointMotionList->mEmoteName, "emote_name"))
	{
		llwarns << "Cannot read optional emote animation name" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	if (mJointMotionList->mEmoteName==mID.asString())
	{
		llwarns << "Malformed animation mEmoteName==mID" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	// Get loop
	if (!dp.unpackF32(mJointMotionList->mLoopInPoint, "loop_in_point") ||
		!llfinite(mJointMotionList->mLoopInPoint))
	{
		llwarns << "Cannot read loop point" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	if (!dp.unpackF32(mJointMotionList->mLoopOutPoint, "loop_out_point") ||
		!llfinite(mJointMotionList->mLoopOutPoint))
	{
		llwarns << "Cannot read loop point" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	S32 temp;
	if (!dp.unpackS32(temp, "loop"))
	{
		llwarns << "Cannot read loop flag" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}
	mJointMotionList->mLoop = (bool)temp;
	// *HACK: to alter Female_land loop setting, while current behavior won't
	// be changed server-side in SL.
	if (mJointMotionList->mLoop && asset_id == ANIM_AGENT_FEMALE_LAND)
	{
		LL_DEBUGS("KeyFrameMotion") << "Female landing animation looping disabled."
									<< LL_ENDL;
		mJointMotionList->mLoop = false;
	}

	// Get easeIn and easeOut
	if (!dp.unpackF32(mJointMotionList->mEaseInDuration, "ease_in_duration") ||
		!llfinite(mJointMotionList->mEaseInDuration))
	{
		llwarns << "Cannot read ease-in duration" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	if (!dp.unpackF32(mJointMotionList->mEaseOutDuration,
					  "ease_out_duration") ||
		!llfinite(mJointMotionList->mEaseOutDuration))
	{
		llwarns << "Cannot read ease-out duration" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	// Get hand pose
	U32 word;
	if (!dp.unpackU32(word, "hand_pose"))
	{
		llwarns << "Cannot read hand pose" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	if (word > LLHandMotion::NUM_HAND_POSES)
	{
		llwarns << "Invalid LLHandMotion::eHandPose index: " << word << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	mJointMotionList->mHandPose = (LLHandMotion::eHandPose)word;

	// Get number of joint motions
	U32 num_motions = 0;
	if (!dp.unpackU32(num_motions, "num_joints"))
	{
		llwarns << "Cannot read number of joints" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	if (num_motions == 0)
	{
		llwarns << "No joint in animation" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}
	else if (num_motions > LL_CHARACTER_MAX_ANIMATED_JOINTS)
	{
		llwarns << "Too many joints in animation" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	mJointMotionList->mJointMotionArray.clear();
	mJointMotionList->mJointMotionArray.reserve(num_motions);
	mJointStates.clear();
	mJointStates.reserve(num_motions);

	// Initialize joint motions
	for (U32 i = 0; i < num_motions; ++i)
	{
		JointMotion* joint_motion = new JointMotion;
		mJointMotionList->mJointMotionArray.push_back(joint_motion);

		std::string joint_name;
		if (!dp.unpackString(joint_name, "joint_name"))
		{
			llwarns << "Cannot read joint name" << llendl;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (joint_name == "mScreen" || joint_name == "mRoot")
		{
			llwarns << "Attempted to animate special '" << joint_name
					<< "' joint." << llendl;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		// Find the corresponding joint
		U32 joint_key = LLJoint::getAliasedJointKey(joint_name);
		LLJoint* joint = mCharacter->getJoint(joint_key);
		if (joint)
		{
			// Canonical name in case this is an alias.
			joint_name = joint->getName();

			S32 joint_num = joint->getJointNum();
			if (joint_num < 0 ||
				joint_num >= (S32)LL_CHARACTER_MAX_ANIMATED_JOINTS)
			{
				llwarns << "Joint number " << joint_num
						<< " is outside of legal range [0-"
						<< LL_CHARACTER_MAX_ANIMATED_JOINTS
						<< "] and will be omitted from animation for joint: "
						<< joint->getName() << llendl;
				joint = NULL;
			}
		}
		else
		{
			llwarns << "Joint not found: " << joint_name << llendl;
			if (!allow_invalid_joints)
			{
				delete mJointMotionList;
				mJointMotionList = NULL;
				return false;
			}
		}

		joint_motion->mJointName = joint_name;
		joint_motion->mJointKey = joint_key;

		LLPointer<LLJointState> joint_state = new LLJointState;
		mJointStates.push_back(joint_state);
		joint_state->setJoint(joint); // note: can accept NULL
		joint_state->setUsage(0);

		// Get joint priority
		S32 joint_priority;
		if (!dp.unpackS32(joint_priority, "joint_priority"))
		{
			llwarns << "Cannot read joint priority." << llendl;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (joint_priority < LLJoint::USE_MOTION_PRIORITY)
		{
			llwarns << "joint priority unknown - too low." << llendl;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		joint_motion->mPriority = (LLJoint::JointPriority)joint_priority;
		if (joint_priority != LLJoint::USE_MOTION_PRIORITY &&
			joint_priority > mJointMotionList->mMaxPriority)
		{
			mJointMotionList->mMaxPriority =
				(LLJoint::JointPriority)joint_priority;
		}

		joint_state->setPriority((LLJoint::JointPriority)joint_priority);

		// Scan rotation curve header
		if (!dp.unpackS32(joint_motion->mRotationCurve.mNumKeys,
						  "num_rot_keys") ||
			joint_motion->mRotationCurve.mNumKeys < 0)
		{
			llwarns << "Cannot read number of rotation keys" << llendl;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		joint_motion->mRotationCurve.mInterpolationType = IT_LINEAR;
		if (joint_motion->mRotationCurve.mNumKeys != 0)
		{
			joint_state->setUsage(joint_state->getUsage() | LLJointState::ROT);
		}

		// Scan rotation curve keys
		RotationCurve *rCurve = &joint_motion->mRotationCurve;
		for (S32 k = 0; k < joint_motion->mRotationCurve.mNumKeys; ++k)
		{
			F32 time;
			if (old_version)
			{
				if (!dp.unpackF32(time, "time") || !llfinite(time))
				{
					llwarns << "Cannot read rotation key (" << k << ")"
							<< llendl;
					delete mJointMotionList;
					mJointMotionList = NULL;
					return false;
				}
			}
			else
			{
				U16 time_short;
				if (!dp.unpackU16(time_short, "time"))
				{
					llwarns << "Cannot read rotation key (" << k << ")"
							<< llendl;
					delete mJointMotionList;
					mJointMotionList = NULL;
					return false;
				}

				time = U16_to_F32(time_short, 0.f,
								  mJointMotionList->mDuration);
				if (time < 0 || time > mJointMotionList->mDuration)
				{
					llwarns << "invalid frame time" << llendl;
					delete mJointMotionList;
					mJointMotionList = NULL;
					return false;
				}
			}

			RotationKey rot_key;
			rot_key.mTime = time;
			LLVector3 rot_angles;

			bool success = true;

			if (old_version)
			{
				success = dp.unpackVector3(rot_angles, "rot_angles") &&
						  rot_angles.isFinite();

				LLQuaternion::Order ro = StringToOrder("ZYX");
				rot_key.mRotation = mayaQ(rot_angles.mV[VX], rot_angles.mV[VY],
										  rot_angles.mV[VZ], ro);
			}
			else
			{
				U16 x, y, z;
				success &= dp.unpackU16(x, "rot_angle_x");
				success &= dp.unpackU16(y, "rot_angle_y");
				success &= dp.unpackU16(z, "rot_angle_z");

				LLVector3 rot_vec(U16_to_F32(x, -1.f, 1.f),
								  U16_to_F32(y, -1.f, 1.f),
								  U16_to_F32(z, -1.f, 1.f));
				rot_key.mRotation.unpackFromVector3(rot_vec);
			}

			if (!rot_key.mRotation.isFinite())
			{
				llwarns << "Non-finite angle in rotation key" << llendl;
				success = false;
			}

			if (!success)
			{
				llwarns << "Cannot read rotation key (" << k << ")" << llendl;
				delete mJointMotionList;
				mJointMotionList = NULL;
				return false;
			}

			rCurve->mKeys[time] = rot_key;
		}

		// Scan position curve header
		if (!dp.unpackS32(joint_motion->mPositionCurve.mNumKeys,
						  "num_pos_keys") ||
			joint_motion->mPositionCurve.mNumKeys < 0)
		{
			llwarns << "Cannot read number of position keys" << llendl;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		joint_motion->mPositionCurve.mInterpolationType = IT_LINEAR;
		if (joint_motion->mPositionCurve.mNumKeys != 0)
		{
			joint_state->setUsage(joint_state->getUsage() | LLJointState::POS);
		}

		// Scan position curve keys
		PositionCurve* curvep = &joint_motion->mPositionCurve;
		bool is_pelvis = joint_motion->mJointKey == LL_JOINT_KEY_PELVIS;
		for (S32 k = 0; k < joint_motion->mPositionCurve.mNumKeys; ++k)
		{
			U16 time_short;
			PositionKey pos_key;

			if (old_version)
			{
				if (!dp.unpackF32(pos_key.mTime, "time") ||
					!llfinite(pos_key.mTime))
				{
					llwarns << "Cannot read position key (" << k << ")"
							<< llendl;
					delete mJointMotionList;
					mJointMotionList = NULL;
					return false;
				}
			}
			else
			{
				if (!dp.unpackU16(time_short, "time"))
				{
					llwarns << "Cannot read position key (" << k << ")"
							<< llendl;
					delete mJointMotionList;
					mJointMotionList = NULL;
					return false;
				}

				pos_key.mTime = U16_to_F32(time_short, 0.f,
										   mJointMotionList->mDuration);
			}

			bool success = true;

			if (old_version)
			{
				success = dp.unpackVector3(pos_key.mPosition, "pos");
				pos_key.mPosition.mV[VX] = llclamp(pos_key.mPosition.mV[VX],
												   -LL_MAX_PELVIS_OFFSET,
												   LL_MAX_PELVIS_OFFSET);
				pos_key.mPosition.mV[VY] = llclamp(pos_key.mPosition.mV[VY],
												   -LL_MAX_PELVIS_OFFSET,
												   LL_MAX_PELVIS_OFFSET);
				pos_key.mPosition.mV[VZ] = llclamp(pos_key.mPosition.mV[VZ],
												   -LL_MAX_PELVIS_OFFSET,
												   LL_MAX_PELVIS_OFFSET);
			}
			else
			{
				U16 x, y, z;

				success &= dp.unpackU16(x, "pos_x");
				success &= dp.unpackU16(y, "pos_y");
				success &= dp.unpackU16(z, "pos_z");

				pos_key.mPosition.mV[VX] = U16_to_F32(x, -LL_MAX_PELVIS_OFFSET,
													  LL_MAX_PELVIS_OFFSET);
				pos_key.mPosition.mV[VY] = U16_to_F32(y, -LL_MAX_PELVIS_OFFSET,
													  LL_MAX_PELVIS_OFFSET);
				pos_key.mPosition.mV[VZ] = U16_to_F32(z, -LL_MAX_PELVIS_OFFSET,
													  LL_MAX_PELVIS_OFFSET);
			}

			if (!(pos_key.mPosition.isFinite()))
			{
				llwarns << "Non-finite position in key" << llendl;
				success = false;
			}

			if (!success)
			{
				llwarns << "Cannot read position key (" << k << ")" << llendl;
				delete mJointMotionList;
				mJointMotionList = NULL;
				return false;
			}

			curvep->mKeys[pos_key.mTime] = pos_key;

			if (is_pelvis)
			{
				mJointMotionList->mPelvisBBox.addPoint(pos_key.mPosition);
			}
		}

		joint_motion->mUsage = joint_state->getUsage();
	}

	// Get number of constraints
	S32 num_constraints = 0;
	if (!dp.unpackS32(num_constraints, "num_constraints"))
	{
		llwarns << "Cannot read the number of constraints" << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	if (num_constraints > MAX_CONSTRAINTS || num_constraints < 0)
	{
		llwarns << "Bad number of constraints: " << num_constraints << llendl;
		delete mJointMotionList;
		mJointMotionList = NULL;
		return false;
	}

	// Get constraints
	std::string str;
	for (S32 i = 0; i < num_constraints; ++i)
	{
		// Read in constraint data
		JointConstraintSharedData* constraintp = new JointConstraintSharedData;
		U8 byte = 0;

		if (!dp.unpackU8(byte, "chain_length"))
		{
			llwarns << "Cannot read constraint chain length" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}
		constraintp->mChainLength = (S32)byte;

		if ((U32)constraintp->mChainLength >
			mJointMotionList->getNumJointMotions())
		{
			llwarns << "Invalid constraint chain length" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!dp.unpackU8(byte, "constraint_type"))
		{
			llwarns << "Cannot read constraint type" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (byte >= NUM_CONSTRAINT_TYPES)
		{
			llwarns << "Invalid constraint type" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}
		constraintp->mConstraintType = (EConstraintType)byte;

		constexpr S32 BIN_DATA_LENGTH = 16;
		U8 bin_data[BIN_DATA_LENGTH + 1];
		if (!dp.unpackBinaryDataFixed(bin_data, BIN_DATA_LENGTH,
										  "source_volume"))
		{
			llwarns << "Cannot read source volume name" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		bin_data[BIN_DATA_LENGTH] = 0; // Ensure null termination
		str = (char*)bin_data;
		constraintp->mSourceConstraintVolId =
			mCharacter->getCollisionVolumeID(str);
		if (constraintp->mSourceConstraintVolId == -1)
		{
			llwarns << "Not a valid source constraint volume: " << str
					<< llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!dp.unpackVector3(constraintp->mSourceConstraintOffset,
							  "source_offset"))
		{
			llwarns << "Cannot read constraint source offset" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!(constraintp->mSourceConstraintOffset.isFinite()))
		{
			llwarns << "Non-finite constraint source offset" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!dp.unpackBinaryDataFixed(bin_data, BIN_DATA_LENGTH,
									  "target_volume"))
		{
			llwarns << "Cannot read target volume name" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		bin_data[BIN_DATA_LENGTH] = 0; // Ensure null termination
		str = (char*)bin_data;
		if (str == "GROUND")
		{
			// Constrain to ground
			constraintp->mConstraintTargetType = CONSTRAINT_TARGET_TYPE_GROUND;
		}
		else
		{
			constraintp->mConstraintTargetType = CONSTRAINT_TARGET_TYPE_BODY;
			constraintp->mTargetConstraintVolId =
				mCharacter->getCollisionVolumeID(str);
			if (constraintp->mSourceConstraintVolId == -1)
			{
				llwarns << "Not a valid target constraint volume: " << str
						<< llendl;
				delete constraintp;
				delete mJointMotionList;
				mJointMotionList = NULL;
				return false;
			}
		}

		if (!dp.unpackVector3(constraintp->mTargetConstraintOffset,
							  "target_offset"))
		{
			llwarns << "Cannot read constraint target offset" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!(constraintp->mTargetConstraintOffset.isFinite()))
		{
			llwarns << "Non-finite constraint target offset" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!dp.unpackVector3(constraintp->mTargetConstraintDir, "target_dir"))
		{
			llwarns << "Cannot read constraint target direction" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!(constraintp->mTargetConstraintDir.isFinite()))
		{
			llwarns << "Non-finite constraint target direction" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!constraintp->mTargetConstraintDir.isExactlyZero())
		{
			constraintp->mUseTargetOffset = true;
#if 0
			constraintp->mTargetConstraintDir *=
				constraintp->mSourceConstraintOffset.length();
#endif
		}

		if (!dp.unpackF32(constraintp->mEaseInStartTime, "ease_in_start") ||
			!llfinite(constraintp->mEaseInStartTime))
		{
			llwarns << "Cannot read constraint ease in start time" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!dp.unpackF32(constraintp->mEaseInStopTime, "ease_in_stop") ||
			!llfinite(constraintp->mEaseInStopTime))
		{
			llwarns << "Cannot read constraint ease in stop time" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!dp.unpackF32(constraintp->mEaseOutStartTime, "ease_out_start") ||
			!llfinite(constraintp->mEaseOutStartTime))
		{
			llwarns << "Cannot read constraint ease out start time" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		if (!dp.unpackF32(constraintp->mEaseOutStopTime, "ease_out_stop") ||
			!llfinite(constraintp->mEaseOutStopTime))
		{
			llwarns << "Cannot read constraint ease out stop time" << llendl;
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}

		mJointMotionList->mConstraints.push_front(constraintp);

		// Note: mChainLength is size-limited (as it comes from a byte)
		constraintp->mJointStateIndices =
			new S32[constraintp->mChainLength + 1];

		// Get joint to which this collision volume is attached
		LLJoint* joint =
			mCharacter->findCollisionVolume(constraintp->mSourceConstraintVolId);
		if (!joint)
		{
			delete constraintp;
			delete mJointMotionList;
			mJointMotionList = NULL;
			return false;
		}
		for (S32 i = 0, count = constraintp->mChainLength + 1; i < count; ++i)
		{
			LLJoint* parent = joint->getParent();
			if (!parent)
			{
				llwarns << "Joint with no parent: " << joint->getName()
						<< " Emote: " << mJointMotionList->mEmoteName
						<< llendl;
				delete constraintp;
				delete mJointMotionList;
				mJointMotionList = NULL;
				return false;
			}
			joint = parent;
			constraintp->mJointStateIndices[i] = -1;
			for (U32 j = 0, count2 = mJointMotionList->getNumJointMotions();
				 j < count2; ++j)
			{
				LLJoint* constraint_joint = getJoint(j);
				if (!constraint_joint)
				{
					llwarns << "Invalid joint " << j << llendl;
					delete constraintp;
					delete mJointMotionList;
					mJointMotionList = NULL;
					return false;
				}
				if (constraint_joint == joint)
				{
					constraintp->mJointStateIndices[i] = (S32)j;
					break;
				}
			}
			if (constraintp->mJointStateIndices[i] < 0)
			{
				llwarns << "No joint index for constraint " << i << llendl;
				delete constraintp;
				delete mJointMotionList;
				mJointMotionList = NULL;
				return false;
			}
		}
	}

	// *FIX: support cleanup of old keyframe data
	LLKeyframeDataCache::addKeyframeData(getID(),  mJointMotionList);
	mAssetStatus = ASSET_LOADED;

	setupPose();

	return true;
}

bool LLKeyframeMotion::serialize(LLDataPacker& dp) const
{
	if (!mJointMotionList || !mCharacter)
	{
		llwarns << "Trying to set serialize a motion without a motion list"
				<< llendl;
		llassert(false);
		return false;
	}

	bool success = true;
	success &= dp.packU16(KEYFRAME_MOTION_VERSION, "version");
	success &= dp.packU16(KEYFRAME_MOTION_SUBVERSION, "sub_version");
	success &= dp.packS32(mJointMotionList->mBasePriority, "base_priority");
	success &= dp.packF32(mJointMotionList->mDuration, "duration");
	success &= dp.packString(mJointMotionList->mEmoteName, "emote_name");
	success &= dp.packF32(mJointMotionList->mLoopInPoint, "loop_in_point");
	success &= dp.packF32(mJointMotionList->mLoopOutPoint, "loop_out_point");
	success &= dp.packS32(mJointMotionList->mLoop, "loop");
	success &= dp.packF32(mJointMotionList->mEaseInDuration,
						  "ease_in_duration");
	success &= dp.packF32(mJointMotionList->mEaseOutDuration,
						  "ease_out_duration");
	success &= dp.packU32(mJointMotionList->mHandPose, "hand_pose");
	success &= dp.packU32(mJointMotionList->getNumJointMotions(),
						  "num_joints");

	LL_DEBUGS("KeyFrameMotion") << "Serialized: version: "
								<< KEYFRAME_MOTION_VERSION
								<< " - sub_version: "
								<< KEYFRAME_MOTION_SUBVERSION
								<< " - base_priority: "
								<< mJointMotionList->mBasePriority
								<< " - duration: "
								<< mJointMotionList->mDuration
								<< " - emote_name: "
								<< mJointMotionList->mEmoteName
								<< " - loop_in_point: "
								<< mJointMotionList->mLoopInPoint
								<< " - loop_out_point: "
								<< mJointMotionList->mLoopOutPoint
								<< " - loop: " << mJointMotionList->mLoop
								<< " - ease_in_duration: "
								<< mJointMotionList->mEaseInDuration
								<< " - ease_out_duration: "
								<< mJointMotionList->mEaseOutDuration
								<< " - hand_pose: "
								<< mJointMotionList->mHandPose
								<< " - num_joints: "
								<< mJointMotionList->getNumJointMotions()
								<< LL_ENDL;

	for (U32 i = 0; i < mJointMotionList->getNumJointMotions(); ++i)
	{
		JointMotion* joint_motionp = mJointMotionList->getJointMotion(i);
		if (!joint_motionp)
		{
			llwarns << "NULL joint motion found !" << llendl;
			continue;
		}
		success &= dp.packString(joint_motionp->mJointName, "joint_name");
		success &= dp.packS32(joint_motionp->mPriority, "joint_priority");
		success &= dp.packS32(joint_motionp->mRotationCurve.mNumKeys,
							  "num_rot_keys");

		LL_DEBUGS("KeyFrameMotion") << "Joint: " << joint_motionp->mJointName
									<< LL_ENDL;
		for (RotationCurve::key_map_t::iterator
				iter = joint_motionp->mRotationCurve.mKeys.begin(),
				end = joint_motionp->mRotationCurve.mKeys.end();
			 iter != end; ++iter)
		{
			RotationKey& rot_key = iter->second;
			U16 time_short = F32_to_U16(rot_key.mTime, 0.f,
										mJointMotionList->mDuration);
			success &= dp.packU16(time_short, "time");

			LLVector3 rot_angles = rot_key.mRotation.packToVector3();
			rot_angles.quantize16(-1.f, 1.f, -1.f, 1.f);

			U16 x = F32_to_U16(rot_angles.mV[VX], -1.f, 1.f);
			U16 y = F32_to_U16(rot_angles.mV[VY], -1.f, 1.f);
			U16 z = F32_to_U16(rot_angles.mV[VZ], -1.f, 1.f);
			success &= dp.packU16(x, "rot_angle_x");
			success &= dp.packU16(y, "rot_angle_y");
			success &= dp.packU16(z, "rot_angle_z");

			LL_DEBUGS("KeyFrameMotion") << " Rot: t=" << rot_key.mTime
										<< " - rotation=" << rot_angles.mV[VX]
										<< "," << rot_angles.mV[VY] << ","
										<< rot_angles.mV[VZ] << LL_ENDL;
		}

		success &= dp.packS32(joint_motionp->mPositionCurve.mNumKeys,
							  "num_pos_keys");
		for (PositionCurve::key_map_t::iterator
				iter = joint_motionp->mPositionCurve.mKeys.begin(),
				end = joint_motionp->mPositionCurve.mKeys.end();
			 iter != end; ++iter)
		{
			PositionKey& pos_key = iter->second;
			U16 time_short = F32_to_U16(pos_key.mTime, 0.f,
										mJointMotionList->mDuration);
			success &= dp.packU16(time_short, "time");

			pos_key.mPosition.quantize16(-LL_MAX_PELVIS_OFFSET,
										 LL_MAX_PELVIS_OFFSET,
										 -LL_MAX_PELVIS_OFFSET,
										 LL_MAX_PELVIS_OFFSET);

			U16 x = F32_to_U16(pos_key.mPosition.mV[VX], -LL_MAX_PELVIS_OFFSET,
							   LL_MAX_PELVIS_OFFSET);
			U16 y = F32_to_U16(pos_key.mPosition.mV[VY], -LL_MAX_PELVIS_OFFSET,
							   LL_MAX_PELVIS_OFFSET);
			U16 z = F32_to_U16(pos_key.mPosition.mV[VZ], -LL_MAX_PELVIS_OFFSET,
							   LL_MAX_PELVIS_OFFSET);
			success &= dp.packU16(x, "pos_x");
			success &= dp.packU16(y, "pos_y");
			success &= dp.packU16(z, "pos_z");

			LL_DEBUGS("KeyFrameMotion") << " Pos: t=" << pos_key.mTime
										<< " - position="
										<< pos_key.mPosition.mV[VX] << ","
										<< pos_key.mPosition.mV[VY] << ","
										<< pos_key.mPosition.mV[VZ] << LL_ENDL;
		}
	}

	success &= dp.packS32(mJointMotionList->mConstraints.size(),
						  "num_constraints");
	LL_DEBUGS("KeyFrameMotion") << "num_constraints: "
								<< mJointMotionList->mConstraints.size()
								<< LL_ENDL;
	for (JointMotionList::constraint_list_t::const_iterator iter =
			mJointMotionList->mConstraints.begin();
		 iter != mJointMotionList->mConstraints.end(); ++iter)
	{
		JointConstraintSharedData* jcsd = *iter;
		success &= dp.packU8(jcsd->mChainLength, "chain_length");
		success &= dp.packU8(jcsd->mConstraintType, "constraint_type");
		char source_volume[16];
		snprintf(source_volume, sizeof(source_volume), "%s",
				 mCharacter->findCollisionVolume(jcsd->mSourceConstraintVolId)->getName().c_str());
		success &= dp.packBinaryDataFixed((U8*)source_volume, 16,
										  "source_volume");
		success &= dp.packVector3(jcsd->mSourceConstraintOffset,
								  "source_offset");
		char target_volume[16];
		if (jcsd->mConstraintTargetType ==
				CONSTRAINT_TARGET_TYPE_GROUND)
		{
			snprintf(target_volume, sizeof(target_volume), "%s", "GROUND");
		}
		else
		{
			snprintf(target_volume, sizeof(target_volume), "%s",
					 mCharacter->findCollisionVolume(jcsd->mTargetConstraintVolId)->getName().c_str());
		}
		success &= dp.packBinaryDataFixed((U8*)target_volume, 16,
										  "target_volume");
		success &= dp.packVector3(jcsd->mTargetConstraintOffset,
								  "target_offset");
		success &= dp.packVector3(jcsd->mTargetConstraintDir, "target_dir");
		success &= dp.packF32(jcsd->mEaseInStartTime, "ease_in_start");
		success &= dp.packF32(jcsd->mEaseInStopTime, "ease_in_stop");
		success &= dp.packF32(jcsd->mEaseOutStartTime, "ease_out_start");
		success &= dp.packF32(jcsd->mEaseOutStopTime, "ease_out_stop");

		LL_DEBUGS("KeyFrameMotion") << " chain_length: " << jcsd->mChainLength
									<< " - constraint_type: "
									<< (S32)jcsd->mConstraintType
									<< " - source_volume: " << source_volume
									<< " - source_offset: "
									<< jcsd->mSourceConstraintOffset
									<< " - target_volume: " << target_volume
									<< " - target_offset: "
									<< jcsd->mTargetConstraintOffset
									<< " - target_dir: "
									<< jcsd->mTargetConstraintDir
									<< " - ease_in_start: "
									<< jcsd->mEaseInStartTime
									<< " - ease_in_stop: "
									<< jcsd->mEaseInStopTime
									<< " - ease_out_start: "
									<< jcsd->mEaseOutStartTime
									<< " - ease_out_stop: "
									<< jcsd->mEaseOutStopTime << LL_ENDL;
	}

	return success;
}

U32	LLKeyframeMotion::getFileSize()
{
	// Serialize into a dummy buffer to calculate required size
	LLDataPackerBinaryBuffer dp;
	serialize(dp);
	return dp.getCurrentSize();
}

bool LLKeyframeMotion::dumpToFile(const std::string& name)
{
	if (!isLoaded())
	{
		llwarns << "Animation not loaded. Cannot write: " << name << llendl;
		return false;
	}

	std::string filename;
	if (!name.empty())
	{
		filename = name;
	}
	else if (!getName().empty())
	{
		filename = getName();
	}
	else
	{
		filename = getID().asString();
	}
	std::string extension = gDirUtilp->getExtension(filename);
	if (extension != "anim" && extension != "tmp")
	{
		filename += ".anim";
	}
	if (gDirUtilp->getDirName(filename).empty())
	{
		filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, filename);
	}
	if (LLFile::isfile(filename))
	{
		llwarns << filename << " already exists. Not overwriting; aborted."
				<< llendl;
		return false;
	}

	bool success = false;

	LL_DEBUGS("KeyFrameMotion") << "Dumping " << filename << LL_ENDL;
	S32 file_size = getFileSize();
	if (file_size > 0)
	{
		U8* buffer = new U8[file_size];
		LLDataPackerBinaryBuffer dp(buffer, file_size);
		if (serialize(dp))
		{
			LLFile outfile(filename, "w+b");
			if (outfile)
			{
				success = outfile.write(buffer, file_size) == file_size;
			}
		}
		delete[] buffer;
	}

	return success;
}

const LLBBoxLocal& LLKeyframeMotion::getPelvisBBox()
{
	return mJointMotionList->mPelvisBBox;
}

void LLKeyframeMotion::setPriority(S32 priority)
{
	if (mJointMotionList)
	{
		S32 priority_delta = priority - mJointMotionList->mBasePriority;
		mJointMotionList->mBasePriority = (LLJoint::JointPriority)priority;
		mJointMotionList->mMaxPriority = mJointMotionList->mBasePriority;

		for (U32 i = 0, count = mJointMotionList->getNumJointMotions();
			 i < count; ++i)
		{
			JointMotion* joint_motion = mJointMotionList->getJointMotion(i);
			if (!joint_motion)
			{
				llwarns << "NULL joint motion found !" << llendl;
				continue;
			}
			joint_motion->mPriority =
				(LLJoint::JointPriority)llclamp((S32)joint_motion->mPriority +
												priority_delta,
												(S32)LLJoint::LOW_PRIORITY,
												(S32)LLJoint::HIGHEST_PRIORITY);
			getJointState(i)->setPriority(joint_motion->mPriority);
		}
	}
}

void LLKeyframeMotion::setEmote(const LLUUID& emote_id)
{
	if (!mJointMotionList)
	{
		return;
	}
	const char* emote_name = gAnimLibrary.animStateToString(emote_id);
	if (emote_name)
	{
		mJointMotionList->mEmoteName = emote_name;
	}
	else
	{
		mJointMotionList->mEmoteName.clear();
	}
}

void LLKeyframeMotion::setEaseIn(F32 ease_in)
{
	if (mJointMotionList)
	{
		mJointMotionList->mEaseInDuration = llmax(ease_in, 0.f);
	}
}

void LLKeyframeMotion::setEaseOut(F32 ease_in)
{
	if (mJointMotionList)
	{
		mJointMotionList->mEaseOutDuration = llmax(ease_in, 0.f);
	}
}

void LLKeyframeMotion::flushKeyframeCache()
{
#if 0	// TODO: Make this safe to do
	LLKeyframeDataCache::clear();
#endif
}

void LLKeyframeMotion::setLoop(bool loop)
{
	if (mJointMotionList)
	{
		mJointMotionList->mLoop = loop;
		mSendStopTimestamp = F32_MAX;
	}
}

void LLKeyframeMotion::setLoopIn(F32 in_point)
{
	if (mJointMotionList)
	{
		mJointMotionList->mLoopInPoint = in_point;

		// Set up loop keys
		for (U32 i = 0, count = mJointMotionList->getNumJointMotions();
			 i < count; ++i)
		{
			JointMotion* joint_motion = mJointMotionList->getJointMotion(i);
			if (!joint_motion)
			{
				llwarns << "NULL joint motion found !" << llendl;
				continue;
			}

			PositionCurve* pos_curve = &joint_motion->mPositionCurve;
			RotationCurve* rot_curve = &joint_motion->mRotationCurve;
			ScaleCurve* scale_curve = &joint_motion->mScaleCurve;

			pos_curve->mLoopInKey.mTime = mJointMotionList->mLoopInPoint;
			rot_curve->mLoopInKey.mTime = mJointMotionList->mLoopInPoint;
			scale_curve->mLoopInKey.mTime = mJointMotionList->mLoopInPoint;

			pos_curve->mLoopInKey.mPosition =
				pos_curve->getValue(mJointMotionList->mLoopInPoint,
									mJointMotionList->mDuration);
			rot_curve->mLoopInKey.mRotation =
				rot_curve->getValue(mJointMotionList->mLoopInPoint,
									mJointMotionList->mDuration);
			scale_curve->mLoopInKey.mScale =
				scale_curve->getValue(mJointMotionList->mLoopInPoint,
									  mJointMotionList->mDuration);
		}
	}
}

void LLKeyframeMotion::setLoopOut(F32 out_point)
{
	if (mJointMotionList)
	{
		mJointMotionList->mLoopOutPoint = out_point;

		// Set up loop keys
		for (U32 i = 0, count = mJointMotionList->getNumJointMotions();
			 i < count; ++i)
		{
			JointMotion* joint_motion = mJointMotionList->getJointMotion(i);
			if (!joint_motion)
			{
				llwarns << "NULL joint motion found !" << llendl;
				continue;
			}

			PositionCurve* pos_curve = &joint_motion->mPositionCurve;
			RotationCurve* rot_curve = &joint_motion->mRotationCurve;
			ScaleCurve* scale_curve = &joint_motion->mScaleCurve;

			pos_curve->mLoopOutKey.mTime = mJointMotionList->mLoopOutPoint;
			rot_curve->mLoopOutKey.mTime = mJointMotionList->mLoopOutPoint;
			scale_curve->mLoopOutKey.mTime = mJointMotionList->mLoopOutPoint;

			pos_curve->mLoopOutKey.mPosition =
				pos_curve->getValue(mJointMotionList->mLoopOutPoint,
									mJointMotionList->mDuration);
			rot_curve->mLoopOutKey.mRotation =
				rot_curve->getValue(mJointMotionList->mLoopOutPoint,
									mJointMotionList->mDuration);
			scale_curve->mLoopOutKey.mScale =
				scale_curve->getValue(mJointMotionList->mLoopOutPoint,
									  mJointMotionList->mDuration);
		}
	}
}

void LLKeyframeMotion::onLoadComplete(const LLUUID& asset_uuid,
									  LLAssetType::EType, void* user_data,
									  S32 status, LLExtStat)
{
	LLUUID* id = (LLUUID*)user_data;

	std::vector<LLCharacter*>::iterator char_iter =
		LLCharacter::sInstances.begin();
	std::vector<LLCharacter*>::iterator char_end =
		LLCharacter::sInstances.end();
	while (char_iter != char_end && (*char_iter)->getID() != *id)
	{
		++char_iter;
	}

	delete id;

	if (char_iter == char_end)
	{
		return;
	}

	LLCharacter* character = *char_iter;

	// Look for an existing instance of this motion
	LLMotion* motionp = character->findMotion(asset_uuid);
	LLKeyframeMotion* kfmotionp = motionp ? motionp->asKeyframeMotion() : NULL;
	if (kfmotionp)
	{
		if (status == 0)
		{
			if (kfmotionp->mAssetStatus == ASSET_LOADED)
			{
				// Asset already loaded
				return;
			}
			LLFileSystem file(asset_uuid);
			S32 size = file.getSize();
			if (size <= 0)
			{
				llwarns << "Empty file for asset Id: " << asset_uuid << llendl;
				return;
			}

			U8* buffer = new U8[size];
			file.read((U8*)buffer, size);

			LL_DEBUGS("KeyFrameMotion") << "Loading keyframe data for: "
										<< kfmotionp->getName() << ":"
										<< kfmotionp->getID() << " (" << size
										<< " bytes)" << LL_ENDL;

			LLDataPackerBinaryBuffer dp(buffer, size);
			if (kfmotionp->deserialize(dp, asset_uuid))
			{
				kfmotionp->mAssetStatus = ASSET_LOADED;
			}
			else
			{
				llwarns << "Failed to decode asset for animation "
						<< kfmotionp->getName() << ":" << kfmotionp->getID()
						<< llendl;
				kfmotionp->mAssetStatus = ASSET_FETCH_FAILED;
			}

			delete[] buffer;
		}
		else
		{
			llwarns << "Failed to load asset for animation "
					<< kfmotionp->getName() << ":" << kfmotionp->getID()
					<< llendl;
			kfmotionp->mAssetStatus = ASSET_FETCH_FAILED;
		}
	}
	else
	{
		llwarns << "No existing motion for asset data, Id: " << asset_uuid
				<< llendl;
	}
}

//--------------------------------------------------------------------
// LLKeyframeDataCache class
//--------------------------------------------------------------------

//static
void LLKeyframeDataCache::removeKeyframeData(const LLUUID& id)
{
	data_map_t::iterator found_data = sKeyframeDataMap.find(id);
	if (found_data != sKeyframeDataMap.end())
	{
		delete found_data->second;
		sKeyframeDataMap.erase(found_data);
	}
}

//static
LLKeyframeMotion::JointMotionList* LLKeyframeDataCache::getKeyframeData(const LLUUID& id)
{
	data_map_t::iterator it = sKeyframeDataMap.find(id);
	return it != sKeyframeDataMap.end() ? it->second : NULL;
}

//static
void LLKeyframeDataCache::clear()
{
	llinfos << "Total cached entries: " << sKeyframeDataMap.size() << llendl;
	for (auto it = sKeyframeDataMap.begin(), end = sKeyframeDataMap.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	sKeyframeDataMap.clear();
	llinfos << "Cache cleared." << llendl;
}

//static
void LLKeyframeDataCache::dumpDiagInfo()
{
	llinfos << "-----------------------------------------------------"
			<< llendl;
	llinfos << "       Global Motion Table" << llendl;
	llinfos << "-----------------------------------------------------"
			<< llendl;

	// Keep track of totals
	U32 total_size = 0;
	// Print each loaded motion list, and its memory usage
	for (data_map_t::iterator it = sKeyframeDataMap.begin(),
							  end = sKeyframeDataMap.end();
		 it != end; ++it)
	{
		LLKeyframeMotion::JointMotionList* motionlistp = it->second;
		llinfos << "Motion: " << it->first << llendl;
		total_size += motionlistp->dumpDiagInfo();
	}

	llinfos << "-----------------------------------------------------"
			<< llendl;
	llinfos << "Total: " << (S32)sKeyframeDataMap.size() << "motions - Size: "
			<< total_size << "Kb" << llendl;
	llinfos << "-----------------------------------------------------"
			<< llendl;
}

//-----------------------------------------------------------------------------
// JointConstraint class
//-----------------------------------------------------------------------------

LLKeyframeMotion::JointConstraint::JointConstraint(JointConstraintSharedData* jcsd)
:	mSharedData(jcsd),
	mSourceVolume(NULL),
	mTargetVolume(NULL),
	mWeight(0.f),
	mTotalLength(0.f),
	mFixupDistanceRMS(0.f),
	mActive(false)
{
	for (S32 i = 0; i < MAX_CHAIN_LENGTH; ++i)
	{
		mJointLengths[i] = 0.f;
		mJointLengthFractions[i] = 0.f;
	}
}
