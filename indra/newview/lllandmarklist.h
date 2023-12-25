/**
 * @file lllandmarklist.h
 * @brief Landmark asset list class
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

#ifndef LL_LLLANDMARKLIST_H
#define LL_LLLANDMARKLIST_H

#include <map>		// For multimap

#include "boost/function.hpp"

#include "llassetstorage.h"
#include "hbfastmap.h"
#include "llerror.h"
#include "lllandmark.h"
#include "lluuid.h"

class LLLineEditor;
class LLInventoryItem;

class LLLandmarkList
{
protected:
	LOG_CLASS(LLLandmarkList);

public:
	~LLLandmarkList();

	bool assetExists(const LLUUID& asset_id);

	typedef boost::function<void(LLLandmark*)> loaded_callback_t;
	LLLandmark* getAsset(const LLUUID& asset_id,
						 loaded_callback_t cb = NULL);

	static void processGetAssetReply(const LLUUID& asset_id,
									 LLAssetType::EType type,
									 void* user_data, S32 status,
									 LLExtStat ext_status);

	// Returns true if loading the landmark with given asset_id has been
	// requested but is not complete yet.
	bool isAssetInLoadedCallbackMap(const LLUUID& asset_id);

private:
	void onRegionHandle(const LLUUID& landmark_id);
	void makeCallbacks(const LLUUID& landmark_id);
	void eraseCallbacks(const LLUUID& asset_id);
	void markBadAsset(const LLUUID& asset_id);

private:
	typedef fast_hmap<LLUUID, LLLandmark*> landmark_list_t;
	landmark_list_t				mList;

	uuid_list_t					mBadList;
	uuid_list_t					mWaitList;

	typedef fast_hmap<LLUUID, F32> landmark_requested_list_t;
	landmark_requested_list_t	mRequestedList;

	// *TODO: make the callback multimap a template class and make use of it
	// here and in LLLandmark.
	typedef std::multimap<LLUUID, loaded_callback_t> loaded_callback_map_t;
	loaded_callback_map_t		mLoadedCallbackMap;
};

extern LLLandmarkList gLandmarkList;

#endif  // LL_LLLANDMARKLIST_H
