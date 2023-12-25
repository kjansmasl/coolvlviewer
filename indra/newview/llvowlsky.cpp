/**
 * @file llvowlsky.cpp
 * @brief LLVOWLSky class implementation
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llvowlsky.h"

#include "llfasttimer.h"

#include "lldrawpoolwlsky.h"
#include "llenvironment.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llviewercontrol.h"
#include "llvovolume.h"				// For LLVOVolume::sRenderMaxVBOSize

#define STAR_NUM_VERTS 1000

// Anything less than 3 makes it impossible to create a closed dome.
constexpr U32 MIN_SKY_DETAIL = 3;
// Anything bigger than about 180 will cause getStripsNumVerts() to exceed
// 65535.
constexpr U32 MAX_SKY_DETAIL = 180;

U32 LLVOWLSky::sWLSkyDetail = 64;

LLVOWLSky::LLVOWLSky(const LLUUID& id, LLViewerRegion* regionp)
:	LLStaticViewerObject(id, LL_VO_WL_SKY, regionp, true),
	mLastWLSkyDetail(sWLSkyDetail)
{
	initStars();
}

//static
void LLVOWLSky::initClass()
{
	updateSettings();
}

//static
void LLVOWLSky::updateSettings()
{
	sWLSkyDetail = llclamp(gSavedSettings.getU32("WLSkyDetail"),
						   MIN_SKY_DETAIL, MAX_SKY_DETAIL);

	constexpr U32 data_mask = LLDrawPoolWLSky::SKY_VERTEX_DATA_MASK;
	const U32 max_verts = LLVOVolume::sRenderMaxVBOSize * 1024 /
						  LLVertexBuffer::calcVertexSize(data_mask);
	bool max_vbo_clamp = false;
	do
	{
		U32 verts_per_stack = getNumSlices();
		U32 stacks_per_seg = (max_verts - verts_per_stack) / verts_per_stack;
		if (stacks_per_seg > 0)
		{
			break;
		}
		max_vbo_clamp = true;
	}
	while (--sWLSkyDetail > MIN_SKY_DETAIL);

	if (max_vbo_clamp)
	{
		llwarns << "Sky details clamped to " << sWLSkyDetail
				<< ": increase RenderMaxVBOSize for more." << llendl;
	}
	// We need to rebuild our current sky geometry
	if (gSky.mVOWLSkyp.notNull())
	{
		gSky.mVOWLSkyp->updateGeometry(gSky.mVOWLSkyp->mDrawable);
	}
}

//static
void LLVOWLSky::cleanupClass()
{
}

LLDrawable* LLVOWLSky::createDrawable()
{
	gPipeline.allocDrawable(this);

	//LLDrawPoolWLSky *poolp = static_cast<LLDrawPoolWLSky *>(
	gPipeline.getPool(LLDrawPool::POOL_WL_SKY);

	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_WL_SKY);

	return mDrawable;
}

LL_INLINE F32 LLVOWLSky::calcPhi(U32 i)
{
	// i should range from [0..SKY_STACKS] so t will range from [0.f .. 1.f]
	F32 t = (F32)i / (F32)getNumStacks();

	// ^4 the parameter of the tesselation to bias things toward 0 (the dome's
	// apex)
	t *= t;
	t *= t;

	// Invert and square the parameter of the tesselation to bias things toward
	// 1 (the horizon)
	t = 1.f - t;
	t *= t;
	t = 1.f - t;

	return (F_PI / 8.f) * t;
}

void LLVOWLSky::resetVertexBuffers()
{
	mStripsVerts.clear();
	mStarsVerts = NULL;
	gPipeline.markRebuild(mDrawable);
}

void LLVOWLSky::cleanupGL()
{
	mStripsVerts.clear();
	mStarsVerts = NULL;
	LLDrawPoolWLSky::cleanupGL();
}

void LLVOWLSky::restoreGL()
{
	LLDrawPoolWLSky::restoreGL();
	gPipeline.markRebuild(mDrawable);
}

bool LLVOWLSky::updateGeometry(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_GEO_SKY);

	initStars();

	if (mLastWLSkyDetail != sWLSkyDetail)
	{
		// Sky detail settings changed so we need to rebuild our vertex buffers
		resetVertexBuffers();
	}

	const U32 max_buffer_bytes = LLVOVolume::sRenderMaxVBOSize * 1024;
	const U32 data_mask = LLDrawPoolWLSky::SKY_VERTEX_DATA_MASK;
	const U32 max_verts = max_buffer_bytes /
						  LLVertexBuffer::calcVertexSize(data_mask);
	const U32 total_stacks = getNumStacks();
	const U32 verts_per_stack = getNumSlices();

	// Each segment has to have one more row of verts than it has stacks then
	// round down
	const U32 stacks_per_seg = (max_verts - verts_per_stack) / verts_per_stack;
	if (stacks_per_seg == 0)
	{
		llwarns << "Failed updating WindLight sky geometry." << llendl;
		return false;
	}

	// Round up to a whole number of segments
	const U32 strips_segments = (total_stacks + stacks_per_seg - 1) /
								stacks_per_seg;

	llinfos << "WL Skydome strips in " << strips_segments << " batches."
			<< llendl;

	mStripsVerts.resize(strips_segments, NULL);

	LLStrider<LLVector3> vertices;
	LLStrider<LLVector2> texcoords;
	LLStrider<U16> indices;
	for (U32 i = 0; i < strips_segments; ++i)
	{
		LLVertexBuffer* segment =
			new LLVertexBuffer(LLDrawPoolWLSky::SKY_VERTEX_DATA_MASK);
#if LL_DEBUG_VB_ALLOC
		segment->setOwner("LLVOWLSky segment");
#endif
		mStripsVerts[i] = segment;

		U32 num_stacks_this_seg = stacks_per_seg;
		if (i == strips_segments - 1 && (total_stacks % stacks_per_seg) != 0)
		{
			// For the last buffer only allocate what we will use
			num_stacks_this_seg = total_stacks % stacks_per_seg;
		}

		// Figure out what range of the sky we are filling
		const U32 begin_stack = i * stacks_per_seg;
		const U32 end_stack = begin_stack + num_stacks_this_seg;
		llassert(end_stack <= total_stacks);

		const U32 num_verts_this_seg = verts_per_stack *
									   (num_stacks_this_seg + 1);
		llassert(num_verts_this_seg <= max_verts);

		const U32 num_indices_this_seg = 1 + num_stacks_this_seg *
										 (2 + 2 * verts_per_stack);
		llassert(num_indices_this_seg * sizeof(U16) <= max_buffer_bytes);

		if (!segment->allocateBuffer(num_verts_this_seg, num_indices_this_seg))
		{
			llwarns << "Failure to allocate a vertex buffer with "
					<< num_verts_this_seg << " vertices and "
					<< num_indices_this_seg << " indices" << llendl;
			return false;
		}

		// Lock the buffer
		bool success = segment->getVertexStrider(vertices) &&
					   segment->getTexCoord0Strider(texcoords) &&
					   segment->getIndexStrider(indices);
		if (!success)
		{
			llwarns << "Failed updating WindLight sky geometry." << llendl;
			return false;
		}

		// Fill it
		buildStripsBuffer(begin_stack, end_stack, vertices, texcoords,
						  indices);

		// And unlock the buffer
		segment->unmapBuffer();
	}

	updateStarColors();
	updateStarGeometry(drawable);

	return true;
}

void LLVOWLSky::drawStars()
{
	// Render the stars as a sphere centered at viewer camera
	if (mStarsVerts.notNull())
	{
		mStarsVerts->setBuffer(LLDrawPoolWLSky::STAR_VERTEX_DATA_MASK);
		mStarsVerts->drawArrays(LLRender::TRIANGLES, 0, STAR_NUM_VERTS * 4);
	}
}

void LLVOWLSky::drawDome()
{
	if (mStripsVerts.empty())
	{
		updateGeometry(mDrawable);
	}

	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);

	U32 total_triangles = 0;
	for (std::vector<LLPointer<LLVertexBuffer> >::const_iterator
			it = mStripsVerts.begin(), end = mStripsVerts.end();
		 it != end; ++it)
	{
		LLVertexBuffer* strips_segment = it->get();

		strips_segment->setBuffer(LLDrawPoolWLSky::SKY_VERTEX_DATA_MASK);
		strips_segment->drawRange(LLRender::TRIANGLE_STRIP, 0,
								  strips_segment->getNumVerts() - 1,
								  strips_segment->getNumIndices(), 0);
		total_triangles += (strips_segment->getNumIndices() + 2) * 3;
	}
	gPipeline.addTrianglesDrawn(total_triangles);

	LLVertexBuffer::unbind();
}

void LLVOWLSky::initStars()
{
	const F32 distance_to_stars = getDomeRadius();

	// Initialize star map
	mStarVertices.resize(STAR_NUM_VERTS);
	mStarColors.resize(STAR_NUM_VERTS);
	mStarIntensities.resize(STAR_NUM_VERTS);

	std::vector<LLVector3>::iterator v_p = mStarVertices.begin();
	std::vector<LLColor4>::iterator v_c = mStarColors.begin();
	std::vector<F32>::iterator v_i = mStarIntensities.begin();

	for (U32 i = 0; i < STAR_NUM_VERTS; ++i)
	{
		v_p->mV[VX] = ll_frand() - 0.5f;
		v_p->mV[VY] = ll_frand() - 0.5f;

		// We only want stars on the top half of the dome !

		v_p->mV[VZ] = ll_frand() * 0.5f;

		v_p->normalize();
		*v_p++ *= distance_to_stars;
		*v_i++ = llmin(powf(ll_frand(), 2.f) + 0.1f, 1.f);
		v_c->mV[VRED]   = 0.75f + ll_frand() * 0.25f;
		v_c->mV[VGREEN] = 1.f;
		v_c->mV[VBLUE]  = 0.75f + ll_frand() * 0.25f;
		v_c->mV[VALPHA] = 1.f;
		(v_c++)->clamp();
	}
}

#if LL_VARIABLE_SKY_DOME_SIZE
//static
F32 LLVOWLSky::getDomeRadius()
{
	// Corresponds as well to the Windlight constant, equal to 15000m
	F32 radius = SKY_DOME_RADIUS;

	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();
	if (skyp)
	{
		// NOTE: this is for now a constant equal to SKY_DOME_RADIUS.
		radius = skyp->getDomeRadius();
	}

	return radius;
}
#endif

void LLVOWLSky::buildStripsBuffer(U32 begin_stack, U32 end_stack,
								  LLStrider<LLVector3>& vertices,
								  LLStrider<LLVector2>& texcoords,
								  LLStrider<U16>& indices)
{
	const F32 radius = getDomeRadius();

	U32 num_slices = getNumSlices();
	U32 num_stacks = getNumStacks();
	llassert(end_stack <= num_stacks);

	// Stacks are iterated one-indexed since phi(0) was handled by the fan
	// above
	for (U32 i = begin_stack + 1; i <= end_stack + 1; ++i)
	{
		F32 phi0 = calcPhi(i);
		F32 sin_phi0 = sinf(phi0);
		F32 scaled_y0 = cosf(phi0) * radius;

		for (U32 j = 0; j < num_slices; ++j)
		{
			F32 theta = F_TWO_PI * (F32)j / (F32)num_slices;

			// Standard transformation from spherical to rectangular
			// coordinates
			F32 x0 = sin_phi0 * cosf(theta);
			F32 z0 = sin_phi0 * sinf(theta);

			if (i == num_stacks - 2)
			{
				*vertices++ = LLVector3(x0 * radius, scaled_y0 - 2048.f,
										z0 * radius);
			}
			else if (i == num_stacks - 1)
			{
				*vertices++ = LLVector3(0.f, scaled_y0 - 2048.f, 0.f);
			}
			else
			{
				*vertices++ = LLVector3(x0 * radius, scaled_y0, z0 * radius);
			}

			// Denerate planar UV coordinates. Note: x and z are transposed in
			// order for things to animate correctly in the global coordinate
			// system where +x is east and +y is north
			*texcoords++ = LLVector2((1.f - z0) * 0.5f, (1.f - x0) * 0.5f);
		}
	}

	// Build triangle strip...
	*indices++ = 0;
	S32 k = 0;
	for (U32 i = 1; i <= end_stack - begin_stack; ++i)
	{
		*indices++ = i * num_slices + k;

		k = (k + 1) % num_slices;
		for (U32 j = 0; j < num_slices; ++j)
		{
			*indices++ = (i - 1) * num_slices + k;
			*indices++ = i * num_slices + k;

			k = (k + 1) % num_slices;
		}

		if (--k < 0)
		{
			k = num_slices - 1;
		}

		*indices++ = i * num_slices + k;
	}
}

void LLVOWLSky::updateStarColors()
{
	std::vector<LLColor4>::iterator v_c = mStarColors.begin();
	std::vector<F32>::iterator v_i = mStarIntensities.begin();
	std::vector<LLVector3>::iterator v_p = mStarVertices.begin();

	constexpr F32 var = 0.15f;
	constexpr F32 min = 0.5f; // 0.75f;

	static S32 swap = 0;
	if (++swap % 2 == 1)
	{
		for (U32 x = 0; x < STAR_NUM_VERTS; ++x)
		{
			LLVector3 tostar = *v_p++;
			tostar.normalize();
			F32 intensity = *v_i++;
			F32 alpha = v_c->mV[VALPHA] + (ll_frand() - 0.5f) * var * intensity;
			if (alpha < min * intensity)
			{
				alpha = min * intensity;
			}
			if (alpha > intensity)
			{
				alpha = intensity;
			}

			alpha = llclamp(alpha, 0.f, 1.f);
			(v_c++)->mV[VALPHA] = alpha;
		}
	}
}

bool LLVOWLSky::updateStarGeometry(LLDrawable* drawable)
{
	if (mStarsVerts.isNull())
	{
		mStarsVerts =
			new LLVertexBuffer(LLDrawPoolWLSky::STAR_VERTEX_DATA_MASK);
#if LL_DEBUG_VB_ALLOC
		mStarsVerts->setOwner("LLVOWLSky stars");
#endif
		if (!mStarsVerts->allocateBuffer(STAR_NUM_VERTS * 6, 0))
		{
			llwarns << "Failure to resize a vertex buffer with "
					<< STAR_NUM_VERTS * 6 << " vertices" << llendl;
			return false;
		}
	}

	LLStrider<LLVector3> verticesp;
	LLStrider<LLColor4U> colorsp;
	LLStrider<LLVector2> texcoordsp;
	bool success = mStarsVerts->getVertexStrider(verticesp) &&
				   mStarsVerts->getColorStrider(colorsp) &&
				   mStarsVerts->getTexCoord0Strider(texcoordsp);
	if (!success)
	{
		llwarns << "Failed updating star geometry." << llendl;
		return false;
	}

	// *TODO: fix LLStrider with a real prefix increment operator so it can be
	// used as a model of OutputIterator. -Brad
	// std::copy(mStarVertices.begin(), mStarVertices.end(), verticesp);

	if (mStarVertices.size() < STAR_NUM_VERTS)
	{
		llwarns_once << "Star reference geometry insufficient." << llendl;
		return false;
	}

	// Texture coordinates:
	static const LLVector2 TEX00 = LLVector2(0.f, 0.f);
	static const LLVector2 TEX01 = LLVector2(0.f, 1.f);
	static const LLVector2 TEX10 = LLVector2(1.f, 0.f);
	static const LLVector2 TEX11 = LLVector2(1.f, 1.f);

	LLVector3 at0, at, left, up;
	LLColor4U col4u;
	for (U32 vtx = 0; vtx < STAR_NUM_VERTS; ++vtx)
	{
		at0 = at = mStarVertices[vtx];
		at.normalize();

		left = at % LLVector3::z_axis;
		up = at % left;

		F32 sc;
		sc = 16.f + ll_frand() * 20.f;
		left *= sc;
		up *= sc;

		*verticesp++ = at0;
		*verticesp++ = at0 + up;
		*verticesp++ = at0 + left + up;
		*verticesp++ = at0;
		*verticesp++ = at0 + left + up;
		*verticesp++ = at0 + left;

		*texcoordsp++ = TEX10;
		*texcoordsp++ = TEX11;
		*texcoordsp++ = TEX01;
		*texcoordsp++ = TEX10;
		*texcoordsp++ = TEX01;
		*texcoordsp++ = TEX00;

		col4u = LLColor4U(mStarColors[vtx]);
		*colorsp++ = col4u;
		*colorsp++ = col4u;
		*colorsp++ = col4u;
		*colorsp++ = col4u;
		*colorsp++ = col4u;
		*colorsp++ = col4u;
	}

	mStarsVerts->unmapBuffer();
	return true;
}
