/**
 * @file llvlmanager.cpp
 * @brief LLVLManager class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llvlmanager.h"

#include "llbitpack.h"
#include "llpatch_code.h"

#include "llagent.h"
#include "llsurface.h"
#include "llviewerregion.h"

LLVLManager gVLManager;

LLVLManager::LLVLManager()
:	mLandBits(0),
	mWindBits(0),
	mCloudBits(0)
{
}

LLVLManager::~LLVLManager()
{
	for (S32 i = 0, count = mPacketData.size(); i < count; ++i)
	{
		delete mPacketData[i];
	}
	mPacketData.clear();
}

void LLVLManager::addLayerData(LLVLData* vl_datap, S32 mesg_size)
{
	S8 type = vl_datap->mType;
	if (type == LAND_LAYER_CODE || type == AURORA_LAND_LAYER_CODE)
	{
		mLandBits += mesg_size * 8;
	}
	else if (type == WIND_LAYER_CODE || type == AURORA_WIND_LAYER_CODE)
	{
		mWindBits += mesg_size * 8;
	}
	else if (type == CLOUD_LAYER_CODE || type == AURORA_CLOUD_LAYER_CODE)
	{
		mCloudBits += mesg_size * 8;
	}
	else if (type != WATER_LAYER_CODE && type != AURORA_WATER_LAYER_CODE)
	{
		llwarns_once << "Unknown layer type: " << type << " (" << (S32)type
					 << ")" << llendl;
	}

	mPacketData.push_back(vl_datap);
}

void LLVLManager::unpackData(S32 num_packets)
{
	S32 count = mPacketData.size();
	for (S32 i = 0; i < count; ++i)
	{
		LLVLData* datap = mPacketData[i];
		if (!datap) continue;	// Paranoia

		LLBitPack bit_pack(datap->mData, datap->mSize);
		LLGroupHeader goph;

		decode_patch_group_header(bit_pack, &goph);

		S8 type = datap->mType;
		if (type == LAND_LAYER_CODE)
		{
			datap->mRegionp->getLand().decompressDCTPatch(bit_pack, &goph,
														  false);
		}
		else if (type == AURORA_LAND_LAYER_CODE)
		{
			datap->mRegionp->getLand().decompressDCTPatch(bit_pack, &goph,
														  true);
		}
		else if (type == WIND_LAYER_CODE || type == AURORA_WIND_LAYER_CODE)
		{
			datap->mRegionp->mWind.decompress(bit_pack, &goph);

		}
		else if (type == CLOUD_LAYER_CODE || type == AURORA_CLOUD_LAYER_CODE)
		{
			datap->mRegionp->mCloudLayer.decompress(bit_pack, &goph);
		}
	}

	for (S32 i = 0; i < count; ++i)
	{
		delete mPacketData[i];
	}
	mPacketData.clear();
}

void LLVLManager::cleanupData(LLViewerRegion* regionp)
{
	size_t cur = 0;
	while (cur < mPacketData.size())
	{
		if (mPacketData[cur]->mRegionp == regionp)
		{
			delete mPacketData[cur];
			mPacketData.erase(mPacketData.begin() + cur);
		}
		else
		{
			++cur;
		}
	}
}
