/**
 * @file llvolumemgr.h
 * @brief LLVolumeMgr class.
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

#ifndef LL_LLVOLUMEMGR_H
#define LL_LLVOLUMEMGR_H

#include <map>

#include "llmutex.h"
#include "llpointer.h"
#include "llthread.h"
#include "llvolume.h"

class LLVolumeParams;
class LLVolumeLODGroup;

class LLVolumeLODGroup
{
protected:
	LOG_CLASS(LLVolumeLODGroup);

public:
	enum
	{
		NUM_LODS = 4
	};

	LLVolumeLODGroup(const LLVolumeParams& params);
	~LLVolumeLODGroup();
	bool cleanupRefs();

	static S32 getDetailFromTan(F32 tan_angle);
	static void getDetailProximity(F32 tan_angle, F32& to_lower,
								   F32& to_higher);
	static F32 getVolumeScaleFromDetail(S32 detail);
	static S32 getVolumeDetailFromScale(F32 scale);

	LLVolume* refLOD(S32 detail);
	bool derefLOD(LLVolume* volumep);
	LL_INLINE S32 getNumRefs() const						{ return mRefs; }

	LL_INLINE const LLVolumeParams* getVolumeParams() const	{ return &mVolumeParams; }

	F32	dump();
	friend std::ostream& operator<<(std::ostream& s,
									const LLVolumeLODGroup& volgroup);

protected:
	LLVolumeParams		mVolumeParams;

	S32					mRefs;

	S32					mAccessCount[NUM_LODS];
	S32					mLODRefs[NUM_LODS];
	LLPointer<LLVolume>	mVolumeLODs[NUM_LODS];

	static F32			sDetailThresholds[NUM_LODS];
	static F32			sDetailScales[NUM_LODS];
};

class LLVolumeMgr
{
protected:
	LOG_CLASS(LLVolumeMgr);

public:
	static void initClass();
	static void cleanupClass();

	LLVolumeLODGroup* getGroup(const LLVolumeParams& vparams);

	// Whatever calls getVolume() never owns the LLVolume* and cannot keep
	// references for long since it may be deleted later. For best results hold
	// it in an LLPointer<LLVolume>.
	LLVolume* refVolume(const LLVolumeParams& volume_params, S32 detail);
	void unrefVolume(LLVolume* volumep);

	void dump();

	friend std::ostream& operator<<(std::ostream& s,
									const LLVolumeMgr& volume_mgr);

private:
	// Use initclass() and cleanupClass()
	LLVolumeMgr() = default;
	~LLVolumeMgr();

	void insertGroup(LLVolumeLODGroup* volgroup);

	LLVolumeLODGroup* createNewGroup(const LLVolumeParams& vparams);

private:
	typedef std::map<const LLVolumeParams*, LLVolumeLODGroup*,
					 LLVolumeParams::compare> volume_lod_group_map_t;
	volume_lod_group_map_t	mVolumeLODGroups;

	LLMutex					mDataMutex;
};

extern LLVolumeMgr* gVolumeMgrp;

#endif // LL_LLVOLUMEMGR_H
