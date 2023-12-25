/**
 * @file llpathfindingnavmeshstatus.h
 * @brief Header file for llpathfindingnavmeshstatus
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

#ifndef LL_LLPATHFINDINGNAVMESHSTATUS_H
#define LL_LLPATHFINDINGNAVMESHSTATUS_H

#include <string>

#include "lluuid.h"

class LLSD;

class LLPathfindingNavMeshStatus
{
protected:
	LOG_CLASS(LLPathfindingNavMeshStatus);

public:
	typedef enum
	{
		kPending,
		kBuilding,
		kComplete,
		kRepending
	} ENavMeshStatus;

	LLPathfindingNavMeshStatus();
	LLPathfindingNavMeshStatus(const LLUUID& region_id);
	LLPathfindingNavMeshStatus(const LLUUID& region_id, const LLSD& content);
	LLPathfindingNavMeshStatus(const LLSD& content);
	LLPathfindingNavMeshStatus(const LLPathfindingNavMeshStatus& status);

	LLPathfindingNavMeshStatus& operator=(const LLPathfindingNavMeshStatus& status);

	LL_INLINE bool isValid() const					{ return mIsValid; }
	LL_INLINE const LLUUID& getRegionUUID() const	{ return mRegionUUID; }
	LL_INLINE U32 getVersion() const				{ return mVersion; }
	LL_INLINE ENavMeshStatus getStatus() const		{ return mStatus; }

private:
	void parseStatus(const LLSD& content);

private:
	bool						mIsValid;
	LLUUID						mRegionUUID;
	U32							mVersion;
	ENavMeshStatus				mStatus;

	static const std::string	sStatusPending;
	static const std::string	sStatusBuilding;
	static const std::string	sStatusComplete;
	static const std::string	sStatusRepending;
};

#endif // LL_LLPATHFINDINGNAVMESHSTATUS_H
