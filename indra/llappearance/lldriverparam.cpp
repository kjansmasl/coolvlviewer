/**
 * @file lldriverparam.cpp
 * @brief A visual parameter that drives (controls) other visual parameters.
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

#include "linden_common.h"

#include "lldriverparam.h"

#include "llavatarappearance.h"
#include "llwearable.h"
#include "llwearabledata.h"

//-----------------------------------------------------------------------------
// LLDriverParamInfo
//-----------------------------------------------------------------------------

LLDriverParamInfo::LLDriverParamInfo()
:	mDriverParam(NULL)
{
}

bool LLDriverParamInfo::parseXml(LLXmlTreeNode* node)
{
	llassert(node->hasName("param") && node->getChildByName("param_driver"));

	if (!LLViewerVisualParamInfo::parseXml(node))
	{
		return false;
	}

	LLXmlTreeNode* param_driver_node = node->getChildByName("param_driver");
	if (!param_driver_node)
	{
		return false;
	}

	for (LLXmlTreeNode* child = param_driver_node->getChildByName("driven");
		 child; child = param_driver_node->getNextNamedChild())
	{
		S32 driven_id;
		static LLStdStringHandle id_string = LLXmlTree::addAttributeString("id");
		if (child->getFastAttributeS32(id_string, driven_id))
		{
			F32 min1 = mMinWeight;
			F32 max1 = mMaxWeight;
			F32 max2 = max1;
			F32 min2 = max1;

			//	driven    ________							//
			//	^        /|       |\						//
			//	|       / |       | \						//
			//	|      /  |       |  \						//
			//	|     /   |       |   \						//
			//	|    /    |       |    \					//
			//-------|----|-------|----|-------> driver		//
			//  | min1   max1    max2  min2

			static LLStdStringHandle min1_string = LLXmlTree::addAttributeString("min1");
			child->getFastAttributeF32(min1_string, min1); // optional
			static LLStdStringHandle max1_string = LLXmlTree::addAttributeString("max1");
			child->getFastAttributeF32(max1_string, max1); // optional
			static LLStdStringHandle max2_string = LLXmlTree::addAttributeString("max2");
			child->getFastAttributeF32(max2_string, max2); // optional
			static LLStdStringHandle min2_string = LLXmlTree::addAttributeString("min2");
			child->getFastAttributeF32(min2_string, min2); // optional

			// Push these on the front of the deque, so that we can construct
			// them in order later (faster)
			mDrivenInfoList.emplace_front(driven_id, min1, max1, max2, min2);
		}
		else
		{
			llerrs << "<driven> Unable to resolve driven parameter: "
				   << driven_id << llendl;
			return false;
		}
	}
	return true;
}

//virtual
void LLDriverParamInfo::toStream(std::ostream& out)
{
	LLViewerVisualParamInfo::toStream(out);
	out << "driver" << "\t";
	out << mDrivenInfoList.size() << "\t";
	for (entry_info_list_t::iterator iter = mDrivenInfoList.begin(),
									 end = mDrivenInfoList.end();
		 iter != end; ++iter)
	{
		LLDrivenEntryInfo driven = *iter;
		out << driven.mDrivenID << "\t";
	}

	out << std::endl;

	// *FIXME: this mDriverParam backlink makes no sense, because the
	// LLDriverParamInfos are static objects; there is only one copy for each
	// param type, so the backlink will just reference the corresponding param
	// in the most recently created avatar. Apparently these toStream() methods
	// are not currently used anywhere, so it's not an urgent problem.
	llwarns_sparse << "Invalid usage of mDriverParam." << llendl;

	LLAvatarAppearance* appearance = mDriverParam ? mDriverParam->getAvatarAppearance()
												  : NULL;
	if (appearance && appearance->isSelf() && appearance->isValid())
	{
		LLViewerVisualParam* param;
		for (entry_info_list_t::iterator iter = mDrivenInfoList.begin(),
										 end = mDrivenInfoList.end();
			 iter != end; ++iter)
		{
			LLDrivenEntryInfo driven = *iter;
			param = (LLViewerVisualParam*)appearance->getVisualParam(driven.mDrivenID);
			if (param)
			{
				param->getInfo()->toStream(out);
				if (param->getWearableType() != mWearableType)
				{
					if (param->getCrossWearable())
					{
						out << "cross-wearable" << "\t";
					}
					else
					{
						out << "ERROR!" << "\t";
					}
				}
				else
				{
					out << "valid" << "\t";
				}
			}
			else
			{
				llwarns << "Could not get parameter " << driven.mDrivenID
						<< " from avatar " << appearance
						<< " for driver parameter " << getID() << llendl;
			}
			out << std::endl;
		}
	}
}

//-----------------------------------------------------------------------------
// LLDriverParam
//-----------------------------------------------------------------------------

LLDriverParam::LLDriverParam(LLAvatarAppearance* appearance,
							 LLWearable* wearable)
:	LLViewerVisualParam(),
	mDefaultVec(),
	mDriven(),
	mCurrentDistortionParam(NULL),
	mAvatarAppearance(appearance),
	mWearablep(wearable)
{
	llassert(mAvatarAppearance);
	llassert(mWearablep == NULL || mAvatarAppearance->isSelf());
	mDefaultVec.clear();
}

LLDriverParam::LLDriverParam(const LLDriverParam& other)
:	LLViewerVisualParam(other),
	mDefaultVec(other.mDefaultVec),
	mDriven(other.mDriven),
	mCurrentDistortionParam(other.mCurrentDistortionParam),
	mAvatarAppearance(other.mAvatarAppearance),
	mWearablep(other.mWearablep)
{
	llassert(mAvatarAppearance);
	llassert(mWearablep == NULL || mAvatarAppearance->isSelf());
}

bool LLDriverParam::setInfo(LLDriverParamInfo* info)
{
	llassert(mInfo == NULL);
	if (info->mID < 0)
	{
		return false;
	}
	mInfo = info;
	mID = info->mID;
	info->mDriverParam = this;

	setWeight(getDefaultWeight(), false);

	return true;
}

//virtual
LLViewerVisualParam* LLDriverParam::cloneParam(LLWearable* wearable) const
{
	llassert(wearable);
	return new LLDriverParam(*this);
}

void LLDriverParam::setWeight(F32 weight, bool upload_bake)
{
	F32 min_weight = getMinWeight();
	F32 max_weight = getMaxWeight();
	if (mIsAnimating)
	{
		// allow overshoot when animating
		mCurWeight = weight;
	}
	else
	{
		mCurWeight = llclamp(weight, min_weight, max_weight);
	}

	//	driven    ________
	//	^        /|       |\       ^
	//	|       / |       | \      |
	//	|      /  |       |  \     |
	//	|     /   |       |   \    |
	//	|    /    |       |    \   |
	//-------|----|-------|----|-------> driver
	//  | min1   max1    max2  min2

	for (entry_list_t::iterator iter = mDriven.begin(), end = mDriven.end();
		 iter != end; ++iter)
	{
		LLDrivenEntry* driven = &(*iter);
		LLDrivenEntryInfo* info = driven->mInfo;

		LLViewerVisualParam* param = driven->mParam;

		F32 driven_weight = 0.f;
		F32 driven_min = param->getMinWeight();
		F32 driven_max = param->getMaxWeight();

		if (mIsAnimating)
		{
			// driven param doesn't interpolate (textures, for example)
			if (!param->getAnimating())
			{
				continue;
			}
			if (mCurWeight < info->mMin1)
			{
				if (info->mMin1 == min_weight)
				{
					if (info->mMin1 == info->mMax1)
					{
						driven_weight = driven_max;
					}
					else
					{
						// Up-slope extrapolation
						F32 t = (mCurWeight - info->mMin1) /
								(info->mMax1 - info->mMin1);
						driven_weight = driven_min +
										t * (driven_max - driven_min);
					}
				}
				else
				{
					driven_weight = driven_min;
				}

				setDrivenWeight(driven, driven_weight, upload_bake);
				continue;
			}
			else if (mCurWeight > info->mMin2)
			{
				if (info->mMin2 == max_weight)
				{
					if (info->mMin2 == info->mMax2)
					{
						driven_weight = driven_max;
					}
					else
					{
						// Down-slope extrapolation
						F32 t = (mCurWeight - info->mMax2) /
								(info->mMin2 - info->mMax2);
						driven_weight = driven_max +
										t * (driven_min - driven_max);
					}
				}
				else
				{
					driven_weight = driven_min;
				}

				setDrivenWeight(driven, driven_weight, upload_bake);
				continue;
			}
		}

		driven_weight = getDrivenWeight(driven, mCurWeight);
		setDrivenWeight(driven,driven_weight,upload_bake);
	}
}

#if 0	// Unused methods
F32	LLDriverParam::getTotalDistortion()
{
	F32 sum = 0.f;
	for (entry_list_t::iterator iter = mDriven.begin(), end = mDriven.end();
		 iter != end; ++iter)
	{
		LLDrivenEntry* driven = &(*iter);
		sum += driven->mParam->getTotalDistortion();
	}

	return sum;
}

const LLVector4a& LLDriverParam::getAvgDistortion()
{
	// It is actually incorrect to take the average of averages, but it is good
	// enough here.
	LLVector4a sum;
	sum.clear();
	S32 count = 0;
	for (entry_list_t::iterator iter = mDriven.begin(), end = mDriven.end();
		 iter != end; ++iter)
	{
		LLDrivenEntry* driven = &(*iter);
		sum.add(driven->mParam->getAvgDistortion());
		++count;
	}
	sum.mul(1.f / (F32)count);

	mDefaultVec = sum;
	return mDefaultVec;
}

F32	LLDriverParam::getMaxDistortion()
{
	F32 max = 0.f;
	for (entry_list_t::iterator iter = mDriven.begin(), end = mDriven.end();
		 iter != end; ++iter)
	{
		LLDrivenEntry* driven = &(*iter);
		F32 param_max = driven->mParam->getMaxDistortion();
		if (param_max > max)
		{
			max = param_max;
		}
	}

	return max;
}

LLVector4a LLDriverParam::getVertexDistortion(S32 index, LLPolyMesh* poly_mesh)
{
	LLVector4a sum;
	sum.clear();
	for (entry_list_t::iterator iter = mDriven.begin(), end = mDriven.end();
		 iter != end; ++iter)
	{
		LLDrivenEntry* driven = &(*iter);
		sum.add(driven->mParam->getVertexDistortion(index, poly_mesh));
	}
	return sum;
}

const LLVector4a* LLDriverParam::getFirstDistortion(U32* index,
													LLPolyMesh** poly_mesh)
{
	mCurrentDistortionParam = NULL;
	const LLVector4a* v = NULL;
	for (entry_list_t::iterator iter = mDriven.begin(), end = mDriven.end();
		 iter != end; ++iter)
	{
		LLDrivenEntry* driven = &(*iter);
		v = driven->mParam->getFirstDistortion(index, poly_mesh);
		if (v)
		{
			mCurrentDistortionParam = driven->mParam;
			break;
		}
	}

	return v;
}

const LLVector4a* LLDriverParam::getNextDistortion(U32* index,
												   LLPolyMesh** poly_mesh)
{
	llassert(mCurrentDistortionParam);
	if (!mCurrentDistortionParam)
	{
		return NULL;
	}

	LLDrivenEntry* driven = NULL;
	entry_list_t::iterator iter;
	entry_list_t::iterator end = mDriven.end();

	// Set mDriven iteration to the right point
	for (iter = mDriven.begin(); iter != end; ++iter)
	{
		driven = &(*iter);
		if (driven->mParam == mCurrentDistortionParam)
		{
			break;
		}
	}

	llassert(driven);
	if (!driven)
	{
		return NULL; // shouldn't happen, but...
	}

	// We are already in the middle of a param's distortions, so get the next
	// one.
	const LLVector4a* v = driven->mParam->getNextDistortion(index, poly_mesh);
	if (!v && iter != end)
	{
		// This param is finished, so start the next param. It might not have
		// any distortions, though, so we have to loop to find the next param
		// that does.
		for (++iter; iter != end; ++iter)
		{
			driven = &(*iter);
			v = driven->mParam->getFirstDistortion(index, poly_mesh);
			if (v)
			{
				mCurrentDistortionParam = driven->mParam;
				break;
			}
		}
	}

	return v;
};
#endif

S32 LLDriverParam::getDrivenParamsCount() const
{
	return mDriven.size();
}

const LLViewerVisualParam* LLDriverParam::getDrivenParam(S32 index) const
{
	if (0 > index || index >= (S32)mDriven.size())
	{
		return NULL;
	}
	return mDriven[index].mParam;
}

void LLDriverParam::setAnimationTarget(F32 target_value, bool upload_bake)
{
	LLVisualParam::setAnimationTarget(target_value, upload_bake);

	for (entry_list_t::iterator iter = mDriven.begin(), end = mDriven.end();
		 iter != end; ++iter)
	{
		LLDrivenEntry* driven = &(*iter);
		F32 driven_weight = getDrivenWeight(driven, mTargetWeight);

		// this isn't normally necessary, as driver params handle interpolation
		// of their driven params but texture params need to know to assume
		// their final value at beginning of interpolation
		driven->mParam->setAnimationTarget(driven_weight, upload_bake);
	}
}

void LLDriverParam::stopAnimating(bool upload_bake)
{
	LLVisualParam::stopAnimating(upload_bake);

	for (entry_list_t::iterator iter = mDriven.begin(), end = mDriven.end();
		 iter != end; ++iter)
	{
		LLDrivenEntry* driven = &(*iter);
		driven->mParam->setAnimating(false);
	}
}

//virtual
bool LLDriverParam::linkDrivenParams(visual_param_mapper mapper,
									 bool only_cross_params)
{
	bool success = true;
	for (LLDriverParamInfo::entry_info_list_t::iterator
			iter = getInfo()->mDrivenInfoList.begin(),
			end = getInfo()->mDrivenInfoList.end();
		 iter != end; ++iter)
	{
		LLDrivenEntryInfo* driven_info = &(*iter);
		S32 driven_id = driven_info->mDrivenID;

		// Check for already existing links. Do not overwrite.
		bool found = false;
		for (entry_list_t::iterator driven_iter = mDriven.begin(),
									driven_end = mDriven.end();
			 driven_iter != driven_end; ++driven_iter)
		{
			if (driven_iter->mInfo->mDrivenID == driven_id)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			LLViewerVisualParam* param =
				(LLViewerVisualParam*)mapper(driven_id);
			if (param)
			{
				param->setParamLocation(this->getParamLocation());
			}
			if (param &&
				(!only_cross_params || param->getCrossWearable()))
			{
				mDriven.emplace_back(param, driven_info);
			}
			else
			{
				success = false;
			}
		}
	}

	return success;
}

void LLDriverParam::resetDrivenParams()
{
	mDriven.clear();
	mDriven.reserve(getInfo()->mDrivenInfoList.size());
}

void LLDriverParam::updateCrossDrivenParams(LLWearableType::EType driven_type)
{
	bool needs_update = getWearableType() == driven_type;

	// if the driver has a driven entry for the passed-in wearable type, we
	// need to refresh the value
	for (entry_list_t::iterator iter = mDriven.begin(), end = mDriven.end();
		 iter != end; ++iter)
	{
		LLDrivenEntry* driven = &(*iter);
		LLViewerVisualParam* param = driven ? driven->mParam : NULL;
		if (param && param->getCrossWearable() &&
			param->getWearableType() == driven_type)
		{
			needs_update = true;
		}
	}

	if (needs_update)
	{
		LLWearableType::EType driver_type = (LLWearableType::EType)getWearableType();

		// If we've gotten here, we've added a new wearable of type "type"
		// Thus this wearable needs to get updates from the driver wearable.
		// The call to setVisualParamWeight seems redundant, but is necessary
		// as the number of driven wearables has changed since the last update.
		LLWearable* wearable;
		wearable = mAvatarAppearance->getWearableData()->getTopWearable(driver_type);
		if (wearable)
		{
			wearable->setVisualParamWeight(mID,
										   wearable->getVisualParamWeight(mID),
										   false);
		}
	}
}

F32 LLDriverParam::getDrivenWeight(const LLDrivenEntry* driven,
								   F32 input_weight)
{
	F32 min_weight = getMinWeight();
	F32 max_weight = getMaxWeight();
	const LLDrivenEntryInfo* info = driven->mInfo;

	F32 driven_weight = 0.f;
	F32 driven_min = driven->mParam->getMinWeight();
	F32 driven_max = driven->mParam->getMaxWeight();

	if (input_weight <= info->mMin1)
	{
		if (info->mMin1 == info->mMax1 && info->mMin1 <= min_weight)
		{
			driven_weight = driven_max;
		}
		else
		{
			driven_weight = driven_min;
		}
	}
	else if (input_weight <= info->mMax1)
	{
		F32 t = (input_weight - info->mMin1) / (info->mMax1 - info->mMin1);
		driven_weight = driven_min + t * (driven_max - driven_min);
	}
	else if (input_weight <= info->mMax2)
	{
		driven_weight = driven_max;
	}
	else if (input_weight <= info->mMin2)
	{
		F32 t = (input_weight - info->mMax2) / (info->mMin2 - info->mMax2);
		driven_weight = driven_max + t * (driven_min - driven_max);
	}
	else
	{
		if (info->mMax2 >= max_weight)
		{
			driven_weight = driven_max;
		}
		else
		{
			driven_weight = driven_min;
		}
	}

	return driven_weight;
}

void LLDriverParam::setDrivenWeight(LLDrivenEntry* driven, F32 driven_weight,
									bool upload_bake)
{
	if (mWearablep && mAvatarAppearance->isValid() &&
		driven->mParam->getCrossWearable() &&
		mAvatarAppearance->getWearableData()->isOnTop(mWearablep))
	{
		// Call setWeight through LLVOAvatarSelf so other wearables can be
		// updated with the correct values
		mAvatarAppearance->setVisualParamWeight((LLVisualParam*)driven->mParam,
												driven_weight, upload_bake);
	}
	else
	{
		driven->mParam->setWeight(driven_weight, upload_bake);
	}
}
