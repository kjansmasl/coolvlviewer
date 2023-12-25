/**
 * @file lltransfersourceasset.h
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

#ifndef LL_LLTRANSFERSOURCEASSET_H
#define LL_LLTRANSFERSOURCEASSET_H

#include "llassetstorage.h"
#include "lltransfermanager.h"

class LLTransferSourceParamsAsset : public LLTransferSourceParams
{
public:
	LLTransferSourceParamsAsset();

	void packParams(LLDataPacker& dp) const override;
	bool unpackParams(LLDataPacker& dp) override;

	void setAsset(const LLUUID& asset_id, LLAssetType::EType asset_type);

	LLUUID getAssetID() const						{ return mAssetID; }
	LLAssetType::EType getAssetType() const			{ return mAssetType; }

protected:
	LLUUID				mAssetID;
	LLAssetType::EType	mAssetType;
};

class LLTransferSourceAsset : public LLTransferSource
{
protected:
	LOG_CLASS(LLTransferSourceAsset);

public:
	LLTransferSourceAsset(const LLUUID& request_id, F32 priority);

	static void responderCallback(const LLUUID& uuid, LLAssetType::EType type,
								  void* user_data, S32 result,
								  LLExtStat ext_status);
protected:
	void initTransfer() override;
	F32 updatePriority() override;
	LLTSCode dataCallback(S32 packet_id, S32 max_bytes, U8** datap,
						  S32& returned_bytes, bool& delete_returned) override;
	void completionCallback(LLTSCode status) override;

	void packParams(LLDataPacker& dp) const override;
	bool unpackParams(LLDataPacker& dp) override;

protected:
	LLTransferSourceParamsAsset mParams;
	bool mGotResponse;

	S32 mCurPos;
};

#endif // LL_LLTRANSFERSOURCEASSET_H
