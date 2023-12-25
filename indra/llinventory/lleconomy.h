/**
 * @file lleconomy.h
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

#ifndef LL_LLECONOMY_H
#define LL_LLECONOMY_H

#include "llsingleton.h"

class LLSD;
class LLMessageSystem;
class LLVector3;

// These are the defaults for SL
constexpr S32 DEFAULT_UPLOAD_COST = 10;
constexpr S32 DEFAULT_GROUP_COST = 100;
constexpr S32 DEFAULT_MAX_PICKS = 10;

class LLEconomy final : public LLSingleton<LLEconomy>
{
	friend class LLSingleton<LLEconomy>;

protected:
	LOG_CLASS(LLEconomy);

public:
	LLEconomy();

	void setDefaultCosts(bool in_sl);
	void processEconomyData(LLMessageSystem* msg);
	void setBenefits(const LLSD& data, const std::string& account_type);

	LL_INLINE S32 getPriceUpload() const			{ return mPriceUpload; }
	LL_INLINE S32 getAnimationUploadCost() const	{ return mAnimationUploadCost; }
	LL_INLINE S32 getSoundUploadCost() const		{ return mSoundUploadCost; }
	LL_INLINE S32 getTextureUploadCost() const		{ return mTextureUploadCost; }
	LL_INLINE S32 getCreateGroupCost() const		{ return mCreateGroupCost; }
	LL_INLINE S32 getAttachmentLimit() const		{ return mAttachmentLimit; }
	LL_INLINE S32 getAnimatedObjectLimit() const	{ return mAnimatedObjectLimit; }
	LL_INLINE S32 getGroupMembershipLimit() const	{ return mGroupMembershipLimit; }

	LL_INLINE S32 getPicksLimit() const
	{
		return mPicksLimit > -1 ? mPicksLimit : DEFAULT_MAX_PICKS;
	}

	const LLSD& getBenefit(const std::string& key) const;

private:
	std::string	mAccountType;

	LLSD		mBenefits;

	// Note: mPriceUpload is now llmax(mAnimationUploadCost, mSoundUploadCost,
	// mTextureUploadCost) when benefits are implemented in the grid (when not
	// all four costs are equal).
	S32			mPriceUpload;
	S32			mAnimationUploadCost;
	S32			mSoundUploadCost;
	S32			mTextureUploadCost;
	S32			mCreateGroupCost;
	S32			mAttachmentLimit;
	S32			mAnimatedObjectLimit;
	S32			mGroupMembershipLimit;
	S32			mPicksLimit;

	bool		mGotBenefits;
};

#endif
