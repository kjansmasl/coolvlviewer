/**
 * @file llcloud.h
 * @brief Description of viewer cloud classes
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLCLOUD_H
#define LL_LLCLOUD_H

// Some ideas on how clouds should work
//
// Each region has a cloud layer
// Each cloud layer has pre-allocated space for N clouds
// The LLSky class knows the max number of clouds to render M.
// All clouds use the same texture, but the tex-coords can take on 8 configurations
// (four rotations, front and back)
//
// The sky's part
// --------------
// The sky knows that A clouds have been assigned to regions and there are B left over.
// Divide B by number of active regions to get C.
// Ask each region to add C more clouds and return total number D.
// Add up all the D's to get a new A.
//
// The cloud layer's part
// ----------------------
// The cloud layer is a grid of possibility.  Each grid's value represents the probablility
// (0.0 to 1.0) that a cloud placement query will succeed.
//
// The sky asks the region to add C more clouds.
// The cloud layer tries a total of E times to place clouds and returns total cloud count.
//
// Clouds move according to local wind velocity.
// If a cloud moves out of region then it's location is sent to neighbor region
// or it is allowed to drift and decay.
//
// The clouds in non-visible regions do not propagate every frame.
// Each frame one non-visible region is allowed to propagate it's clouds
// (might have to check to see if incoming cloud was already visible or not).
//
//

#include "llframetimer.h"
#include "llmath.h"
#include "llpointer.h"
#include "llvector3d.h"
#include "llvector3.h"
#include "llcolor4.h"
#include "llvector4.h"

constexpr U32 CLOUD_GRIDS_PER_EDGE = 16;

constexpr F32 CLOUD_PUFF_WIDTH	= 64.f;
constexpr F32 CLOUD_PUFF_HEIGHT	= 48.f;

class LLWind;
class LLVOClouds;
class LLViewerRegion;
class LLCloudLayer;
class LLBitPack;
class LLGroupHeader;

constexpr S32 CLOUD_GROUPS_PER_EDGE = 4;

class LLCloudPuff
{
	friend class LLCloudGroup;

public:
	LLCloudPuff();

	void updatePuffs(F32 dt);
	void updatePuffOwnership();

	LL_INLINE const LLVector3d& getPositionGlobal() const	{ return mPositionGlobal; }
	LL_INLINE F32 getAlpha() const							{ return mAlpha; }
	LL_INLINE U32 getLifeState() const						{ return mLifeState; }
	LL_INLINE void setLifeState(U32 state)					{ mLifeState = state; }
	LL_INLINE bool isDead() const							{ return mAlpha <= 0.f; }

protected:
	F32			mAlpha;
	F32			mRate;
	LLVector3d	mPositionGlobal;
	U32			mLifeState;
};

class LLCloudGroup
{
public:
	LLCloudGroup();

	void cleanup();

	LL_INLINE void setCloudLayerp(LLCloudLayer* clp)		{ mCloudLayerp = clp; }
	void setCenterRegion(F32 x, F32 y);

	void updatePuffs(F32 dt);
	void updatePuffOwnership();
	void updatePuffCount();

	bool inGroup(const LLCloudPuff& puff) const;

	LL_INLINE F32 getDensity() const						{ return mDensity; }
	LL_INLINE S32 getNumPuffs() const						{ return (S32)mCloudPuffs.size(); }
	LL_INLINE const LLCloudPuff& getPuff(S32 i)				{ return mCloudPuffs[i]; }

protected:
	LLCloudLayer*				mCloudLayerp;
	std::vector<LLCloudPuff>	mCloudPuffs;
	LLPointer<LLVOClouds>		mVOCloudsp;
	LLVector3					mCenterRegion;
	F32							mDensity;
	S32							mTargetPuffCount;
	// Last time altitude was checked
	F32							mLastAltitudeUpdate;
};

class LLCloudLayer
{
public:
	LLCloudLayer();
	~LLCloudLayer();

	void create(LLViewerRegion* regionp);
	void destroy();

	void reset();	// Clears all active cloud puffs

	void generateDensity();
	void resetDensity();

	void updatePuffs(F32 dt);
	void updatePuffOwnership();
	void updatePuffCount();

	bool shouldUpdateDensity();

	LLCloudGroup* findCloudGroup(const LLCloudPuff& puff);

	void setRegion(LLViewerRegion* regionp);
	LL_INLINE LLViewerRegion* getRegion() const				{ return mRegionp; }
	void setWindPointer(LLWind* windp);
	LL_INLINE void setOriginGlobal(const LLVector3d& orig)	{ mOriginGlobal = orig; }
	LL_INLINE F32 getMetersPerEdge() const					{ return mMetersPerEdge; }

	void setBrightness(F32 brightness);
	void setSunColor(const LLColor4& color);

	// "position" is in local coordinates
	F32 getDensityRegion(const LLVector3& pos_region);

	void decompress(LLBitPack& bitpack, LLGroupHeader* group_header);

	LL_INLINE LLCloudLayer* getNeighbor(S32 n) const		{ return mNeighbors[n]; }

	void connectNeighbor(LLCloudLayer* cloudp, U32 direction);
	void disconnectNeighbor(U32 direction);
	void disconnectAllNeighbors();

	static F32 getCloudsAltitude();
	static bool needClassicClouds();

public:
	LLVector3d 		mOriginGlobal;
	F32				mMetersPerEdge;
	F32				mMetersPerGrid;
	F32				mMaxAlpha;				// The max cloud puff render alpha

protected:
	LLCloudLayer*	mNeighbors[4];
	LLWind*			mWindp;
	LLViewerRegion*	mRegionp;
	F32*			mDensityp;				// The probability density grid
	F32				mLastDensityUpdate;		// Last time density was updated

	LLCloudGroup	mCloudGroups[CLOUD_GROUPS_PER_EDGE][CLOUD_GROUPS_PER_EDGE];

	static F32		sCloudsAltitude;
};

#endif	// LL_LLCLOUD_H
