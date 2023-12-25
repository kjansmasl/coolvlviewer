/**
 * @file llpathfindingnavmeshstatus.cpp
 * @brief Implementation of llpathfindingnavmeshstatus
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

#include "llpathfindingnavmeshstatus.h"

#include "llsd.h"

#define REGION_FIELD  "region_id"
#define STATUS_FIELD  "status"
#define VERSION_FIELD "version"

const std::string LLPathfindingNavMeshStatus::sStatusPending("pending");
const std::string LLPathfindingNavMeshStatus::sStatusBuilding("building");
const std::string LLPathfindingNavMeshStatus::sStatusComplete("complete");
const std::string LLPathfindingNavMeshStatus::sStatusRepending("repending");

LLPathfindingNavMeshStatus::LLPathfindingNavMeshStatus()
:	mIsValid(false),
	mRegionUUID(),
	mVersion(0U),
	mStatus(kComplete)
{
}

LLPathfindingNavMeshStatus::LLPathfindingNavMeshStatus(const LLUUID& region_id)
:	mIsValid(false),
	mRegionUUID(region_id),
	mVersion(0U),
	mStatus(kComplete)
{
}

LLPathfindingNavMeshStatus::LLPathfindingNavMeshStatus(const LLUUID& region_id,
													   const LLSD& content)
:	mIsValid(true),
	mRegionUUID(region_id),
	mVersion(0U),
	mStatus(kComplete)
{
	parseStatus(content);
}

LLPathfindingNavMeshStatus::LLPathfindingNavMeshStatus(const LLSD& content)
:	mIsValid(true),
	mVersion(0U),
	mStatus(kComplete)
{
	llassert(content.has(REGION_FIELD));
	llassert(content.get(REGION_FIELD).isUUID());
	mRegionUUID = content.get(REGION_FIELD).asUUID();

	parseStatus(content);
}

LLPathfindingNavMeshStatus::LLPathfindingNavMeshStatus(const LLPathfindingNavMeshStatus& status)
:	mIsValid(status.mIsValid),
	mRegionUUID(status.mRegionUUID),
	mVersion(status.mVersion),
	mStatus(status.mStatus)
{
}

LLPathfindingNavMeshStatus &LLPathfindingNavMeshStatus::operator=(const LLPathfindingNavMeshStatus& status)
{
	mIsValid = status.mIsValid;
	mRegionUUID = status.mRegionUUID;
	mVersion = status.mVersion;
	mStatus = status.mStatus;

	return *this;
}

void LLPathfindingNavMeshStatus::parseStatus(const LLSD& content)
{
	if (content.has(VERSION_FIELD) &&
		content.get(VERSION_FIELD).isInteger() &&
		content.get(VERSION_FIELD).asInteger() >= 0)
	{
		mVersion = static_cast<U32>(content.get(VERSION_FIELD).asInteger());
	}
	else
	{
		llwarns << "Malformed navmesh status data: missing version"
				<< llendl;
	}

	if (!content.has(STATUS_FIELD) || !content.get(STATUS_FIELD).isString())
	{
		llwarns << "Malformed navmesh status data: missing status. Aborting !"
				<< llendl;
		return;
	}
	std::string status = content.get(STATUS_FIELD).asString();

	if (LLStringUtil::compareStrings(status, sStatusPending) == 0)
	{
		mStatus = kPending;
	}
	else if (LLStringUtil::compareStrings(status, sStatusBuilding) == 0)
	{
		mStatus = kBuilding;
	}
	else if (LLStringUtil::compareStrings(status, sStatusComplete) == 0)
	{
		mStatus = kComplete;
	}
	else if (LLStringUtil::compareStrings(status, sStatusRepending) == 0)
	{
		mStatus = kRepending;
	}
	else
	{
		mStatus = kComplete;
		llwarns << "Malformed navmesh status data: bad status" << llendl;
	}
}
