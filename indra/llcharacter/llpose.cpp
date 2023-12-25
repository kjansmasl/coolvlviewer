/**
 * @file llpose.cpp
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

#include "linden_common.h"

#include "llpose.h"

#include "llmotion.h"
#include "llmath.h"
#include "llstl.h"

//-----------------------------------------------------------------------------
// LLPose class
//-----------------------------------------------------------------------------

LLPose::LLPose()
:	mWeight(0.f)
{
}

LLJointState* LLPose::getFirstJointState()
{
	mListIter = mJointMap.begin();
	if (mListIter == mJointMap.end())
	{
		return NULL;
	}
	return mListIter->second;
}

LLJointState* LLPose::getNextJointState()
{
	if (++mListIter == mJointMap.end())
	{
		return NULL;
	}
	return mListIter->second;
}

bool LLPose::addJointState(const LLPointer<LLJointState>& jstate)
{
	U32 joint_key = jstate->getJoint()->getKey();
	if (!mJointMap.count(joint_key))
	{
		mJointMap[joint_key] = jstate;
	}
	return true;
}

bool LLPose::removeJointState(const LLPointer<LLJointState>& jstate)
{
	mJointMap.erase(jstate->getJoint()->getKey());
	return true;
}

bool LLPose::removeAllJointStates()
{
	mJointMap.clear();
	return true;
}

LLJointState* LLPose::findJointState(LLJoint* joint)
{
	joint_map_iterator iter = mJointMap.find(joint->getKey());
	if (iter == mJointMap.end())
	{
		return NULL;
	}
	return iter->second;
}

LLJointState* LLPose::findJointState(U32 key)
{
	joint_map_iterator iter = mJointMap.find(key);
	if (iter == mJointMap.end())
	{
		return NULL;
	}
	return iter->second;
}

void LLPose::setWeight(F32 weight)
{
	joint_map_iterator iter;
	for (iter = mJointMap.begin(); iter != mJointMap.end(); ++iter)
	{
		LLJointState* js = iter->second;
		if (js)
		{
			js->setWeight(weight);
		}
	}
	mWeight = weight;
}

//-----------------------------------------------------------------------------
// LLJointStateBlender class
//-----------------------------------------------------------------------------

LLJointStateBlender::LLJointStateBlender()
{
	for (S32 i = 0; i < JSB_NUM_JOINT_STATES; ++i)
	{
		mJointStates[i] = NULL;
		mPriorities[i] = S32_MIN;
		mAdditiveBlends[i] = false;
	}
}

bool LLJointStateBlender::addJointState(const LLPointer<LLJointState>& joint_state,
										S32 priority, bool additive_blend)
{
	llassert(joint_state);

	if (!joint_state->getJoint())
	{
		// This joint state doesn't point to an actual joint, so we do not care
		// about applying it
		return false;
	}

	for (S32 i = 0; i < JSB_NUM_JOINT_STATES; ++i)
	{
		if (mJointStates[i].isNull())
		{
			mJointStates[i] = joint_state;
			mPriorities[i] = priority;
			mAdditiveBlends[i] = additive_blend;
			return true;
		}
		if (priority > mPriorities[i])
		{
			// We are at a higher priority than the current joint state in this
			// slot so shift everyone over previous joint states (newer
			// motions) with same priority and source motion should stay in
			// place
			for (S32 j = JSB_NUM_JOINT_STATES - 1; j > i; --j)
			{
				mJointStates[j] = mJointStates[j - 1];
				mPriorities[j] = mPriorities[j - 1];
				mAdditiveBlends[j] = mAdditiveBlends[j - 1];
			}
			// now store ourselves in this slot
			mJointStates[i] = joint_state;
			mPriorities[i] = priority;
			mAdditiveBlends[i] = additive_blend;
			return true;
		}
	}

	return false;
}

void LLJointStateBlender::blendJointStates(bool apply_now)
{
	// We need at least one joint to blend; if there is one, it will be in slot
	// zero according to insertion logic. Instead of resetting joint state to
	// default, just leave it unchanged from last frame.
	if (mJointStates[0].isNull())
	{
		return;
	}

	LLJoint* target_joint = apply_now ? mJointStates[0]->getJoint()
									  : &mJointCache;

	constexpr S32 POS_WEIGHT = 0;
	constexpr S32 ROT_WEIGHT = 1;
	constexpr S32 SCALE_WEIGHT = 2;

	F32 sum_weights[3];
	U32 sum_usage = 0;

	LLVector3 blended_pos = target_joint->getPosition();
	LLQuaternion blended_rot = target_joint->getRotation();
	LLVector3 blended_scale = target_joint->getScale();

	LLVector3 added_pos, added_scale;
	LLQuaternion added_rot;

	sum_weights[POS_WEIGHT] = 0.f;
	sum_weights[ROT_WEIGHT] = 0.f;
	sum_weights[SCALE_WEIGHT] = 0.f;

	for (S32 joint_state_index = 0;
		 joint_state_index < JSB_NUM_JOINT_STATES &&
		 mJointStates[joint_state_index].notNull();
		 ++joint_state_index)
	{
		LLJointState* jsp = mJointStates[joint_state_index];
		U32 current_usage = jsp->getUsage();
		F32 current_weight = jsp->getWeight();

		if (current_weight == 0.f)
		{
			continue;
		}

		if (mAdditiveBlends[joint_state_index])
		{
			if (current_usage & LLJointState::POS)
			{
				F32 new_weight_sum = llmin(1.f,
										   current_weight +
										   sum_weights[POS_WEIGHT]);

				// Add in pos for this jointstate modulated by weight
				added_pos += jsp->getPosition() *
							 (new_weight_sum - sum_weights[POS_WEIGHT]);
			}

			if (current_usage & LLJointState::SCALE)
			{
				F32 new_weight_sum = llmin(1.f,
										   current_weight +
										   sum_weights[SCALE_WEIGHT]);

				// Add in scale for this jointstate modulated by weight
				added_scale += jsp->getScale() *
							   (new_weight_sum - sum_weights[SCALE_WEIGHT]);
			}

			if (current_usage & LLJointState::ROT)
			{
				F32 new_weight_sum = llmin(1.f,
										   current_weight +
										   sum_weights[ROT_WEIGHT]);

				// Add in rotation for this jointstate modulated by weight
				added_rot = nlerp(new_weight_sum - sum_weights[ROT_WEIGHT],
								  added_rot, jsp->getRotation()) * added_rot;
			}
		}
		else	// Blend two jointstates together
		{
			// Blend position
			if (current_usage & LLJointState::POS)
			{
				if (sum_usage & LLJointState::POS)
				{
					F32 new_weight_sum = llmin(1.f,
											   current_weight +
											   sum_weights[POS_WEIGHT]);

					// Blend positions from both
					blended_pos = lerp(jsp->getPosition(), blended_pos,
									   sum_weights[POS_WEIGHT] /
									   new_weight_sum);
					sum_weights[POS_WEIGHT] = new_weight_sum;
				}
				else
				{
					// Copy position from current
					blended_pos = jsp->getPosition();
					sum_weights[POS_WEIGHT] = current_weight;
				}
			}

			// Now do scale
			if (current_usage & LLJointState::SCALE)
			{
				if (sum_usage & LLJointState::SCALE)
				{
					F32 new_weight_sum = llmin(1.f,
											   current_weight +
											   sum_weights[SCALE_WEIGHT]);

					// blend scales from both
					blended_scale = lerp(jsp->getScale(), blended_scale,
										 sum_weights[SCALE_WEIGHT] /
										 new_weight_sum);
					sum_weights[SCALE_WEIGHT] = new_weight_sum;
				}
				else
				{
					// Copy scale from current
					blended_scale = jsp->getScale();
					sum_weights[SCALE_WEIGHT] = current_weight;
				}
			}

			// Rotation
			if (current_usage & LLJointState::ROT)
			{
				if (sum_usage & LLJointState::ROT)
				{
					F32 new_weight_sum = llmin(1.f,
											   current_weight +
											   sum_weights[ROT_WEIGHT]);

					// Blend rotations from both
					blended_rot = nlerp(sum_weights[ROT_WEIGHT] /
										new_weight_sum, jsp->getRotation(),
										blended_rot);
					sum_weights[ROT_WEIGHT] = new_weight_sum;
				}
				else
				{
					// Copy rotation from current
					blended_rot = jsp->getRotation();
					sum_weights[ROT_WEIGHT] = current_weight;
				}
			}

			// Update resulting usage mask
			sum_usage = sum_usage | current_usage;
		}
	}

	if (!added_scale.isFinite())
	{
		added_scale.clear();
	}

	if (!blended_scale.isFinite())
	{
		blended_scale.set(1, 1, 1);
	}

	// Apply transforms
	target_joint->setPosition(blended_pos + added_pos);
	target_joint->setScale(blended_scale + added_scale);
	target_joint->setRotation(added_rot * blended_rot);

	if (apply_now)
	{
		// Now clear joint states
		for (S32 i = 0; i < JSB_NUM_JOINT_STATES; ++i)
		{
			mJointStates[i] = NULL;
		}
	}
}

void LLJointStateBlender::interpolate(F32 u)
{
	// Only interpolate if we have a joint state
	if (!mJointStates[0])
	{
		return;
	}

	LLJoint* target_joint = mJointStates[0]->getJoint();
	if (!target_joint)
	{
		return;
	}

	target_joint->setPosition(lerp(target_joint->getPosition(),
								   mJointCache.getPosition(), u));
	target_joint->setScale(lerp(target_joint->getScale(),
								mJointCache.getScale(), u));
	target_joint->setRotation(nlerp(u, target_joint->getRotation(),
									mJointCache.getRotation()));
}

void LLJointStateBlender::clear()
{
	for (S32 i = 0; i < JSB_NUM_JOINT_STATES; ++i)
	{
		mJointStates[i] = NULL;
	}
}

void LLJointStateBlender::resetCachedJoint()
{
	if (mJointStates[0])
	{
		LLJoint* source_joint = mJointStates[0]->getJoint();
		if (source_joint)
		{
			mJointCache.setPosition(source_joint->getPosition());
			mJointCache.setScale(source_joint->getScale());
			mJointCache.setRotation(source_joint->getRotation());
		}
	}
}

//-----------------------------------------------------------------------------
// LLPoseBlender class
//-----------------------------------------------------------------------------

LLPoseBlender::LLPoseBlender()
:	mNextPoseSlot(0)
{
}

LLPoseBlender::~LLPoseBlender()
{
	for_each(mJointStateBlenderPool.begin(), mJointStateBlenderPool.end(),
			 DeletePairedPointer());
	mJointStateBlenderPool.clear();
}

bool LLPoseBlender::addMotion(LLMotion* motion)
{
	if (!motion) return false;

	LLPose* pose = motion->getPose();
	if (!pose) return false;

	for (LLJointState* jsp = pose->getFirstJointState(); jsp;
		 jsp = pose->getNextJointState())
	{
		LLJoint* jointp = jsp->getJoint();
		blender_map_t::iterator it = mJointStateBlenderPool.find(jointp);
		LLJointStateBlender* joint_blender;
		if (it == mJointStateBlenderPool.end())
		{
			// This is the first time we are animating this joint, so create a
			// new jointblender and add it to our pool
			joint_blender = new LLJointStateBlender();
			mJointStateBlenderPool[jointp] = joint_blender;
		}
		else
		{
			joint_blender = it->second;
		}

		bool additive = motion->getBlendType() == LLMotion::ADDITIVE_BLEND;
		if (jsp->getPriority() == LLJoint::USE_MOTION_PRIORITY)
		{
			joint_blender->addJointState(jsp, motion->getPriority(), additive);
		}
		else
		{
			joint_blender->addJointState(jsp, jsp->getPriority(), additive);
		}

		// Add it to our list of active blenders
		if (std::find(mActiveBlenders.begin(), mActiveBlenders.end(),
					  joint_blender) == mActiveBlenders.end())
		{
			mActiveBlenders.push_front(joint_blender);
		}
	}

	return true;
}

void LLPoseBlender::blendAndApply()
{
	for (blender_list_t::iterator iter = mActiveBlenders.begin();
		 iter != mActiveBlenders.end(); )
	{
		LLJointStateBlender* jsbp = *iter++;
		if (jsbp)
		{
			jsbp->blendJointStates();
		}
	}

	// We are done now so there are no more active blenders for this frame
	mActiveBlenders.clear();
}

void LLPoseBlender::blendAndCache(bool reset_cached_joints)
{
	for (blender_list_t::iterator iter = mActiveBlenders.begin();
		 iter != mActiveBlenders.end(); ++iter)
	{
		LLJointStateBlender* jsbp = *iter;
		if (!jsbp) continue;

		if (reset_cached_joints)
		{
			jsbp->resetCachedJoint();
		}
		jsbp->blendJointStates(false);
	}
}

void LLPoseBlender::interpolate(F32 u)
{
	for (blender_list_t::iterator iter = mActiveBlenders.begin();
		 iter != mActiveBlenders.end(); ++iter)
	{
		LLJointStateBlender* jsbp = *iter;
		if (jsbp)
		{
			jsbp->interpolate(u);
		}
	}
}

void LLPoseBlender::clearBlenders()
{
	for (blender_list_t::iterator iter = mActiveBlenders.begin();
		 iter != mActiveBlenders.end(); ++iter)
	{
		LLJointStateBlender* jsbp = *iter;
		if (jsbp)
		{
			jsbp->clear();
		}
	}

	mActiveBlenders.clear();
}
