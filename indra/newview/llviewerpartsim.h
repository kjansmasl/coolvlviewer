/**
 * @file llviewerpartsim.h
 * @brief LLViewerPart class header file
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLVIEWERPARTSIM_H
#define LL_LLVIEWERPARTSIM_H

#include "llframetimer.h"
#include "llpartdata.h"
#include "llpointer.h"

#include "llviewerpartsource.h"

class LLViewerPart;
class LLViewerRegion;
class LLViewerTexture;
class LLVOPartGroup;

typedef void (*LLVPCallback)(LLViewerPart& part, F32 dt);

///////////////////
//
// An individual particle
//

class LLViewerPart : public LLPartData
{
public:
	LLViewerPart();
	~LLViewerPart();

	void init(LLPointer<LLViewerPartSource> sourcep, LLViewerTexture* imagep,
			  LLVPCallback cb);

public:
	// Callback function for more complicated behaviors
	LLVPCallback					mVPCallback;

	// Particle source used for this object
	LLPointer<LLViewerPartSource>	mPartSourcep;

	// particle to connect to if this is part of a particle ribbon
	LLViewerPart*					mParent;
	// child particle for clean reference destruction
	LLViewerPart*					mChild;

	// Current particle state (possibly used for rendering)
	LLPointer<LLViewerTexture>		mImagep;
	LLVector3						mPosAgent;
	LLVector3						mVelocity;
	LLVector3						mAxis;
	LLVector3						mAccel;
	LLColor4						mColor;
	LLVector2						mScale;
	F32								mStartGlow;
	F32								mEndGlow;
	LLColor4U						mGlow;

	// Particle ID used primarily for moving between groups
	U32								mPartID;
	// Last time the particle was updated
	F32								mLastUpdateTime;
	// Offset against current group mSkippedTime
	F32								mSkipOffset;

	static U32						sNextPartID;
};

class LLViewerPartGroup
{
protected:
	LOG_CLASS(LLViewerPartGroup);

public:
	LLViewerPartGroup(const LLVector3& center, F32 box_radius, bool hud);
	virtual ~LLViewerPartGroup();

	void cleanup();

	bool addPart(LLViewerPart* part, F32 desired_size = -1.f);

	void updateParticles(F32 lastdt);

	bool posInGroup(const LLVector3& pos, F32 desired_size = -1.f);

	void shift(const LLVector3& offset);

	LL_INLINE F32 getBoxRadius()						{ return mBoxRadius; }
	LL_INLINE F32 getBoxSide()							{ return mBoxSide; }

	LL_INLINE const LLVector3& getCenterAgent() const	{ return mCenterAgent; }
	LL_INLINE S32 getCount() const						{ return mParticles.size(); }
	LL_INLINE LLViewerRegion* getRegion() const			{ return mRegionp; }

	void removeParticlesByID(U32 source_id);

public:
	LLPointer<LLVOPartGroup>	mVOPartGroupp;

	typedef std::vector<LLViewerPart*> part_list_t;
	part_list_t					mParticles;

	U32							mID;
	F32							mSkippedTime;
	bool						mUniformParticles;

protected:
	bool						mHud;

	F32							mBoxRadius;
	F32							mBoxSide;

	LLVector3					mCenterAgent;
	LLVector3					mMinObjPos;
	LLVector3					mMaxObjPos;

	LLViewerRegion*				mRegionp;
};

class LLViewerPartSim
{
	friend class LLViewerPartGroup;

protected:
	LOG_CLASS(LLViewerPartSim);

public:
	LLViewerPartSim() = default;

	void initClass();		// Called from LLWorld::initClass()
	void cleanupClass();	// Called from LLWorld::cleanupClass()

	typedef std::vector<LLViewerPartGroup*> group_list_t;

	void shift(const LLVector3& offset);

	void updateSimulation();

	void addPartSource(LLPointer<LLViewerPartSource> sourcep);

	void cleanupRegion(LLViewerRegion* regionp);

	LL_INLINE F32 getRefRate()							{ return sParticleAdaptiveRate; }
	LL_INLINE F32 getBurstRate()						{ return sParticleBurstRate; }

	void addPart(LLViewerPart* part);
	void updatePartBurstRate() ;
	void clearParticlesByID(U32 system_id);
	void clearParticlesByOwnerID(const LLUUID& task_id);
	void clearParticlesByRootObjectID(const LLUUID& object_id);
	void removeLastCreatedSource();

	// Note: 'max" gets clamped between 0 and 8192
	static void setMaxPartCount(S32 max);
	LL_INLINE static S32  getMaxPartCount()				{ return sMaxParticleCount; }
	LL_INLINE static void incPartCount(S32 count)		{ sParticleCount += count; }
	LL_INLINE static void decPartCount(S32 count)		{ sParticleCount -= count; }

	LL_INLINE static bool aboveParticleLimit()			{ return sParticleCount > sMaxParticleCount; }
	// Just decides whether this particle should be added or not (for particle
	// count capping):
	static bool shouldAddPart();

protected:
	LLViewerPartGroup* createViewerPartGroup(const LLVector3& pos_agent,
											 F32 desired_size, bool hud);
	LLViewerPartGroup* put(LLViewerPart* part);

#if LL_DEBUG
public:
	static void checkParticleCount(U32 size = 0);

	static S32 sParticleCount2;
#endif

protected:
	group_list_t	mViewerPartGroups;

	typedef std::vector<LLPointer<LLViewerPartSource> > source_list_t;
	source_list_t	mViewerPartSources;

	LLFrameTimer	mSimulationTimer;

	static S32		sMaxParticleCount;
	static S32		sParticleCount;
	static F32		sParticleAdaptiveRate;
	static F32		sParticleBurstRate;

	static constexpr S32 MAX_PART_COUNT = 8192;
	static constexpr F32 PART_THROTTLE_THRESHOLD = 0.9f;
	static constexpr F32 PART_THROTTLE_RESCALE =
		PART_THROTTLE_THRESHOLD / (1.f - PART_THROTTLE_THRESHOLD);
	static constexpr F32 PART_ADAPT_RATE_MULT = 2.f;
	static constexpr F32 PART_ADAPT_RATE_MULT_RECIP =
							1.f / PART_ADAPT_RATE_MULT;
};

extern LLViewerPartSim gViewerPartSim;

#endif // LL_LLVIEWERPARTSIM_H
