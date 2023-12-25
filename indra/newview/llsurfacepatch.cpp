/**
 * @file llsurfacepatch.cpp
 * @brief LLSurfacePatch class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llsurfacepatch.h"

#include "llnoise.h"

#include "llappviewer.h"			// For gFrameTime*
#include "lldrawpool.h"
#include "llpatchvertexarray.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llsurface.h"
#include "llviewercamera.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvlcomposition.h"

const U32 gDirAdjacent[8][2] =
{
	{ 4, 7 },
	{ 4, 5 },
	{ 5, 6 },
	{ 6, 7 },
	{ 0, 1 },
	{ 1, 2 },
	{ 2, 3 },
	{ 0, 3 }
};
const U32 gDirOpposite[8] = { 2, 3, 0, 1, 6, 7, 4, 5 };

F32 LLSurfacePatch::sNextAllowedReloadTime = 0.f;
F32 LLSurfacePatch::sAutoReloadDelay = 0.f;
bool LLSurfacePatch::sNeedsPatchesReload = false;

LLSurfacePatch::LLSurfacePatch()
:	mHasReceivedData(false),
	mSTexUpdate(false),
	mDirty(false),
	mDirtyZStats(true),
	mHeightsGenerated(false),
	mDataOffset(0),
	mDataZ(NULL),
	mDataNorm(NULL),
	mVObjp(NULL),
	mOriginRegion(0.f, 0.f, 0.f),
	mCenterRegion(0.f, 0.f, 0.f),
	mMinZ(0.f),
	mMaxZ(0.f),
	mMeanZ(0.f),
	mRadius(0.f),
	mMinComposition(0.f),
	mMaxComposition(0.f),
	mMeanComposition(0.f),
	// This flag is used to communicate between adjacent surfaces and is set to
	// non-zero values by higher classes.
	mConnectedEdge(NO_EDGE),
	mLastUpdateTime(0),
	mFirstFailureTime(0.f),
	mSurfacep(NULL)
{
	for (S32 i = 0; i < 8; ++i)
	{
		setNeighborPatch(i, NULL);
	}
	for (S32 i = 0; i < 9; ++i)
	{
		mNormalsInvalid[i] = true;
	}
}

LLSurfacePatch::~LLSurfacePatch()
{
	mVObjp = NULL;
}

void LLSurfacePatch::dirty()
{
	// These are outside of the loop in case we are still waiting for a dirty
	// from the texture being updated...
	if (mVObjp.notNull())
	{
		mVObjp->dirtyGeom();
	}
	else
	{
		llwarns << "No viewer object for this surface patch !" << llendl;
	}

	mDirtyZStats = true;
	mHeightsGenerated = false;

	if (!mDirty)
	{
		mDirty = true;
		mSurfacep->dirtySurfacePatch(this);
	}
}

void LLSurfacePatch::setSurface(LLSurface* surfacep)
{
	mSurfacep = surfacep;

	LLViewerRegion* regionp = mSurfacep->getRegion();
	if (!regionp)	// Paranoia
	{
		return;
	}
	// Surface patch object already created.
	if (mVObjp.notNull())
	{
		return;
	}

	llassert(mSurfacep->mType == 'l');

	mVObjp =
		(LLVOSurfacePatch*)gObjectList.createObjectViewer(LLViewerObject::LL_VO_SURFACE_PATCH,
														  regionp);
	mVObjp->setPatch(this);
	mVObjp->setPositionRegion(mCenterRegion);
	gPipeline.createObject(mVObjp);
}

void LLSurfacePatch::disconnectNeighbor(LLSurface* surfacep)
{
	LLSurfacePatch* neighbor;

	for (U32 i = 0; i < 8; ++i)
	{
		neighbor = getNeighborPatch(i);
		if (neighbor)
		{
			if (neighbor->mSurfacep == surfacep)
			{
				setNeighborPatch(i, NULL);
				mNormalsInvalid[i] = true;
			}
		}
	}

	// Clean up connected edges
	neighbor = getNeighborPatch(EAST);
	if (neighbor)
	{
		if (neighbor->mSurfacep == surfacep)
		{
			mConnectedEdge &= ~EAST_EDGE;
		}
	}
	neighbor = getNeighborPatch(NORTH);
	if (neighbor)
	{
		if (neighbor->mSurfacep == surfacep)
		{
			mConnectedEdge &= ~NORTH_EDGE;
		}
	}
	neighbor = getNeighborPatch(WEST);
	if (neighbor)
	{
		if (neighbor->mSurfacep == surfacep)
		{
			mConnectedEdge &= ~WEST_EDGE;
		}
	}
	neighbor = getNeighborPatch(SOUTH);
	if (neighbor)
	{
		if (neighbor->mSurfacep == surfacep)
		{
			mConnectedEdge &= ~SOUTH_EDGE;
		}
	}
}

LLVector3 LLSurfacePatch::getPointAgent(U32 x, U32 y) const
{
	LLVector3 pos;
	if (mSurfacep)
	{
		U32 surface_stride = mSurfacep->getGridsPerEdge();
		U32 point_offset = x + y * surface_stride;
		pos = getOriginAgent();
		pos.mV[VX] += x	* mSurfacep->getMetersPerGrid();
		pos.mV[VY] += y * mSurfacep->getMetersPerGrid();
		pos.mV[VZ] = *(mDataZ + point_offset);
	}
	return pos;
}

LLVector2 LLSurfacePatch::getTexCoords(U32 x, U32 y) const
{
	LLVector3 rel_pos;
	if (mSurfacep)
	{
		U32 surface_stride = mSurfacep->getGridsPerEdge();
		U32 point_offset = x + y * surface_stride;
		LLVector3 pos = getOriginAgent();
		pos.mV[VX] += x	* mSurfacep->getMetersPerGrid();
		pos.mV[VY] += y * mSurfacep->getMetersPerGrid();
		pos.mV[VZ] = *(mDataZ + point_offset);
		rel_pos = pos - mSurfacep->getOriginAgent();
		rel_pos *= 1.f / surface_stride;
	}
	return LLVector2(rel_pos.mV[VX], rel_pos.mV[VY]);
}

void LLSurfacePatch::eval(U32 x, U32 y, U32 stride, LLVector3* vertex,
						  LLVector3* normal, LLVector2* tex0, LLVector2* tex1)
{
	if (!mSurfacep || !mSurfacep->getGridsPerEdge() || !mVObjp)
	{
		return;
	}
	LLViewerRegion* regionp = mSurfacep->getRegion();
	if (!regionp)
	{
		return;
	}

	llassert_always(vertex && normal && tex0 && tex1);

	U32 surface_stride = mSurfacep->getGridsPerEdge();
	U32 point_offset = x + y * surface_stride;

	*normal = getNormal(x, y);

	LLVector3 pos_agent = getOriginAgent();
	pos_agent.mV[VX] += x * mSurfacep->getMetersPerGrid();
	pos_agent.mV[VY] += y * mSurfacep->getMetersPerGrid();
	pos_agent.mV[VZ] = *(mDataZ + point_offset);
	*vertex = pos_agent - mVObjp->getRegion()->getOriginAgent();

	LLVector3 rel_pos = pos_agent - mSurfacep->getOriginAgent();
	LLVector3 tex_pos = rel_pos * (1.f / surface_stride);
	tex0->mV[0] = tex_pos.mV[0];
	tex0->mV[1] = tex_pos.mV[1];
	tex1->mV[0] = regionp->getCompositionXY(llfloor(mOriginRegion.mV[0]) + x,
											llfloor(mOriginRegion.mV[1]) + y);

	constexpr F32 XYSCALEINV = 0.2222222222f / (4.9215f * 7.f);
	F32 vec[3] =
	{
		(F32)fmod((F32)(mOriginGlobal.mdV[0] + x) * XYSCALEINV, 256.f),
		(F32)fmod((F32)(mOriginGlobal.mdV[1] + y) * XYSCALEINV, 256.f),
		0.f
	};
	F32 rand_val = llclamp(noise2(vec)* 0.75f + 0.5f, 0.f, 1.f);
	tex1->mV[1] = rand_val;
}

void LLSurfacePatch::calcNormal(U32 x, U32 y, U32 stride)
{
	if (!mSurfacep)
	{
		return;
	}

	U32 patch_width = mSurfacep->mPVArray.mPatchWidth;
	U32 surface_stride = mSurfacep->getGridsPerEdge();

	const F32 mpg = mSurfacep->getMetersPerGrid() * stride;

	S32 poffsets[2][2][2];
	poffsets[0][0][0] = x - stride;
	poffsets[0][0][1] = y - stride;

	poffsets[0][1][0] = x - stride;
	poffsets[0][1][1] = y + stride;

	poffsets[1][0][0] = x + stride;
	poffsets[1][0][1] = y - stride;

	poffsets[1][1][0] = x + stride;
	poffsets[1][1][1] = y + stride;

	const LLSurfacePatch* ppatches[2][2];

	// LLVector3 p1, p2, p3, p4;

	ppatches[0][0] = this;
	ppatches[0][1] = this;
	ppatches[1][0] = this;
	ppatches[1][1] = this;

	U32 i, j;
	for (i = 0; i < 2; ++i)
	{
		for (j = 0; j < 2; ++j)
		{
			if (poffsets[i][j][0] < 0)
			{
				if (!ppatches[i][j]->getNeighborPatch(WEST))
				{
					poffsets[i][j][0] = 0;
				}
				else
				{
					poffsets[i][j][0] += patch_width;
					ppatches[i][j] = ppatches[i][j]->getNeighborPatch(WEST);
				}
			}
			if (poffsets[i][j][1] < 0)
			{
				if (!ppatches[i][j]->getNeighborPatch(SOUTH))
				{
					poffsets[i][j][1] = 0;
				}
				else
				{
					poffsets[i][j][1] += patch_width;
					ppatches[i][j] = ppatches[i][j]->getNeighborPatch(SOUTH);
				}
			}
			if (poffsets[i][j][0] >= (S32)patch_width)
			{
				if (!ppatches[i][j]->getNeighborPatch(EAST))
				{
					poffsets[i][j][0] = patch_width - 1;
				}
				else
				{
					poffsets[i][j][0] -= patch_width;
					ppatches[i][j] = ppatches[i][j]->getNeighborPatch(EAST);
				}
			}
			if (poffsets[i][j][1] >= (S32)patch_width)
			{
				if (!ppatches[i][j]->getNeighborPatch(NORTH))
				{
					poffsets[i][j][1] = patch_width - 1;
				}
				else
				{
					poffsets[i][j][1] -= patch_width;
					ppatches[i][j] = ppatches[i][j]->getNeighborPatch(NORTH);
				}
			}
		}
	}

	LLVector3 p00(-mpg, -mpg,
				  *(ppatches[0][0]->mDataZ + poffsets[0][0][0] +
					poffsets[0][0][1] * surface_stride));
	LLVector3 p01(-mpg, +mpg,
				  *(ppatches[0][1]->mDataZ + poffsets[0][1][0] +
					poffsets[0][1][1] * surface_stride));
	LLVector3 p10(+mpg, -mpg,
				  *(ppatches[1][0]->mDataZ + poffsets[1][0][0] +
					poffsets[1][0][1] * surface_stride));
	LLVector3 p11(+mpg, +mpg,
				  *(ppatches[1][1]->mDataZ + poffsets[1][1][0] +
					poffsets[1][1][1] * surface_stride));

	LLVector3 c1 = p11 - p00;
	LLVector3 c2 = p01 - p10;

	LLVector3 normal = c1;
	normal %= c2;
	normal.normalize();

	*(mDataNorm + surface_stride * y + x) = normal;
}

const LLVector3& LLSurfacePatch::getNormal(U32 x, U32 y) const
{
	U32 surface_stride = mSurfacep->getGridsPerEdge();
	return *(mDataNorm + surface_stride * y + x);
}

void LLSurfacePatch::updateCameraDistanceRegion(const LLVector3& pos_region)
{
	if (LLPipeline::sDynamicLOD)
	{
		if (!gShiftFrame)
		{
			LLVector3 dv = pos_region;
			dv -= mCenterRegion;
			mVisInfo.mDistance = llmax(0.f, (F32)(dv.length() - mRadius)) /
								 llmax(LLVOSurfacePatch::sLODFactor, 0.1f);
		}
	}
	else
	{
		mVisInfo.mDistance = 0.f;
	}
}

F32 LLSurfacePatch::getDistance() const
{
	return mVisInfo.mDistance;
}

// Called when a patch has changed its height field
// data.
void LLSurfacePatch::updateVerticalStats()
{
	if (!mDirtyZStats || !mSurfacep || !mSurfacep->getRegion())
	{
		return;
	}

	U32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();
	U32 grids_per_edge = mSurfacep->getGridsPerEdge();

	F32 z = *mDataZ;
	mMinZ = mMaxZ = z;

	U32 k = 0;
	F32 total = 0.f;
	// Iterate to +1 because we need to do the edges correctly.
	for (U32 j = 0; j <= grids_per_patch_edge; ++j)
	{
		for (U32 i = 0; i <= grids_per_patch_edge; ++i)
		{
			z = *(mDataZ + i + j * grids_per_edge);

			if (z < mMinZ)
			{
				mMinZ = z;
			}
			if (z > mMaxZ)
			{
				mMaxZ = z;
			}
			total += z;
			++k;
		}
	}
	mMeanZ = total / (F32)k;
	mCenterRegion.mV[VZ] = 0.5f * (mMinZ + mMaxZ);

	F32 meters_per_grid = mSurfacep->getMetersPerGrid();
	LLVector3 diam_vec(meters_per_grid * grids_per_patch_edge,
					   meters_per_grid * grids_per_patch_edge,
					   mMaxZ - mMinZ);
	mRadius = diam_vec.length() * 0.5f;

	mSurfacep->mMaxZ = llmax(mMaxZ, mSurfacep->mMaxZ);
	mSurfacep->mMinZ = llmin(mMinZ, mSurfacep->mMinZ);
	mSurfacep->mHasZData = true;
	mSurfacep->getRegion()->calculateCenterGlobal();

	if (mVObjp.notNull())
	{
		mVObjp->dirtyPatch();
	}
	mDirtyZStats = false;
}

void LLSurfacePatch::updateNormals()
{
	if (!mSurfacep || mSurfacep->mType == 'w')
	{
		return;
	}
	U32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();
	U32 grids_per_edge = mSurfacep->getGridsPerEdge();

	bool dirty_patch = false;

	// Update the east edge
	if (mNormalsInvalid[EAST] || mNormalsInvalid[NORTHEAST] ||
		mNormalsInvalid[SOUTHEAST])
	{
		for (U32 j = 0; j <= grids_per_patch_edge; ++j)
		{
			calcNormal(grids_per_patch_edge, j, 2);
			calcNormal(grids_per_patch_edge - 1, j, 2);
			calcNormal(grids_per_patch_edge - 2, j, 2);
		}
		dirty_patch = true;
	}

	// Update the north edge
	if (mNormalsInvalid[NORTHEAST] || mNormalsInvalid[NORTH] ||
		mNormalsInvalid[NORTHWEST])
	{
		for (U32 i = 0; i <= grids_per_patch_edge; ++i)
		{
			calcNormal(i, grids_per_patch_edge, 2);
			calcNormal(i, grids_per_patch_edge - 1, 2);
			calcNormal(i, grids_per_patch_edge - 2, 2);
		}
		dirty_patch = true;
	}

	// Update the west edge
	if (mNormalsInvalid[NORTHWEST] || mNormalsInvalid[WEST] ||
		mNormalsInvalid[SOUTHWEST])
	{
		for (U32 j = 0; j < grids_per_patch_edge; ++j)
		{
			calcNormal(0, j, 2);
			calcNormal(1, j, 2);
		}
		dirty_patch = true;
	}

	// Update the south edge
	if (mNormalsInvalid[SOUTHWEST] || mNormalsInvalid[SOUTH] ||
		mNormalsInvalid[SOUTHEAST])
	{
		for (U32 i = 0; i < grids_per_patch_edge; ++i)
		{
			calcNormal(i, 0, 2);
			calcNormal(i, 1, 2);
		}
		dirty_patch = true;
	}

	// Invalidating the northeast corner is different, because depending on
	// what the adjacent neighbors are, we'll want to do different things.
	if (mNormalsInvalid[NORTHEAST])
	{
		if (!getNeighborPatch(NORTHEAST))
		{
			if (!getNeighborPatch(NORTH))
			{
				if (!getNeighborPatch(EAST))
				{
					// No north or east neighbors. Pull from the diagonal in
					// your own patch.
					*(mDataZ + grids_per_patch_edge +
					  grids_per_patch_edge * grids_per_edge) =
						*(mDataZ + grids_per_patch_edge - 1 +
						  (grids_per_patch_edge - 1) * grids_per_edge);
				}
				else if (getNeighborPatch(EAST)->getHasReceivedData())
				{
					// East, but not north. Pull from your east neighbor's
					// northwest point.
					*(mDataZ + grids_per_patch_edge +
					  grids_per_patch_edge * grids_per_edge) =
						*(getNeighborPatch(EAST)->mDataZ +
						  (grids_per_patch_edge - 1) * grids_per_edge);
				}
				else
				{
					*(mDataZ + grids_per_patch_edge +
					  grids_per_patch_edge * grids_per_edge) =
						*(mDataZ + grids_per_patch_edge - 1 +
						  (grids_per_patch_edge - 1) * grids_per_edge);
				}
			}
			// At this point, we know we have a north
			else if (getNeighborPatch(EAST))
			{
				// North and east neighbors, but not northeast.
				// Pull from diagonal in your own patch.
				*(mDataZ + grids_per_patch_edge +
				  grids_per_patch_edge * grids_per_edge) =
					*(mDataZ + grids_per_patch_edge - 1 +
					  (grids_per_patch_edge - 1) * grids_per_edge);
			}
			else if (getNeighborPatch(NORTH)->getHasReceivedData())
			{
				// North, but not east. Pull from your north neighbor's
				// southeast corner.
				*(mDataZ + grids_per_patch_edge +
				  grids_per_patch_edge * grids_per_edge) =
					*(getNeighborPatch(NORTH)->mDataZ +
					  grids_per_patch_edge - 1);
			}
			else
			{
				*(mDataZ + grids_per_patch_edge +
				  grids_per_patch_edge * grids_per_edge) =
					*(mDataZ + grids_per_patch_edge - 1 +
					  (grids_per_patch_edge - 1) * grids_per_edge);
			}
		}
		else if (getNeighborPatch(NORTHEAST)->mSurfacep != mSurfacep)
		{
			if ((!getNeighborPatch(NORTH) ||
				 getNeighborPatch(NORTH)->mSurfacep != mSurfacep) &&
				(!getNeighborPatch(EAST) ||
				 getNeighborPatch(EAST)->mSurfacep != mSurfacep))
			{
				*(mDataZ + grids_per_patch_edge +
				  grids_per_patch_edge * grids_per_edge) =
					*(getNeighborPatch(NORTHEAST)->mDataZ);
			}
		}
#if 0
		else
		{
			// We have got a northeast patch in the same surface. The z and
			// normals will be handled by that patch.
		}
#endif
		calcNormal(grids_per_patch_edge, grids_per_patch_edge, 2);
		calcNormal(grids_per_patch_edge, grids_per_patch_edge - 1, 2);
		calcNormal(grids_per_patch_edge - 1, grids_per_patch_edge, 2);
		calcNormal(grids_per_patch_edge - 1, grids_per_patch_edge - 1, 2);
		dirty_patch = true;
	}

	// Update the middle normals
	if (mNormalsInvalid[MIDDLEMAP])
	{
		for (U32 j = 2; j < grids_per_patch_edge - 2; ++j)
		{
			for (U32 i = 2; i < grids_per_patch_edge - 2; ++i)
			{
				calcNormal(i, j, 2);
			}
		}
		dirty_patch = true;
	}

	if (dirty_patch)
	{
		mSurfacep->dirtySurfacePatch(this);
	}

	for (U32 i = 0; i < 9; ++i)
	{
		mNormalsInvalid[i] = false;
	}
}

void LLSurfacePatch::updateEastEdge()
{
	U32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();
	U32 grids_per_edge = mSurfacep->getGridsPerEdge();

	F32* west_surface;
	F32* east_surface;
	if (!getNeighborPatch(EAST))
	{
		west_surface = mDataZ + grids_per_patch_edge;
		east_surface = mDataZ + grids_per_patch_edge - 1;
	}
	else if (mConnectedEdge & EAST_EDGE)
	{
		west_surface = mDataZ + grids_per_patch_edge;
		east_surface = getNeighborPatch(EAST)->mDataZ;
	}
	else
	{
		return;
	}

	// If patchp is on the east edge of its surface, then we update the east
	// side buffer
	for (U32 j = 0; j < grids_per_patch_edge; ++j)
	{
		U32 k = j * grids_per_edge;
		*(west_surface + k) = *(east_surface + k);	// update buffer Z
	}
}

void LLSurfacePatch::updateNorthEdge()
{
	U32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();
	U32 grids_per_edge = mSurfacep->getGridsPerEdge();

	F32* south_surface;
	F32* north_surface;
	if (!getNeighborPatch(NORTH))
	{
		south_surface = mDataZ + grids_per_patch_edge * grids_per_edge;
		north_surface = mDataZ + (grids_per_patch_edge - 1) * grids_per_edge;
	}
	else if (mConnectedEdge & NORTH_EDGE)
	{
		south_surface = mDataZ + grids_per_patch_edge * grids_per_edge;
		north_surface = getNeighborPatch(NORTH)->mDataZ;
	}
	else
	{
		return;
	}

	// Update patchp's north edge ...
	for (U32 i = 0; i < grids_per_patch_edge; ++i)
	{
		*(south_surface + i) = *(north_surface + i);	// update buffer Z
	}
}

// Returns true when the update is considered done for this patch.
bool LLSurfacePatch::updateTexture()
{
	if (!mSTexUpdate || !mSurfacep)
	{
		return true;
	}
	LLViewerRegion* regionp = mSurfacep->getRegion();
	if (!regionp)
	{
		return true;
	}

	// Wait for all neighbours data to be received.
	LLSurfacePatch* patchp = getNeighborPatch(EAST);
	if (patchp && !patchp->getHasReceivedData())
	{
		return false;
	}
	patchp = getNeighborPatch(WEST);
	if (patchp && !patchp->getHasReceivedData())
	{
		return false;
	}
	patchp = getNeighborPatch(SOUTH);
	if (patchp && !patchp->getHasReceivedData())
	{
		return false;
	}
	patchp = getNeighborPatch(NORTH);
	if (patchp && !patchp->getHasReceivedData())
	{
		return false;
	}

	LLVLComposition* comp = regionp->getComposition();
	// Do check the parameters are ready now, to avoid a failed call to
	// LLVLComposition::generateTexture() in LLSurfacePatch::updateGL(). HB
	if (!comp->getParamsReady())
	{
		return false;
	}

	if (!mHeightsGenerated)
	{
		F32 meters_per_grid = mSurfacep->getMetersPerGrid();
		F32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();
		F32 patch_size = meters_per_grid * (grids_per_patch_edge + 1);
		LLVector3d origin_region = getOriginGlobal() -
								   mSurfacep->getOriginGlobal();
		F32 x = origin_region.mdV[VX];
		F32 y = origin_region.mdV[VY];
		if (!comp->generateHeights(x, y, patch_size, patch_size))
		{
			return false;
		}
		mHeightsGenerated = true;
	}

	// detailTexturesReady() must be called periodically. HB
	if (!comp->detailTexturesReady())
	{
		return false;
	}

	if (mVObjp.isNull())
	{
		return false;
	}

	mVObjp->dirtyGeom();
	gPipeline.markGLRebuild(mVObjp);
	LL_DEBUGS("MarkGLRebuild") << "Marked for GL rebuild: " << std::hex
							   << (intptr_t)mVObjp.get() << std::dec
							   << LL_ENDL;

	// If auto-reloading, we can accept a few seconds of fps rate slow down,
	// and keep updating the patch till it gets loaded, instead of aborting
	// it and getting a failed mini-map texture, like in LL's viewer and most
	// (all ?) other TPVs. HB
	return sAutoReloadDelay == 0.f;
}

void LLSurfacePatch::updateGL()
{
	if (!mSurfacep)	return;	// Paranoia

	LLViewerRegion* regionp = mSurfacep->getRegion();
	if (!regionp) return;

	F32 meters_per_grid = mSurfacep->getMetersPerGrid();
	F32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();

	LLVector3d origin_region = getOriginGlobal() -
							   mSurfacep->getOriginGlobal();

	updateCompositionStats(regionp);

	F32 x = origin_region.mdV[VX];
	F32 y = origin_region.mdV[VY];
	F32 size = meters_per_grid * grids_per_patch_edge;
	if (regionp->getComposition()->generateTexture(x, y, size, size))
	{
		mSTexUpdate = false;
		mFirstFailureTime = 0.f;
		// Also generate the water texture
		mSurfacep->generateWaterTexture(x, y, size, size);
		return;	// Success
	}

	// *HACK: register the first time we failed to generate a texture for
	// this patch, and if we are failing for too long, force-reload all
	// patches. HB
	if (sAutoReloadDelay > 0.f)
	{
		if (mFirstFailureTime < 0.f)
		{
			// Do not retry this hack indefinitely !
			return;
		}
		else if (mFirstFailureTime <= sNextAllowedReloadTime)
		{
			mFirstFailureTime = gFrameTimeSeconds;
		}
		else if (gFrameTimeSeconds - mFirstFailureTime > sAutoReloadDelay)
		{
			sNeedsPatchesReload = true;
			mFirstFailureTime = -1.f;	// Do not retry
		}
	}
}

//static
void LLSurfacePatch::setAutoReloadDelay(U32 delay)
{
	if (delay)
	{
		delay = llclamp(delay, 5, 30);
	}
	sAutoReloadDelay = delay;
}

//static
void LLSurfacePatch::allPatchesReloaded()
{
	// Set the delay till the next possible auto-retry to minimum 30s and
	// maximum twice the auto-reload delay. HB
	sNextAllowedReloadTime = gFrameTimeSeconds +
							 llmax(2.f * sAutoReloadDelay, 30.f);
	sNeedsPatchesReload = false;
}

void LLSurfacePatch::dirtyZ()
{
	mSTexUpdate = true;

	// Invalidate all normals in this patch
	for (U32 i = 0; i < 9; ++i)
	{
		mNormalsInvalid[i] = true;
	}

	// Invalidate normals in this and neighboring patches
	for (U32 i = 0; i < 8; ++i)
	{
		LLSurfacePatch* neighbor = getNeighborPatch(i);
		if (neighbor)
		{
			U32 opposite = gDirOpposite[i];
			neighbor->mNormalsInvalid[opposite] = true;
			neighbor->dirty();
			if (i < 4)
			{
				neighbor->mNormalsInvalid[gDirAdjacent[opposite][0]] = true;
				neighbor->mNormalsInvalid[gDirAdjacent[opposite][1]] = true;
			}
		}
	}

	dirty();
	mLastUpdateTime = gFrameTime;
}

void LLSurfacePatch::setOriginGlobal(const LLVector3d& origin_global)
{
	if (!mSurfacep) return;	// Paranoia

	mOriginGlobal = origin_global;

	LLVector3 origin_region;
	origin_region.set(mOriginGlobal - mSurfacep->getOriginGlobal());

	mOriginRegion = origin_region;
	mCenterRegion.mV[VX] = origin_region.mV[VX] + 0.5f *
						   mSurfacep->getGridsPerPatchEdge() *
						   mSurfacep->getMetersPerGrid();
	mCenterRegion.mV[VY] = origin_region.mV[VY] + 0.5f *
						   mSurfacep->getGridsPerPatchEdge() *
						   mSurfacep->getMetersPerGrid();

	mVisInfo.mIsVisible = false;
	mVisInfo.mDistance = 512.f;
	mVisInfo.mRenderLevel = 0;
	mVisInfo.mRenderStride = mSurfacep->getGridsPerPatchEdge();
}

void LLSurfacePatch::connectNeighbor(LLSurfacePatch* neighbor_patchp,
									 U32 direction)
{
	llassert(neighbor_patchp);
	mNormalsInvalid[direction] = true;
	neighbor_patchp->mNormalsInvalid[gDirOpposite[direction]] = true;

	setNeighborPatch(direction, neighbor_patchp);
	neighbor_patchp->setNeighborPatch(gDirOpposite[direction], this);

	if (direction == EAST)
	{
		mConnectedEdge |= EAST_EDGE;
		neighbor_patchp->mConnectedEdge |= WEST_EDGE;
	}
	else if (direction == NORTH)
	{
		mConnectedEdge |= NORTH_EDGE;
		neighbor_patchp->mConnectedEdge |= SOUTH_EDGE;
	}
	else if (direction == WEST)
	{
		mConnectedEdge |= WEST_EDGE;
		neighbor_patchp->mConnectedEdge |= EAST_EDGE;
	}
	else if (direction == SOUTH)
	{
		mConnectedEdge |= SOUTH_EDGE;
		neighbor_patchp->mConnectedEdge |= NORTH_EDGE;
	}
}

void LLSurfacePatch::updateVisibility()
{
	if (mVObjp.isNull())
	{
		return;
	}

	LLVector4a center;
	center.load3((mCenterRegion + mSurfacep->getOriginAgent()).mV);
	LLVector4a radius;
	radius.splat(mRadius);
	// Sphere in frustum on global coordinates
	if (gViewerCamera.AABBInFrustumNoFarClip(center, radius))
	{
		// We now need to calculate the render stride based on patchp's
		// distance from LLCamera render_stride is governed by a relation
		// something like this...
		//
		//                       delta_angle * patch.distance
		// render_stride <=  ----------------------------------------
		//                           mMetersPerGrid
		//
		// where 'delta_angle' is the desired solid angle of the average
		// polygon on a patch.
		//
		// Any render_stride smaller than the RHS would be 'satisfactory'.
		// Smaller strides give more resolution, but efficiency suggests that
		// we use the largest of the render_strides that obey the relation.
		// Flexibility is achieved by modulating 'delta_angle' until we have an
		// acceptable number of triangles.

		U32 old_render_stride = mVisInfo.mRenderStride;

		// Calculate the render_stride using information in agent
		constexpr F32 DEFAULT_DELTA_ANGLE = 0.15f;
		F32 stride_per_distance = DEFAULT_DELTA_ANGLE /
								  mSurfacep->getMetersPerGrid();
		U32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();
		U32 max_render_stride = lltrunc(mVisInfo.mDistance *
										stride_per_distance);
		max_render_stride = llmin(max_render_stride, 2 * grids_per_patch_edge);

		// We only use render_strides that are powers of two, so we use look-up
		// tables to figure out the render_level and corresponding
		// render_stride
		U32 new_render_level = mVisInfo.mRenderLevel =
			mSurfacep->getRenderLevel(max_render_stride);
		mVisInfo.mRenderStride = mSurfacep->getRenderStride(new_render_level);

		// The reason we check !mIsVisible is because non-visible patches
		// normals are not updated when their data is changed. When this
		// changes we can get rid of mIsVisible altogether.
		if (mVisInfo.mRenderStride != old_render_stride)
		{
			if (mVObjp.notNull())
			{
				mVObjp->dirtyGeom();
				LLSurfacePatch* neighbor = getNeighborPatch(WEST);
				if (neighbor && neighbor->mVObjp.notNull())
				{
					neighbor->mVObjp->dirtyGeom();
				}
				neighbor = getNeighborPatch(SOUTH);
				if (neighbor && neighbor->mVObjp.notNull())
				{
					neighbor->mVObjp->dirtyGeom();
				}
			}
		}
		mVisInfo.mIsVisible = true;
	}
	else
	{
		mVisInfo.mIsVisible = false;
	}
}

void LLSurfacePatch::updateCompositionStats(LLViewerRegion* regionp)
{
	LLViewerLayer* vlp = regionp->getComposition();
	if (!vlp) return;	// Paranoia

	LLVector3 origin = getOriginAgent() - mSurfacep->getOriginAgent();
	F32 mpg = mSurfacep->getMetersPerGrid();
	F32 x = origin.mV[VX];
	F32 y = origin.mV[VY];
	F32 multiplier = (F32)(mSurfacep->getGridsPerPatchEdge() + 1);
	F32 width = mpg * multiplier;
	F32 height = mpg * multiplier;

	F32 mean = 0.f;
	F32 min = vlp->getValueScaled(x, y);
	F32 max = min;
	U32 count = 0;
	for (F32 j = 0; j < height; j += mpg)
	{
		for (F32 i = 0; i < width; i += mpg)
		{
			F32 comp = vlp->getValueScaled(x + i, y + j);
			mean += comp;
			if (comp < min)
			{
				min = comp;
			}
			if (comp > max)
			{
				max = comp;
			}
			++count;
		}
	}
	if (count)
	{
		mean /= count;
	}

	mMinComposition = min;
	mMeanComposition = mean;
	mMaxComposition = max;
}

void LLSurfacePatch::setNeighborPatch(U32 direction, LLSurfacePatch* neighborp)
{
	mNeighborPatches[direction] = neighborp;
	mNormalsInvalid[direction] = true;
	if (direction < 4)
	{
		mNormalsInvalid[gDirAdjacent[direction][0]] = true;
		mNormalsInvalid[gDirAdjacent[direction][1]] = true;
	}
}
