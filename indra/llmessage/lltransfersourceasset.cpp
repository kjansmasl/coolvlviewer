/**
 * @file lltransfersourceasset.cpp
 * @brief Transfer system for sending an asset.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "lltransfersourceasset.h"

#include "lldatapacker.h"
#include "lldir.h"
#include "llfilesystem.h"
#include "llmessage.h"

LLTransferSourceAsset::LLTransferSourceAsset(const LLUUID& request_id, F32 prio)
:	LLTransferSource(LLTST_ASSET, request_id, prio),
	mGotResponse(false),
	mCurPos(0)
{
}

void LLTransferSourceAsset::initTransfer()
{
	if (gAssetStoragep)
	{
		// *HACK: asset transfers will only be coming from the viewer to the
		// simulator. This is subset of assets we allow to be simply pulled
		// straight from the asset system.
		if (LLAssetType::lookupIsAssetFetchByIDAllowed(mParams.getAssetType()))
		{
			LLUUID* tidp = new LLUUID(getID());
			gAssetStoragep->getAssetData(mParams.getAssetID(),
										 mParams.getAssetType(),
										 responderCallback, tidp, false);
		}
		else
		{
			llwarns << "Attempted to request blocked asset "
					<< mParams.getAssetID() << ":"
					<< LLAssetType::lookupHumanReadable(mParams.getAssetType())
					<< llendl;
			sendTransferStatus(LLTS_ERROR);
		}
	}
	else
	{
		llwarns << "Attempted to request asset " << mParams.getAssetID() << ":"
				<< LLAssetType::lookupHumanReadable(mParams.getAssetType())
				<< " without an asset storage system !" << llendl;
		sendTransferStatus(LLTS_ERROR);
	}
}

F32 LLTransferSourceAsset::updatePriority()
{
	return 0.f;
}

LLTSCode LLTransferSourceAsset::dataCallback(S32 packet_id, S32 max_bytes,
											U8** data_handle,
											S32& returned_bytes,
											bool& delete_returned)
{
	if (!mGotResponse)
	{
		return LLTS_SKIP;
	}

	if (!gAssetStoragep)
	{
		llwarns << "Aborting transfer after asset storage shut down !"
				<< llendl;
		return LLTS_ERROR;
	}

	LLFileSystem vf(mParams.getAssetID());
	if (!vf.getSize())
	{
		// Something bad happened with the asset request!
		return LLTS_ERROR;
	}

	if (packet_id != mLastPacketID + 1)
	{
		llwarns << "Cannot handle out of order file transfer !" << llendl;
		llassert(false);
		return LLTS_ERROR;
	}

	// grab a buffer from the right place in the file
	if (!vf.seek(mCurPos, 0))
	{
		llwarns << "Cannot seek to " << mCurPos << " length " << vf.getSize()
				<< " while sending " << mParams.getAssetID() << llendl;
		return LLTS_ERROR;
	}

	delete_returned = true;
	U8* tmpp = new U8[max_bytes];
	*data_handle = tmpp;
	if (!vf.read(tmpp, max_bytes))
	{
		// Read failure, need to deal with it.
		delete[] tmpp;
		*data_handle = NULL;
		returned_bytes = 0;
		delete_returned = false;
		return LLTS_ERROR;
	}

	returned_bytes = vf.getLastBytesRead();
	mCurPos += returned_bytes;


	if (vf.eof())
	{
		if (!returned_bytes)
		{
			delete[] tmpp;
			*data_handle = NULL;
			returned_bytes = 0;
			delete_returned = false;
		}
		return LLTS_DONE;
	}

	return LLTS_OK;
}

void LLTransferSourceAsset::completionCallback(LLTSCode status)
{
	// No matter what happens, all we want to do is close the vfile if
	// we have got it open.
}

void LLTransferSourceAsset::packParams(LLDataPacker& dp) const
{
	mParams.packParams(dp);
}

bool LLTransferSourceAsset::unpackParams(LLDataPacker &dp)
{
	return mParams.unpackParams(dp);
}

void LLTransferSourceAsset::responderCallback(const LLUUID& uuid,
											  LLAssetType::EType type,
											  void* user_data, S32 result,
											  LLExtStat ext_status)
{
	LLUUID* tidp = (LLUUID*)user_data;
	LLUUID transfer_id = *(tidp);
	delete tidp;
	tidp = NULL;

	if (!gAssetStoragep)
	{
		llwarns << "Aborting transfer after asset storage shut down !"
				<< llendl;
		return;
	}

	LLTransferSourceAsset* tsap =
		(LLTransferSourceAsset*)gTransferManager.findTransferSource(transfer_id);
	if (!tsap)
	{
		llwarns << "Aborting transfer " << transfer_id
				<< " callback, transfer source went away" << llendl;
		return;
	}

	if (result)
	{
		llwarns << "AssetStorage: Error "
				<< gAssetStoragep->getErrorString(result)
				<< ", downloading uuid: " << uuid << llendl;
	}

	LLTSCode status;

	tsap->mGotResponse = true;
	if (LL_ERR_NOERR == result)
	{
		// Everything's OK.
		LLFileSystem vf(uuid);
		tsap->mSize = vf.getSize();
		status = LLTS_OK;
	}
	else
	{
		// Uh oh, something bad happened when we tried to get this asset!
		switch (result)
		{
		case LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE:
			status = LLTS_UNKNOWN_SOURCE;
			break;
		default:
			status = LLTS_ERROR;
		}
	}

	tsap->sendTransferStatus(status);
}

LLTransferSourceParamsAsset::LLTransferSourceParamsAsset()
:	LLTransferSourceParams(LLTST_ASSET),
	mAssetType(LLAssetType::AT_NONE)
{
}

void LLTransferSourceParamsAsset::setAsset(const LLUUID& asset_id,
										   LLAssetType::EType asset_type)
{
	mAssetID = asset_id;
	mAssetType = asset_type;
}

void LLTransferSourceParamsAsset::packParams(LLDataPacker& dp) const
{
	dp.packUUID(mAssetID, "AssetID");
	dp.packS32(mAssetType, "AssetType");
}

bool LLTransferSourceParamsAsset::unpackParams(LLDataPacker& dp)
{
	S32 tmp_at;

	dp.unpackUUID(mAssetID, "AssetID");
	dp.unpackS32(tmp_at, "AssetType");

	mAssetType = (LLAssetType::EType)tmp_at;

	return true;
}
