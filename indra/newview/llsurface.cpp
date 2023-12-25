/**
 * @file llsurface.cpp
 * @brief Implementation of LLSurface class
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

#include "llviewerprecompiledheaders.h"

#include "llsurface.h"

#include "llbitpack.h"
#include "llgl.h"
#include "llnoise.h"
#include "llregionhandle.h"
#include "llrender.h"
#include "llpatch_code.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawable.h"
#include "lldrawpoolterrain.h"
#include "llpatchvertexarray.h"
#include "llpipeline.h"
#include "llsurfacepatch.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llvlcomposition.h"
#include "llvosurfacepatch.h"
#include "llvowater.h"
#include "llworld.h"

LLColor4U MAX_WATER_COLOR(0, 48, 96, 240);

U32 LLSurface::sTextureSize = 256;
S32 LLSurface::sTexelsUpdated = 0;
F32 LLSurface::sTextureUpdateTime = 0.f;
LLStat LLSurface::sTexelsUpdatedPerSecStat;

constexpr U32 MAX_TEXTURE_SIZE = 1024;
constexpr U32 MIN_TEXTURE_SIZE = 128;

//static
void LLSurface::setTextureSize(U32 size)
{
	if (size & (size - 1))
	{
		size = get_next_power_two(size, MAX_TEXTURE_SIZE);
	}
	sTextureSize = llclamp(size, MIN_TEXTURE_SIZE, MAX_TEXTURE_SIZE);
}

LLSurface::LLSurface(U32 type, LLViewerRegion* regionp)
:	mGridsPerEdge(0),
	mOOGridsPerEdge(0.f),
	mPatchesPerEdge(0),
	mNumberOfPatches(0),
	mType(type),
	mDetailTextureScale(0.f),
	mOriginGlobal(0.0, 0.0, 0.0),
	mSTexturep(NULL),
	mWaterTexturep(NULL),
	mGridsPerPatchEdge(0),
	mMetersPerGrid(1.f),
	mMetersPerEdge(1.f),
	mTextureSize(sTextureSize),
	mRegionp(regionp),
	mSurfaceZ(NULL),
	mNorm(NULL),
	mPatchList(NULL),
	mVisiblePatchCount(0),
	mHasZData(false),
	mMinZ(10000.f),
	mMaxZ(-10000.f),
	mSurfacePatchUpdateCount(0)
{
	for (S32 i = 0; i < 8; ++i)
	{
		mNeighbors[i] = NULL;
	}
}

LLSurface::~LLSurface()
{
	delete[] mSurfaceZ;
	mSurfaceZ = NULL;

	delete[] mNorm;

	mGridsPerEdge = 0;
	mGridsPerPatchEdge = 0;
	mPatchesPerEdge = 0;
	mNumberOfPatches = 0;
	destroyPatchData();

	LLDrawPoolTerrain* poolp =
		(LLDrawPoolTerrain*)gPipeline.findPool(LLDrawPool::POOL_TERRAIN,
											   mSTexturep);
	if (!poolp)
	{
		llwarns << "No pool for terrain on destruction !" << llendl;
	}
	else if (poolp->mReferences.empty())
	{
		gPipeline.removePool(poolp);
		// Do not enable this until we blitz the draw pool for it as well.
		if (mSTexturep)
		{
			mSTexturep = NULL;
		}
		if (mWaterTexturep)
		{
			mWaterTexturep = NULL;
		}
	}
	else
	{
		llwarns << "Terrain pool not empty !" << llendl;
		llassert(false);
	}
}

void LLSurface::setRegion(LLViewerRegion* regionp)
{
	mRegionp = regionp;
	mWaterObjp = NULL; // Depends on regionp, needs recreating
}

// Assumes that arguments are powers of 2, and that
// grids_per_edge / grids_per_patch_edge = power of 2
void LLSurface::create(S32 grids_per_edge, S32 grids_per_patch_edge,
					   const LLVector3d& origin_global, U32 width)
{
	// Initialize various constants for the surface
	mGridsPerEdge = grids_per_edge + 1;  // Add 1 for the east and north buffer
	mOOGridsPerEdge = 1.f / mGridsPerEdge;
	mGridsPerPatchEdge = grids_per_patch_edge;
	mPatchesPerEdge = (mGridsPerEdge - 1) / mGridsPerPatchEdge;
	mNumberOfPatches = mPatchesPerEdge * mPatchesPerEdge;
	mMetersPerGrid = F32(width) / F32(mGridsPerEdge - 1);
	mMetersPerEdge = mMetersPerGrid * (mGridsPerEdge - 1);

	// Variable region size support.
	if (width > mTextureSize)
	{
		// Clamp down to max permitted size. HB
		if (width > MAX_TEXTURE_SIZE)
		{
			width = MAX_TEXTURE_SIZE;
		}
		// Some OpenSim regions may not have a width corresponding to a power
		// of two, and the GL textures for the terrain do need a power of 2.
		else if (width & (width - 1))
		{
			mTextureSize = get_next_power_two(width, MAX_TEXTURE_SIZE);
		}
		else
		{
			mTextureSize = width;
		}
	}

	mOriginGlobal.set(origin_global);

#if 0	// Scales different than 1.f are not currently supported...
	mPVArray.create(mGridsPerEdge, mGridsPerPatchEdge,
					gWorld.getRegionScale());
#else
	mPVArray.create(mGridsPerEdge, mGridsPerPatchEdge, 1.f);
#endif

	S32 number_of_grids = mGridsPerEdge * mGridsPerEdge;

	// Initialize data arrays for surface
	mSurfaceZ = new F32[number_of_grids];
	mNorm = new LLVector3[number_of_grids];

	// Reset the surface to be a flat square grid
	for (S32 i = 0; i < number_of_grids; ++i)
	{
		// Surface is flat and zero: normals all point up
		mSurfaceZ[i] = 0.f;
		mNorm[i].set(0.f, 0.f, 1.f);
	}

	mVisiblePatchCount = 0;

	// Initialize textures
	initTextures();

	// Has to be done after texture initialization
	createPatchData();
}

LLViewerTexture* LLSurface::getSTexture()
{
	createSTexture();
	return mSTexturep;
}

LLViewerTexture* LLSurface::getWaterTexture()
{
	createWaterTexture();
	return mWaterTexturep;
}

void LLSurface::createSTexture()
{
	if (mSTexturep)
	{
		// Done already !
		return;
	}

	// Fill with dummy gray data.
	LLPointer<LLImageRaw> raw = new LLImageRaw(mTextureSize, mTextureSize, 3);
	U8* default_texture = raw->getData();
	if (!default_texture)
	{
		return;
	}

	for (U32 i = 0; i < mTextureSize; ++i)
	{
		for (U32 j = 0; j < mTextureSize; ++j)
		{
			U32 index = (i * mTextureSize + j) * 3;
			*(default_texture + index) = 128;
			*(default_texture + ++index) = 128;
			*(default_texture + ++index) = 128;
		}
	}

	mSTexturep = LLViewerTextureManager::getLocalTexture(raw.get(), false);
	mSTexturep->dontDiscard();
	gGL.getTexUnit(0)->bind(mSTexturep);
	mSTexturep->setAddressMode(LLTexUnit::TAM_CLAMP);
}

void LLSurface::createWaterTexture()
{
	if (mWaterTexturep)
	{
		// Done already !
		return;
	}

	// Create the water texture
	LLPointer<LLImageRaw> raw = new LLImageRaw(mTextureSize / 2,
											   mTextureSize / 2, 4);
	U8* default_texture = raw->getData();
	if (!default_texture)
	{
		return;
	}

	for (U32 i = 0; i < mTextureSize; i += 2)
	{
		for (U32 j = 0; j < mTextureSize; j += 2)
		{
			U32 index = i * mTextureSize + j * 2;
			*(default_texture + index) = MAX_WATER_COLOR.mV[0];
			*(default_texture + ++index) = MAX_WATER_COLOR.mV[1];
			*(default_texture + ++index) = MAX_WATER_COLOR.mV[2];
			*(default_texture + ++index) = MAX_WATER_COLOR.mV[3];
		}
	}

	mWaterTexturep = LLViewerTextureManager::getLocalTexture(raw.get(), false);
	mWaterTexturep->dontDiscard();
	gGL.getTexUnit(0)->bind(mWaterTexturep);
	mWaterTexturep->setAddressMode(LLTexUnit::TAM_CLAMP);
}

void LLSurface::initTextures()
{
	// Main surface texture
	createSTexture();

	// Water texture
	static LLCachedControl<bool> render_water(gSavedSettings, "RenderWater");
	if (render_water)
	{
		createWaterTexture();
		mWaterObjp =
			(LLVOWater*)gObjectList.createObjectViewer(LLViewerObject::LL_VO_WATER,
													   mRegionp);
		gPipeline.createObject(mWaterObjp);
		LLVector3d water_pos_glob = from_region_handle(mRegionp->getHandle());
		F64 middle = (F64)(mRegionp->getWidth() / 2);
		water_pos_glob += LLVector3d(middle, middle,
									 (F64)DEFAULT_WATER_HEIGHT);
		mWaterObjp->setPositionGlobal(water_pos_glob);
	}
}

void LLSurface::setOriginGlobal(const LLVector3d& origin_global)
{
	mOriginGlobal = origin_global;

	// Need to update the southwest corners of the patches
	F32 surface = mMetersPerGrid * mGridsPerPatchEdge;
	LLVector3d new_origin_global;
	for (S32 j = 0; j < mPatchesPerEdge; ++j)
	{
		for (S32 i = 0; i < mPatchesPerEdge; ++i)
		{
			LLSurfacePatch* patchp = getPatch(i, j);
			new_origin_global = patchp->getOriginGlobal();
			new_origin_global.mdV[0] = mOriginGlobal.mdV[0] + i * surface;
			new_origin_global.mdV[1] = mOriginGlobal.mdV[1] + j * surface;
			patchp->setOriginGlobal(new_origin_global);
		}
	}

	// *HACK !
	if (mWaterObjp.notNull() && mWaterObjp->mDrawable.notNull())
	{
		const F64 middle = (F64)(mRegionp->getWidth() / 2);
		const F64 x = origin_global.mdV[VX] + middle;
		const F64 y = origin_global.mdV[VY] + middle;
		const F64 z = mWaterObjp->getPositionGlobal().mdV[VZ];
		mWaterObjp->setPositionGlobal(LLVector3d(x, y, z));
	}
}

void LLSurface::getNeighboringRegions(std::vector<LLViewerRegion*>& regions)
{
	for (S32 i = 0; i < 8; ++i)
	{
		if (mNeighbors[i])
		{
			regions.push_back(mNeighbors[i]->getRegion());
		}
	}
}

void LLSurface::getNeighboringRegionsStatus(std::vector<S32>& regions)
{
	for (S32 i = 0; i < 8; ++i)
	{
		if (mNeighbors[i])
		{
			regions.push_back(i);
		}
	}
}

void LLSurface::connectNeighbor(LLSurface* neighborp, U32 direction)
{
	mNeighbors[direction] = neighborp;
	if (!neighborp)
	{
		llwarns << "Trying to connect a NULL neighbour in direction: "
				<< direction << llendl;
		return;
	}

	neighborp->mNeighbors[gDirOpposite[direction]] = this;

	// Variable region size support
	S32 ppe[2];
	S32 own_offset[2] = { 0, 0 };
	S32 neighbor_offset[2] = { 0, 0 };
	U32 own_xpos, own_ypos, neighbor_xpos, neighbor_ypos;
	S32 neighbor_ppe = neighborp->mPatchesPerEdge;
	// Used for x:
	ppe[0] = mPatchesPerEdge < neighbor_ppe ? mPatchesPerEdge
											: neighbor_ppe;
	// Used for y
	ppe[1] = ppe[0];

	from_region_handle(mRegionp->getHandle(), &own_xpos, &own_ypos);
	from_region_handle(neighborp->getRegion()->getHandle(),
					   &neighbor_xpos, &neighbor_ypos);

	if (own_ypos >= neighbor_ypos)
	{
		neighbor_offset[1] = (own_ypos - neighbor_ypos) / mGridsPerPatchEdge;
		ppe[1] = llmin(mPatchesPerEdge, neighbor_ppe - neighbor_offset[1]);
	}
	else
	{
		own_offset[1] = (neighbor_ypos - own_ypos) / mGridsPerPatchEdge;
		ppe[1] = llmin(mPatchesPerEdge - own_offset[1], neighbor_ppe);
	}

	if (own_xpos >= neighbor_xpos)
	{
		neighbor_offset[0] = (own_xpos - neighbor_xpos) / mGridsPerPatchEdge;
		ppe[0] = llmin(mPatchesPerEdge, neighbor_ppe - neighbor_offset[0]);
	}
	else
	{
		own_offset[0] = (neighbor_xpos - own_xpos) / mGridsPerPatchEdge;
		ppe[0] = llmin(mPatchesPerEdge - own_offset[0], neighbor_ppe);
	}

	// Connect patches
	LLSurfacePatch* patchp;
	LLSurfacePatch* neighbor_patchp;
	if (direction == NORTHEAST)
	{
		patchp = getPatch(mPatchesPerEdge - 1, mPatchesPerEdge - 1);
		neighbor_patchp = neighborp->getPatch(neighbor_offset[0],
											  neighbor_offset[1]);
		if (!patchp || !neighbor_patchp)
		{
			mNeighbors[direction] = NULL;
			return;
		}

		patchp->connectNeighbor(neighbor_patchp, direction);
		neighbor_patchp->connectNeighbor(patchp, gDirOpposite[direction]);

		patchp->updateNorthEdge(); // Only update one of north or east.
		patchp->dirtyZ();
	}
	else if (direction == NORTHWEST)
	{
		patchp = getPatch(0, mPatchesPerEdge - 1);

		S32 offset = mPatchesPerEdge + neighbor_offset[1] - own_offset[1];
		neighbor_patchp = neighborp->getPatch(neighbor_offset[0] - 1, offset);
		if (!patchp || !neighbor_patchp)
		{
			mNeighbors[direction] = NULL;
			return;
		}

		patchp->connectNeighbor(neighbor_patchp, direction);
		neighbor_patchp->connectNeighbor(patchp, gDirOpposite[direction]);
	}
	else if (direction == SOUTHWEST)
	{
		patchp = getPatch(0, 0);
		neighbor_patchp = neighborp->getPatch(neighbor_offset[0] - 1,
											  neighbor_offset[1] - 1);
		if (!patchp || !neighbor_patchp)
		{
			mNeighbors[direction] = NULL;
			return;
		}

		patchp->connectNeighbor(neighbor_patchp, direction);
		neighbor_patchp->connectNeighbor(patchp, gDirOpposite[direction]);

		neighbor_patchp->updateEastEdge(); // Only update one of north or east.
		neighbor_patchp->dirtyZ();
	}
	else if (direction == SOUTHEAST)
	{
		patchp = getPatch(mPatchesPerEdge - 1, 0);

		S32 offset = mPatchesPerEdge + neighbor_offset[0] - own_offset[0];
		neighbor_patchp = neighborp->getPatch(offset, neighbor_offset[1] - 1);
		if (!patchp || !neighbor_patchp)
		{
			mNeighbors[direction] = NULL;
			return;
		}

		patchp->connectNeighbor(neighbor_patchp, direction);
		neighbor_patchp->connectNeighbor(patchp, gDirOpposite[direction]);
	}
	else if (direction == EAST)
	{
		// Do east/west connections, first
		for (S32 i = 0; i < ppe[1]; ++i)
		{
			patchp = getPatch(mPatchesPerEdge - 1, i + own_offset[1]);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(0, i + neighbor_offset[1]);
			if (!neighbor_patchp) continue;	// Paranoia

			patchp->connectNeighbor(neighbor_patchp, direction);
			neighbor_patchp->connectNeighbor(patchp, gDirOpposite[direction]);

			patchp->updateEastEdge();
			patchp->dirtyZ();
		}

		// Now do northeast/southwest connections
		for (S32 i = 0; i < ppe[1] - 1; ++i)
		{
			patchp = getPatch(mPatchesPerEdge - 1, i + own_offset[1]);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(0,
												  i + 1 + neighbor_offset[1]);
			if (!neighbor_patchp) continue;	// Paranoia

			patchp->connectNeighbor(neighbor_patchp, NORTHEAST);
			neighbor_patchp->connectNeighbor(patchp, SOUTHWEST);
		}
		// Now do southeast/northwest connections
		for (S32 i = 1; i < ppe[1]; ++i)
		{
			patchp = getPatch(mPatchesPerEdge - 1, i + own_offset[1]);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(0,
												  i - 1 + neighbor_offset[1]);
			if (!neighbor_patchp) continue;	// Paranoia

			patchp->connectNeighbor(neighbor_patchp, SOUTHEAST);
			neighbor_patchp->connectNeighbor(patchp, NORTHWEST);
		}
	}
	else if (direction == NORTH)
	{
		// Do north/south connections, first
		for (S32 i = 0; i < ppe[0]; ++i)
		{
			patchp = getPatch(i + own_offset[0], mPatchesPerEdge - 1);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(i + neighbor_offset[0], 0);
			if (!neighbor_patchp) continue;	// Paranoia

			patchp->connectNeighbor(neighbor_patchp, direction);
			neighbor_patchp->connectNeighbor(patchp, gDirOpposite[direction]);

			patchp->updateNorthEdge();
			patchp->dirtyZ();
		}

		// Do northeast/southwest connections
		for (S32 i = 0; i < ppe[0] - 1; ++i)
		{
			patchp = getPatch(i + own_offset[0], mPatchesPerEdge - 1);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(i + 1 + neighbor_offset[0],
												  0);
			if (!neighbor_patchp) continue;

			patchp->connectNeighbor(neighbor_patchp, NORTHEAST);
			neighbor_patchp->connectNeighbor(patchp, SOUTHWEST);
		}
		// Do southeast/northwest connections
		for (S32 i = 1; i < ppe[0]; ++i)
		{
			patchp = getPatch(i + own_offset[0], mPatchesPerEdge - 1);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(i - 1 + neighbor_offset[0],
												  0);

			patchp->connectNeighbor(neighbor_patchp, NORTHWEST);
			neighbor_patchp->connectNeighbor(patchp, SOUTHEAST);
		}
	}
	else if (direction == WEST)
	{
		// Do east/west connections, first
		for (S32 i = 0; i < ppe[1]; ++i)
		{
			patchp = getPatch(0, i + own_offset[1]);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(neighbor_ppe - 1,
												  i + neighbor_offset[1]);
			if (!neighbor_patchp) continue;	// Paranoia

			patchp->connectNeighbor(neighbor_patchp, direction);
			neighbor_patchp->connectNeighbor(patchp, gDirOpposite[direction]);

			neighbor_patchp->updateEastEdge();
			neighbor_patchp->dirtyZ();
		}

		// Now do northeast/southwest connections
		for (S32 i = 1; i < ppe[1]; ++i)
		{
			patchp = getPatch(0, i + own_offset[1]);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(neighbor_ppe - 1,
												  i - 1 + neighbor_offset[1]);
			if (!neighbor_patchp) continue;	// Paranoia

			patchp->connectNeighbor(neighbor_patchp, SOUTHWEST);
			neighbor_patchp->connectNeighbor(patchp, NORTHEAST);
		}

		// Now do northwest/southeast connections
		for (S32 i = 0; i < ppe[1] - 1; ++i)
		{
			patchp = getPatch(0, i + own_offset[1]);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(neighbor_ppe - 1,
												  i + 1 + neighbor_offset[1]);
			if (!neighbor_patchp) continue;	// Paranoia

			patchp->connectNeighbor(neighbor_patchp, NORTHWEST);
			neighbor_patchp->connectNeighbor(patchp, SOUTHEAST);
		}
	}
	else if (direction == SOUTH)
	{
		// Do north/south connections, first
		for (S32 i = 0; i < ppe[0]; ++i)
		{
			patchp = getPatch(i + own_offset[0], 0);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(i + neighbor_offset[0],
												  neighbor_ppe - 1);
			if (!neighbor_patchp) continue;	// Paranoia

			patchp->connectNeighbor(neighbor_patchp, direction);
			neighbor_patchp->connectNeighbor(patchp, gDirOpposite[direction]);

			neighbor_patchp->updateNorthEdge();
			neighbor_patchp->dirtyZ();
		}

		// Now do northeast/southwest connections
		for (S32 i = 1; i < ppe[0]; ++i)
		{
			patchp = getPatch(i + own_offset[0], 0);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(i - 1 + neighbor_offset[0],
												  neighbor_ppe - 1);
			if (!neighbor_patchp) continue;	// Paranoia

			patchp->connectNeighbor(neighbor_patchp, SOUTHWEST);
			neighbor_patchp->connectNeighbor(patchp, NORTHEAST);
		}
		// Now do northeast/southwest connections
		for (S32 i = 0; i < ppe[0] - 1; ++i)
		{
			patchp = getPatch(i + own_offset[0], 0);
			if (!patchp) continue;	// Paranoia

			neighbor_patchp = neighborp->getPatch(i + 1 + neighbor_offset[0],
												  neighbor_ppe - 1);
			if (!neighbor_patchp) continue;	// Paranoia

			patchp->connectNeighbor(neighbor_patchp, SOUTHEAST);
			neighbor_patchp->connectNeighbor(patchp, NORTHWEST);
		}
	}
}

void LLSurface::disconnectNeighbor(LLSurface* surfacep)
{
	for (S32 i = 0; i < 8; ++i)
	{
		if (surfacep == mNeighbors[i])
		{
			mNeighbors[i] = NULL;
		}
	}

	// Iterate through surface patches, removing any connectivity to removed
	// surface.
	for (S32 i = 0; i < mNumberOfPatches; ++i)
	{
		LLSurfacePatch* patchp = mPatchList + i;
		if (patchp)	// Paranoia
		{
			patchp->disconnectNeighbor(surfacep);
		}
	}
}

void LLSurface::disconnectAllNeighbors()
{
	for (S32 i = 0; i < 8; ++i)
	{
		LLSurface* neighborp = mNeighbors[i];
		if (neighborp)
		{
			neighborp->disconnectNeighbor(this);
			mNeighbors[i] = NULL;
		}
	}
}

LLVector3 LLSurface::getOriginAgent() const
{
	return gAgent.getPosAgentFromGlobal(mOriginGlobal);
}

void LLSurface::moveZ(S32 x, S32 y, F32 delta)
{
	llassert(x >= 0 && y >= 0 && x < mGridsPerEdge && y < mGridsPerEdge);
	mSurfaceZ[x + y * mGridsPerEdge] += delta;
}

void LLSurface::updatePatchVisibilities()
{
	if (gShiftFrame || !mRegionp)
	{
		return;
	}

	LLVector3 pos_region =
		mRegionp->getPosRegionFromGlobal(gAgent.getCameraPositionGlobal());

	mVisiblePatchCount = 0;
	for (S32 i = 0; i < mNumberOfPatches; ++i)
	{
		LLSurfacePatch* patchp = mPatchList + i;
		patchp->updateVisibility();
		if (patchp->getVisible())
		{
			++mVisiblePatchCount;
			patchp->updateCameraDistanceRegion(pos_region);
		}
	}
}

void LLSurface::idleUpdate(F32 max_update_time)
{
	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_TERRAIN))
	{
		return;
	}

	// Perform idle time update of non-critical stuff; in this case, texture
	// and normal updates.
	LLTimer update_timer;

	// If the Z height data has changed, we need to rebuild our property line
	// vertex arrays.
	if (!mDirtyPatchList.empty())
	{
		mRegionp->dirtyHeights();
	}

	bool timed_out = false;
	bool did_update = false;
	for (patch_list_t::iterator iter = mDirtyPatchList.begin(),
								end = mDirtyPatchList.end();
		 iter != end; )
	{
		patch_list_t::iterator curiter = iter++;
		LLSurfacePatch* patchp = *curiter;	// Cannot be NULL

		// Always call updateNormals() / updateVerticalStats() every frame to
		// avoid artifacts
		patchp->updateNormals();
		patchp->updateVerticalStats();

		// Note: the first patch in the list will always see its texture
		// updated: this ensures a slow trickle even in the case we would
		// already have timed out... HB
		if (!timed_out)
		{
			if (patchp->updateTexture())
			{
				patchp->clearDirty();
				mDirtyPatchList.erase(curiter);
				did_update = true;
			}
			timed_out = update_timer.getElapsedTimeF32() >= max_update_time;
		}
	}
	if (did_update)
	{
		// Some patches changed, update region reflection probes.
		mRegionp->updateReflectionProbes();
	}

	// *HACK: force-reload all the surface patches when at least one is failing
	// to load for too long. HB
	if (LLSurfacePatch::needsPatchesReload())
	{
		gWorld.reloadAllSurfacePatches();
	}
}

void LLSurface::decompressDCTPatch(LLBitPack& bitpack, LLGroupHeader* gopp,
								   bool large_patch)
{
	LLPatchHeader ph;
	S32 patch[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];

	init_patch_decompressor(gopp->patch_size);
	gopp->stride = mGridsPerEdge;
	set_group_of_patch_header(gopp);

	while (true)
	{
		// Variable region size support via large_patch
		decode_patch_header(bitpack, &ph, large_patch);
		if (ph.quant_wbits == END_OF_PATCHES)
		{
			break;
		}

		// Variable region size support
		S32 j, i;
		if (large_patch)
		{
			i = ph.patchids >> 16;		// x
			j = ph.patchids & 0xFFFF;	// y
		}
		else
		{
			i = ph.patchids >> 5;		// x
			j = ph.patchids & 0x1F;		// y
		}

		if (i >= mPatchesPerEdge || j >= mPatchesPerEdge)
		{
			llwarns << "Received invalid terrain packet: patch header incorrect !  Patches per edge = "
					<< mPatchesPerEdge << " - i = " << i << " - j = " << j
					<< " - dc_offset = " << ph.dc_offset << " - range = "
					<< (S32)ph.range << " - quant_wbits = "
					<< (S32)ph.quant_wbits << " patchids = "
					<< (S32)ph.patchids << llendl;
#if 0		// Do not disconnect any more: just ignore the bogus packet.
            gAppViewerp->badNetworkHandler();
#endif
			return;
		}

		LLSurfacePatch* patchp = &mPatchList[j * mPatchesPerEdge + i];
		if (!patchp) break;	// Paranoia

		decode_patch(bitpack, patch);
		decompress_patch(patchp->getDataZ(), patch, &ph);

		// Update edges for neighbors. We need to guarantee that this gets done
		// before we generate vertical stats.
		patchp->updateNorthEdge();
		patchp->updateEastEdge();
		if (patchp->getNeighborPatch(WEST))
		{
			patchp->getNeighborPatch(WEST)->updateEastEdge();
		}
		if (patchp->getNeighborPatch(SOUTHWEST))
		{
			patchp->getNeighborPatch(SOUTHWEST)->updateEastEdge();
			patchp->getNeighborPatch(SOUTHWEST)->updateNorthEdge();
		}
		if (patchp->getNeighborPatch(SOUTH))
		{
			patchp->getNeighborPatch(SOUTH)->updateNorthEdge();
		}

		// Dirty patch statistics, and flag that the patch has data.
		patchp->dirtyZ();
		patchp->setHasReceivedData();
	}
}

F32 LLSurface::resolveHeightRegion(F32 x, F32 y) const
{
	F32 height = 0.f;
	F32 oometerspergrid = 1.f / mMetersPerGrid;

	// Check to see if v is actually above surface
	// We use (mGridsPerEdge-1) below rather than (mGridsPerEdge)
	// because of the east and north buffers

	if (x >= 0.f && x <= mMetersPerEdge && y >= 0.f && y <= mMetersPerEdge)
	{
		const S32 left   = llfloor(x * oometerspergrid);
		const S32 bottom = llfloor(y * oometerspergrid);

		// Do not walk off the edge of the array !
		const S32 right	= left + 1 < (S32)mGridsPerEdge - 1 ? left + 1 : left;
		const S32 top = bottom + 1 < (S32)mGridsPerEdge - 1 ? bottom + 1
															: bottom;

		// Figure out if v is in first or second triangle of the square
		// and calculate the slopes accordingly
		//    |       |
		// -(i,j+1)---(i+1,j+1)--
		//    |  1   /  |          ^
		//    |    /  2 |          |
		//    |  /      |          j
		// --(i,j)----(i+1,j)--
		//    |       |
		//
		//      i ->
		// where N = mGridsPerEdge

		const F32 left_bottom = getZ(left, bottom);
		const F32 right_bottom = getZ(right, bottom);
		const F32 left_top = getZ(left, top);
		const F32 right_top = getZ(right, top);

		// dx and dy are incremental steps from (mSurface + k)
		F32 dx = x - left * mMetersPerGrid;
		F32 dy = y - bottom * mMetersPerGrid;

		if (dy > dx)
		{
			// Triangle 1
			dy *= left_top  - left_bottom;
			dx *= right_top - left_top;
		}
		else
		{
			// Triangle 2
			dx *= right_bottom - left_bottom;
			dy *= right_top - right_bottom;
		}
		height = left_bottom + (dx + dy) * oometerspergrid;
	}

	return height;
}

F32 LLSurface::resolveHeightGlobal(const LLVector3d& v) const
{
	return mRegionp ? resolveHeightRegion(mRegionp->getPosRegionFromGlobal(v))
					: 0.f;
}

LLVector3 LLSurface::resolveNormalGlobal(const LLVector3d& pos_global) const
{
	if (!mSurfaceZ)
	{
		// Hmm. Uninitialized surface !
		return LLVector3::z_axis;
	}
	//
	// Returns the vector normal to a surface at location specified by vector v
	//
	LLVector3 normal;

	if (pos_global.mdV[VX] >= mOriginGlobal.mdV[VX] &&
		pos_global.mdV[VX] < mOriginGlobal.mdV[VX] + mMetersPerEdge &&
		pos_global.mdV[VY] >= mOriginGlobal.mdV[VY] &&
		pos_global.mdV[VY] < mOriginGlobal.mdV[VY] + mMetersPerEdge)
	{
		F32 oometerspergrid = 1.f / mMetersPerGrid;
		U32 i = (U32)((pos_global.mdV[VX] - mOriginGlobal.mdV[VX]) *
					  oometerspergrid);
		U32 j = (U32)((pos_global.mdV[VY] - mOriginGlobal.mdV[VY]) *
					  oometerspergrid);
		U32 k = i + j * mGridsPerEdge;

		// Figure out if v is in first or second triangle of the square and
		// calculate the slopes accordingly
		//    |       |
		// -(k+N)---(k+1+N)--
		//    |  1 /  |          ^
		//    |   / 2 |          |
		//    |  /    |          j
		// --(k)----(k+1)--
		//    |       |
		//
		//      i ->
		// where N = mGridsPerEdge

		// dx and dy are incremental steps from (mSurface + k)
		F32 dx = (F32)(pos_global.mdV[VX] - i * mMetersPerGrid -
					   mOriginGlobal.mdV[VX]);
		F32 dy = (F32)(pos_global.mdV[VY] - j * mMetersPerGrid -
					   mOriginGlobal.mdV[VY]);
		if (dy > dx)
		{
			// Triangle 1
			F32 dzx = *(mSurfaceZ + k + 1 + mGridsPerEdge) -
					  *(mSurfaceZ + k + mGridsPerEdge);
			F32 dzy = *(mSurfaceZ + k) - *(mSurfaceZ + k + mGridsPerEdge);
			normal.set(-dzx, dzy, 1.f);
		}
		else
		{
			// Triangle 2
			F32 dzx = *(mSurfaceZ + k) - *(mSurfaceZ + k + 1);
			F32 dzy = *(mSurfaceZ + k + 1 + mGridsPerEdge) -
					  *(mSurfaceZ + k + 1);
			normal.set(dzx, -dzy, 1.f);
		}
	}

	normal.normalize();
	return normal;
}

// x and y should be region-local coordinates.
// If x and y are outside of the surface, then the returned
// index will be for the nearest boundary patch.
//
// 12      | 13| 14|       15
//         |   |   |
//     +---+---+---+---+
//     | 12| 13| 14| 15|
// ----+---+---+---+---+-----
// 8   | 8 | 9 | 10| 11|   11
// ----+---+---+---+---+-----
// 4   | 4 | 5 | 6 | 7 |    7
// ----+---+---+---+---+-----
//     | 0 | 1 | 2 | 3 |
//     +---+---+---+---+
//         |   |   |
// 0       | 1 | 2 |        3
//
LLSurfacePatch* LLSurface::resolvePatchRegion(F32 x, F32 y) const
{

	// When x and y are not region-local do the following first
	S32 i, j;
	if (x < 0.f)
	{
		i = 0;
	}
	else if (x >= mMetersPerEdge)
	{
		i = mPatchesPerEdge - 1;
	}
	else
	{
		i = (U32)(x / (mMetersPerGrid * mGridsPerPatchEdge));
	}

	if (y < 0.f)
	{
		j = 0;
	}
	else if (y >= mMetersPerEdge)
	{
		j = mPatchesPerEdge - 1;
	}
	else
	{
		j = (U32)(y / (mMetersPerGrid * mGridsPerPatchEdge));
	}

	// *NOTE: Super paranoia code follows.
	S32 index = i + j * mPatchesPerEdge;
	if (index < 0 || index >= mNumberOfPatches)
	{
		if (!mNumberOfPatches)
		{
			llwarns << "No patches for current region !" << llendl;
			return NULL;
		}
		S32 old_index = index;
		index = llclamp(old_index, 0, (mNumberOfPatches - 1));
		llwarns << "Clamping out of range patch index " << old_index
				<< " to " << index << llendl;
	}

	return &(mPatchList[index]);
}

LLSurfacePatch* LLSurface::resolvePatchRegion(const LLVector3& pos_region) const
{
	return resolvePatchRegion(pos_region.mV[VX], pos_region.mV[VY]);
}

LLSurfacePatch* LLSurface::resolvePatchGlobal(const LLVector3d& pos_global) const
{
	if (mRegionp)
	{
		LLVector3 pos_region = mRegionp->getPosRegionFromGlobal(pos_global);
		return resolvePatchRegion(pos_region);
	}
	return NULL;
}

std::ostream& operator<<(std::ostream& s, const LLSurface& S)
{
	s << "{ \n";
	s << "  mGridsPerEdge = " << S.mGridsPerEdge - 1 << " + 1\n";
	s << "  mGridsPerPatchEdge = " << S.mGridsPerPatchEdge << "\n";
	s << "  mPatchesPerEdge = " << S.mPatchesPerEdge << "\n";
	s << "  mOriginGlobal = " << S.mOriginGlobal << "\n";
	s << "  mMetersPerGrid = " << S.mMetersPerGrid << "\n";
	s << "  mVisiblePatchCount = " << S.mVisiblePatchCount << "\n";
	s << "}";
	return s;
}

void LLSurface::createPatchData()
{
	// Paranoia since createPatchData() is called only from create(). HB
	if (!mNumberOfPatches)
	{
		llassert(false);
		return;
	}

	// Allocate memory
	mPatchList = new LLSurfacePatch[mNumberOfPatches];

	// One of each for each camera
	mVisiblePatchCount = mNumberOfPatches;

	for (S32 j = 0; j < mPatchesPerEdge; ++j)
	{
		for (S32 i = 0; i < mPatchesPerEdge; ++i)
		{
			LLSurfacePatch* patchp = getPatch(i, j);
			if (patchp)	// paranoia
			{
				patchp->setSurface(this);
			}
		}
	}

	for (S32 j = 0; j < mPatchesPerEdge; ++j)
	{
		for (S32 i = 0; i < mPatchesPerEdge; ++i)
		{
			LLSurfacePatch* patchp = getPatch(i, j);
			if (!patchp) continue;	// Paranoia

			patchp->mHasReceivedData = false;
			patchp->mSTexUpdate = true;

			S32 data_offset = i * mGridsPerPatchEdge +
							  j * mGridsPerPatchEdge * mGridsPerEdge;

			patchp->setDataZ(mSurfaceZ + data_offset);
			patchp->setDataNorm(mNorm + data_offset);

			// We make each patch point to its neighbors so we can do
			// resolution checking  when butting up different resolutions.
			// Patches that do not have neighbors somewhere will point to NULL
			// on that side.
			if (i < mPatchesPerEdge - 1)
			{
				patchp->setNeighborPatch(EAST, getPatch(i + 1, j));
			}
			else
			{
				patchp->setNeighborPatch(EAST, NULL);
			}

			if (j < mPatchesPerEdge - 1)
			{
				patchp->setNeighborPatch(NORTH, getPatch(i, j + 1));
			}
			else
			{
				patchp->setNeighborPatch(NORTH, NULL);
			}

			if (i > 0)
			{
				patchp->setNeighborPatch(WEST, getPatch(i - 1, j));
			}
			else
			{
				patchp->setNeighborPatch(WEST, NULL);
			}

			if (j > 0)
			{
				patchp->setNeighborPatch(SOUTH, getPatch(i, j - 1));
			}
			else
			{
				patchp->setNeighborPatch(SOUTH, NULL);
			}

			if (i < mPatchesPerEdge - 1 && j < mPatchesPerEdge - 1)
			{
				patchp->setNeighborPatch(NORTHEAST, getPatch(i + 1, j + 1));
			}
			else
			{
				patchp->setNeighborPatch(NORTHEAST, NULL);
			}

			if (i > 0 && j < mPatchesPerEdge - 1)
			{
				patchp->setNeighborPatch(NORTHWEST, getPatch(i - 1, j + 1));
			}
			else
			{
				patchp->setNeighborPatch(NORTHWEST, NULL);
			}

			if (i > 0  &&  j > 0)
			{
				patchp->setNeighborPatch(SOUTHWEST, getPatch(i - 1, j - 1));
			}
			else
			{
				patchp->setNeighborPatch(SOUTHWEST, NULL);
			}

			if (i < mPatchesPerEdge - 1 && j > 0)
			{
				patchp->setNeighborPatch(SOUTHEAST, getPatch(i + 1, j - 1));
			}
			else
			{
				patchp->setNeighborPatch(SOUTHEAST, NULL);
			}

			LLVector3d origin_global;
			origin_global.mdV[0] = mOriginGlobal.mdV[0] + i * mMetersPerGrid *
								   mGridsPerPatchEdge;
			origin_global.mdV[1] = mOriginGlobal.mdV[0] + j * mMetersPerGrid *
								   mGridsPerPatchEdge;
			origin_global.mdV[2] = 0.f;
			patchp->setOriginGlobal(origin_global);
		}
	}
}

void LLSurface::destroyPatchData()
{
	// Delete all of the cached patch data for these patches.
	delete[] mPatchList;
	mPatchList = NULL;
	mVisiblePatchCount = 0;
}

U32 LLSurface::getRenderLevel(U32 render_stride) const
{
	return mPVArray.mRenderLevelp[render_stride];
}

U32 LLSurface::getRenderStride(U32 render_level) const
{
	return mPVArray.mRenderStridep[render_level];
}

LLSurfacePatch* LLSurface::getPatch(S32 x, S32 y) const
{
	if (x < 0 || y < 0 || x >= mPatchesPerEdge || y >= mPatchesPerEdge)
	{
		llwarns << "Asking for patch out of bounds: x = " << x << " - y = "
				<< y << " - Number of patches per edge: " << mPatchesPerEdge
			   << llendl;
		return NULL;
	}

	return mPatchList + x + y * mPatchesPerEdge;
}

void LLSurface::dirtyAllPatches()
{
	for (S32 i = 0; i < mNumberOfPatches; ++i)
	{
		mPatchList[i].dirtyZ();
	}
}

void LLSurface::dirtySurfacePatch(LLSurfacePatch* patchp)
{
	// Put surface patch at the end of the dirty surface patch list.
	// Note: patchp cannot be NULL, because dirtySurfacePatch() is only
	// ever called by LLSurfacePatch with 'this' for patchp. In case this
	// would change, we would need to avoid pushing a NULL patchp. HB
	mDirtyPatchList.push_back(patchp);
}

void LLSurface::setWaterHeight(F32 height)
{
	if (mWaterObjp.notNull())
	{
		LLVector3 water_pos_region = mWaterObjp->getPositionRegion();
		bool changed = water_pos_region.mV[VZ] != height;
		water_pos_region.mV[VZ] = height;
		mWaterObjp->setPositionRegion(water_pos_region);
		if (changed)
		{
			gWorld.updateWaterObjects();
		}
	}
	else
	{
		llwarns << "No water object !" << llendl;
	}
}

F32 LLSurface::getWaterHeight() const
{
	return mWaterObjp.notNull() ? mWaterObjp->getPositionRegion().mV[VZ]
								: DEFAULT_WATER_HEIGHT;
}

bool LLSurface::generateWaterTexture(F32 x, F32 y, F32 width, F32 height)
{
	if (!getWaterTexture())
	{
		return false;
	}

	S32 tex_width = mWaterTexturep->getWidth();
	S32 tex_height = mWaterTexturep->getHeight();
	S32 tex_comps = mWaterTexturep->getComponents();
	S32 tex_stride = tex_width * tex_comps;
	LLPointer<LLImageRaw> raw = new LLImageRaw(tex_width, tex_height,
											   tex_comps);
	U8* rawp = raw->getData();

	F32 scale = mRegionp->getWidth() * getMetersPerGrid() / (F32)tex_width;
	F32 scale_inv = 1.f / scale;

	S32 x_begin, y_begin, x_end, y_end;

	x_begin = ll_round(x * scale_inv);
	y_begin = ll_round(y * scale_inv);
	x_end = ll_round((x + width) * scale_inv);
	y_end = ll_round((y + width) * scale_inv);

	if (x_end > tex_width)
	{
		x_end = tex_width;
	}
	if (y_end > tex_width)
	{
		y_end = tex_width;
	}

	// OK, for now, just have the composition value equal the height at the
	// point.
	LLVector3 location;
	LLColor4U coloru;
	const F32 water_height = getWaterHeight();
	for (S32 j = y_begin; j < y_end; ++j)
	{
		for (S32 i = x_begin; i < x_end; ++i)
		{
			S32 offset = j * tex_stride + i * tex_comps;
			location.mV[VX] = i * scale;
			location.mV[VY] = j * scale;

			// Sample multiple points
			const F32 height = resolveHeightRegion(location);

			if (height > water_height)
			{
				// Above water...
				coloru = MAX_WATER_COLOR;
				coloru.mV[3] = ABOVE_WATERLINE_ALPHA;
				*(rawp + offset) = coloru.mV[0];
				*(rawp + ++offset) = coloru.mV[1];
				*(rawp + ++offset) = coloru.mV[2];
				*(rawp + ++offset) = coloru.mV[3];
			}
			else
			{
				// Want non-linear curve for transparency gradient
				coloru = MAX_WATER_COLOR;
				const F32 frac = 1.f - 2.f / (2.f - height + water_height);
				S32 alpha = 64 + ll_round((255 - 64) * frac);

				alpha = llmin(ll_round((F32)MAX_WATER_COLOR.mV[3]), alpha);
				alpha = llmax(64, alpha);

				coloru.mV[3] = alpha;
				*(rawp + offset) = coloru.mV[0];
				*(rawp + ++offset) = coloru.mV[1];
				*(rawp + ++offset) = coloru.mV[2];
				*(rawp + ++offset) = coloru.mV[3];
			}
		}
	}

	if (!mWaterTexturep->hasGLTexture())
	{
		mWaterTexturep->createGLTexture(0, raw);
	}

	mWaterTexturep->setSubImage(raw, x_begin, y_begin, x_end - x_begin,
								y_end - y_begin);
	return true;
}
