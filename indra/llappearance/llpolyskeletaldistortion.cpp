/**
 * @file llpolyskeletaldistortion.cpp
 * @brief Implementation of LLPolySkeletalDistortion classes
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

#include "llpolyskeletaldistortion.h"

#include "llavatarappearance.h"
#include "llavatarjoint.h"
#include "llfasttimer.h"
#include "llpolymorph.h"
#include "llpreprocessor.h"
#include "llwearable.h"

//-----------------------------------------------------------------------------
// LLPolySkeletalDistortionInfo() class
//-----------------------------------------------------------------------------

bool LLPolySkeletalDistortionInfo::parseXml(LLXmlTreeNode* node)
{
	llassert(node->hasName("param") && node->getChildByName("param_skeleton"));

	if (!LLViewerVisualParamInfo::parseXml(node))
	{
		return false;
	}

	LLXmlTreeNode* skel_param = node->getChildByName("param_skeleton");
	if (!skel_param)
	{
		llwarns << "Failed to getChildByName(\"param_skeleton\")"
				<< llendl;
		return false;
	}

	for (LLXmlTreeNode* bone = skel_param->getFirstChild(); bone;
		 bone = skel_param->getNextChild())
	{
		if (bone->hasName("bone"))
		{
			std::string name;
			LLVector3 scale;
			LLVector3 pos;
			bool haspos = false;

			static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
			if (!bone->getFastAttributeString(name_string, name))
			{
				llwarns << "No bone name specified for skeletal param."
						<< llendl;
				continue;
			}

			static LLStdStringHandle scale_string = LLXmlTree::addAttributeString("scale");
			if (!bone->getFastAttributeVector3(scale_string, scale))
			{
				llwarns << "No scale specified for bone " << name << "."
						<< llendl;
				continue;
			}

			// optional offset deformation (translation)
			static LLStdStringHandle offset_string = LLXmlTree::addAttributeString("offset");
			if (bone->getFastAttributeVector3(offset_string, pos))
			{
				haspos = true;
			}
			mBoneInfoList.emplace_back(name, scale, pos, haspos);
		}
		else
		{
			llwarns << "Unrecognized element " << bone->getName()
					<< " in skeletal distortion" << llendl;
			continue;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// LLPolySkeletalDistortion() class
//-----------------------------------------------------------------------------

LLPolySkeletalDistortion::LLPolySkeletalDistortion(LLAvatarAppearance* avatarp)
:	LLViewerVisualParam(),
	mDefaultVec(),
	mJointScales(),
	mJointOffsets(),
	mAvatar(avatarp)
{
	mDefaultVec.splat(0.001f);
}

LLPolySkeletalDistortion::LLPolySkeletalDistortion(const LLPolySkeletalDistortion& other)
:	LLViewerVisualParam(other),
	mDefaultVec(other.mDefaultVec),
	mJointScales(other.mJointScales),
	mJointOffsets(other.mJointOffsets),
	mAvatar(other.mAvatar)
{
}

bool LLPolySkeletalDistortion::setInfo(LLPolySkeletalDistortionInfo* info)
{
	if (info->mID < 0)
	{
		return false;
	}

	mInfo = info;
	mID = info->mID;
	setWeight(getDefaultWeight(), false);

	for (LLPolySkeletalDistortionInfo::bone_info_list_t::iterator
				iter = getInfo()->mBoneInfoList.begin(),
				end = getInfo()->mBoneInfoList.end();
		 iter != end; ++iter)
	{
		LLPolySkeletalBoneInfo* bone_info = &(*iter);
		if (!bone_info) continue;	// Paranoia

		LLJoint* joint = mAvatar->getJoint(bone_info->mJointKey);
		if (!joint)
		{
			// There is no point continuing after this error since it means
			// that either the skeleton or lad file is broken.
			llwarns << "Joint " << LLJoint::getName(bone_info->mJointKey)
					<< " not found." << llendl;
			return false;
		}

		// Store it
		mJointScales[joint] = bone_info->mScaleDeformation;

		// Apply to children that need to inherit it
		for (S32 i = 0, count = joint->mChildren.size(); i < count; ++i)
		{
			LLAvatarJoint* child_joint = joint->mChildren[i]->asAvatarJoint();
			if (child_joint && child_joint->inheritScale())
			{
				LLVector3 deformation(child_joint->getScale());
				deformation.scaleVec(bone_info->mScaleDeformation);
				mJointScales[child_joint] = deformation;
			}
		}

		if (bone_info->mHasPositionDeformation)
		{
			mJointOffsets[joint] = bone_info->mPositionDeformation;
		}
	}
	return true;
}

//virtual
LLViewerVisualParam* LLPolySkeletalDistortion::cloneParam(LLWearable* wearable) const
{
	return new LLPolySkeletalDistortion(*this);
}

void LLPolySkeletalDistortion::apply(ESex avatar_sex)
{
	LL_FAST_TIMER(FTM_POLYSKELETAL_DISTORTION_APPLY);

	F32 effective_weight = (getSex() & avatar_sex) ? mCurWeight
												   : getDefaultWeight();

	for (joint_vec_map_t::iterator iter = mJointScales.begin(),
								   end = mJointScales.end();
		 iter != end; ++iter)
	{
		LLJoint* joint = iter->first;
		if (!joint) continue;	// Paranoia

		const LLVector3& scale_delta = iter->second;
		LLVector3 new_scale = joint->getScale() +
							  (effective_weight - mLastWeight) * scale_delta;
		joint->setScale(new_scale, true);
	}

	for (joint_vec_map_t::iterator iter = mJointOffsets.begin(),
								   end = mJointOffsets.end();
		 iter != end; ++iter)
	{
		LLJoint* joint = iter->first;
		if (!joint) continue;	// Paranoia

		const LLVector3& pos_delta = iter->second;
		LLVector3 new_pos = joint->getPosition() +
							effective_weight * pos_delta -
							mLastWeight * pos_delta;
		joint->setPosition(new_pos, true);
	}

	if (mLastWeight != mCurWeight && !mIsAnimating)
	{
		mAvatar->bumpSkeletonSerialNum();
	}
	mLastWeight = mCurWeight;
}

//-----------------------------------------------------------------------------

LLPolyMorphData* clone_morph_param_duplicate(const LLPolyMorphData* src_data,
											 const std::string& name)
{
	LLPolyMorphData* cloned_morph_data = new LLPolyMorphData(*src_data);
	if (!cloned_morph_data || !cloned_morph_data->isSuccesfullyAllocated())
	{
		llwarns << "Failure to clone parameter !" << llendl;
		if (cloned_morph_data)
		{
			delete cloned_morph_data;
		}
		return NULL;
	}

	cloned_morph_data->mName = name;
	for (U32 v = 0, count = cloned_morph_data->mNumIndices; v < count; ++v)
	{
		cloned_morph_data->mCoords[v] = src_data->mCoords[v];
		cloned_morph_data->mNormals[v] = src_data->mNormals[v];
		cloned_morph_data->mBinormals[v] = src_data->mBinormals[v];
	}
	return cloned_morph_data;
}

LLPolyMorphData* clone_morph_param_direction(const LLPolyMorphData* src_data,
											 const LLVector3& direction,
											 const std::string& name)
{
	LLPolyMorphData* cloned_morph_data = new LLPolyMorphData(*src_data);
	if (!cloned_morph_data || !cloned_morph_data->isSuccesfullyAllocated())
	{
		llwarns << "Failure to clone parameter !" << llendl;
		if (cloned_morph_data)
		{
			delete cloned_morph_data;
		}
		return NULL;
	}

	cloned_morph_data->mName = name;
	LLVector4a dir;
	dir.load3(direction.mV);

	for (U32 v = 0, count = cloned_morph_data->mNumIndices; v < count; ++v)
	{
		cloned_morph_data->mCoords[v] = dir;
		cloned_morph_data->mNormals[v].clear();
		cloned_morph_data->mBinormals[v].clear();
	}
	return cloned_morph_data;
}

LLPolyMorphData* clone_morph_param_cleavage(const LLPolyMorphData* src_data,
											F32 scale, const std::string& name)
{
	LLPolyMorphData* cloned_morph_data = new LLPolyMorphData(*src_data);
	if (!cloned_morph_data || !cloned_morph_data->isSuccesfullyAllocated())
	{
		llwarns << "Failure to clone parameter !" << llendl;
		if (cloned_morph_data)
		{
			delete cloned_morph_data;
		}
		return NULL;
	}

	cloned_morph_data->mName = name;

	LLVector4a sc;
	sc.splat(scale);

	LLVector4a nsc;
	nsc.set(scale, -scale, scale, scale);

	for (U32 v = 0, count = cloned_morph_data->mNumIndices; v < count; ++v)
	{
	    if (cloned_morph_data->mCoords[v][1] < 0)
	    {
			cloned_morph_data->mCoords[v].setMul(src_data->mCoords[v], nsc);
			cloned_morph_data->mNormals[v].setMul(src_data->mNormals[v], nsc);
			cloned_morph_data->mBinormals[v].setMul(src_data->mBinormals[v], nsc);
		}
		else
		{
			cloned_morph_data->mCoords[v].setMul(src_data->mCoords[v], sc);
			cloned_morph_data->mNormals[v].setMul(src_data->mNormals[v], sc);
			cloned_morph_data->mBinormals[v].setMul(src_data->mBinormals[v], sc);
		}
	}
	return cloned_morph_data;
}
