/**
 * @file llwearabledata.cpp
 * @brief LLWearableData class implementation
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#include "llwearabledata.h"

#include "llavatarappearance.h"
#include "llavatarappearancedefines.h"
#include "lldriverparam.h"
#include "llmd5.h"

using namespace LLAvatarAppearanceDefines;

LLWearableData::LLWearableData()
:	mAvatarAppearance(NULL),
//MK
	mCanWearFunc(NULL),
	mCanUnwearFunc(NULL)
//mk
{
}

LLWearable* LLWearableData::getWearable(LLWearableType::EType type, U32 index)
{
	wearableentry_map_t::iterator it = mWearableDatas.find(type);
	if (it == mWearableDatas.end())
	{
		return NULL;
	}
	wearableentry_vec_t& wearable_vec = it->second;
	return index < wearable_vec.size() ? wearable_vec[index] : NULL;
}

bool LLWearableData::setWearable(LLWearableType::EType type, U32 index,
								 LLWearable* wearable)
{
//MK
	if (mCanWearFunc && !mCanWearFunc(type))
	{
		return false;
	}
//mk

	LLWearable* old_wearable = getWearable(type, index);
	if (!old_wearable)
	{
		pushWearable(type, wearable);
		return true;
	}

//MK
	if (mCanUnwearFunc && !mCanUnwearFunc(type))
	{
		// cannot remove this outfit, so cannot replace it either
		return false;
	}
//mk

	wearableentry_map_t::iterator it = mWearableDatas.find(type);
	if (it == mWearableDatas.end())
	{
		llwarns << "invalid type, type " << type << " index " << index
				<< llendl;
		return false;
	}
	wearableentry_vec_t& wearable_vec = it->second;
	if (index >= wearable_vec.size())
	{
		llwarns << "invalid index, type " << type << " index " << index
				<< llendl;
		return false;
	}
	else
	{
		wearable_vec[index] = wearable;
		old_wearable->setUpdated();
		wearableUpdated(wearable, false);
	}

	return true;
}

bool LLWearableData::pushWearable(LLWearableType::EType type,
								  LLWearable* wearable, bool trigger_updated)
{
	if (!wearable)
	{
		llwarns << "Null wearable sent for type " << type << llendl;
		return false;
	}

	if (canAddWearable(type))
	{
//MK
		if (mCanWearFunc && !mCanWearFunc(type))
		{
			return false;
		}
//mk
		mWearableDatas[type].push_back(wearable);
		if (trigger_updated)
		{
			wearableUpdated(wearable, false);
		}
	}

	return true;
}

//virtual
void LLWearableData::wearableUpdated(LLWearable* wearable, bool removed)
{
	wearable->setUpdated();
	// *FIXME: avoid updating params via wearables when rendering server-baked
	// appearance.
#if 0
	if (mAvatarAppearance->isUsingServerBakes() &&
		!mAvatarAppearance->isUsingLocalAppearance())
	{
		return;
	}
#endif
	if (!removed)
	{
		pullCrossWearableValues(wearable->getType());
	}
}

void LLWearableData::eraseWearable(LLWearable* wearable)
{
	if (!wearable)
	{
		return;
	}

	U32 index;
	if (getWearableIndex(wearable, index))
	{
		eraseWearable(wearable->getType(), index);
	}
}

void LLWearableData::eraseWearable(LLWearableType::EType type, U32 index)
{
	LLWearable* wearable = getWearable(type, index);
	if (wearable)
	{
		mWearableDatas[type].erase(mWearableDatas[type].begin() + index);
		wearableUpdated(wearable, true);
	}
}

void LLWearableData::clearWearableType(LLWearableType::EType type)
{
	wearableentry_map_t::iterator it = mWearableDatas.find(type);
	if (it == mWearableDatas.end())
	{
		return;
	}
	wearableentry_vec_t& wearable_vec = it->second;
	wearable_vec.clear();
}

bool LLWearableData::swapWearables(LLWearableType::EType type, U32 index_a,
								   U32 index_b)
{
	wearableentry_map_t::iterator it = mWearableDatas.find(type);
	if (it == mWearableDatas.end())
	{
		return false;
	}

	wearableentry_vec_t& wearable_vec = it->second;
	if (index_a >= wearable_vec.size() || index_b >= wearable_vec.size())
	{
		return false;
	}

	LLWearable* wearable = wearable_vec[index_a];
	wearable_vec[index_a] = wearable_vec[index_b];
	wearable_vec[index_b] = wearable;
	return true;
}

void LLWearableData::pullCrossWearableValues(LLWearableType::EType type)
{
	if (!mAvatarAppearance)
	{
		llwarns << "NULL mAvatarAppearance !" << llendl;
		llassert(false);
		return;
	}

	// Scan through all of the avatar's visual parameters
	for (LLViewerVisualParam* param =
			(LLViewerVisualParam*)mAvatarAppearance->getFirstVisualParam();
		 param;
		 param = (LLViewerVisualParam*)mAvatarAppearance->getNextVisualParam())
	{
		if (param)
		{
			LLDriverParam* driver_param = param->asDriverParam();
			if (driver_param)
			{
				// Parameter is a driver parameter, have it update its
				// cross-driven params
				driver_param->updateCrossDrivenParams(type);
			}
		}
	}
}

bool LLWearableData::getWearableIndex(const LLWearable* wearable,
									  U32& index_found) const
{
	if (!wearable)
	{
		return false;
	}

	LLWearableType::EType type = wearable->getType();
	wearableentry_map_t::const_iterator it = mWearableDatas.find(type);
	if (it == mWearableDatas.end())
	{
		llwarns << "Tried to get wearable index with an invalid type !"
				<< llendl;
		return false;
	}

	const wearableentry_vec_t& wearable_vec = it->second;
	for (U32 index = 0, count = wearable_vec.size(); index < count; ++index)
	{
		if (wearable_vec[index] == wearable)
		{
			index_found = index;
			return true;
		}
	}

	return false;
}

U32 LLWearableData::getClothingLayerCount() const
{
	U32 count = 0;
	for (S32 i = 0; i < LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;
		if (LLWearableType::getAssetType(type) == LLAssetType::AT_CLOTHING)
		{
			count += getWearableCount(type);
		}
	}
	return count;
}

bool LLWearableData::canAddWearable(LLWearableType::EType type) const
{
	LLAssetType::EType a_type = LLWearableType::getAssetType(type);
	if (a_type == LLAssetType::AT_CLOTHING)
	{
		return getClothingLayerCount() < MAX_CLOTHING_LAYERS;
	}
	else if (a_type == LLAssetType::AT_BODYPART)
	{
		return getWearableCount(type) < 1;
	}
	return false;
}

bool LLWearableData::isOnTop(LLWearable* wearable) const
{
	return wearable && getTopWearable(wearable->getType()) == wearable;
}

const LLWearable* LLWearableData::getWearable(LLWearableType::EType type,
											  U32 index) const
{
	wearableentry_map_t::const_iterator it = mWearableDatas.find(type);
	if (it == mWearableDatas.end())
	{
		return NULL;
	}
	const wearableentry_vec_t& wearable_vec = it->second;
	return index < wearable_vec.size() ? wearable_vec[index] : NULL;
}

LLWearable* LLWearableData::getTopWearable(LLWearableType::EType type)
{
	U32 count = getWearableCount(type);
	return count ? getWearable(type, count - 1) : NULL;
}

const LLWearable* LLWearableData::getTopWearable(LLWearableType::EType type) const
{
	U32 count = getWearableCount(type);
	return count ? getWearable(type, count - 1) : NULL;
}

LLWearable* LLWearableData::getBottomWearable(LLWearableType::EType type)
{
	return getWearableCount(type) ? getWearable(type, 0) : NULL;
}

const LLWearable* LLWearableData::getBottomWearable(LLWearableType::EType type) const
{
	return getWearableCount(type) ? getWearable(type, 0) : NULL;
}

U32 LLWearableData::getWearableCount(LLWearableType::EType type) const
{
	wearableentry_map_t::const_iterator it = mWearableDatas.find(type);
	if (it == mWearableDatas.end())
	{
		return 0;
	}
	const wearableentry_vec_t& wearable_vec = it->second;
	return wearable_vec.size();
}

U32 LLWearableData::getWearableCount(U32 tex_idx) const
{
	LLWearableType::EType wearable_type =
		LLAvatarAppearanceDictionary::getTEWearableType((ETextureIndex)tex_idx);
	return getWearableCount(wearable_type);
}

LLUUID LLWearableData::computeBakedTextureHash(EBakedTextureIndex baked_index,
											   // Set to false if you want to
											   // upload the baked texture w/o
											   // putting it in the cache
											   bool generate_valid_hash)
{
	const LLAvatarAppearanceDictionary::BakedEntry* baked_dict =
		gAvatarAppDictp->getBakedTexture(baked_index);

	LLUUID hash_id;
	LLMD5 hash;
	bool hash_computed = false;
	for (U8 i = 0, count = baked_dict->mWearables.size(); i < count; ++i)
	{
		LLWearableType::EType baked_type = baked_dict->mWearables[i];
		U32 num_wearables = getWearableCount(baked_type);
		for (U32 index = 0; index < num_wearables; ++index)
		{
			const LLWearable* wearable = getWearable(baked_type,index);
			if (wearable)
			{
				wearable->addToBakedTextureHash(hash);
				hash_computed = true;
			}
		}
	}

	if (hash_computed)
	{
		hash.update((const unsigned char*)baked_dict->mWearablesHashID.mData,
					UUID_BYTES);

		if (!generate_valid_hash)
		{
			invalidateBakedTextureHash(hash);
		}
		hash.finalize();
		hash.raw_digest(hash_id.mData);
	}

	return hash_id;
}
