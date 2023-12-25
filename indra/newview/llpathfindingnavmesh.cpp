/**
 * @file llpathfindingnavmesh.cpp
 * @brief Implementation of llpathfindingnavmesh
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

#include "llpathfindingnavmesh.h"

#include "llsd.h"
#include "llsdserialize.h"

#include "llpathfindingnavmeshstatus.h"

#define NAVMESH_VERSION_FIELD "navmesh_version"
#define NAVMESH_DATA_FIELD    "navmesh_data"

LLPathfindingNavMesh::LLPathfindingNavMesh(const LLUUID& region_id)
:	mNavMeshStatus(region_id),
	mNavMeshRequestStatus(kNavMeshRequestUnknown),
	mNavMeshSignal(),
	mNavMeshData()
{
}

LLPathfindingNavMesh::navmesh_slot_t LLPathfindingNavMesh::registerNavMeshListener(navmesh_cb_t callback)
{
	return mNavMeshSignal.connect(callback);
}

bool LLPathfindingNavMesh::hasNavMeshVersion(const LLPathfindingNavMeshStatus& status) const
{
	return mNavMeshStatus.getVersion() == status.getVersion() &&
		   (mNavMeshRequestStatus == kNavMeshRequestStarted ||
			mNavMeshRequestStatus == kNavMeshRequestCompleted ||
			(mNavMeshRequestStatus == kNavMeshRequestChecking &&
			 !mNavMeshData.empty()));
}

void LLPathfindingNavMesh::handleNavMeshWaitForRegionLoad()
{
	setRequestStatus(kNavMeshRequestWaiting);
}

void LLPathfindingNavMesh::handleNavMeshCheckVersion()
{
	setRequestStatus(kNavMeshRequestChecking);
}

void LLPathfindingNavMesh::handleRefresh(const LLPathfindingNavMeshStatus& status)
{
	if (mNavMeshStatus.getRegionUUID() != status.getRegionUUID())
	{
		llwarns << "Navmesh status received for another region: ignoring."
				<< llendl;
		return;
	}
	if (mNavMeshStatus.getVersion() != status.getVersion())
	{
		llwarns << "Navmesh status received with bad version: ignoring."
				<< llendl;
		return;
	}
	mNavMeshStatus = status;
	if (mNavMeshRequestStatus == kNavMeshRequestChecking)
	{
		if (!mNavMeshData.empty())
		{
			setRequestStatus(kNavMeshRequestCompleted);
		}
		else
		{
			llwarns << "Empty navmesh data received !" << llendl;
		}
	}
	else
	{
		sendStatus();
	}
}

void LLPathfindingNavMesh::handleNavMeshNewVersion(const LLPathfindingNavMeshStatus& status)
{
	if (mNavMeshStatus.getRegionUUID() != status.getRegionUUID())
	{
		llwarns << "Navmesh version received for another region: ignoring."
				<< llendl;
		return;
	}
	if (mNavMeshStatus.getVersion() == status.getVersion())
	{
		mNavMeshStatus = status;
		sendStatus();
	}
	else
	{
		mNavMeshData.clear();
		mNavMeshStatus = status;
		setRequestStatus(kNavMeshRequestNeedsUpdate);
	}
}

void LLPathfindingNavMesh::handleNavMeshStart(const LLPathfindingNavMeshStatus& status)
{
	if (mNavMeshStatus.getRegionUUID() != status.getRegionUUID())
	{
		llwarns << "Navmesh start signal received for another region: ignoring."
				<< llendl;
		return;
	}
	mNavMeshStatus = status;
	setRequestStatus(kNavMeshRequestStarted);
}

void LLPathfindingNavMesh::handleNavMeshResult(const LLSD& content, U32 version)
{
	if (content.has(NAVMESH_VERSION_FIELD) &&
		content.get(NAVMESH_VERSION_FIELD).isInteger() &&
		content.get(NAVMESH_VERSION_FIELD).asInteger() >= 0)
	{
		U32 advertized = (U32)content.get(NAVMESH_VERSION_FIELD).asInteger();
		if (advertized != version)
		{
			llwarns << "Mismatch between expected and embedded navmesh versions occurred"
					<< llendl;
			version = advertized;
		}
	}
	else
	{
		llwarns << "Malformed navmesh data: missing version" << llendl;
	}

	if (mNavMeshStatus.getVersion() == version)
	{
		ENavMeshRequestStatus status;
		if (content.has(NAVMESH_DATA_FIELD))
		{
			const LLSD::Binary& value =
				content.get(NAVMESH_DATA_FIELD).asBinary();
			bool valid = false;
			size_t decomp_size = 0;
			U8* buffer = unzip_llsdNavMesh(valid, decomp_size, value.data(),
										   value.size());
			if (!valid || !buffer)
			{
				llwarns << "Unable to decompress the navmesh llsd." << llendl;
				status = kNavMeshRequestError;
			}
			else
			{
				mNavMeshData.resize(decomp_size);
				memcpy(&mNavMeshData[0], &buffer[0], decomp_size);
				status = kNavMeshRequestCompleted;
			}
			if (buffer)
			{
				free(buffer);
			}
		}
		else
		{
			llwarns << "No mesh data received" << llendl;
			status = kNavMeshRequestError;
		}
		setRequestStatus(status);
	}
}

void LLPathfindingNavMesh::handleNavMeshNotEnabled()
{
	mNavMeshData.clear();
	setRequestStatus(kNavMeshRequestNotEnabled);
}

void LLPathfindingNavMesh::handleNavMeshError()
{
	mNavMeshData.clear();
	setRequestStatus(kNavMeshRequestError);
}

void LLPathfindingNavMesh::handleNavMeshError(U32 version)
{
	if (mNavMeshStatus.getVersion() == version)
	{
		handleNavMeshError();
	}
}

void LLPathfindingNavMesh::setRequestStatus(ENavMeshRequestStatus statusp)
{
	mNavMeshRequestStatus = statusp;
	sendStatus();
}

void LLPathfindingNavMesh::sendStatus()
{
	mNavMeshSignal(mNavMeshRequestStatus, mNavMeshStatus, mNavMeshData);
}
