/**
 * @file llmaterialtable.cpp
 * @brief Table of material names and IDs for viewer
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

#include "llmaterialtable.h"
#include "llstl.h"
#include "imageids.h"
#include "sound_ids.h"

constexpr F32 DEFAULT_FRICTION = 0.5f;
constexpr F32 DEFAULT_RESTITUTION = 0.4f;

LLMaterialTable gMaterialTable;

LLMaterialTable::LLMaterialTable()
{
	// Concrete
	mMaterialInfoList.emplace_back(LL_MCODE_STONE, "Stone", 0.8f, 0.4f);
	// Steel
	mMaterialInfoList.emplace_back(LL_MCODE_METAL, "Metal", 0.3f, 0.4f);
	// Borosilicate glass
	mMaterialInfoList.emplace_back(LL_MCODE_GLASS, "Glass", 0.2f, 0.7f);
	// Southern pine
	mMaterialInfoList.emplace_back(LL_MCODE_WOOD, "Wood", 0.6f, 0.5f);
	// Saltwater
	mMaterialInfoList.emplace_back(LL_MCODE_FLESH, "Flesh", 0.9f, 0.3f);
	// HDPE
	mMaterialInfoList.emplace_back(LL_MCODE_PLASTIC, "Plastic", 0.4f, 0.7f);
	mMaterialInfoList.emplace_back(LL_MCODE_RUBBER, "Rubber", 0.9f, 0.9f);
	mMaterialInfoList.emplace_back(LL_MCODE_LIGHT, "Light", 0.2f, 0.7f);

	// Collision sounds
	mCollisionsSounds.reserve(28);
	mCollisionsSounds.emplace(SND_FLESH_FLESH);
	mCollisionsSounds.emplace(SND_FLESH_PLASTIC);
	mCollisionsSounds.emplace(SND_FLESH_RUBBER);
	mCollisionsSounds.emplace(SND_GLASS_FLESH);
	mCollisionsSounds.emplace(SND_GLASS_GLASS);
	mCollisionsSounds.emplace(SND_GLASS_PLASTIC);
	mCollisionsSounds.emplace(SND_GLASS_RUBBER);
	mCollisionsSounds.emplace(SND_GLASS_WOOD);
	mCollisionsSounds.emplace(SND_METAL_FLESH);
	mCollisionsSounds.emplace(SND_METAL_GLASS);
	mCollisionsSounds.emplace(SND_METAL_METAL);
	mCollisionsSounds.emplace(SND_METAL_PLASTIC);
	mCollisionsSounds.emplace(SND_METAL_RUBBER);
	mCollisionsSounds.emplace(SND_METAL_WOOD);
	mCollisionsSounds.emplace(SND_PLASTIC_PLASTIC);
	mCollisionsSounds.emplace(SND_RUBBER_PLASTIC);
	mCollisionsSounds.emplace(SND_RUBBER_RUBBER);
	mCollisionsSounds.emplace(SND_STONE_FLESH);
	mCollisionsSounds.emplace(SND_STONE_GLASS);
	mCollisionsSounds.emplace(SND_STONE_METAL);
	mCollisionsSounds.emplace(SND_STONE_PLASTIC);
	mCollisionsSounds.emplace(SND_STONE_RUBBER);
	mCollisionsSounds.emplace(SND_STONE_STONE);
	mCollisionsSounds.emplace(SND_STONE_WOOD);
	mCollisionsSounds.emplace(SND_WOOD_FLESH);
	mCollisionsSounds.emplace(SND_WOOD_PLASTIC);
	mCollisionsSounds.emplace(SND_WOOD_RUBBER);
	mCollisionsSounds.emplace(SND_WOOD_WOOD);
	mCollisionsSounds.emplace(SND_OPENSIM_COLLISION);
}

void LLMaterialTable::initTableTransNames(name_map_t namemap)
{
	for (info_list_t::iterator iter = mMaterialInfoList.begin();
		 iter != mMaterialInfoList.end(); ++iter)
	{
		LLMaterialInfo& info = *iter;
		info.mName = namemap[info.mName];
	}
}

U8 LLMaterialTable::getMCode(const std::string& name) const
{
	for (info_list_t::const_iterator iter = mMaterialInfoList.begin(),
									 end = mMaterialInfoList.end();
		 iter != end; ++iter)
	{
		const LLMaterialInfo& info = *iter;
		if (name == info.mName)
		{
			return info.mMCode;
		}
	}
	return 0;
}

const std::string& LLMaterialTable::getName(U8 mcode) const
{
	mcode &= LL_MCODE_MASK;
	for (info_list_t::const_iterator iter = mMaterialInfoList.begin(),
									 end = mMaterialInfoList.end();
		 iter != end; ++iter)
	{
		const LLMaterialInfo& info = *iter;
		if (mcode == info.mMCode)
		{
			return info.mName;
		}
	}
	return LLStringUtil::null;
}

F32 LLMaterialTable::getRestitution(U8 mcode) const
{
	for (info_list_t::const_iterator iter = mMaterialInfoList.begin(),
									 end = mMaterialInfoList.end();
		 iter != end; ++iter)
	{
		const LLMaterialInfo& info = *iter;
		if (mcode == info.mMCode)
		{
			return info.mRestitution;
		}
	}
	return DEFAULT_RESTITUTION;
}

F32 LLMaterialTable::getFriction(U8 mcode) const
{
	for (info_list_t::const_iterator iter = mMaterialInfoList.begin(),
									 end = mMaterialInfoList.end();
		 iter != end; ++iter)
	{
		const LLMaterialInfo& info = *iter;
		if (mcode == info.mMCode)
		{
			return info.mFriction;
		}
	}
	return DEFAULT_FRICTION;
}
