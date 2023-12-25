/**
 * @file llvosurfacepatch.cpp
 * @brief Viewer-object derived "surface patch", which is a piece of terrain
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

#include "llvosurfacepatch.h"

#include "llfasttimer.h"
#include "llprimitive.h"

#include "lldrawable.h"
#include "lldrawpoolterrain.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llsurfacepatch.h"
#include "llsurface.h"
#include "llviewerobjectlist.h"
#include "llviewershadermgr.h"
#include "llvlcomposition.h"
#include "llvovolume.h"

F32 LLVOSurfacePatch::sLODFactor = 1.f;

///////////////////////////////////////////////////////////////////////////////
// LLTerrainPartition class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLTerrainPartition::LLTerrainPartition(LLViewerRegion* regionp)
:	LLSpatialPartition(LLDrawPoolTerrain::VERTEX_DATA_MASK, false, regionp)
{
	mOcclusionEnabled = false;
	mInfiniteFarClip = true;
	mDrawableType = LLPipeline::RENDER_TYPE_TERRAIN;
	mPartitionType = LLViewerRegion::PARTITION_TERRAIN;
}

//virtual
LLVertexBuffer* LLTerrainPartition::createVertexBuffer(U32 type_mask)
{
	LLVertexBuffer* bufferp;
	if (gUsePBRShaders)
	{
		bufferp = new LLVertexBuffer(type_mask);
	}
	else
	{
		// Note: texture coordinates 2 and 3 exist, but use the same data as
		// texture coordinate 1
		constexpr U32 typemask = LLVertexBuffer::MAP_VERTEX |
								 LLVertexBuffer::MAP_NORMAL |
								 LLVertexBuffer::MAP_TEXCOORD0 |
								 LLVertexBuffer::MAP_TEXCOORD1 |
								 LLVertexBuffer::MAP_COLOR;
		bufferp = new LLVertexBuffer(typemask);
		// Mask out coordinates 2 and 3 in the mask passed to
		// setupVertexBuffer() (this used to be done via a virtual method in
		// a LLVertexBufferTerrain() derived class). HB
		bufferp->setTypeMaskMask(LLVertexBuffer::MAP_TEXCOORD2 |
								 LLVertexBuffer::MAP_TEXCOORD3);
	}
#if LL_DEBUG_VB_ALLOC
	bufferp->setOwner("LLTerrainPartition");
#endif
	return bufferp;
}

//virtual
void LLTerrainPartition::getGeometry(LLSpatialGroup* group)
{
	LL_FAST_TIMER(FTM_REBUILD_TERRAIN_VB);

	LLVertexBuffer* buffer = group->mVertexBuffer;

	// Get vertex buffer striders
	LLStrider<LLVector3> vertices;
	LLStrider<LLVector3> normals;
	LLStrider<LLVector2> texcoords2;
	LLStrider<LLVector2> texcoords;
	LLStrider<U16> indices;
	if (!buffer || !buffer->getVertexStrider(vertices) ||
		!buffer->getNormalStrider(normals) ||
		!buffer->getTexCoord0Strider(texcoords) ||
		!buffer->getTexCoord1Strider(texcoords2) ||
		!buffer->getIndexStrider(indices))
	{
		return;
	}

	U32 indices_index = 0;
	U32 index_offset = 0;

	for (S32 i = 0, count = mFaceList.size(); i < count; ++i)
	{
		LLFace* facep = mFaceList[i];
		if (!facep) continue;	// Paranoia

		facep->setIndicesIndex(indices_index);
		facep->setGeomIndex(index_offset);
		facep->setVertexBuffer(buffer);

		LLVOSurfacePatch* patchp = (LLVOSurfacePatch*)facep->getViewerObject();
		patchp->getGeometry(vertices, normals, texcoords, texcoords2, indices);

		indices_index += facep->getIndicesCount();
		index_offset += facep->getGeomCount();
	}

	buffer->unmapBuffer();
	mFaceList.clear();
}

///////////////////////////////////////////////////////////////////////////////
// LLVOSurfacePatch class
///////////////////////////////////////////////////////////////////////////////

LLVOSurfacePatch::LLVOSurfacePatch(const LLUUID& id, LLViewerRegion* regionp)
:	LLStaticViewerObject(id, LL_VO_SURFACE_PATCH, regionp),
	mDirtiedPatch(false),
	mPool(NULL),
	mBaseComp(0),
	mPatchp(NULL),
	mDirtyTexture(false),
	mDirtyTerrain(false),
	mLastNorthStride(0),
	mLastEastStride(0),
	mLastStride(0),
	mLastLength(0)
{
	// Terrain must draw during selection passes so it can block objects behind
	// it.
	mCanSelect = true;
	// Hack for setting scale for bounding boxes/visibility.
	setScale(LLVector3(16.f, 16.f, 16.f));
}

//virtual
LLVOSurfacePatch::~LLVOSurfacePatch()
{
	mPatchp = NULL;
}

//virtual
void LLVOSurfacePatch::markDead()
{
	if (mPatchp)
	{
		mPatchp->clearVObj();
		mPatchp = NULL;
	}
	LLViewerObject::markDead();
}

//virtual
void LLVOSurfacePatch::setPixelAreaAndAngle()
{
	mAppAngle = 50;
	mPixelArea = 500 * 500;
}

LLFacePool* LLVOSurfacePatch::getPool()
{
	mPool =
		(LLDrawPoolTerrain*)gPipeline.getPool(LLDrawPool::POOL_TERRAIN,
											  mPatchp->getSurface()->getSTexture());
	return mPool;
}

//virtual
LLDrawable* LLVOSurfacePatch::createDrawable()
{
	gPipeline.allocDrawable(this);

	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_TERRAIN);

	mBaseComp = llfloor(mPatchp->getMinComposition());
	S32 min_comp = llfloor(mPatchp->getMinComposition());
	S32 max_comp = llceil(mPatchp->getMaxComposition());
	if (max_comp - min_comp + 1 > 3 &&
		mPatchp->getMinComposition() - min_comp >
			max_comp - mPatchp->getMaxComposition())
	{
		// The top side runs over more
		++mBaseComp;
	}

	mDrawable->addFace(getPool(), NULL);

	return mDrawable;
}

//virtual
void LLVOSurfacePatch::updateGL()
{
	if (mPatchp)
	{
		mPatchp->updateGL();
	}
}

//virtual
bool LLVOSurfacePatch::updateGeometry(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_UPDATE_TERRAIN);

	dirtySpatialGroup();

	S32 min_comp = lltrunc(mPatchp->getMinComposition());
	S32 max_comp = lltrunc(ceil(mPatchp->getMaxComposition()));

	// Pick the two closest detail textures for this patch then create the draw
	// pool for it. Actually, should get the average composition instead of the
	// center.
	S32 new_base_comp = lltrunc(mPatchp->getMinComposition());
	if (max_comp - min_comp + 1 > 3 &&
		mPatchp->getMinComposition() - min_comp >
			max_comp - mPatchp->getMaxComposition())
	{
		// The top side runs over more
		++new_base_comp;
	}
	mBaseComp = new_base_comp;

	// Figure out the strides

	mLastStride = mPatchp->getRenderStride();
	mLastLength = mPatchp->getSurface()->getGridsPerPatchEdge() / mLastStride;

	if (mPatchp->getNeighborPatch(NORTH))
	{
		mLastNorthStride = mPatchp->getNeighborPatch(NORTH)->getRenderStride();
	}
	else
	{
		mLastNorthStride = mLastStride;
	}

	if (mPatchp->getNeighborPatch(EAST))
	{
		mLastEastStride = mPatchp->getNeighborPatch(EAST)->getRenderStride();
	}
	else
	{
		mLastEastStride = mLastStride;
	}

	return true;
}

//virtual
void LLVOSurfacePatch::updateFaceSize(S32 idx)
{
	if (idx)
	{
		llwarns << "Terrain partition requested invalid face !" << llendl;
		return;
	}

	LLFace* facep = mDrawable->getFace(idx);
	if (!facep)
	{
		return;
	}

	S32 num_vertices = 0;
	S32 num_indices = 0;
	if (mLastStride)
	{
		getGeomSizesMain(mLastStride, num_vertices, num_indices);
		getGeomSizesNorth(mLastStride, mLastNorthStride, num_vertices,
						  num_indices);
		getGeomSizesEast(mLastStride, mLastEastStride, num_vertices,
						 num_indices);
	}
	facep->setSize(num_vertices, num_indices);
}

void LLVOSurfacePatch::getGeometry(LLStrider<LLVector3>& verticesp,
								   LLStrider<LLVector3>& normalsp,
								   LLStrider<LLVector2>& tex_coords0p,
								   LLStrider<LLVector2>& tex_coords1p,
								   LLStrider<U16>& indicesp)
{
	LLFace* facep = mDrawable->getFace(0);
	if (facep)
	{
		U32 index_offset = facep->getGeomIndex();

		updateMainGeometry(facep, verticesp, normalsp, tex_coords0p,
						   tex_coords1p, indicesp, index_offset);
		updateNorthGeometry(facep, verticesp, normalsp, tex_coords0p,
							tex_coords1p, indicesp, index_offset);
		updateEastGeometry(facep, verticesp, normalsp, tex_coords0p,
							tex_coords1p, indicesp, index_offset);
	}
}

void LLVOSurfacePatch::updateMainGeometry(LLFace* facep,
										  LLStrider<LLVector3>& verticesp,
										  LLStrider<LLVector3>& normalsp,
										  LLStrider<LLVector2>& tex_coords0p,
										  LLStrider<LLVector2>& tex_coords1p,
										  LLStrider<U16>& indicesp,
										  U32& index_offset)
{
	llassert(mLastStride > 0);

	U32 render_stride = mLastStride;
	U32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();
	S32 vert_size = patch_size / render_stride;

	// Render the main patch

	U32 index;
	S32 num_vertices = 0;
	S32 num_indices = 0;
	// First, figure out how many vertices we need...
	getGeomSizesMain(render_stride, num_vertices, num_indices);
	if (num_vertices > 0)
	{
		facep->mCenterAgent = mPatchp->getPointAgent(8, 8);

		// Generate patch points first
		for (S32 j = 0; j < vert_size; ++j)
		{
			for (S32 i = 0; i < vert_size; ++i)
			{
				S32 x = i * render_stride;
				S32 y = j * render_stride;
				mPatchp->eval(x, y, render_stride, verticesp.get(),
							  normalsp.get(), tex_coords0p.get(),
							  tex_coords1p.get());
				verticesp++;
				normalsp++;
				tex_coords0p++;
				tex_coords1p++;
			}
		}

		for (S32 j = 0; j < vert_size - 1; ++j)
		{
			if (j % 2)
			{
				for (S32 i = vert_size - 1; i > 0; --i)
				{
					index = (i - 1) + j * vert_size;
					*(indicesp++) = index_offset + index;

					index = i + (j + 1) * vert_size;
					*(indicesp++) = index_offset + index;

					index = (i - 1) + (j + 1) * vert_size;
					*(indicesp++) = index_offset + index;

					index = (i - 1) + j * vert_size;
					*(indicesp++) = index_offset + index;

					index = i + j * vert_size;
					*(indicesp++) = index_offset + index;

					index = i + (j + 1) * vert_size;
					*(indicesp++) = index_offset + index;
				}
			}
			else
			{
				for (S32 i = 0; i < vert_size - 1; ++i)
				{
					index = i + j * vert_size;
					*(indicesp++) = index_offset + index;

					index = (i + 1) + (j + 1) * vert_size;
					*(indicesp++) = index_offset + index;

					index = i + (j + 1) * vert_size;
					*(indicesp++) = index_offset + index;

					index = i + j * vert_size;
					*(indicesp++) = index_offset + index;

					index = (i + 1) + j * vert_size;
					*(indicesp++) = index_offset + index;

					index = (i + 1) + (j + 1) * vert_size;
					*(indicesp++) = index_offset + index;
				}
			}
		}
	}
	index_offset += num_vertices;
}

void LLVOSurfacePatch::updateNorthGeometry(LLFace* facep,
										LLStrider<LLVector3>& verticesp,
										LLStrider<LLVector3>& normalsp,
										LLStrider<LLVector2>& tex_coords0p,
										LLStrider<LLVector2>& tex_coords1p,
										LLStrider<U16>& indicesp,
										U32& index_offset)
{
	S32 num_vertices;

	U32 render_stride = mLastStride;
	S32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();
	S32 length = patch_size / render_stride;
	S32 half_length = length / 2;
	U32 north_stride = mLastNorthStride;

	// Render the north strip

	// Stride lengths are the same
	if (north_stride == render_stride)
	{
		num_vertices = 2 * length + 1;

		facep->mCenterAgent = (mPatchp->getPointAgent(8, 15) +
							   mPatchp->getPointAgent(8, 16)) * 0.5f;

		// Main patch
		for (S32 i = 0; i < length; ++i)
		{
			S32 x = i * render_stride;
			S32 y = 16 - render_stride;
			mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(),
						  tex_coords0p.get(), tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}

		// North patch
		for (S32 i = 0; i <= length; ++i)
		{
			S32 x = i * render_stride;
			mPatchp->eval(x, 16, render_stride, verticesp.get(),
						  normalsp.get(), tex_coords0p.get(),
						  tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}

		for (S32 i = 0; i < length; ++i)
		{
			// Generate indices
			*(indicesp++) = index_offset + i;
			*(indicesp++) = index_offset + length + i + 1;
			*(indicesp++) = index_offset + length + i;

			if (i != length - 1)
			{
				*(indicesp++) = index_offset + i;
				*(indicesp++) = index_offset + i + 1;
				*(indicesp++) = index_offset + length + i + 1;
			}
		}
	}
	else if (north_stride > render_stride)
	{
		// North stride is longer (has less vertices)
		num_vertices = length + length / 2 + 1;

		facep->mCenterAgent = (mPatchp->getPointAgent(7, 15) +
							   mPatchp->getPointAgent(8, 16)) * 0.5f;

		// Iterate through this patch's points
		for (S32 i = 0; i < length; ++i)
		{
			S32 x = i * render_stride;
			S32 y = 16 - render_stride;

			mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(),
						  tex_coords0p.get(), tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}

		// Iterate through the north patch's points
		for (S32 i = 0; i <= length; i += 2)
		{
			S32 x = i * render_stride;
			mPatchp->eval(x, 16, render_stride, verticesp.get(),
						  normalsp.get(), tex_coords0p.get(),
						  tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}

		for (S32 i = 0; i < length; ++i)
		{
			if (!(i % 2))
			{
				*(indicesp++) = index_offset + i;
				*(indicesp++) = index_offset + i + 1;
				*(indicesp++) = index_offset + length + i / 2;

				*(indicesp++) = index_offset + i + 1;
				*(indicesp++) = index_offset + length + i / 2 + 1;
				*(indicesp++) = index_offset + length + i / 2;
			}
			else if (i < (length - 1))
			{
				*(indicesp++) = index_offset + i;
				*(indicesp++) = index_offset + i + 1;
				*(indicesp++) = index_offset + length + i / 2 + 1;
			}
		}
	}
	else
	{
		// North stride is shorter (more vertices)
		length = patch_size / north_stride;
		half_length = length / 2;
		num_vertices = length + half_length + 1;

		facep->mCenterAgent = (mPatchp->getPointAgent(15, 7) +
							   mPatchp->getPointAgent(16, 8)) * 0.5f;

		// Iterate through this patch's points
		for (S32 i = 0; i < length; i += 2)
		{
			S32 x = i * north_stride;
			S32 y = 16 - render_stride;
			mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(),
						  tex_coords0p.get(), tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}

		// Iterate through the north patch's points
		for (S32 i = 0; i <= length; ++i)
		{
			S32 x = i * north_stride;
			mPatchp->eval(x, 16, render_stride, verticesp.get(),
						  normalsp.get(), tex_coords0p.get(),
						  tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}

		for (S32 i = 0; i < length; ++i)
		{
			if (!(i % 2))
			{
				*(indicesp++) = index_offset + half_length + i;
				*(indicesp++) = index_offset + i / 2;
				*(indicesp++) = index_offset + half_length + i + 1;
			}
			else if (i < length - 2)
			{
				*(indicesp++) = index_offset + half_length + i;
				*(indicesp++) = index_offset + i / 2;
				*(indicesp++) = index_offset + i / 2 + 1;

				*(indicesp++) = index_offset + half_length + i;
				*(indicesp++) = index_offset + i / 2 + 1;
				*(indicesp++) = index_offset + half_length + i + 1;
			}
			else
			{
				*(indicesp++) = index_offset + half_length + i;
				*(indicesp++) = index_offset + i / 2;
				*(indicesp++) = index_offset + half_length + i + 1;
			}
		}
	}
	index_offset += num_vertices;
}

void LLVOSurfacePatch::updateEastGeometry(LLFace* facep,
										  LLStrider<LLVector3>& verticesp,
										  LLStrider<LLVector3>& normalsp,
										  LLStrider<LLVector2>& tex_coords0p,
										  LLStrider<LLVector2>& tex_coords1p,
										  LLStrider<U16>& indicesp,
										  U32& index_offset)
{
	S32 num_vertices;
	U32 render_stride = mLastStride;
	S32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();
	S32 length = patch_size / render_stride;
	S32 half_length = length / 2;
	U32 east_stride = mLastEastStride;
	// Stride lengths are the same
	if (east_stride == render_stride)
	{
		num_vertices = 2 * length + 1;

		facep->mCenterAgent = (mPatchp->getPointAgent(8, 15) +
							   mPatchp->getPointAgent(8, 16)) * 0.5f;

		// Main patch
		for (S32 i = 0; i < length; ++i)
		{
			S32 x = 16 - render_stride;
			S32 y = i * render_stride;
			mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(),
						  tex_coords0p.get(), tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}

		// East patch
		for (S32 i = 0; i <= length; ++i)
		{
			S32 y = i * render_stride;
			mPatchp->eval(16, y, render_stride, verticesp.get(),
						  normalsp.get(), tex_coords0p.get(),
						  tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}

		for (S32 i = 0; i < length; ++i)
		{
			// Generate indices
			*(indicesp++) = index_offset + i;
			*(indicesp++) = index_offset + length + i;
			*(indicesp++) = index_offset + length + i + 1;

			if (i != length - 1)
			{
				*(indicesp++) = index_offset + i;
				*(indicesp++) = index_offset + length + i + 1;
				*(indicesp++) = index_offset + i + 1;
			}
		}
	}
	else if (east_stride > render_stride)
	{
		// East stride is longer (has less vertices)
		num_vertices = length + half_length + 1;

		facep->mCenterAgent = (mPatchp->getPointAgent(7, 15) +
							   mPatchp->getPointAgent(8, 16)) * 0.5f;

		// Iterate through this patch's points
		for (S32 i = 0; i < length; ++i)
		{
			S32 x = 16 - render_stride;
			S32 y = i * render_stride;
			mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(),
						  tex_coords0p.get(), tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}
		// Iterate through the east patch's points
		for (S32 i = 0; i <= length; i += 2)
		{
			S32 y = i * render_stride;
			mPatchp->eval(16, y, render_stride, verticesp.get(),
						  normalsp.get(), tex_coords0p.get(),
						  tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}

		for (S32 i = 0; i < length; ++i)
		{
			if (!(i % 2))
			{
				*(indicesp++) = index_offset + i;
				*(indicesp++) = index_offset + length + i / 2;
				*(indicesp++) = index_offset + i + 1;

				*(indicesp++) = index_offset + i + 1;
				*(indicesp++) = index_offset + length + i / 2;
				*(indicesp++) = index_offset + length + i / 2 + 1;
			}
			else if (i < (length - 1))
			{
				*(indicesp++) = index_offset + i;
				*(indicesp++) = index_offset + length + i / 2 + 1;
				*(indicesp++) = index_offset + i + 1;
			}
		}
	}
	else
	{
		// East stride is shorter (more vertices)
		length = patch_size / east_stride;
		half_length = length / 2;
		num_vertices = length + length / 2 + 1;

		facep->mCenterAgent = (mPatchp->getPointAgent(15, 7) +
							   mPatchp->getPointAgent(16, 8)) * 0.5f;

		// Iterate through this patch's points
		for (S32 i = 0; i < length; i += 2)
		{
			S32 x = 16 - render_stride;
			S32 y = i * east_stride;
			mPatchp->eval(x, y, render_stride, verticesp.get(), normalsp.get(),
						  tex_coords0p.get(), tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}
		// Iterate through the east patch's points
		for (S32 i = 0; i <= length; ++i)
		{
			S32 y = i * east_stride;
			mPatchp->eval(16, y, render_stride, verticesp.get(),
						  normalsp.get(), tex_coords0p.get(),
						  tex_coords1p.get());
			verticesp++;
			normalsp++;
			tex_coords0p++;
			tex_coords1p++;
		}

		for (S32 i = 0; i < length; ++i)
		{
			if (!(i % 2))
			{
				*(indicesp++) = index_offset + half_length + i;
				*(indicesp++) = index_offset + half_length + i + 1;
				*(indicesp++) = index_offset + i / 2;
			}
			else if (i < length - 2)
			{
				*(indicesp++) = index_offset + half_length + i;
				*(indicesp++) = index_offset + i / 2 + 1;
				*(indicesp++) = index_offset + i / 2;

				*(indicesp++) = index_offset + half_length + i;
				*(indicesp++) = index_offset + half_length + i + 1;
				*(indicesp++) = index_offset + i / 2 + 1;
			}
			else
			{
				*(indicesp++) = index_offset + half_length + i;
				*(indicesp++) = index_offset + half_length + i + 1;
				*(indicesp++) = index_offset + i / 2;
			}
		}
	}
	index_offset += num_vertices;
}

void LLVOSurfacePatch::setPatch(LLSurfacePatch* patchp)
{
	mPatchp = patchp;
	dirtyPatch();
}

void LLVOSurfacePatch::dirtyPatch()
{
	mDirtiedPatch = true;
	dirtyGeom();
	mDirtyTerrain = true;
	LLVector3 center = mPatchp->getCenterRegion();
	LLSurface* surfacep = mPatchp->getSurface();

	setPositionRegion(center);

	F32 scale_factor = surfacep->getGridsPerPatchEdge() *
					   surfacep->getMetersPerGrid();
	setScale(LLVector3(scale_factor, scale_factor,
					   mPatchp->getMaxZ() - mPatchp->getMinZ()));
}

void LLVOSurfacePatch::dirtyGeom()
{
	if (mDrawable)
	{
		gPipeline.markRebuild(mDrawable);
		LLFace* facep = mDrawable->getFace(0);
		if (facep)
		{
			facep->setVertexBuffer(NULL);
		}
		mDrawable->movePartition();
	}
}

void LLVOSurfacePatch::getGeomSizesMain(S32 stride, S32& num_vertices,
										S32& num_indices)
{
	S32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();

	// First, figure out how many vertices we need...
	S32 vert_size = patch_size / stride;
	if (vert_size >= 2)
	{
		num_vertices += vert_size * vert_size;
		num_indices += 6 * (vert_size - 1) * (vert_size - 1);
	}
}

void LLVOSurfacePatch::getGeomSizesNorth(S32 stride, S32 north_stride,
										 S32& num_vertices, S32& num_indices)
{
	S32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();
	S32 length = patch_size / stride;
	// Stride lengths are the same
	if (north_stride == stride)
	{
		num_vertices += 2 * length + 1;
		num_indices += length * 6 - 3;
	}
	else if (north_stride > stride)
	{
		// North stride is longer (has less vertices)
		num_vertices += length + length / 2 + 1;
		num_indices += (length / 2) * 9 - 3;
	}
	else
	{
		// North stride is shorter (more vertices)
		length = patch_size / north_stride;
		num_vertices += length + length / 2 + 1;
		num_indices += 9 * (length / 2) - 3;
	}
}

void LLVOSurfacePatch::getGeomSizesEast(S32 stride, S32 east_stride,
										S32& num_vertices, S32& num_indices)
{
	S32 patch_size = mPatchp->getSurface()->getGridsPerPatchEdge();
	S32 length = patch_size / stride;
	// Stride lengths are the same
	if (east_stride == stride)
	{
		num_vertices += 2 * length + 1;
		num_indices += length * 6 - 3;
	}
	else if (east_stride > stride)
	{
		// East stride is longer (has less vertices)
		num_vertices += length + length / 2 + 1;
		num_indices += (length / 2) * 9 - 3;
	}
	else
	{
		// East stride is shorter (more vertices)
		length = patch_size / east_stride;
		num_vertices += length + length / 2 + 1;
		num_indices += 9* (length / 2) - 3;
	}
}

bool LLVOSurfacePatch::lineSegmentIntersect(const LLVector4a& start,
											const LLVector4a& end, S32 face,
											bool pick_transparent,
											bool pick_rigged, S32* face_hitp,
											LLVector4a* intersection,
											LLVector2* tex_coord,
											LLVector4a* normal,
											LLVector4a* tangent)

{
	if (!lineSegmentBoundingBox(start, end))
	{
		return false;
	}

	LLVector4a da;
	da.setSub(end, start);
	LLVector3 delta(da.getF32ptr());

	LLVector3 pdelta = delta;
	pdelta.mV[2] = 0;

	F32 plength = pdelta.length();

	F32 tdelta = plength != 0.f ? 1.f / plength : F32_MAX / 10000.f;

	LLVector3 v_start(start.getF32ptr());
	LLVector3 origin = v_start - mRegionp->getOriginAgent();

	if (mRegionp->getLandHeightRegion(origin) > origin.mV[2])
	{
		// Origin is under ground, treat as no intersection
		return false;
	}

	// Step one meter at a time until intersection point found

	// VECTORIZE THIS
	const LLVector4a* exta = mDrawable->getSpatialExtents();

	LLVector3 ext[2];
	ext[0].set(exta[0].getF32ptr());
	ext[1].set(exta[1].getF32ptr());

	F32 rad = (delta * tdelta).lengthSquared();

	F32 t = 0.f;
	while (t <= 1.f)
	{
		LLVector3 sample = origin + delta * t;

		if (AABBSphereIntersectR2(ext[0], ext[1],
								  sample + mRegionp->getOriginAgent(),
								  rad))
		{
			F32 height = mRegionp->getLandHeightRegion(sample);
			if (height > sample.mV[2])
			{
				// Ray went below ground, positive intersection. Quick and
				// dirty binary search to get impact point.
				tdelta = -tdelta * 0.5f;
				F32 err_dist = 0.001f;
				F32 dist = fabsf(sample.mV[2] - height);

				while (dist > err_dist && tdelta * tdelta > 0.f)
				{
					t += tdelta;
					sample = origin + delta * t;
					height = mRegionp->getLandHeightRegion(sample);
					if ((tdelta < 0 && height < sample.mV[2]) ||
						(height > sample.mV[2] && tdelta > 0))
					{	// jumped over intersection point, go back
						tdelta = -tdelta;
					}
					tdelta *= 0.5f;
					dist = fabsf(sample.mV[2] - height);
				}

				if (intersection)
				{
					F32 height = mRegionp->getLandHeightRegion(sample);
					if (fabsf(sample.mV[2] - height) < delta.length() * tdelta)
					{
						sample.mV[2] = mRegionp->getLandHeightRegion(sample);
					}
					intersection->load3((sample + mRegionp->getOriginAgent()).mV);
				}

				if (normal)
				{
					normal->load3((mRegionp->getLand().resolveNormalGlobal(mRegionp->getPosGlobalFromRegion(sample))).mV);
				}

				return true;
			}
		}

		t += tdelta;
		if (t > 1 && t < 1.f + tdelta * 0.99f)
		{
			// Make sure end point is checked (saves vertical lines coming up
			// negative)
			t = 1.f;
		}
	}

	return false;
}

void LLVOSurfacePatch::updateSpatialExtents(LLVector4a& new_min,
											LLVector4a& new_max)
{
	LLVector3 posAgent = getPositionAgent();
	LLVector3 scale = getScale();
	// Make z-axis scale at least 1 to avoid shadow artifacts on totally flat
	// land
	scale.mV[VZ] = llmax(scale.mV[VZ], 1.f);

	// Changing to 2.f makes the culling a -little- better, but still wrong
	new_min.load3((posAgent - scale * 0.5f).mV);

	new_max.load3((posAgent + scale * 0.5f).mV);
	LLVector4a pos;
	pos.setAdd(new_min, new_max);
	pos.mul(0.5f);
	mDrawable->setPositionGroup(pos);
}
