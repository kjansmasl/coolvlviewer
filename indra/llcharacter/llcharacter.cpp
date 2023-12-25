/**
 * @file llcharacter.cpp
 * @brief Implementation of LLCharacter class.
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

#include "llcharacter.h"

#include "llfasttimer.h"
#include "llstl.h"
#include "llstring.h"

#define SKEL_HEADER "Linden Skeleton 1.0"

LLStringTable LLCharacter::sVisualParamNames(1024);

std::vector<LLCharacter*> LLCharacter::sInstances;

//static
void LLCharacter::initClass()
{
	// Let's avoid memory fragmentation over time...
	sInstances.reserve(256);
}

//static
void LLCharacter::dumpStats()
{
	llinfos << "LLCharacter: sInstances capacity reached: "
			<< sInstances.capacity() << llendl;
}

LLCharacter::LLCharacter()
:	mPreferredPelvisHeight(0.f),
	mSex(SEX_FEMALE),
	mAppearanceSerialNum(0),
	mSkeletonSerialNum(0)
{
	mMotionController.setCharacter(this);
	sInstances.push_back(this);
	mPauseRequest = new LLPauseRequestHandle();
}

LLCharacter::~LLCharacter()
{
	for (LLVisualParam* param = getFirstVisualParam(); param;
		 param = getNextVisualParam())
	{
		delete param;
	}

	mVisualParamIndexMap.clear();
	mVisualParamNameMap.clear();
	mAnimationData.clear();

	for (U32 i = 0, count = mDeferredDeletions.size(); i < count; ++i)
	{
		delete mDeferredDeletions[i];
	}
	mDeferredDeletions.clear();

	for (U32 i = 0, size = sInstances.size(); i < size; ++i)
	{
		if (sInstances[i] == this)
		{
			if (i != size - 1)
			{
				sInstances[i] = sInstances[size - 1];
			}
			sInstances.pop_back();
			break;
		}
	}
}

S32 LLCharacter::getVisualParamCountInGroup(const EVisualParamGroup group) const
{
	S32 rtn = 0;
	for (visual_param_index_map_t::const_iterator
			iter = mVisualParamIndexMap.begin(),
			end = mVisualParamIndexMap.end();
	     iter != end; )
	{
		if ((iter++)->second->getGroup() == group)
		{
			++rtn;
		}
	}
	return rtn;
}

S32 LLCharacter::getVisualParamID(LLVisualParam* id)
{
	for (visual_param_index_map_it_t iter = mVisualParamIndexMap.begin(),
									 end = mVisualParamIndexMap.end();
		 iter != end; ++iter)
	{
		if (iter->second == id)
		{
			return iter->first;
		}
	}
	return 0;
}

LLJoint* LLCharacter::getJoint(U32 key)
{
	LLJoint* joint = NULL;

	LLJoint* root = getRootJoint();
	if (root)
	{
		joint = root->findJoint(key);
	}

	if (!joint)
	{
		llwarns << "Failed to find joint for joint key: " << key << llendl;
	}

	return joint;
}

bool LLCharacter::isMotionActive(const LLUUID& id)
{
	LLMotion* motionp = mMotionController.findMotion(id);
	return motionp && mMotionController.isMotionActive(motionp);
}

//virtual
void LLCharacter::updateMotions(e_update_t update_type)
{
	if (update_type == HIDDEN_UPDATE)
	{
		LL_FAST_TIMER(FTM_UPDATE_HIDDEN_ANIMATION);
		mMotionController.updateMotionsMinimal();
	}
	else
	{
		LL_FAST_TIMER(FTM_UPDATE_ANIMATION);
		// Un-pause if the number of outstanding pause requests has dropped to
		// the initial one
		if (mMotionController.isPaused() && mPauseRequest->getNumRefs() == 1)
		{
			mMotionController.unpauseAllMotions();
		}
		bool force_update = (update_type == FORCE_UPDATE);
		{
			LL_FAST_TIMER(FTM_UPDATE_MOTIONS);
			mMotionController.updateMotions(force_update);
		}
	}
}

void LLCharacter::dumpCharacter(LLJoint* joint)
{
	// Handle top level entry into recursion
	if (!joint)
	{
		llinfos << "DEBUG: Dumping Character @" << this << llendl;
		dumpCharacter(getRootJoint());
		llinfos << "DEBUG: Done." << llendl;
		return;
	}

	// Print joint info
	llinfos << "DEBUG: " << joint->getName() << " ("
			<< (joint->getParent() ? joint->getParent()->getName()
								   : std::string("ROOT")) << ")" << llendl;

	// Recurse
	for (LLJoint::child_list_t::iterator iter = joint->mChildren.begin(),
										 end = joint->mChildren.end();
		 iter != end; ++iter)
	{
		LLJoint* child_joint = *iter;
		dumpCharacter(child_joint);
	}
}

void* LLCharacter::getAnimationData(const std::string& name)
{
	return get_ptr_in_map(mAnimationData, name);
}

bool LLCharacter::setVisualParamWeight(const LLVisualParam* which_param,
									   F32 weight, bool upload_bake)
{
	S32 index = which_param->getID();
	visual_param_index_map_it_t index_iter = mVisualParamIndexMap.find(index);
	if (index_iter != mVisualParamIndexMap.end())
	{
		index_iter->second->setWeight(weight, upload_bake);
		return true;
	}
	return false;
}

bool LLCharacter::setVisualParamWeight(const char* param_name, F32 weight,
									   bool upload_bake)
{
	std::string tname(param_name);
	LLStringUtil::toLower(tname);
	char* tableptr = sVisualParamNames.checkString(tname);
	if (tableptr)
	{
		visual_param_name_map_t::iterator name_iter;
		name_iter = mVisualParamNameMap.find(tableptr);
		if (name_iter != mVisualParamNameMap.end())
		{
			name_iter->second->setWeight(weight, upload_bake);
			return true;
		}
	}
	llwarns << "Invalid visual parameter: " << param_name << llendl;
	return false;
}

bool LLCharacter::setVisualParamWeight(S32 index, F32 weight, bool upload_bake)
{
	visual_param_index_map_it_t index_iter = mVisualParamIndexMap.find(index);
	if (index_iter != mVisualParamIndexMap.end())
	{
		index_iter->second->setWeight(weight, upload_bake);
		return true;
	}
	llwarns << "Invalid visual parameter index: " << index << llendl;
	return false;
}

F32 LLCharacter::getVisualParamWeight(LLVisualParam* which_param)
{
	S32 index = which_param->getID();
	visual_param_index_map_it_t index_iter = mVisualParamIndexMap.find(index);
	if (index_iter != mVisualParamIndexMap.end())
	{
		return index_iter->second->getWeight();
	}

	llwarns << "Invalid visual parameter*, index = " << index << llendl;
	return 0.f;
}

F32 LLCharacter::getVisualParamWeight(const char* param_name)
{
	std::string tname(param_name);
	LLStringUtil::toLower(tname);
	char* tableptr = sVisualParamNames.checkString(tname);
	if (tableptr)
	{
		visual_param_name_map_t::iterator name_iter;
		name_iter = mVisualParamNameMap.find(tableptr);
		if (name_iter != mVisualParamNameMap.end())
		{
			return name_iter->second->getWeight();
		}
	}
	llwarns << "Invalid visual parameter: " << param_name << llendl;
	return 0.f;
}

F32 LLCharacter::getVisualParamWeight(S32 index)
{
	visual_param_index_map_it_t index_iter = mVisualParamIndexMap.find(index);
	if (index_iter != mVisualParamIndexMap.end())
	{
		return index_iter->second->getWeight();
	}

	llwarns << "Invalid visual parameter index: " << index << llendl;
	return 0.f;
}

void LLCharacter::clearVisualParamWeights()
{
	for (LLVisualParam* param = getFirstVisualParam(); param;
		 param = getNextVisualParam())
	{
		if (param->isTweakable())
		{
			param->setWeight(param->getDefaultWeight(), false);
		}
	}
}

LLVisualParam* LLCharacter::getVisualParam(const char* param_name)
{
	std::string tname(param_name);
	LLStringUtil::toLower(tname);
	char* tableptr = sVisualParamNames.checkString(tname);
	if (tableptr)
	{
		visual_param_name_map_t::iterator name_iter;
		name_iter = mVisualParamNameMap.find(tableptr);
		if (name_iter != mVisualParamNameMap.end())
		{
			return name_iter->second;
		}
	}
	llwarns << "Invalid visual parameter: " << param_name << llendl;
	return NULL;
}

void LLCharacter::addSharedVisualParam(LLVisualParam* param)
{
	S32 index = param->getID();
	visual_param_index_map_it_t index_iter = mVisualParamIndexMap.find(index);
	LLVisualParam* current_param = 0;
	if (index_iter != mVisualParamIndexMap.end())
	{
		current_param = index_iter->second;
	}
	if (current_param)
	{
		LLVisualParam* next_param = current_param;
		while (next_param->getNextParam())
		{
			next_param = next_param->getNextParam();
		}
		next_param->setNextParam(param);
	}
	else
	{
		llwarns << "Shared visual parameter " << param->getName()
				<< " does not already exist with ID " << param->getID()
				<< llendl;
	}
}

void LLCharacter::addVisualParam(LLVisualParam* param)
{
	S32 index = param->getID();
	// Add Index map
	std::pair<visual_param_index_map_it_t, bool> idxres =
		mVisualParamIndexMap.emplace(index, param);
	if (!idxres.second)
	{
		visual_param_index_map_it_t index_iter = idxres.first;
		LLVisualParam* old_param = (LLVisualParam*)index_iter->second;
		if (old_param != param)
		{
			if (old_param->getName() == param->getName())
			{
				llinfos << "New visual parameter '" << param->getName()
						<< "' is replacing an existing one with the same ID and name."
						<< llendl;
			}
			else
			{
				llwarns << "New visual parameter '" << param->getName()
						<< "' is replacing an already existing visual parameter '"
						<< old_param->getName() << "' with the same ID."
						<< llendl;
			}
			index_iter->second = param;
#if 0		// *HACK: deleting the old param now would cause a crash when
			// editing the Appearance after a "Rebuild character" action,
			// because the old param is still referenced in the wearables that
			// were loaded while the character rebuild happened... So, we
			// instead store the old param pointer in a vector, and delete it
			// only on character destruction. HB
			delete old_param;
#else
			mDeferredDeletions.push_back(old_param);
#endif
		}
		else
		{
			llwarns << "Visual parameter '" << param->getName()
					<< "' already added !" << llendl;
		}
	}

	if (param->getInfo())
	{
		// Add name map
		std::string tname(param->getName());
		LLStringUtil::toLower(tname);
		char* tableptr = sVisualParamNames.addString(tname);
		std::pair<visual_param_name_map_t::iterator, bool> nameres =
			mVisualParamNameMap.emplace(tableptr, param);
		if (!nameres.second)
		{
			// Already exists, copy param
			visual_param_name_map_t::iterator name_iter = nameres.first;
			name_iter->second = param;
		}
	}
}

void LLCharacter::updateVisualParams()
{
	for (LLVisualParam* param = getFirstVisualParam(); param;
		 param = getNextVisualParam())
	{
		if (param->isAnimating())
		{
			continue;
		}
		// Only apply parameters whose effective weight has changed
		F32 effective_weight = (param->getSex() & mSex) ? param->getWeight()
														: param->getDefaultWeight();
		if (effective_weight != param->getLastWeight())
		{
			param->apply(mSex);
		}
	}
}

LLAnimPauseRequest LLCharacter::requestPause()
{
	mMotionController.pauseAllMotions();
	return mPauseRequest;
}
