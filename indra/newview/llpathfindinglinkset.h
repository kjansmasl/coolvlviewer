/**
 * @file llpathfindinglinkset.h
 * @brief Definition of a pathfinding linkset that contains various properties required for havok pathfinding.
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef LL_LLPATHFINDINGLINKSET_H
#define LL_LLPATHFINDINGLINKSET_H

#include "llpathfindingobject.h"

class LLSD;

constexpr S32 MIN_WALKABILITY_VALUE = 0;
constexpr S32 MAX_WALKABILITY_VALUE = 100;

class LLPathfindingLinkset : public LLPathfindingObject
{
protected:
	LOG_CLASS(LLPathfindingLinkset);

public:
	typedef enum
	{
		kUnknown,
		kWalkable,
		kStaticObstacle,
		kDynamicObstacle,
		kMaterialVolume,
		kExclusionVolume,
		kDynamicPhantom
	} ELinksetUse;

	LLPathfindingLinkset(const LLSD& terrain_data);
	LLPathfindingLinkset(const LLUUID& id, const LLSD& data);
	LLPathfindingLinkset(const LLPathfindingLinkset& obj);

	LLPathfindingLinkset& operator=(const LLPathfindingLinkset& obj);

	LL_INLINE LLPathfindingLinkset* asLinkset() override
	{
		return this;
	}

	LL_INLINE const LLPathfindingLinkset* asLinkset() const override
	{
		return this;
	}

	LL_INLINE U32 getLandImpact() const					{ return mLandImpact; }
	LL_INLINE bool isTerrain() const					{ return mIsTerrain; }
	LL_INLINE bool isModifiable() const					{ return mIsModifiable; }
	LL_INLINE bool canBeVolume() const					{ return mCanBeVolume; }
	bool isPhantom() const;

	static ELinksetUse getLinksetUseWithToggledPhantom(ELinksetUse use);

	LL_INLINE ELinksetUse getLinksetUse() const			{ return mLinksetUse; }

	LL_INLINE bool isScripted() const					{ return mIsScripted; }
	LL_INLINE bool hasIsScripted() const				{ return mHasIsScripted; }

	LL_INLINE S32 getWalkabilityCoefficientA() const	{ return mWalkabilityCoefficientA; }
	LL_INLINE S32 getWalkabilityCoefficientB() const	{ return mWalkabilityCoefficientB; }
	LL_INLINE S32 getWalkabilityCoefficientC() const	{ return mWalkabilityCoefficientC; }
	LL_INLINE S32 getWalkabilityCoefficientD() const	{ return mWalkabilityCoefficientD; }

	bool showUnmodifiablePhantomWarning(ELinksetUse use) const;
	bool showPhantomToggleWarning(ELinksetUse use) const;
	bool showCannotBeVolumeWarning(ELinksetUse use) const;
	LLSD encodeAlteredFields(ELinksetUse use, S32 a, S32 b, S32 c,
							 S32 d) const;

private:
	typedef enum
	{
		kNavMeshGenerationIgnore,
		kNavMeshGenerationInclude,
		kNavMeshGenerationExclude
	} ENavMeshGenerationCategory;

	void parseLinksetData(const LLSD& data);
	void parsePathfindingData(const LLSD& data);

	static bool isPhantom(ELinksetUse use);
	static ELinksetUse getLinksetUse(bool phantom, ENavMeshGenerationCategory category);
	static ENavMeshGenerationCategory getNavMeshGenerationCategory(ELinksetUse use);
	static LLSD convertCategoryToLLSD(ENavMeshGenerationCategory category);
	static ENavMeshGenerationCategory convertCategoryFromLLSD(const LLSD& llsd);

private:
	S32			mWalkabilityCoefficientA;
	S32			mWalkabilityCoefficientB;
	S32			mWalkabilityCoefficientC;
	S32			mWalkabilityCoefficientD;
	U32			mLandImpact;
	ELinksetUse	mLinksetUse;
	bool		mIsTerrain;
	bool		mIsModifiable;
	bool		mCanBeVolume;
	bool		mIsScripted;
	bool		mHasIsScripted;
};

#endif // LL_LLPATHFINDINGLINKSET_H
