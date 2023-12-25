/**
 * @file llvopartgroup.cpp
 * @brief Group of particle systems
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

#include "llvopartgroup.h"

#include "llfasttimer.h"
#include "llmessage.h"

#include "llagent.h"				// For getCameraPositionAgent()
#include "llappviewer.h"			// For gFrameTimeSeconds
#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerpartsim.h"
#include "llvoclouds.h"

// Some tuned constant, limits on how much particle area to draw
constexpr F32 MAX_PARTICLE_AREA_SCALE = 0.02f;

extern U64 gFrameTime;

///////////////////////////////////////////////////////////////////////////////
// LLVOPartGroup class
///////////////////////////////////////////////////////////////////////////////

LLVOPartGroup::LLVOPartGroup(const LLUUID& id, LLViewerRegion* regionp,
							 LLPCode pcode)
:	LLAlphaObject(id, pcode, regionp),
	mViewerPartGroupp(NULL)
{
	setNumTEs(1);
	setTETexture(0, LLUUID::null);
	// Users cannot select particle systems
	mCanSelect = false;
}

F32 LLVOPartGroup::getBinRadius()
{
	return mViewerPartGroupp->getBoxSide();
}

void LLVOPartGroup::updateSpatialExtents(LLVector4a& new_min,
										 LLVector4a& new_max)
{
	const LLVector3& pos_agent = getPositionAgent();
	LLVector4a p;
	p.load3(pos_agent.mV);

	LLVector4a scale;
	scale.splat(mScale.mV[0] + mViewerPartGroupp->getBoxSide() * 0.5f);

	new_min.setSub(p, scale);
	new_max.setAdd(p, scale);
	llassert(new_min.isFinite3() && new_max.isFinite3() && p.isFinite3());

	mDrawable->setPositionGroup(p);
}

void LLVOPartGroup::setPixelAreaAndAngle()
{
	// mPixelArea is calculated during render
	F32 mid_scale = getMidScale();
	F32 range = (getRenderPosition() - gViewerCamera.getOrigin()).length();

	if (range < 0.001f || isHUDAttachment())		// range == zero
	{
		mAppAngle = 180.f;
	}
	else
	{
		mAppAngle = atan2f(mid_scale, range) * RAD_TO_DEG;
	}
}

LLDrawable* LLVOPartGroup::createDrawable()
{
	gPipeline.allocDrawable(this);
	mDrawable->setLit(false);
	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_PARTICLES);
	return mDrawable;
}

const LLUUID& LLVOPartGroup::getPartOwner(S32 idx)
{
	if (idx >= 0 && idx < (S32)mViewerPartGroupp->mParticles.size())
	{
		LLViewerPart* part = mViewerPartGroupp->mParticles[idx];
		if (part && part->mPartSourcep.notNull())
		{
			return part->mPartSourcep->getOwnerUUID();
		}
	}
	return LLUUID::null;
}

const LLUUID& LLVOPartGroup::getPartSource(S32 idx)
{
	if (idx >= 0 && idx < (S32)mViewerPartGroupp->mParticles.size())
	{
		LLViewerPart* part = mViewerPartGroupp->mParticles[idx];
		if (part)
		{
			LLViewerPartSource* psrc = part->mPartSourcep.get();
			if (psrc)
			{
				LLViewerObject* objp = psrc->mSourceObjectp.get();
				if (objp)
				{
					return objp->getID();
				}
			}
		}
	}
	return LLUUID::null;
}

F32 LLVOPartGroup::getPartSize(S32 idx)
{
	if (idx >= 0 && idx < (S32)mViewerPartGroupp->mParticles.size())
	{
		LLViewerPart* part = mViewerPartGroupp->mParticles[idx];
		if (part)
		{
			return part->mScale.mV[0];
		}
	}

	return 0.f;
}

//virtual
bool LLVOPartGroup::getBlendFunc(S32 face, U32& src, U32& dst)
{
	if (face < 0 || face >= (S32)mViewerPartGroupp->mParticles.size())
	{
		LL_DEBUGS_SPARSE("Particles") << "Index out of range for mParticles size"
									  << LL_ENDL;
		return false;
	}

	LLViewerPart* part = mViewerPartGroupp->mParticles[face];
	src = part->mBlendFuncSource;
	dst = part->mBlendFuncDest;
	return true;
}

//virtual
const LLVector3& LLVOPartGroup::getCameraPosition() const
{
	return gAgent.getCameraPositionAgent();
}

bool LLVOPartGroup::updateGeometry(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_UPDATE_PARTICLES);

	S32 num_parts = mViewerPartGroupp->getCount();
	LLSpatialGroup* group = drawable->getSpatialGroup();
	if (!group && num_parts)
	{
		drawable->movePartition();
		group = drawable->getSpatialGroup();
	}

	if (group && group->isVisible())
	{
		dirtySpatialGroup();
	}

	if (!num_parts)
	{
		if (group && drawable->getNumFaces())
		{
			group->setState(LLSpatialGroup::GEOM_DIRTY);
		}
		drawable->setNumFaces(0, NULL, getTEImage(0));
		return true;
	}

 	if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES))
	{
		return true;
	}

	if (num_parts > drawable->getNumFaces())
	{
		drawable->setNumFacesFast(num_parts + num_parts / 4, NULL,
								  getTEImage(0));
	}

	F32 tot_area = 0;

	F32 max_area = LLViewerPartSim::getMaxPartCount() *
				   MAX_PARTICLE_AREA_SCALE;
	F32 pixel_meter_ratio = gViewerCamera.getPixelMeterRatio();
	pixel_meter_ratio *= pixel_meter_ratio;

	F32 max_scale = 0.f;
	S32 count = 0;
	mDepth = 0.f;
	LLVector3 camera_agent = getCameraPosition();
	U32 part_count = mViewerPartGroupp->mParticles.size();
#if LL_DEBUG
	LLViewerPartSim::checkParticleCount(part_count);
#endif
	for (U32 i = 0; i < part_count; ++i)
	{
		const LLViewerPart* partp = mViewerPartGroupp->mParticles[i];

		// Remember the largest particle
		max_scale = llmax(max_scale, partp->mScale.mV[0], partp->mScale.mV[1]);

		if (partp->mFlags & LLPartData::LL_PART_RIBBON_MASK)
		{
			// Include ribbon segment length in scale
			const LLVector3* pos_agent = NULL;
			if (partp->mParent)
			{
				pos_agent = &(partp->mParent->mPosAgent);
			}
			else if (partp->mPartSourcep.notNull())
			{
				pos_agent = &(partp->mPartSourcep->mPosAgent);
			}

			if (pos_agent)
			{
				F32 dist = (*pos_agent-partp->mPosAgent).length();

				max_scale = llmax(max_scale, dist);
			}
		}

		LLVector3 part_pos_agent(partp->mPosAgent);
		LLVector3 at(part_pos_agent - camera_agent);

		F32 camera_dist_squared = at.lengthSquared();
		F32 inv_camera_dist_squared;
		if (camera_dist_squared > 1.f)
		{
			inv_camera_dist_squared = 1.f / camera_dist_squared;
		}
		else
		{
			inv_camera_dist_squared = 1.f;
		}
		F32 area = partp->mScale.mV[0] * partp->mScale.mV[1] *
				   inv_camera_dist_squared;
		tot_area = llmax(tot_area, area);

		if (tot_area > max_area)
		{
			break;
		}

		++count;

		LLFace* facep = drawable->getFace(i);
		if (!facep)
		{
			continue;
		}

		facep->setTEOffset(i);
		// Only discard particles > 5 m from the camera
		constexpr F32 NEAR_PART_DIST_SQ = 5.f * 5.f;
		// Only less than 5 mm x 5 mm at 1 m from camera
		constexpr F32 MIN_PART_AREA = .005f * .005f;

		if (camera_dist_squared > NEAR_PART_DIST_SQ && area < MIN_PART_AREA)
		{
			facep->setSize(0, 0);
			continue;
		}

		facep->setSize(4, 6);

		facep->setViewerObject(this);

		if (partp->mFlags & LLPartData::LL_PART_EMISSIVE_MASK)
		{
			facep->setState(LLFace::FULLBRIGHT);
		}
		else
		{
			facep->clearState(LLFace::FULLBRIGHT);
		}

		facep->mCenterLocal = partp->mPosAgent;
		facep->setFaceColor(partp->mColor);

		LLViewerTexture* texp = partp->mImagep.get();
		facep->setDiffuseMap(texp);
		// Check if this particle texture is replaced by a parcel media texture
		if (texp && texp->hasParcelMedia())
		{
			texp->getParcelMedia()->addMediaToFace(facep);
		}

		mPixelArea = tot_area * pixel_meter_ratio;
		// Scale area to increase priority a bit
		constexpr F32 area_scale = 10.f;
		facep->setVirtualSize(mPixelArea * area_scale);
	}
	for (S32 i = count, faces = drawable->getNumFaces(); i < faces; ++i)
	{
		LLFace* facep = drawable->getFace(i);
		if (facep)
		{
			facep->setTEOffset(i);
			facep->setSize(0, 0);
		}
	}

	// Record max scale (used to stretch bounding box for visibility culling)
	mScale.set(max_scale, max_scale, max_scale);

	mDrawable->movePartition();

	return true;
}

bool LLVOPartGroup::lineSegmentIntersect(const LLVector4a& start,
										 const LLVector4a& end,
										 S32 face, bool pick_transparent,
										 bool pick_rigged, S32* face_hit,
										 LLVector4a* intersection,
										 LLVector2* tex_coord,
										 LLVector4a* normal,
										 LLVector4a* tangent)
{
	LLVector4a dir;
	dir.setSub(end, start);

	F32 closest_t = 2.f;
	bool ret = false;

	LLVector4a v[4];
	LLStrider<LLVector4a> verticesp;

	for (U32 idx = 0, count = mViewerPartGroupp->mParticles.size();
		 idx < count; ++idx)
	{
		const LLViewerPart& part =
			*((LLViewerPart*)mViewerPartGroupp->mParticles[idx]);

		verticesp = v;
		getGeometry(part, verticesp);

		F32 a, b, t;
		if (LLTriangleRayIntersect(v[0], v[1], v[2], start, dir, a, b, t) ||
			LLTriangleRayIntersect(v[1], v[3], v[2], start, dir, a, b, t))
		{
			if (t >= 0.f && t <= 1.f && t < closest_t)
			{
				ret = true;
				closest_t = t;
				if (face_hit)
				{
					*face_hit = idx;
				}

				if (intersection)
				{
					LLVector4a intersect = dir;
					intersect.mul(closest_t);
					intersection->setAdd(intersect, start);
				}
			}
		}
	}

	return ret;
}

void LLVOPartGroup::getGeometry(const LLViewerPart& part,
								LLStrider<LLVector4a>& verticesp)
{
	if (part.mFlags & LLPartData::LL_PART_RIBBON_MASK)
	{
		LLVector4a axis, pos, paxis, ppos;
		F32 scale, pscale;

		pos.load3(part.mPosAgent.mV);
		axis.load3(part.mAxis.mV);
		scale = part.mScale.mV[0];

		if (part.mParent)
		{
			ppos.load3(part.mParent->mPosAgent.mV);
			paxis.load3(part.mParent->mAxis.mV);
			pscale = part.mParent->mScale.mV[0];
		}
		else if (part.mPartSourcep->mSourceObjectp.notNull())
		{
			// Use source object as position
			LLVector3 v =
				LLVector3::z_axis *
				part.mPartSourcep->mSourceObjectp->getRenderRotation();
			paxis.load3(v.mV);
			ppos.load3(part.mPartSourcep->mPosAgent.mV);
			pscale = part.mStartScale.mV[0];
		}
		else
		{
			// No source object, no parent, nothing to draw
			ppos = pos;
			pscale = scale;
			paxis = axis;
		}

		LLVector4a p0, p1, p2, p3;

		scale *= 0.5f;
		pscale *= 0.5f;

		axis.mul(scale);
		paxis.mul(pscale);

		p0.setAdd(pos, axis);
		p1.setSub(pos,axis);
		p2.setAdd(ppos, paxis);
		p3.setSub(ppos, paxis);

		*verticesp++ = p2;
		*verticesp++ = p3;
		*verticesp++ = p0;
		*verticesp++ = p1;
	}
	else
	{
		LLVector4a part_pos_agent;
		part_pos_agent.load3(part.mPosAgent.mV);
		LLVector4a camera_agent;
		camera_agent.load3(getCameraPosition().mV);
		LLVector4a at;
		at.setSub(part_pos_agent, camera_agent);
		LLVector4a up(0, 0, 1);
		LLVector4a right;

		right.setCross3(at, up);
		right.normalize3fast();
		up.setCross3(right, at);
		up.normalize3fast();

		if ((part.mFlags & LLPartData::LL_PART_FOLLOW_VELOCITY_MASK) &&
			!part.mVelocity.isExactlyZero())
		{
			LLVector4a normvel;
			normvel.load3(part.mVelocity.mV);
			normvel.normalize3fast();
			LLVector2 up_fracs;
			up_fracs.mV[0] = normvel.dot3(right).getF32();
			up_fracs.mV[1] = normvel.dot3(up).getF32();
			up_fracs.normalize();
			LLVector4a new_up;
			LLVector4a new_right;

#if 0
			new_up = up_fracs.mV[0] * right + up_fracs.mV[1] * up;
#endif
			LLVector4a t = right;
			t.mul(up_fracs.mV[0]);
			new_up = up;
			new_up.mul(up_fracs.mV[1]);
			new_up.add(t);

#if 0
			new_right = up_fracs.mV[1] * right - up_fracs.mV[0] * up;
#endif
			t = right;
			t.mul(up_fracs.mV[1]);
			new_right = up;
			new_right.mul(up_fracs.mV[0]);
			t.sub(new_right);

			up = new_up;
			right = t;
			up.normalize3fast();
			right.normalize3fast();
		}

		right.mul(0.5f * part.mScale.mV[0]);
		up.mul(0.5f * part.mScale.mV[1]);

		// *HACK: the verticesp->mV[3] = 0.f here are to set the texture index
		// to 0 (particles do not use texture batching, maybe they should) this
		// works because there is actually a 4th float stored after the vertex
		// position which is used as a texture index also, somebody please
		// VECTORIZE THIS

		LLVector4a ppapu;
		LLVector4a ppamu;

		ppapu.setAdd(part_pos_agent, up);
		ppamu.setSub(part_pos_agent, up);

		verticesp->setSub(ppapu, right);
		verticesp++->getF32ptr()[3] = 0.f;
		verticesp->setSub(ppamu, right);
		verticesp++->getF32ptr()[3] = 0.f;
		verticesp->setAdd(ppapu, right);
		verticesp++->getF32ptr()[3] = 0.f;
		verticesp->setAdd(ppamu, right);
		verticesp++->getF32ptr()[3] = 0.f;
	}
}

void LLVOPartGroup::getGeometry(S32 idx, LLStrider<LLVector4a>& verticesp,
								LLStrider<LLVector3>& normalsp,
								LLStrider<LLVector2>& texcoordsp,
								LLStrider<LLColor4U>& colorsp,
								LLStrider<LLColor4U>& emissivep,
								LLStrider<U16>& indicesp)
{
	if (idx < 0 || idx >= (S32)mViewerPartGroupp->mParticles.size())
	{
		return;
	}

	const LLViewerPart& part =
		*((LLViewerPart*)(mViewerPartGroupp->mParticles[idx]));

	getGeometry(part, verticesp);

	LLColor4U color = LLColor4U(part.mColor);

	LLColor4U pglow, pcolor;
	if (part.mFlags & LLPartData::LL_PART_RIBBON_MASK)
	{
		// Make sure color blends properly
		if (part.mParent)
		{
			pglow = part.mParent->mGlow;
			pcolor = LLColor4U(part.mParent->mColor);
		}
		else
		{
			pglow = LLColor4U(0, 0, 0, (U8)ll_roundp(255.f * part.mStartGlow));
			pcolor = LLColor4U(part.mStartColor);
		}
	}
	else
	{
		pglow = part.mGlow;
		pcolor = color;
	}

	*colorsp++ = pcolor;
	*colorsp++ = pcolor;
	*colorsp++ = color;
	*colorsp++ = color;

#if 1
	if ((pglow.mV[3] > 0 || part.mGlow.mV[3] > 0) && gPipeline.shadersLoaded())
#endif
	{
		*emissivep++ = pglow;
		*emissivep++ = pglow;
		*emissivep++ = part.mGlow;
		*emissivep++ = part.mGlow;
	}

	if (!(part.mFlags & LLPartData::LL_PART_EMISSIVE_MASK))
	{
		// Not fullbright, needs normal
		LLVector3 normal = -gViewerCamera.getXAxis();
		*normalsp++ = normal;
		*normalsp++ = normal;
		*normalsp++ = normal;
		*normalsp++ = normal;
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLParticlePartition class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLParticlePartition::LLParticlePartition(LLViewerRegion* regionp)
:	LLSpatialPartition(LLDrawPoolAlpha::VERTEX_DATA_MASK |
					   LLVertexBuffer::MAP_TEXTURE_INDEX,
					   true, regionp)
{
	mRenderPass = LLRenderPass::PASS_ALPHA;
	mDrawableType = LLPipeline::RENDER_TYPE_PARTICLES;
	mPartitionType = LLViewerRegion::PARTITION_PARTICLE;
	mSlopRatio = 0.f;
	mLODPeriod = 1;
}

LLHUDParticlePartition::LLHUDParticlePartition(LLViewerRegion* regionp)
:	LLParticlePartition(regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_HUD_PARTICLES;
	mPartitionType = LLViewerRegion::PARTITION_HUD_PARTICLE;
}

//static
bool LLParticlePartition::createVB(LLPointer<LLVertexBuffer>& vb,
								   U32 vert_count, U32 idx_count)
{
	constexpr U32 VERTEX_DATA_MASK = LLVertexBuffer::MAP_VERTEX |
									 LLVertexBuffer::MAP_TANGENT |
									 LLVertexBuffer::MAP_NORMAL |
									 LLVertexBuffer::MAP_TEXCOORD0 |
									 LLVertexBuffer::MAP_TEXCOORD1 |
									 LLVertexBuffer::MAP_TEXCOORD2 |
									 LLVertexBuffer::MAP_COLOR |
									 LLVertexBuffer::MAP_EMISSIVE |
									 LLVertexBuffer::MAP_TEXTURE_INDEX;
	vb = new LLVertexBuffer(VERTEX_DATA_MASK);
	if (!vb->allocateBuffer(vert_count, idx_count))
	{
		llwarns << "Failure to allocate a vertex buffer with "
				<< vert_count << " vertices and "
				<< idx_count << " indices" << llendl;
		return false;	// Abort
	}

	// Initialize index and texture coordinates only when buffer is reallocated
	U16* indicesp = (U16*)vb->mapIndexBuffer(0, idx_count);
	U16 geom_idx = 0;
	for (U32 i = 0; i < idx_count; i += 6)
	{
		*indicesp++ = geom_idx;
		*indicesp++ = geom_idx + 1;
		*indicesp++ = geom_idx + 2;

		*indicesp++ = geom_idx + 1;
		*indicesp++ = geom_idx + 3;
		*indicesp++ = geom_idx + 2;

		geom_idx += 4;
	}

	LLStrider<LLVector2> texcoordsp;
	if (!vb->getTexCoord0Strider(texcoordsp))
	{
		return false;	// Abort
	}

	for (U32 i = 0; i < vert_count; i += 4)
	{
		*texcoordsp++ = LLVector2(0.f, 1.f);
		*texcoordsp++ = LLVector2(0.f, 0.f);
		*texcoordsp++ = LLVector2(1.f, 1.f);
		*texcoordsp++ = LLVector2(1.f, 0.f);
	}

	return true;	// Success
}

//virtual
void LLParticlePartition::rebuildGeom(LLSpatialGroup* group)
{
	if (group->isDead() || !group->hasState(LLSpatialGroup::GEOM_DIRTY))
	{
		return;
	}

	if (group->changeLOD())
	{
		group->mLastUpdateDistance = group->mDistance;
		group->mLastUpdateViewAngle = group->mViewAngle;
	}

	LL_FAST_TIMER(FTM_REBUILD_PARTICLE_VBO);

	group->clearDrawMap();

	// Get geometry count
	U32 idx_count = 0;
	U32 vert_count = 0;
	addGeometryCount(group, vert_count, idx_count);
	bool has_vb = vert_count > 0 && idx_count > 0;
	if (has_vb &&
		(group->mVertexBuffer.isNull() ||
		 group->mVertexBuffer->getNumVerts() < vert_count ||
		 group->mVertexBuffer->getNumIndices() < idx_count))
	{
		has_vb = createVB(group->mVertexBuffer, vert_count, idx_count);
	}
	if (has_vb)
	{
		group->mBuilt = 1.f;
		getGeometry(group);
	}
	else
	{
		group->mVertexBuffer = NULL;
		group->mBufferMap.clear();
	}

	group->mLastUpdateTime = gFrameTimeSeconds;
	group->clearState(LLSpatialGroup::GEOM_DIRTY);
}

//virtual
void LLParticlePartition::addGeometryCount(LLSpatialGroup* group,
										   U32& vertex_count,
										   U32& index_count)
{
	mFaceList.clear();

	const LLVector3 camera_at_axis = gViewerCamera.getAtAxis();
	const LLVector3 camera_origin = gViewerCamera.getOrigin();
	for (LLSpatialGroup::element_iter i = group->getDataBegin(),
									  end = group->getDataEnd();
		 i != end; ++i)
	{
		LLDrawable* drawablep = (LLDrawable*)(*i)->getDrawable();
		if (!drawablep || drawablep->isDead())
		{
			continue;
		}

		LLAlphaObject* obj = drawablep->getVObj().get()->asAlphaObject();
		if (!obj)
		{
			llwarns_once << "Not an alpha object for drawable " << std::hex
						 << drawablep << std::dec << llendl;
			continue;
		}

		obj->mDepth = 0.f;

		U32 count = 0;
		for (S32 j = 0, faces = drawablep->getNumFaces(); j < faces; ++j)
		{
			drawablep->updateFaceSize(j);

			LLFace* facep = drawablep->getFace(j);
			if (!facep || !facep->hasGeometry())
			{
				continue;
			}

			vertex_count += facep->getGeomCount();
			index_count += facep->getIndicesCount();

			++count;
			facep->mDistance = (facep->mCenterLocal - camera_origin) *
							   camera_at_axis;
			obj->mDepth += facep->mDistance;

			mFaceList.push_back(facep);
			llassert(facep->getIndicesCount() < 65536);
		}

		obj->mDepth /= count;
	}
}

//virtual
void LLParticlePartition::getGeometry(LLSpatialGroup* group)
{
	LL_FAST_TIMER(FTM_REBUILD_PARTICLE_GEOM);

	std::sort(mFaceList.begin(), mFaceList.end(),
			  LLFace::CompareDistanceGreater());

	group->clearDrawMap();

	LLVertexBuffer* buffer = group->mVertexBuffer.get();
	if (!buffer)
	{
		return;
	}

	LLStrider<LLVector4a> verticesp;
	LLStrider<LLVector3> normalsp;
	LLStrider<LLColor4U> colorsp;
	LLStrider<LLColor4U> emissivep;
	if (!buffer->getVertexStrider(verticesp) ||
		!buffer->getNormalStrider(normalsp) ||
		!buffer->getColorStrider(colorsp) ||
		!buffer->getEmissiveStrider(emissivep))
	{
		return;
	}

	S32 geom_idx = 0;
	S32 indices_idx = 0;

	LLSpatialGroup::drawmap_elem_t& draw_vec = group->mDrawMap[mRenderPass];
	for (U32 i = 0, count = mFaceList.size(); i < count; ++i)
	{
		LLFace* facep = mFaceList[i];

		LLAlphaObject* object = facep->getViewerObject()->asAlphaObject();
		if (!object)
		{
			llwarns_once << "Not an alpha object for face " << std::hex
						 << facep << std::dec << llendl;
			continue;
		}

		facep->setGeomIndex(geom_idx);
		facep->setIndicesIndex(indices_idx);

		LLStrider<LLVector4a> cur_vert = verticesp + geom_idx;
		LLStrider<LLVector3> cur_norm = normalsp + geom_idx;
		LLStrider<LLColor4U> cur_col = colorsp + geom_idx;
		LLStrider<LLColor4U> cur_glow = emissivep + geom_idx;

		// Not actually used
		LLStrider<LLVector2> cur_tc;
		LLStrider<U16> cur_idx;
		geom_idx += 4;
		indices_idx += 6;

		LLColor4U* start_glow = cur_glow.get();
		object->getGeometry(facep->getTEOffset(), cur_vert, cur_norm, cur_tc,
							cur_col, cur_glow, cur_idx);
		bool has_glow = cur_glow.get() != start_glow;

		llassert(facep->getGeomCount() == 4 && facep->getIndicesCount() == 6);

		S32 idx = draw_vec.size() - 1;

		bool fullbright = facep->isState(LLFace::FULLBRIGHT);
		F32 vsize = facep->getVirtualSize();

		U32 bf_src = LLRender::BF_SOURCE_ALPHA;
		U32 bf_dst = LLRender::BF_ONE_MINUS_SOURCE_ALPHA;
		if (!object->getBlendFunc(facep->getTEOffset(), bf_src, bf_dst))
		{
			continue;
		}

		bool batched = false;

		if (idx >= 0)
		{
			LLDrawInfo* info = draw_vec[idx];

			if (info->mTexture == facep->getTexture() &&
				info->mHasGlow == has_glow &&
				info->mFullbright == fullbright &&
				info->mBlendFuncDst == bf_dst &&
				info->mBlendFuncSrc == bf_src)
			{
				if (info->mEnd == facep->getGeomIndex() - 1)
				{
					batched = true;
					info->mCount += facep->getIndicesCount();
					info->mEnd += facep->getGeomCount();
					info->mVSize = llmax(info->mVSize, vsize);
				}
				else if (info->mStart == facep->getGeomIndex() +
										 facep->getGeomCount() + 1)
				{
					batched = true;
					info->mCount += facep->getIndicesCount();
					info->mStart -= facep->getGeomCount();
					info->mOffset = facep->getIndicesStart();
					info->mVSize = llmax(info->mVSize, vsize);
				}
			}
		}

		if (!batched)
		{
			U32 start = facep->getGeomIndex();
			U32 end = start + facep->getGeomCount() - 1;
			U32 offset = facep->getIndicesStart();
			U32 count = facep->getIndicesCount();
			LLDrawInfo* info = new LLDrawInfo(start, end, count, offset,
											  facep->getTexture(), buffer,
											  fullbright);
			const LLVector4a* exts = group->getObjectExtents();
			info->mExtents[0] = exts[0];
			info->mExtents[1] = exts[1];
			info->mVSize = vsize;
			info->mBlendFuncDst = bf_dst;
			info->mBlendFuncSrc = bf_src;
			info->mHasGlow = has_glow;
			draw_vec.emplace_back(info);
			// For alpha sorting
			facep->setDrawInfo(info);
		}
	}

	buffer->unmapBuffer();
	mFaceList.clear();
}

LLDrawable* LLVOHUDPartGroup::createDrawable()
{
	gPipeline.allocDrawable(this);
	mDrawable->setLit(false);
	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
	return mDrawable;
}
