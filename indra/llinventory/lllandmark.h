/**
 * @file lllandmark.h
 * @brief Landmark asset class
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


#ifndef LL_LLLANDMARK_H
#define LL_LLLANDMARK_H

#include <map>		// For multimap

#include "boost/function.hpp"

#include "hbfastmap.h"
#include "llframetimer.h"
#include "lluuid.h"
#include "llvector3d.h"

class LLMessageSystem;
class LLHost;

class LLLandmark
{
protected:
	LOG_CLASS(LLLandmark);

public:
	// For calling back interested parties when a region handle comes back.
	typedef boost::function<void(const LLUUID& region_id,
								 const U64& region_handle)> region_handle_callback_t;

	~LLLandmark() {}

	// Returns true if the position is known.
	bool getGlobalPos(LLVector3d& pos);

	// Setter used in conjunction if more information needs to be collected
	// from the server.
	void setGlobalPos(const LLVector3d& pos);

	// Returns true if the region is known
	bool getRegionID(LLUUID& region_id);

	// Returns the local coordinates if known
	LLVector3 getRegionPos() const;

	// Constructs a new LLLandmark from a string. Return NULL if there is an
	// error.
	static LLLandmark* constructFromString(const char* buffer, S32 buff_size);

	// Registers callbacks that this class handles
	static void registerCallbacks(LLMessageSystem* msg);

	// Requests information about region_id to region_handle. The callback
	// pointer will be erased but NOT deleted after the callback is made.
	// This method may call into the message system to get the information.
	static void requestRegionHandle(LLMessageSystem* msg,
									const LLHost& upstream_host,
									const LLUUID& region_id,
									region_handle_callback_t callback);

	// Call this method to create a lookup for this region. This simplifies a
	// lot of the code.
	static void setRegionHandle(const LLUUID& region_id, U64 region_handle);

private:
	LLLandmark(const LLUUID& region_id, const LLVector3& local_pos);
	LLLandmark(const LLVector3d& global_pos);

	static void processRegionIDAndHandle(LLMessageSystem* msg, void**);
	static void expireOldEntries();

private:
	LLUUID		mRegionID;
	LLVector3	mRegionPos;
	LLVector3d	mGlobalPos;
	bool		mGlobalPositionKnown;

	struct CacheInfo
	{
		U64 mRegionHandle;
		LLFrameTimer mTimer;
	};

	static std::pair<LLUUID, U64> mLocalRegion;

	typedef fast_hmap<LLUUID, CacheInfo> region_map_t;
	static region_map_t mRegions;

	typedef std::multimap<LLUUID, region_handle_callback_t> region_callback_map_t;
	static region_callback_map_t mRegionCallback;
};

#endif
