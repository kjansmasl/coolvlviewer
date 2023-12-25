/**
 * @file llsurface.h
 * @brief Description of LLSurface class
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLSURFACE_H
#define LL_LLSURFACE_H

#include <list>

#include "llvector3.h"
#include "llvector3d.h"
#include "llvector4.h"
#include "llmatrix3.h"
#include "llmatrix4.h"
#include "llquaternion.h"

#include "llcolor4u.h"
#include "llcolor4.h"

#include "llvowater.h"
#include "llpatchvertexarray.h"
#include "llviewertexture.h"

class LLTimer;
class LLUUID;
class LLStat;

constexpr U8 NO_EDGE    = 0x00;
constexpr U8 EAST_EDGE  = 0x01;
constexpr U8 NORTH_EDGE = 0x02;
constexpr U8 WEST_EDGE  = 0x04;
constexpr U8 SOUTH_EDGE = 0x08;

constexpr S32 ONE_MORE_THAN_NEIGHBOR	= 1;
constexpr S32 EQUAL_TO_NEIGHBOR 		= 0;
constexpr S32 ONE_LESS_THAN_NEIGHBOR	= -1;

// The alpha of water when the land elevation is above the waterline.
constexpr S32 ABOVE_WATERLINE_ALPHA = 32;

class LLViewerRegion;
class LLSurfacePatch;
class LLBitPack;
class LLGroupHeader;

class LLSurface
{
	friend class LLSurfacePatch;

protected:
	LOG_CLASS(LLSurface);

public:
	LLSurface(U32 type, LLViewerRegion* regionp = NULL);
	~LLSurface();

	// Allocates and initializes surface
	void create(S32 surface_grid_width, S32 surface_patch_width,
				const LLVector3d& origin_global, U32 width);

	void setRegion(LLViewerRegion* regionp);

	void setOriginGlobal(const LLVector3d& origin_global);

	void connectNeighbor(LLSurface* neighborp, U32 direction);
	void disconnectNeighbor(LLSurface* neighborp);
	void disconnectAllNeighbors();

	void decompressDCTPatch(LLBitPack& bitpack, LLGroupHeader* gopp,
							bool large_patch);
	void updatePatchVisibilities();

	LL_INLINE F32 getZ(U32 k) const						{ return mSurfaceZ[k]; }
	LL_INLINE F32 getZ(S32 i, S32 j) const				{ return mSurfaceZ[i + j * mGridsPerEdge]; }

	LLVector3 getOriginAgent() const;
	LL_INLINE const LLVector3d& getOriginGlobal() const	{ return mOriginGlobal; }

	LL_INLINE F32 getMetersPerGrid() const				{ return mMetersPerGrid; }
	LL_INLINE S32 getGridsPerEdge() const				{ return mGridsPerEdge; }
	LL_INLINE S32 getPatchesPerEdge() const				{ return mPatchesPerEdge; }
	LL_INLINE S32 getGridsPerPatchEdge() const			{ return mGridsPerPatchEdge; }

	U32 getRenderStride(U32 render_level) const;
	U32 getRenderLevel(U32 render_stride) const;

	// Returns the height of the surface immediately above (or below) location,
	// or if location is not above surface returns zero.
	F32 resolveHeightRegion(F32 x, F32 y) const;
	LL_INLINE F32 resolveHeightRegion(const LLVector3& location) const
	{
		return resolveHeightRegion(location.mV[VX], location.mV[VY]);
	}
	F32 resolveHeightGlobal(const LLVector3d& position_global) const;
	//  Returns normal to surface
	LLVector3 resolveNormalGlobal(const LLVector3d& v) const;

	LLSurfacePatch* resolvePatchRegion(F32 x, F32 y) const;
	LLSurfacePatch* resolvePatchRegion(const LLVector3& pos_region) const;
	LLSurfacePatch* resolvePatchGlobal(const LLVector3d& pos_global) const;

	// Update methods (called during idle, normally)
	void idleUpdate(F32 max_update_time);

	// Returns true if "position" is within the bounds of surface. "position"
	// is region-local.
	LL_INLINE bool containsPosition(const LLVector3& position)
	{
		return position.mV[VX] >= 0.f && position.mV[VX] <= mMetersPerEdge &&
			   position.mV[VY] >= 0.f && position.mV[VY] <= mMetersPerEdge;
	}

	void moveZ(S32 x, S32 y, F32 delta);

	LL_INLINE LLViewerRegion* getRegion() const			{ return mRegionp; }

	LL_INLINE F32 getMinZ() const						{ return mMinZ; }
	LL_INLINE F32 getMaxZ() const						{ return mMaxZ; }

	void setWaterHeight(F32 height);
	F32 getWaterHeight() const;

	LLViewerTexture* getSTexture();
	LLViewerTexture* getWaterTexture();
	LL_INLINE bool hasZData() const						{ return mHasZData; }

	// Use this to dirty all patches when changing terrain parameters
	void dirtyAllPatches();

	void dirtySurfacePatch(LLSurfacePatch* patchp);
	LL_INLINE LLVOWater* getWaterObj()					{ return mWaterObjp; }

	static void setTextureSize(U32 size);

	friend std::ostream& operator<<(std::ostream& s, const LLSurface& S);

	void getNeighboringRegions(std::vector<LLViewerRegion*>& regions);
	void getNeighboringRegionsStatus(std::vector<S32>& regions);

public:
	// Number of grid points on one side of a region, including +1 buffer for
	// north and east edge.
	S32 mGridsPerEdge;

	F32 mOOGridsPerEdge;			// Inverse of grids per edge

	S32 mPatchesPerEdge;			// Number of patches on one side of a region
	S32 mNumberOfPatches;			// Total number of patches


	// Each surface points at 8 neighbors (or NULL)
	// +---+---+---+
	// |NW | N | NE|
	// +---+---+---+
	// | W | 0 | E |
	// +---+---+---+
	// |SW | S | SE|
	// +---+---+---+
	LLSurface* mNeighbors[8];		// Adjacent patches

	U32 mType;						// Useful for identifying derived classes

	//  Number of times to repeat detail texture across this surface
	F32 mDetailTextureScale;

	static F32 sTextureUpdateTime;
	static S32 sTexelsUpdated;
	static LLStat sTexelsUpdatedPerSecStat;

protected:
	void createSTexture();
	void createWaterTexture();
	void initTextures();
	void initWater();

	void createPatchData();			// Allocates memory for patches.
	void destroyPatchData();		// Deallocates memory for patches.

	// Generate texture from composition values.
	bool generateWaterTexture(F32 x, F32 y, F32 width, F32 height);

	LLSurfacePatch* getPatch(S32 x, S32 y) const;

private:
	// Patch whose coordinate system this surface is using.
	LLViewerRegion*				mRegionp;

	// Default size of the surface texture
	static U32					sTextureSize;

protected:
	LLVector3d					mOriginGlobal;	// In absolute frame
	LLSurfacePatch*				mPatchList;		// Array of all patches

	// Array of grid data, mGridsPerEdge * mGridsPerEdge
	F32*						mSurfaceZ;

	// Array of grid normals, mGridsPerEdge * mGridsPerEdge
	LLVector3*					mNorm;

	typedef std::list<LLSurfacePatch*> patch_list_t;
	patch_list_t				mDirtyPatchList;

	// The textures should never be directly initialized !  Use the setter
	// methods !
	LLPointer<LLViewerTexture>	mSTexturep;		// Texture for surface
	LLPointer<LLViewerTexture>	mWaterTexturep;	// Water texture

	LLPointer<LLVOWater>		mWaterObjp;

	// When we want multiple cameras we'll need one of each these for each
	// camera
	S32 mVisiblePatchCount;

	// Number of grid points on a side of a patch
	U32							mGridsPerPatchEdge;
	// Converts (i, j) indexes to distance
	F32							mMetersPerGrid;
	// = mMetersPerGrid * (mGridsPerEdge - 1)
	F32							mMetersPerEdge;
	// Size for the surface texture.
	U32							mTextureSize;

	LLPatchVertexArray			mPVArray;

	// Number of frames since last update.
	S32							mSurfacePatchUpdateCount;

	// Min and max Z for this region (during the session)
	F32							mMinZ;
	F32							mMaxZ;
	// Wether or not we have received any patch data for this surface:
	bool						mHasZData;
};

//        .   __.
//     Z /|\   /| Y                                 North
//        |   /
//        |  /             |<----------------- mGridsPerSurfaceEdge --------------->|
//        | /              __________________________________________________________
//        |/______\ X     /_______________________________________________________  /
//                /      /      /      /      /      /      /      /M*M-2 /M*M-1 / /
//                      /______/______/______/______/______/______/______/______/ /
//                     /      /      /      /      /      /      /      /      / /
//                    /______/______/______/______/______/______/______/______/ /
//                   /      /      /      /      /      /      /      /      / /
//                  /______/______/______/______/______/______/______/______/ /
//      West       /      /      /      /      /      /      /      /      / /
//                /______/______/______/______/______/______/______/______/ /     East
//               /...   /      /      /      /      /      /      /      / /
//              /______/______/______/______/______/______/______/______/ /
//       _.    / 2M   /      /      /      /      /      /      /      / /
//       /|   /______/______/______/______/______/______/______/______/ /
//      /    / M    / M+1  / M+2  / ...  /      /      /      / 2M-1 / /
//     j    /______/______/______/______/______/______/______/______/ /
//         / 0    / 1    / 2    / ...  /      /      /      / M-1  / /
//        /______/______/______/______/______/______/______/______/_/
//                                South             |<-L->|
//             i -->
//
// where M = mSurfPatchWidth
// and L = mPatchGridWidth
//
// Notice that mGridsPerSurfaceEdge = a power of two + 1
// This provides a buffer on the east and north edges that will allow us to
// fill the cracks between adjacent surfaces when rendering.

#endif
