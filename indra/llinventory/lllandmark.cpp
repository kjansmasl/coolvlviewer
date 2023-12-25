/**
 * @file lllandmark.cpp
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

#include "linden_common.h"

#include <errno.h>

#include "lllandmark.h"

#include "llregionhandle.h"
#include "llmessage.h"

std::pair<LLUUID, U64> LLLandmark::mLocalRegion;
LLLandmark::region_map_t LLLandmark::mRegions;
LLLandmark::region_callback_map_t LLLandmark::mRegionCallback;

LLLandmark::LLLandmark(const LLUUID& region_id, const LLVector3& local_pos)
:	mGlobalPositionKnown(false),
	mRegionID(region_id),
	mRegionPos(local_pos)
{
}

LLLandmark::LLLandmark(const LLVector3d& global_pos)
:	mGlobalPositionKnown(true),
	mGlobalPos(global_pos)
{
}

bool LLLandmark::getGlobalPos(LLVector3d& pos)
{
	if (mGlobalPositionKnown)
	{
		pos = mGlobalPos;
	}
	else if (mRegionID.notNull())
	{
		F32 g_x = -1.f;
		F32 g_y = -1.f;
		if (mRegionID == mLocalRegion.first)
		{
			from_region_handle(mLocalRegion.second, &g_x, &g_y);
		}
		else
		{
			region_map_t::iterator it = mRegions.find(mRegionID);
			if (it != mRegions.end())
			{
				from_region_handle(it->second.mRegionHandle, &g_x, &g_y);
			}
		}
		if (g_x > 0.f && g_y > 0.f)
		{
			pos.mdV[0] = g_x + mRegionPos.mV[0];
			pos.mdV[1] = g_y + mRegionPos.mV[1];
			pos.mdV[2] = mRegionPos.mV[2];
			setGlobalPos(pos);
		}
	}
	return mGlobalPositionKnown;
}

void LLLandmark::setGlobalPos(const LLVector3d& pos)
{
	mGlobalPos = pos;
	mGlobalPositionKnown = true;
}

bool LLLandmark::getRegionID(LLUUID& region_id)
{
	if (mRegionID.notNull())
	{
		region_id = mRegionID;
		return true;
	}
	return false;
}

LLVector3 LLLandmark::getRegionPos() const
{
	return mRegionPos;
}

//static
LLLandmark* LLLandmark::constructFromString(const char* buffer, S32 buff_size)
{
	S32 chars_read_total = 0;
	S32 chars_read = 0;

	// Read the version
	U32 version = 0;
	S32 count = sscanf(buffer, "Landmark version %u\n%n", &version,
					   &chars_read);
	if (count != 1)
	{
		llwarns << "Bad landmark asset. Cannot read version." << llendl;
		return NULL;
	}
	chars_read_total += chars_read;
	if (chars_read_total >= buff_size)
	{
		llwarns << "Bad landmark asset (truncated or corrupted)." << llendl;
		return NULL;
	}

	if (version == 1)
	{
		// Read the global position
		LLVector3d pos;
		count = sscanf(buffer + chars_read_total, "position %lf %lf %lf\n%n",
					   pos.mdV + VX, pos.mdV + VY, pos.mdV + VZ, &chars_read);
		if (count == 3)
		{
			return new LLLandmark(pos);
		}
		llwarns << "Bad landmark asset. Incorrect position." << llendl;
		return NULL;
	}

	if (version != 2)
	{
		llwarns << "Unsupported landmark asset version !" << llendl;
		return NULL;
	}

	// *NOTE: changing the buffer size will require changing the scanf call
	// below.
	char region_id_str[MAX_STRING];
	count = sscanf(buffer + chars_read_total, "region_id %254s\n%n",
				   region_id_str, &chars_read);
	if (count != 1)
	{
		llwarns << "Bad landmark asset. Cannot read region Id." << llendl;
		return NULL;
	}
	chars_read_total += chars_read;
	if (chars_read_total >= buff_size)
	{
		llwarns << "Bad landmark asset (truncated or corrupted)." << llendl;
		return NULL;
	}

	if (!LLUUID::validate(region_id_str))
	{
		llwarns << "Bad landmark asset: invalid region Id: " << region_id_str
				<< llendl;
		return NULL;
	}
	LLUUID region_id(region_id_str);
	if (region_id.isNull())
	{
		llwarns << "Bad landmark asset: null region Id." << llendl;
		return NULL;
	}

	// Read the local position
	LLVector3 lpos;
	count = sscanf(buffer + chars_read_total, "local_pos %f %f %f\n%n",
				   lpos.mV + VX, lpos.mV + VY, lpos.mV + VZ, &chars_read);
	if (count != 3)
	{
		llwarns << "Bad landmark asset. Cannot read position." << llendl;
		return NULL;
	}

	return new LLLandmark(region_id, lpos);
}

//static
void LLLandmark::registerCallbacks(LLMessageSystem* msg)
{
	msg->setHandlerFunc("RegionIDAndHandleReply", &processRegionIDAndHandle);
}

//static
void LLLandmark::requestRegionHandle(LLMessageSystem* msg,
									 const LLHost& upstream_host,
									 const LLUUID& region_id,
									 region_handle_callback_t callback)
{
	if (region_id.isNull())
	{
		// Do not bother with checking...
		LL_DEBUGS("Landmark") << "Null region Id" << LL_ENDL;
		if (callback)
		{
			constexpr U64 U64_ZERO = 0;
			callback(region_id, U64_ZERO);
		}
	}
	else if (region_id == mLocalRegion.first)
	{
		LL_DEBUGS("Landmark") << "Local region" << LL_ENDL;
		if (callback)
		{
			callback(region_id, mLocalRegion.second);
		}
	}
	else
	{
		region_map_t::iterator it = mRegions.find(region_id);
		if (it == mRegions.end())
		{
			LL_DEBUGS("Landmark") << "Upstream region" << LL_ENDL;
			if (callback)
			{
				region_callback_map_t::value_type vt(region_id, callback);
				mRegionCallback.insert(vt);
			}
			LL_DEBUGS("Landmark") << "Landmark requesting information about: "
								  << region_id << LL_ENDL;
			msg->newMessage("RegionHandleRequest");
			msg->nextBlock("RequestBlock");
			msg->addUUID("RegionID", region_id);
			msg->sendReliable(upstream_host);
		}
		else if (callback)
		{
			// We have the answer locally, just call the callback.
			LL_DEBUGS("Landmark") << "Cached upstream region" << LL_ENDL;
			callback(region_id, it->second.mRegionHandle);
		}
	}

	// As good a place as any to expire old entries.
	expireOldEntries();
}

//static
void LLLandmark::setRegionHandle(const LLUUID& region_id, U64 region_handle)
{
	mLocalRegion.first = region_id;
	mLocalRegion.second = region_handle;
}

//static
void LLLandmark::processRegionIDAndHandle(LLMessageSystem* msg, void**)
{
	LLUUID region_id;
	msg->getUUID("ReplyBlock", "RegionID", region_id);
	mRegions.erase(region_id);
	CacheInfo info;
	constexpr F32 CACHE_EXPIRY_SECONDS = 600.f;	// 10 minutes
	info.mTimer.setTimerExpirySec(CACHE_EXPIRY_SECONDS);
	msg->getU64("ReplyBlock", "RegionHandle", info.mRegionHandle);
	mRegions.emplace(region_id, info);

#if LL_DEBUG
	U32 grid_x, grid_y;
	grid_from_region_handle(info.mRegionHandle, &grid_x, &grid_y);
	LL_DEBUGS("Landmark") << "Landmark got reply for region: " << region_id
						  << " " << grid_x << "," << grid_y << LL_ENDL;
#endif

	// Make all the callbacks here.
	region_callback_map_t::iterator it;
	while ((it = mRegionCallback.find(region_id)) != mRegionCallback.end())
	{
		it->second(region_id, info.mRegionHandle);
		mRegionCallback.erase(it);
	}
}

//static
void LLLandmark::expireOldEntries()
{
	for (region_map_t::iterator it = mRegions.begin(), end = mRegions.end();
		 it != end; )
	{
		if (it->second.mTimer.hasExpired())
		{
			mRegions.hmap_erase(it++);
		}
		else
		{
			++it;
		}
	}
}
