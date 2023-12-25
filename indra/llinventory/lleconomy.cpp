/** 
 * @file lleconomy.cpp
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

#include "lleconomy.h"

#include "llsdutil.h"
#include "llmessage.h"

LLEconomy::LLEconomy()
:	mPriceUpload(-1),
	mAnimationUploadCost(-1),
	mSoundUploadCost(-1),
	mTextureUploadCost(-1),
	mCreateGroupCost(-1),
	mAttachmentLimit(-1),
	mAnimatedObjectLimit(-1),
	mGroupMembershipLimit(-1),
	mPicksLimit(-1),
	mGotBenefits(false)
{
}

void LLEconomy::setDefaultCosts(bool in_sl)
{
	mPriceUpload = mAnimationUploadCost = mSoundUploadCost =
				   mTextureUploadCost = in_sl ? DEFAULT_UPLOAD_COST : 0;
	mCreateGroupCost = in_sl ? DEFAULT_GROUP_COST : 0;
	llinfos << "Price per upload: " << mPriceUpload
			<< " - Price for group creation: " << mCreateGroupCost << llendl;
}

void LLEconomy::processEconomyData(LLMessageSystem* msg)
{
	if (mGotBenefits)
	{
		llinfos << "Received legacy message for economy data after valid user account benefits were set. Ignoring."
				<< llendl;
		return;
	}

	msg->getS32Fast(_PREHASH_Info, _PREHASH_PriceUpload, mPriceUpload);
	msg->getS32Fast(_PREHASH_Info, _PREHASH_PriceGroupCreate,
					mCreateGroupCost);
#if 0	// Old economy data, never used...
	S32 i;
	msg->getS32Fast(_PREHASH_Info, _PREHASH_ObjectCapacity, i);
	msg->getS32Fast(_PREHASH_Info, _PREHASH_ObjectCount, i);
	msg->getS32Fast(_PREHASH_Info, _PREHASH_PriceEnergyUnit, i);
	msg->getS32Fast(_PREHASH_Info, _PREHASH_PriceObjectClaim, i);
	msg->getS32Fast(_PREHASH_Info, _PREHASH_PricePublicObjectDecay, i);
	msg->getS32Fast(_PREHASH_Info, _PREHASH_PricePublicObjectDelete, i);
	msg->getS32Fast(_PREHASH_Info, _PREHASH_PriceRentLight, i);
	msg->getS32Fast(_PREHASH_Info, _PREHASH_TeleportMinPrice, i);
	F32 f;
	msg->getF32Fast(_PREHASH_Info, _PREHASH_TeleportPriceExponent, f);
#endif
	llinfos << "Received economy data. Price per upload: " << mPriceUpload
			<< " - Price for group creation: " << mCreateGroupCost << llendl;

	if (mAnimationUploadCost == -1)
	{
		mAnimationUploadCost = mPriceUpload;
	}
	if (mSoundUploadCost == -1)
	{
		mSoundUploadCost = mPriceUpload;
	}
	if (mTextureUploadCost == -1)
	{
		mTextureUploadCost = mPriceUpload;
	}
}

bool get_S32_value(const LLSD& sd, const LLSD::String& key, S32& value)
{
	if (sd.has(key))
	{
		value = sd[key].asInteger();
		llinfos << "  - " << key << ": " << value << llendl;
		return true;
	}
	return false;
}

void LLEconomy::setBenefits(const LLSD& data, const std::string& account_type)
{
	LL_DEBUGS("Benefits") << ll_pretty_print_sd(data) << LL_ENDL;
	llinfos << "Account type: " << account_type << " - Setting benefits:"
			<< llendl;
	mAccountType = account_type;
	mBenefits = data;

	mGotBenefits = true;

	if (get_S32_value(data, "animation_upload_cost", mAnimationUploadCost))
	{
		if (mAnimationUploadCost > mPriceUpload)
		{
			mPriceUpload = mAnimationUploadCost;
		}
	}
	else
	{
		mGotBenefits = false;
	}

	if (get_S32_value(data, "sound_upload_cost", mSoundUploadCost))
	{
		if (mSoundUploadCost > mPriceUpload)
		{
			mPriceUpload = mSoundUploadCost;
		}
	}
	else
	{
		mGotBenefits = false;
	}

	if (get_S32_value(data, "texture_upload_cost", mTextureUploadCost))
	{
		if (mTextureUploadCost > mPriceUpload)
		{
			mPriceUpload = mTextureUploadCost;
		}
	}
	else
	{
		mGotBenefits = false;
	}

	if (!get_S32_value(data, "create_group_cost", mCreateGroupCost))
	{
		mGotBenefits = false;
	}

	get_S32_value(data, "attachment_limit", mAttachmentLimit);
	get_S32_value(data, "animated_object_limit", mAnimatedObjectLimit);
	get_S32_value(data, "group_membership_limit", mGroupMembershipLimit);
	get_S32_value(data, "picks_limit", mPicksLimit);

	llinfos << "Done." << llendl;
}

const LLSD& LLEconomy::getBenefit(const std::string& key) const
{
	static const LLSD empty;
	return mBenefits.has(key) ? mBenefits[key] : empty;
}
