/**
 * @file llwearablelist.h
 * @brief LLWearableList class header file
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

#ifndef LL_LLWEARABLELIST_H
#define LL_LLWEARABLELIST_H

#include "hbfastmap.h"
#include "llsingleton.h"

#include "llviewerwearable.h"

// Globally constructed; be careful that there is no dependency with gAgent.

// *BUG: mList's system of mapping between asset Ids and wearables is flawed
// since LLWearable* has an associated itemID, and you can have multiple
// inventory items pointing to the same asset (i.e. more than one item Id per
// asset Id). EXT-6252

class LLWearableList final : public LLSingleton<LLWearableList>
{
	friend class LLSingleton<LLWearableList>;

protected:
	LOG_CLASS(LLWearableList);

public:
	LLWearableList()					{}
	~LLWearableList() override;

	void cleanup();

	LL_INLINE S32 getLength()			{ return mList.size(); }

	void getAsset(const LLAssetID& asset_id, const std::string& wearable_name,
				  LLAvatarAppearance* avatarp, LLAssetType::EType asset_type,
				  void (*asset_arrived_callback)(LLViewerWearable*, void*),
				  void* userdata);

	LLViewerWearable* createCopy(LLViewerWearable* old_wearable,
								 const std::string& new_name = std::string());
	LLViewerWearable* createNewWearable(LLWearableType::EType type,
										LLAvatarAppearance* avatarp);

private:
	// Used for the create... functions
	LLViewerWearable* generateNewWearable();

	// Callback
	static void processGetAssetReply(const char* filename,
									 const LLAssetID& asset_id,
									 void* user_data, S32 status,
									 LLExtStat ext_status);

private:
	typedef fast_hmap<LLUUID, LLViewerWearable*> wearable_map_t;
	wearable_map_t mList;
};

#endif  // LL_LLWEARABLELIST_H
