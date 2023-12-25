/**
 * @file llvolumemgr.cpp
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

#include "llvolumemgr.h"
#include "llvolume.h"

constexpr F32 BASE_THRESHOLD = 0.03f;

LLVolumeMgr* gVolumeMgrp = NULL;

// Static members
F32 LLVolumeLODGroup::sDetailThresholds[NUM_LODS] = { BASE_THRESHOLD,
													  2 * BASE_THRESHOLD,
													  8 * BASE_THRESHOLD,
													  100 * BASE_THRESHOLD };
F32 LLVolumeLODGroup::sDetailScales[NUM_LODS] = { 1.f, 1.5f, 2.5f, 4.f };

//static
void LLVolumeMgr::initClass()
{
	if (gVolumeMgrp)
	{
		llerrs << "A volume manager already exists !" << llendl;
	}
	gVolumeMgrp = new LLVolumeMgr;
}

//static
void LLVolumeMgr::cleanupClass()
{
	if (gVolumeMgrp)
	{
		delete gVolumeMgrp;
		gVolumeMgrp = NULL;
		llinfos << "Volume manager destroyed." << llendl;
	}
}

LLVolumeMgr::~LLVolumeMgr()
{
	U32 remaining = 0;

	mDataMutex.lock();
	for (volume_lod_group_map_t::iterator iter = mVolumeLODGroups.begin(),
			 							  end = mVolumeLODGroups.end();
		 iter != end; ++iter)
	{
		LLVolumeLODGroup* volgroupp = iter->second;
		if (volgroupp->cleanupRefs() == false)
		{
			++remaining;
		}
 		delete volgroupp;
	}
	mVolumeLODGroups.clear();
	mDataMutex.unlock();

	if (remaining)
	{
		llwarns << "There were " << remaining
				<< " remaining references in the volume manager." << llendl;
	}
}

// Always only ever store the results of refVolume in a LLPointer. Note however
// that LLVolumeLODGroup which contains the volume also holds a LLPointer so
// the volume will only go away after anything holding the volume and the
// LODGroup are destroyed
LLVolume* LLVolumeMgr::refVolume(const LLVolumeParams& volume_params,
								 S32 detail)
{
	if (detail < 0 || detail >= LLVolumeLODGroup::NUM_LODS)
	{
		llwarns << "Attempt to reference a volume for out of range LOD: "
				<< detail << llendl;
		return NULL;
	}

	mDataMutex.lock();
	volume_lod_group_map_t::iterator iter = mVolumeLODGroups.find(&volume_params);
	LLVolumeLODGroup* volgroupp;
	if (iter == mVolumeLODGroups.end())
	{
		volgroupp = createNewGroup(volume_params);
	}
	else
	{
		volgroupp = iter->second;
	}
	mDataMutex.unlock();
	return volgroupp->refLOD(detail);
}

LLVolumeLODGroup* LLVolumeMgr::getGroup(const LLVolumeParams& volume_params)
{
	LLVolumeLODGroup* volgroupp = NULL;
	mDataMutex.lock();
	volume_lod_group_map_t::const_iterator iter = mVolumeLODGroups.find(&volume_params);
	if (iter != mVolumeLODGroups.end())
	{
		volgroupp = iter->second;
	}
	mDataMutex.unlock();
	return volgroupp;
}

void LLVolumeMgr::unrefVolume(LLVolume* volumep)
{
	if (volumep->isUnique())
	{
		// TomY: Don't need to manage this volume. It is a unique instance.
		return;
	}
	const LLVolumeParams* params = &(volumep->getParams());
	mDataMutex.lock();
	volume_lod_group_map_t::iterator iter = mVolumeLODGroups.find(params);
	if (iter == mVolumeLODGroups.end())
	{
		mDataMutex.unlock();
		llwarns << "Tried to cleanup unknown volume type ! " << *params
				<< llendl;
		llassert(false);
		return;
	}
	else
	{
		LLVolumeLODGroup* volgroupp = iter->second;
		if (volgroupp)
		{
			volgroupp->derefLOD(volumep);
			if (volgroupp->getNumRefs() == 0)
			{
				mVolumeLODGroups.erase(params);
				delete volgroupp;
			}
		}
		else
		{
			llwarns << "Found a NULL volume LOD group !" << llendl;
		}
	}
	mDataMutex.unlock();
}

void LLVolumeMgr::insertGroup(LLVolumeLODGroup* volgroup)
{
	mVolumeLODGroups[volgroup->getVolumeParams()] = volgroup;
}

LLVolumeLODGroup* LLVolumeMgr::createNewGroup(const LLVolumeParams& volume_params)
{
	LLVolumeLODGroup* volgroup = new LLVolumeLODGroup(volume_params);
	insertGroup(volgroup);
	return volgroup;
}

void LLVolumeMgr::dump()
{
	F32 avg = 0.f;
	mDataMutex.lock();
	for (volume_lod_group_map_t::iterator iter = mVolumeLODGroups.begin(),
			 							  end = mVolumeLODGroups.end();
		 iter != end; ++iter)
	{
		LLVolumeLODGroup* volgroupp = iter->second;
		avg += volgroupp->dump();
	}
	S32 count = mVolumeLODGroups.size();
	avg = count ? avg / (F32)count : 0.0f;
	mDataMutex.unlock();
	llinfos << "Average usage of LODs " << avg << llendl;
}

std::ostream& operator<<(std::ostream& s, const LLVolumeMgr& volume_mgr)
{
	s << "{ numLODgroups=" << volume_mgr.mVolumeLODGroups.size() << ", ";

	S32 total_refs = 0;

	// Cheating out const-ness...
	LLMutex* mutex = &const_cast<LLVolumeMgr&>(volume_mgr).mDataMutex;
	mutex->lock();

	for (LLVolumeMgr::volume_lod_group_map_t::const_iterator
			iter = volume_mgr.mVolumeLODGroups.begin(),
			end = volume_mgr.mVolumeLODGroups.end();
		 iter != end; ++iter)
	{
		LLVolumeLODGroup* volgroupp = iter->second;
		total_refs += volgroupp->getNumRefs();
		s << ", " << (*volgroupp);
	}

	mutex->unlock();

	s << ", total_refs=" << total_refs << " }";
	return s;
}

LLVolumeLODGroup::LLVolumeLODGroup(const LLVolumeParams& params)
:	mVolumeParams(params),
	mRefs(0)
{
	for (S32 i = 0; i < NUM_LODS; ++i)
	{
		mLODRefs[i] = 0;
		mAccessCount[i] = 0;
	}
}

LLVolumeLODGroup::~LLVolumeLODGroup()
{
	for (S32 i = 0; i < NUM_LODS; ++i)
	{
		llassert_always(mLODRefs[i] == 0);
	}
}

// Called from LLVolumeMgr::cleanup
bool LLVolumeLODGroup::cleanupRefs()
{
	bool res = true;
	if (mRefs != 0)
	{
		llwarns << "Volume group has remaining refs:" << getNumRefs()
				<< llendl;
		mRefs = 0;
		for (S32 i = 0; i < NUM_LODS; ++i)
		{
			if (mLODRefs[i] > 0)
			{
				llwarns << " LOD " << i << " refs = " << mLODRefs[i] << llendl;
				mLODRefs[i] = 0;
				mVolumeLODs[i] = NULL;
			}
		}
		llwarns << *getVolumeParams() << llendl;
		res = false;
	}
	return res;
}

LLVolume* LLVolumeLODGroup::refLOD(S32 detail)
{
	if (detail < 0 || detail >= NUM_LODS)
	{
		llwarns << "Attempt to reference out of range LOD " << detail
				<< " in volume group " << std::hex << this << std::dec
				<< llendl;
		llassert(false);
		return NULL;
	}

	++mAccessCount[detail];
	++mRefs;

	if (mVolumeLODs[detail].isNull())
	{
		mVolumeLODs[detail] = new LLVolume(mVolumeParams,
										   sDetailScales[detail]);
	}
	++mLODRefs[detail];

	return mVolumeLODs[detail];
}

bool LLVolumeLODGroup::derefLOD(LLVolume* volumep)
{
	if (mRefs > 0)
	{
		--mRefs;
	}
	else
	{
		llwarns << "Attempt to dereference a zero count volume: "	
				<< std::hex << volumep << std::dec << llendl;
		llassert(false);
		return false;
	}

	for (S32 i = 0; i < NUM_LODS; ++i)
	{
		if (mVolumeLODs[i] == volumep)
		{
#if 0		// SJB: Possible opt: keep other LODs around
			if (!mLODRefs[i])
			{
				mVolumeLODs[i] = NULL;
			}
			else
#endif
			if (mLODRefs[i] > 0)
			{
				--mLODRefs[i];
			}
			else
			{
				llwarns << "Unreferenced LOD (" << i << ") for volume: "
						<< std::hex << volumep << std::dec << llendl;
			}
			return true;
		}
	}

	llwarns << "Attempt to dereference a non-matching LOD in volume LOD group for volume: "
			<< std::hex << volumep << std::dec << llendl;

	return false;
}

S32 LLVolumeLODGroup::getDetailFromTan(F32 tan_angle)
{
	for (S32 i = 0; i < NUM_LODS - 1; ++i)
	{
		if (tan_angle <= sDetailThresholds[i])
		{
			return i;
		}
	}
	return NUM_LODS - 1;
}

void LLVolumeLODGroup::getDetailProximity(F32 tan_angle, F32& to_lower,
										  F32& to_higher)
{
	S32 detail = getDetailFromTan(tan_angle);

	if (detail > 0)
	{
		to_lower = tan_angle - sDetailThresholds[detail];
	}
	else
	{
		to_lower = 1024.f * 1024.f;
	}

	if (detail < NUM_LODS - 1)
	{
		to_higher = sDetailThresholds[detail + 1] - tan_angle;
	}
	else
	{
		to_higher = 1024.f * 1024.f;
	}
}

F32 LLVolumeLODGroup::getVolumeScaleFromDetail(S32 detail)
{
	return sDetailScales[detail];
}

S32 LLVolumeLODGroup::getVolumeDetailFromScale(F32 scale)
{
	for (S32 i = 1; i < 4; ++i)
	{
		if (sDetailScales[i] > scale)
		{
			return i - 1;
		}
	}

	return 3;
}

F32 LLVolumeLODGroup::dump()
{
	F32 usage = 0.f;
	for (S32 i = 0; i < NUM_LODS; ++i)
	{
		if (mAccessCount[i] > 0)
		{
			usage += 1.f;
		}
	}
	usage = usage / (F32)NUM_LODS;

	std::string dump_str = llformat("%.3f %d %d %d %d", usage,
									mAccessCount[0], mAccessCount[1],
									mAccessCount[2], mAccessCount[3]);

	llinfos << dump_str << llendl;
	return usage;
}

std::ostream& operator<<(std::ostream& s, const LLVolumeLODGroup& volgroup)
{
	s << "{ numRefs=" << volgroup.getNumRefs();
	s << ", mParams=" << volgroup.getVolumeParams();
	s << " }";

	return s;
}
