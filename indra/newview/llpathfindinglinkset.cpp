/**
 * @file llpathfindinglinkset.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llpathfindinglinkset.h"

#include "llsd.h"

#define LINKSET_LAND_IMPACT_FIELD	"landimpact"
#define LINKSET_MODIFIABLE_FIELD	"modifiable"
#define LINKSET_CATEGORY_FIELD		"navmesh_category"
#define LINKSET_CAN_BE_VOLUME		"can_be_volume"
#define LINKSET_IS_SCRIPTED_FIELD	"is_scripted"
#define LINKSET_PHANTOM_FIELD		"phantom"
#define LINKSET_WALKABILITY_A_FIELD	"A"
#define LINKSET_WALKABILITY_B_FIELD	"B"
#define LINKSET_WALKABILITY_C_FIELD	"C"
#define LINKSET_WALKABILITY_D_FIELD	"D"

#define LINKSET_CATEGORY_VALUE_INCLUDE	0
#define LINKSET_CATEGORY_VALUE_EXCLUDE	1
#define LINKSET_CATEGORY_VALUE_IGNORE	2

LLPathfindingLinkset::LLPathfindingLinkset(const LLSD& terrain_data)
:	LLPathfindingObject(),
	mIsTerrain(true),
	mLandImpact(0U),
	mIsModifiable(false),
	mCanBeVolume(false),
	mIsScripted(false),
	mHasIsScripted(true),
	mLinksetUse(kUnknown),
	mWalkabilityCoefficientA(MIN_WALKABILITY_VALUE),
	mWalkabilityCoefficientB(MIN_WALKABILITY_VALUE),
	mWalkabilityCoefficientC(MIN_WALKABILITY_VALUE),
	mWalkabilityCoefficientD(MIN_WALKABILITY_VALUE)
{
	parsePathfindingData(terrain_data);
}

LLPathfindingLinkset::LLPathfindingLinkset(const LLUUID& id, const LLSD& data)
:	LLPathfindingObject(id, data),
	mIsTerrain(false),
	mLandImpact(0U),
	mIsModifiable(true),
	mCanBeVolume(true),
	mIsScripted(false),
	mHasIsScripted(false),
	mLinksetUse(kUnknown),
	mWalkabilityCoefficientA(MIN_WALKABILITY_VALUE),
	mWalkabilityCoefficientB(MIN_WALKABILITY_VALUE),
	mWalkabilityCoefficientC(MIN_WALKABILITY_VALUE),
	mWalkabilityCoefficientD(MIN_WALKABILITY_VALUE)
{
	parseLinksetData(data);
	parsePathfindingData(data);
}

LLPathfindingLinkset::LLPathfindingLinkset(const LLPathfindingLinkset& obj)
:	LLPathfindingObject(obj),
	mIsTerrain(obj.mIsTerrain),
	mLandImpact(obj.mLandImpact),
	mIsModifiable(obj.mIsModifiable),
	mCanBeVolume(obj.mCanBeVolume),
	mIsScripted(obj.mIsScripted),
	mHasIsScripted(obj.mHasIsScripted),
	mLinksetUse(obj.mLinksetUse),
	mWalkabilityCoefficientA(obj.mWalkabilityCoefficientA),
	mWalkabilityCoefficientB(obj.mWalkabilityCoefficientB),
	mWalkabilityCoefficientC(obj.mWalkabilityCoefficientC),
	mWalkabilityCoefficientD(obj.mWalkabilityCoefficientD)
{
}

LLPathfindingLinkset& LLPathfindingLinkset::operator=(const LLPathfindingLinkset& obj)
{
	dynamic_cast<LLPathfindingObject&>(*this) = obj;

	mIsTerrain = obj.mIsTerrain;
	mLandImpact = obj.mLandImpact;
	mIsModifiable = obj.mIsModifiable;
	mCanBeVolume = obj.mCanBeVolume;
	mIsScripted = obj.mIsScripted;
	mHasIsScripted = obj.mHasIsScripted;
	mLinksetUse = obj.mLinksetUse;
	mWalkabilityCoefficientA = obj.mWalkabilityCoefficientA;
	mWalkabilityCoefficientB = obj.mWalkabilityCoefficientB;
	mWalkabilityCoefficientC = obj.mWalkabilityCoefficientC;
	mWalkabilityCoefficientD = obj.mWalkabilityCoefficientD;

	return *this;
}

bool LLPathfindingLinkset::isPhantom() const
{
	return isPhantom(getLinksetUse());
}

LLPathfindingLinkset::ELinksetUse LLPathfindingLinkset::getLinksetUseWithToggledPhantom(ELinksetUse use)
{
	bool phantom = isPhantom(use);
	ENavMeshGenerationCategory category = getNavMeshGenerationCategory(use);
	return getLinksetUse(!phantom, category);
}

bool LLPathfindingLinkset::showUnmodifiablePhantomWarning(ELinksetUse use) const
{
	return !isModifiable() && isPhantom() != isPhantom(use);
}

bool LLPathfindingLinkset::showPhantomToggleWarning(ELinksetUse use) const
{
	return isModifiable() && isPhantom() != isPhantom(use);
}

bool LLPathfindingLinkset::showCannotBeVolumeWarning(ELinksetUse use) const
{
	return !canBeVolume() &&
		   (use == kMaterialVolume || use == kExclusionVolume);
}

LLSD LLPathfindingLinkset::encodeAlteredFields(ELinksetUse use, S32 a, S32 b,
											   S32 c, S32 d) const
{
	LLSD data;

	if (!isTerrain() && use != kUnknown && getLinksetUse() != use &&
		(canBeVolume() || (use != kMaterialVolume && use != kExclusionVolume)))
	{
		if (isModifiable())
		{
			data[LINKSET_PHANTOM_FIELD] = isPhantom(use);
		}

		data[LINKSET_CATEGORY_FIELD] =
			convertCategoryToLLSD(getNavMeshGenerationCategory(use));
	}

	if (mWalkabilityCoefficientA != a)
	{
		data[LINKSET_WALKABILITY_A_FIELD] = llclamp(a, MIN_WALKABILITY_VALUE,
													MAX_WALKABILITY_VALUE);
	}

	if (mWalkabilityCoefficientB != b)
	{
		data[LINKSET_WALKABILITY_B_FIELD] = llclamp(b, MIN_WALKABILITY_VALUE,
													MAX_WALKABILITY_VALUE);
	}

	if (mWalkabilityCoefficientC != c)
	{
		data[LINKSET_WALKABILITY_C_FIELD] = llclamp(c, MIN_WALKABILITY_VALUE,
													MAX_WALKABILITY_VALUE);
	}

	if (mWalkabilityCoefficientD != d)
	{
		data[LINKSET_WALKABILITY_D_FIELD] = llclamp(d, MIN_WALKABILITY_VALUE,
													MAX_WALKABILITY_VALUE);
	}

	return data;
}

void LLPathfindingLinkset::parseLinksetData(const LLSD& data)
{
	if (data.has(LINKSET_LAND_IMPACT_FIELD) &&
		data.get(LINKSET_LAND_IMPACT_FIELD).isInteger() &&
		data.get(LINKSET_LAND_IMPACT_FIELD).asInteger() >= 0)
	{
		mLandImpact = data.get(LINKSET_LAND_IMPACT_FIELD).asInteger();
	}
	else
	{
		llwarns << "Malformed pathfinding linkset data: no land impact"
				<< llendl;
	}

	if (data.has(LINKSET_MODIFIABLE_FIELD) &&
		data.get(LINKSET_MODIFIABLE_FIELD).isBoolean())
	{
		mIsModifiable = data.get(LINKSET_MODIFIABLE_FIELD).asBoolean();
	}
	else
	{
		llwarns << "Malformed pathfinding linkset data: missing modify flag"
				<< llendl;
	}

	mHasIsScripted = data.has(LINKSET_IS_SCRIPTED_FIELD);
	if (mHasIsScripted)
	{
		if (data.get(LINKSET_IS_SCRIPTED_FIELD).isBoolean())
		{
			mIsScripted = data.get(LINKSET_IS_SCRIPTED_FIELD).asBoolean();
		}
		else
		{
			llwarns << "Malformed pathfinding linkset data: bad scripted flag"
					<< llendl;
		}
	}
}

void LLPathfindingLinkset::parsePathfindingData(const LLSD& data)
{
	bool phantom = false;
	if (data.has(LINKSET_PHANTOM_FIELD))
	{
		if (data.get(LINKSET_PHANTOM_FIELD).isBoolean())
		{
			phantom = data.get(LINKSET_PHANTOM_FIELD).asBoolean();
		}
		else
		{
			llwarns << "Malformed pathfinding terrain data: invalid phantom flag"
					<< llendl;
		}
	}

	if (data.has(LINKSET_CATEGORY_FIELD))
	{
		mLinksetUse =
			getLinksetUse(phantom,
						  convertCategoryFromLLSD(data.get(LINKSET_CATEGORY_FIELD)));
	}
	else
	{
		llwarns << "Malformed pathfinding terrain data: missing category"
				<< llendl;
	}

	if (data.has(LINKSET_CAN_BE_VOLUME))
	{
		if (data.get(LINKSET_CAN_BE_VOLUME).isBoolean())
		{
			mCanBeVolume = data.get(LINKSET_CAN_BE_VOLUME).asBoolean();
		}
		else
		{
			llwarns << "Malformed pathfinding terrain data: invalid volume flag"
					<< llendl;
		}
	}

	if (data.has(LINKSET_WALKABILITY_A_FIELD) &&
		data.get(LINKSET_WALKABILITY_A_FIELD).isInteger())
	{
		mWalkabilityCoefficientA =
			data.get(LINKSET_WALKABILITY_A_FIELD).asInteger();
		if (mWalkabilityCoefficientA < MIN_WALKABILITY_VALUE)
		{
			llwarns << "Malformed pathfinding terrain data: Bad walkability A value (too low). Clamping to "
					<< MIN_WALKABILITY_VALUE << llendl;
			mWalkabilityCoefficientA = MIN_WALKABILITY_VALUE;
		}
		if (mWalkabilityCoefficientA > MAX_WALKABILITY_VALUE)
		{
			llwarns << "Malformed pathfinding terrain data: Bad walkability A value (too high). Clamping to "
					<< MAX_WALKABILITY_VALUE << llendl;
			mWalkabilityCoefficientA = MAX_WALKABILITY_VALUE;
		}
	}
	else
	{
		llwarns << "Malformed pathfinding terrain data: missing walkability A"
				<< llendl;
	}

	if (data.has(LINKSET_WALKABILITY_B_FIELD) &&
		data.get(LINKSET_WALKABILITY_B_FIELD).isInteger())
	{
		mWalkabilityCoefficientB =
			data.get(LINKSET_WALKABILITY_B_FIELD).asInteger();
		if (mWalkabilityCoefficientB < MIN_WALKABILITY_VALUE)
		{
			llwarns << "Malformed pathfinding terrain data: Bad walkability B value (too low). Clamping to "
					<< MIN_WALKABILITY_VALUE << llendl;
			mWalkabilityCoefficientB = MIN_WALKABILITY_VALUE;
		}
		if (mWalkabilityCoefficientB > MAX_WALKABILITY_VALUE)
		{
			llwarns << "Malformed pathfinding terrain data: Bad walkability B value (too high). Clamping to "
					<< MAX_WALKABILITY_VALUE << llendl;
			mWalkabilityCoefficientB = MAX_WALKABILITY_VALUE;
		}
	}
	else
	{
		llwarns << "Malformed pathfinding terrain data: missing walkability B"
				<< llendl;
	}

	if (data.has(LINKSET_WALKABILITY_C_FIELD) &&
		data.get(LINKSET_WALKABILITY_C_FIELD).isInteger())
	{
		mWalkabilityCoefficientC =
			data.get(LINKSET_WALKABILITY_C_FIELD).asInteger();
		if (mWalkabilityCoefficientC < MIN_WALKABILITY_VALUE)
		{
			llwarns << "Malformed pathfinding terrain data: Bad walkability C value (too low). Clamping to "
					<< MIN_WALKABILITY_VALUE << llendl;
			mWalkabilityCoefficientC = MIN_WALKABILITY_VALUE;
		}
		if (mWalkabilityCoefficientC > MAX_WALKABILITY_VALUE)
		{
			llwarns << "Malformed pathfinding terrain data: Bad walkability C value (too high). Clamping to "
					<< MAX_WALKABILITY_VALUE << llendl;
			mWalkabilityCoefficientC = MAX_WALKABILITY_VALUE;
		}
	}
	else
	{
		llwarns << "Malformed pathfinding terrain data: missing walkability C"
				<< llendl;
	}

	if (data.has(LINKSET_WALKABILITY_D_FIELD) &&
		data.get(LINKSET_WALKABILITY_D_FIELD).isInteger())
	{
		mWalkabilityCoefficientD =
			data.get(LINKSET_WALKABILITY_D_FIELD).asInteger();
		if (mWalkabilityCoefficientD < MIN_WALKABILITY_VALUE)
		{
			llwarns << "Malformed pathfinding terrain data: Bad walkability D value (too low). Clamping to "
					<< MIN_WALKABILITY_VALUE << llendl;
			mWalkabilityCoefficientD = MIN_WALKABILITY_VALUE;
		}
		if (mWalkabilityCoefficientD > MAX_WALKABILITY_VALUE)
		{
			llwarns << "Malformed pathfinding terrain data: Bad walkability D value (too high). Clamping to "
					<< MAX_WALKABILITY_VALUE << llendl;
			mWalkabilityCoefficientD = MAX_WALKABILITY_VALUE;
		}
	}
	else
	{
		llwarns << "Malformed pathfinding terrain data: missing walkability D"
				<< llendl;
	}
}

bool LLPathfindingLinkset::isPhantom(ELinksetUse use)
{
	switch (use)
	{
		case kWalkable:
		case kStaticObstacle:
		case kDynamicObstacle:
			return false;

		case kMaterialVolume:
		case kExclusionVolume:
		case kDynamicPhantom:
			return true;

		default:
			llassert(false);
			return false;
	}
}

LLPathfindingLinkset::ELinksetUse LLPathfindingLinkset::getLinksetUse(bool phantom,
																	  ENavMeshGenerationCategory category)
{
	if (phantom)
	{
		switch (category)
		{
			case kNavMeshGenerationIgnore:
				return kDynamicPhantom;

			case kNavMeshGenerationInclude:
				return kMaterialVolume;

			case kNavMeshGenerationExclude:
				return kExclusionVolume;

			default:
				llassert(false);
				return kUnknown;
		}
	}

	switch (category)
	{
		case kNavMeshGenerationIgnore:
			return kDynamicObstacle;

		case kNavMeshGenerationInclude:
			return kWalkable;

		case kNavMeshGenerationExclude:
			return kStaticObstacle;

		default:
			llassert(false);
			return kUnknown;
	}
}

LLPathfindingLinkset::ENavMeshGenerationCategory LLPathfindingLinkset::getNavMeshGenerationCategory(ELinksetUse use)
{
	switch (use)
	{
		case kWalkable:
		case kMaterialVolume:
			return kNavMeshGenerationInclude;

		case kStaticObstacle:
		case kExclusionVolume:
			return kNavMeshGenerationExclude;

		case kDynamicObstacle:
		case kDynamicPhantom:
			return kNavMeshGenerationIgnore;

		default:
			llassert(false);
			return kNavMeshGenerationIgnore;
	}
}

LLSD LLPathfindingLinkset::convertCategoryToLLSD(ENavMeshGenerationCategory category)
{
	switch (category)
	{
		case kNavMeshGenerationIgnore:
			return LLSD(LINKSET_CATEGORY_VALUE_IGNORE);

		case kNavMeshGenerationInclude:
			return LLSD(LINKSET_CATEGORY_VALUE_INCLUDE);

		case kNavMeshGenerationExclude:
			return LLSD(LINKSET_CATEGORY_VALUE_EXCLUDE);

		default:
			llassert(false);
			return LLSD(LINKSET_CATEGORY_VALUE_IGNORE);
	}
}

LLPathfindingLinkset::ENavMeshGenerationCategory LLPathfindingLinkset::convertCategoryFromLLSD(const LLSD& data)
{
	if (!data.isInteger())
	{
		llassert(false);
		return kNavMeshGenerationIgnore;
	}

	switch (data.asInteger())
	{
		case LINKSET_CATEGORY_VALUE_IGNORE:
			return kNavMeshGenerationIgnore;

		case LINKSET_CATEGORY_VALUE_INCLUDE:
			return kNavMeshGenerationInclude;

		case LINKSET_CATEGORY_VALUE_EXCLUDE:
			return kNavMeshGenerationExclude;

		default:
			llassert(false);
			return kNavMeshGenerationIgnore;
	}
}
