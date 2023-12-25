/**
 * @file llvowater.cpp
 * @brief LLVOWater class implementation
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llvowater.h"

#include "imageids.h"
#include "llfasttimer.h"

#include "llagent.h"
#include "lldrawable.h"
#include "lldrawpoolwater.h"
#include "llface.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llsurface.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llvosky.h"
#include "llworld.h"

///////////////////////////////////

template<class T> LL_INLINE T LERP(T a, T b, F32 factor)
{
	return a + (b - a) * factor;
}

LLVOWater::LLVOWater(const LLUUID& id, LLViewerRegion* regionp, LLPCode pcode)
:	LLStaticViewerObject(id, pcode, regionp),
	mRenderType(LLPipeline::RENDER_TYPE_WATER),
	mUseTexture(true),
	mIsEdgePatch(false)
{
	// Terrain must draw during selection passes so it can block objects behind
	// it.
	mCanSelect = false;

	// Hack for setting scale for bounding boxes/visibility.
	// Variable region size support
	setScale(LLVector3(regionp->getWidth(), regionp->getWidth(), 0.f));
}

void LLVOWater::setPixelAreaAndAngle()
{
	mAppAngle = 50;
	mPixelArea = 500 * 500;
}

LLDrawable* LLVOWater::createDrawable()
{
	gPipeline.allocDrawable(this);
	mDrawable->setLit(false);
	mDrawable->setRenderType(mRenderType);

	LLDrawPoolWater* pool = (LLDrawPoolWater*)gPipeline.getPool(LLDrawPool::POOL_WATER);

	if (mUseTexture)
	{
		mDrawable->setNumFaces(1, pool, mRegionp->getLand().getWaterTexture());
	}
	else
	{
		mDrawable->setNumFaces(1, pool, gWorld.getDefaultWaterTexture());
	}

	return mDrawable;
}

bool LLVOWater::updateGeometry(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_UPDATE_WATER);

	if (drawable->getNumFaces() < 1)
	{
		LLDrawPoolWater* poolp =
			(LLDrawPoolWater*)gPipeline.getPool(LLDrawPool::POOL_WATER);
		drawable->addFace(poolp, NULL);
	}
	LLFace* face = drawable->getFace(0);
	if (!face)
	{
		llerrs << "Could not add a new face !" << llendl;
	}

	S32 size = 1;
	if (LLPipeline::waterReflectionType())
	{
		size = 16;
	}
	const U32 num_quads = size * size;
	// A quad is 4 vertices and 6 indices (making 2 triangles)
	constexpr U32 vertices_per_quad = 4;
	constexpr U32 indices_per_quad = 6;
	face->setSize(vertices_per_quad * num_quads,
				  indices_per_quad * num_quads);

	LLVertexBuffer* buffp = face->getVertexBuffer();
	if (!buffp || buffp->getNumIndices() != face->getIndicesCount() ||
		buffp->getNumVerts() != face->getGeomCount())
	{
		buffp = new LLVertexBuffer(LLDrawPoolWater::VERTEX_DATA_MASK);
#if LL_DEBUG_VB_ALLOC
		buffp->setOwner("LLVOWater");
#endif
		if (!buffp->allocateBuffer(face->getGeomCount(),
								   face->getIndicesCount()))
		{
			llwarns << "Failure to allocate a vertex buffer with "
					<< face->getGeomCount() << " vertices and "
					<< face->getIndicesCount() << " indices" << llendl;
			return false;
		}
		face->setIndicesIndex(0);
		face->setGeomIndex(0);
		face->setVertexBuffer(buffp);
	}

	LLStrider<LLVector3> verticesp, normalsp;
	LLStrider<LLVector2> texcoordsp;
	LLStrider<U16> indicesp;
	U16 index_offset = face->getGeometry(verticesp, normalsp, texcoordsp,
										 indicesp);

	LLVector3 position_agent = getPositionAgent();
	face->mCenterAgent = position_agent;
	face->mCenterLocal = position_agent;

	F32 step_x = getScale().mV[0] / (F32)size;
	F32 step_y = getScale().mV[1] / (F32)size;

	const LLVector3 up(0.f, step_y * 0.5f, 0.f);
	const LLVector3 right(step_x * 0.5f, 0.f, 0.f);
	const LLVector3 normal(0.f, 0.f, 1.f);

	F32 size_inv = 1.f / (F32)size;

	for (S32 y = 0; y < size; ++y)
	{
		for (S32 x = 0; x < size; ++x)
		{
			S32 toffset = index_offset + 4 * (y * size + x);
			LLVector3 pos = position_agent - getScale() * 0.5f;
			pos.mV[VX] += (x + 0.5f) * step_x;
			pos.mV[VY] += (y + 0.5f) * step_y;

			*verticesp++  = pos - right + up;
			*verticesp++  = pos - right - up;
			*verticesp++  = pos + right + up;
			*verticesp++  = pos + right - up;

			*texcoordsp++ = LLVector2(x * size_inv, (y + 1) * size_inv);
			*texcoordsp++ = LLVector2(x * size_inv, y * size_inv);
			*texcoordsp++ = LLVector2((x + 1) * size_inv, (y + 1) * size_inv);
			*texcoordsp++ = LLVector2((x + 1) * size_inv, y * size_inv);

			*normalsp++   = normal;
			*normalsp++   = normal;
			*normalsp++   = normal;
			*normalsp++   = normal;

			*indicesp++ = toffset + 0;
			*indicesp++ = toffset + 1;
			*indicesp++ = toffset + 2;

			*indicesp++ = toffset + 1;
			*indicesp++ = toffset + 3;
			*indicesp++ = toffset + 2;
		}
	}

	buffp->unmapBuffer();

	mDrawable->movePartition();

	return true;
}

void setVecZ(LLVector3& v)
{
	v.mV[VX] = v.mV[VY] = 0;
	v.mV[VZ] = 1;
}

void LLVOWater::updateSpatialExtents(LLVector4a& new_min, LLVector4a& new_max)
{
	LLVector4a pos;
	pos.load3(getPositionAgent().mV);

	LLVector4a scale;
	scale.load3(getScale().mV);
	scale.mul(0.5f);

	new_min.setSub(pos, scale);
	new_max.setAdd(pos, scale);

	pos.setAdd(new_min, new_max);
	pos.mul(0.5f);

	mDrawable->setPositionGroup(pos);
}

LLWaterPartition::LLWaterPartition(LLViewerRegion* regionp)
:	LLSpatialPartition(0, false, regionp)
{
	mInfiniteFarClip = true;
	mDrawableType = LLPipeline::RENDER_TYPE_WATER;
	mPartitionType = LLViewerRegion::PARTITION_WATER;
}

LLVoidWaterPartition::LLVoidWaterPartition(LLViewerRegion* regionp)
:	LLWaterPartition(regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_VOIDWATER;
	mPartitionType = LLViewerRegion::PARTITION_VOIDWATER;
}
