/**
 * @file llviewerwearable.h
 * @brief LLViewerWearable class header file
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

#ifndef LL_VIEWER_WEARABLE_H
#define LL_VIEWER_WEARABLE_H

#include "llavatarappearance.h"
#include "llavatarappearancedefines.h"
#include "llextendedstatus.h"
#include "llwearable.h"
#include "lluuid.h"

class LLViewerWearable : public LLWearable
{
	friend class LLWearableList;

protected:
	LOG_CLASS(LLViewerWearable);

	//--------------------------------------------------------------------
	// Constructors and destructors
	//--------------------------------------------------------------------
private:
	// Private constructor used by LLWearableList
	LLViewerWearable(const LLTransactionID& transactionID);
	LLViewerWearable(const LLAssetID& assetID);

public:
	~LLViewerWearable() override = default;

	LL_INLINE LLViewerWearable* asViewerWearable() override
	{
		return this;
	}

	LL_INLINE const LLViewerWearable* asViewerWearable() const override
	{
		return this;
	}

	//--------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------
	LL_INLINE const LLUUID& getItemID() const			{ return mItemID; }
	LL_INLINE const LLAssetID& getAssetID() const		{ return mAssetID; }

	LL_INLINE const LLTransactionID& getTransactionID() const
	{
		return mTransactionID;
	}

	void setItemID(const LLUUID& item_id);

	bool isDirty() const;
	bool isOldVersion() const;

	void writeToAvatar(LLAvatarAppearance* avatarp) override;

	LL_INLINE void removeFromAvatar(bool upload_bake)
	{
		LLViewerWearable::removeFromAvatar(mType, upload_bake);
	}

	static void removeFromAvatar(LLWearableType::EType type, bool upload_bake);

	EImportResult importStream(std::istream& input_stream,
							   LLAvatarAppearance* avatarp) override;

	void setParamsToDefaults();
	void setTexturesToDefaults();

	// true when doing preview renders, some updates will be suppressed.
	LL_INLINE void setVolatile(bool is_volatile)		{ mVolatile = is_volatile; }
	LL_INLINE bool getVolatile()						{ return mVolatile; }

	LLUUID getDefaultTextureImageID(LLAvatarAppearanceDefines::ETextureIndex index) override;

	void saveNewAsset() const;
	static void onSaveNewAssetComplete(const LLUUID& asset_uuid,
									   void* user_data, S32 status,
									   LLExtStat ext_status);

	void copyDataFrom(const LLViewerWearable* src);

	friend std::ostream& operator<<(std::ostream &s,
									const LLViewerWearable &w);

	void revertValues() override;
	void saveValues() override;

	LL_INLINE void revertValuesWithoutUpdate()			{ LLWearable::revertValues(); }

	// Something happened that requires the wearable's label to be updated
	// (e.g. worn/unworn).
	void setUpdated() const override;

	// The wearable was worn. make sure the name of the wearable object matches
	// the LLViewerInventoryItem, not the wearable asset itself.
	void refreshName();

	// Update the baked texture hash.
	void addToBakedTextureHash(LLMD5& hash) const override;

protected:
	LLAssetID				mAssetID;
	LLTransactionID			mTransactionID;
	// ID of the inventory item in the agent's inventory:
	LLUUID					mItemID;

	// true when rendering preview images. Can suppress some updates.
	bool					mVolatile;

	// Cache used by getDefaultTextureImageID() for speed
	static std::map<LLAvatarAppearanceDefines::ETextureIndex, LLUUID> sCachedTextures;
};

class LLWearableSaveData
{
public:
	LLWearableSaveData(LLWearableType::EType type);
	~LLWearableSaveData();

	LL_INLINE static void resetSavedWearableCount()		{ sSavedWearableCount = 0; }
	LL_INLINE static bool pendingSavedWearables()		{ return sSavedWearableCount != 0; }

public:
	LLWearableType::EType	mType;
	bool					mResetCOFTimer;

	static bool				sResetCOFTimer;

private:
	static U32				sSavedWearableCount;
};

#endif  // LL_VIEWER_WEARABLE_H
