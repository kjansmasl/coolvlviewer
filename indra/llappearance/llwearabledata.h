/**
 * @file llwearabledata.h
 * @brief LLWearableData class header file
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

#ifndef LL_WEARABLEDATA_H
#define LL_WEARABLEDATA_H

#include "llavatarappearancedefines.h"
#include "llerror.h"
#include "llwearable.h"

class LLAvatarAppearance;

class LLWearableData
{
protected:
	LOG_CLASS(LLWearableData);

public:
	LLWearableData();
	virtual ~LLWearableData() = default;

	LL_INLINE void setAvatarAppearance(LLAvatarAppearance* appearance)
	{
		mAvatarAppearance = appearance;
	}

	LLWearable* getWearable(LLWearableType::EType type, U32 index);
	const LLWearable* getWearable(LLWearableType::EType type, U32 index) const;
	LLWearable* getTopWearable(LLWearableType::EType type);
	const LLWearable* getTopWearable(LLWearableType::EType type) const;
	LLWearable* getBottomWearable(LLWearableType::EType type);
	const LLWearable* getBottomWearable(LLWearableType::EType type) const;
	U32 getWearableCount(LLWearableType::EType type) const;
	U32 getWearableCount(U32 tex_idx) const;
	bool getWearableIndex(const LLWearable* wearable, U32& index) const;
	U32 getClothingLayerCount() const;
	bool canAddWearable(LLWearableType::EType type) const;

	bool isOnTop(LLWearable* wearable) const;

	LLUUID computeBakedTextureHash(LLAvatarAppearanceDefines::EBakedTextureIndex idx,
								   bool generate_valid_hash = true);

protected:
//MK
	LL_INLINE void setCanWearFunc(bool (*func)(LLWearableType::EType))
	{
		mCanWearFunc = func;
	}

	LL_INLINE void setCanUnwearFunc(bool (*func)(LLWearableType::EType))
	{
		mCanUnwearFunc = func;
	}
//mk

	// Low-level data structure setter - public access is via setWearableItem, etc.

	// These two methods return false when they fail (e.g. because of RLV
	// restrictions)
	bool setWearable(LLWearableType::EType type, U32 index,
					 LLWearable* wearable);
	bool pushWearable(LLWearableType::EType type, LLWearable* wearable,
					  bool trigger_updated = true);

	virtual void wearableUpdated(LLWearable* wearable, bool removed);

	void eraseWearable(LLWearable* wearable);
	void eraseWearable(LLWearableType::EType type, U32 index);
	void clearWearableType(LLWearableType::EType type);
	bool swapWearables(LLWearableType::EType type, U32 index_a, U32 index_b);

	virtual void invalidateBakedTextureHash(LLMD5& hash) const	{}

private:
	void pullCrossWearableValues(LLWearableType::EType type);

public:
	static constexpr U32 MAX_CLOTHING_LAYERS = 60;

protected:
	LLAvatarAppearance* mAvatarAppearance;

	// All wearables of a certain type (e.g. all shirts):
	typedef std::vector<LLWearable*> wearableentry_vec_t;

	// Wearable "categories" arranged by wearable type:
	typedef std::map<LLWearableType::EType,
					 wearableentry_vec_t> wearableentry_map_t;
	wearableentry_map_t mWearableDatas;

private:
	bool (*mCanWearFunc)(LLWearableType::EType type);
	bool (*mCanUnwearFunc)(LLWearableType::EType type);
};

#endif // LL_WEARABLEDATA_H
