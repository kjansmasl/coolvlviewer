/**
 * @file llface.cpp
 * @brief LLFace class implementation
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

#include "llface.h"

#include "llfasttimer.h"
#include "llgl.h"
#include "llmatrix4a.h"
#include "llrender.h"
#include "llvolume.h"

#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertextureanim.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoclouds.h"
#include "llvopartgroup.h"
#include "llvosky.h"
#include "llvovolume.h"

// gcc 12+ sees uninitialized LLVector4a's where there are none... HB
#if defined(GCC_VERSION) && GCC_VERSION >= 120000
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#define LL_MAX_INDICES_COUNT 1000000

static LLStaticHashedString sTextureIndexIn("texture_index_in");
static LLStaticHashedString sColorIn("color_in");

#define DOTVEC(a,b) (a.mV[0]*b.mV[0] + a.mV[1]*b.mV[1] + a.mV[2]*b.mV[2])

/*
For each vertex, given:
	B - binormal
	T - tangent
	N - normal
	P - position

The resulting texture coordinate <u,v> is:

	u = 2(B dot P)
	v = 2(T dot P)
*/
void planarProjection(LLVector2& tc, const LLVector4a& normal,
					  const LLVector4a& center, const LLVector4a& vec)
{
	LLVector4a binormal;
	F32 d = normal[0];
	if (d <= -0.5f)
	{
		binormal.set(0.f, -1.f, 0.f);
	}
	else if (d >= 0.5f)
	{
		binormal.set(0.f, 1.f, 0.f);
	}
	else if (normal[1] > 0.f)
	{
		binormal.set(-1.f, 0.f, 0.f);
	}
	else
	{
		binormal.set(1.f, 0.f, 0.f);
	}

	LLVector4a tangent;
	tangent.setCross3(binormal, normal);

	tc.mV[1] = -2.f * tangent.dot3(vec).getF32() + 0.5f;
	tc.mV[0] = 2.f * binormal.dot3(vec).getF32() + 0.5f;
}

void LLFace::init(LLDrawable* drawablep, LLViewerObject* objp)
{
	mLastUpdateTime = gFrameTimeSeconds;
	mLastMoveTime = 0.f;
	mLastSkinTime = gFrameTimeSeconds;
	mVSize = 0.f;
	mPixelArea = 16.f;
	mState = GLOBAL;
	mDrawOrderIndex = 0;
	mDrawPoolp = NULL;
	mPoolType = 0;
	mCenterLocal = objp->getPosition();
	mCenterAgent = drawablep->getPositionAgent();
	mDistance = 0.f;

	mGeomCount = 0;
	mGeomIndex = 0;
	mIndicesCount = 0;

	// Special value to indicate uninitialized position
	mIndicesIndex = 0xFFFFFFFF;

	for (U32 i = 0; i < LLRender::NUM_TEXTURE_CHANNELS; ++i)
	{
		mIndexInTex[i] = 0;
		mTexture[i] = NULL;
	}

	mTEOffset = -1;
	mTextureIndex = FACE_DO_NOT_BATCH_TEXTURES;

	setDrawable(drawablep);
	mVObjp = objp;

	mReferenceIndex = -1;

	mTextureMatrix = NULL;
	mDrawInfo = NULL;
	mAvatar = NULL;

	mFaceColor = LLColor4(1.f, 0.f, 0.f, 1.f);

	mImportanceToCamera = 0.f;
	mBoundingSphereRadius = 0.f;

	mHasMedia = false;
	mIsMediaAllowed = true;
}

void LLFace::destroy()
{
#if LL_DEBUG
	if (gDebugGL)
	{
		gPipeline.checkReferences(this);
	}
#endif

	for (U32 i = 0; i < LLRender::NUM_TEXTURE_CHANNELS; ++i)
	{
		if (mTexture[i].notNull())
		{
			mTexture[i]->removeFace(i, this);
		}
	}

	if (mDrawPoolp)
	{
		mDrawPoolp->removeFace(this);
		mDrawPoolp = NULL;
	}

	if (mTextureMatrix)
	{
		delete mTextureMatrix;
		mTextureMatrix = NULL;

		if (mDrawablep.notNull())
		{
			LLSpatialGroup* group = mDrawablep->getSpatialGroup();
			if (group)
			{
				group->dirtyGeom();
				gPipeline.markRebuild(group);
			}
		}
	}

	mDrawInfo = NULL;
	mDrawablep = NULL;
	mVObjp = NULL;
}

void LLFace::setPool(LLFacePool* poolp, LLViewerTexture* texp)
{
	if (!poolp)
	{
		llerrs << "Setting pool to null !" << llendl;
	}

	if (poolp != mDrawPoolp)
	{
		// Remove from old pool
		if (mDrawPoolp)
		{
			mDrawPoolp->removeFace(this);

			if (mDrawablep)
			{
				gPipeline.markRebuild(mDrawablep);
			}
		}
		mGeomIndex = 0;

		// Add to new pool
		if (poolp)
		{
			poolp->addFace(this);
		}
		mDrawPoolp = poolp;
	}

	setDiffuseMap(texp);
}

void LLFace::setTexture(U32 ch, LLViewerTexture* texp)
{
	llassert(ch < LLRender::NUM_TEXTURE_CHANNELS);

	if (mTexture[ch] == texp)
	{
		return;
	}

	if (mTexture[ch].notNull())
	{
		mTexture[ch]->removeFace(ch, this);
	}

	if (texp)
	{
		texp->addFace(ch, this);
	}

	mTexture[ch] = texp;
}

void LLFace::dirtyTexture()
{
	LLDrawable* drawablep = getDrawable();
	if (!drawablep)
	{
		return;
	}

	if (mVObjp.notNull() && mVObjp->getVolume())
	{
		LLVOVolume* vobj = drawablep->getVOVolume();
		bool mark_rebuild = false;
		bool update_complexity = false;
		for (U32 ch = 0; ch < LLRender::NUM_TEXTURE_CHANNELS; ++ch)
		{
			if (mTexture[ch].notNull() && mTexture[ch]->getComponents() == 4)
			{
				mark_rebuild = true;
				// Dirty texture on an alpha object should be treated as an LoD
				// update
				if (vobj)
				{
					vobj->mLODChanged = true;
					// If vobj is an avatar, its render complexity may have
					// changed
					update_complexity = true;
				}
			}
		}
		if (mark_rebuild)
		{
			gPipeline.markRebuild(drawablep, LLDrawable::REBUILD_VOLUME);
		}
		if (update_complexity)
		{
			vobj->updateVisualComplexity();
		}
	}

	gPipeline.markTextured(drawablep);
}

#if LL_FIX_MAT_TRANSPARENCY
void LLFace::notifyAboutCreatingTexture(LLViewerTexture* texp)
{
	LLDrawable* drawablep = getDrawable();
	if (drawablep && mVObjp.notNull() && mVObjp->getVolume())
	{
		LLVOVolume* vobj = drawablep->getVOVolume();
		if (vobj && vobj->notifyAboutCreatingTexture(texp))
		{
			gPipeline.markTextured(drawablep);
			gPipeline.markRebuild(drawablep, LLDrawable::REBUILD_VOLUME);
		}
	}
}

void LLFace::notifyAboutMissingAsset(LLViewerTexture* texp)
{
	LLDrawable* drawablep = getDrawable();
	if (drawablep && mVObjp.notNull() && mVObjp->getVolume())
	{
		LLVOVolume* vobj = drawablep->getVOVolume();
		if (vobj && vobj->notifyAboutMissingAsset(texp))
		{
			gPipeline.markTextured(drawablep);
			gPipeline.markRebuild(drawablep, LLDrawable::REBUILD_VOLUME);
		}
	}
}
#endif

void LLFace::switchTexture(U32 ch, LLViewerTexture* texp)
{
	llassert(ch < LLRender::NUM_TEXTURE_CHANNELS);

	if (mTexture[ch] == texp)
	{
		return;
	}

	if (!texp)
	{
		llerrs << "Cannot switch to a null texture." << llendl;
		return;
	}

	if (mTexture[ch].notNull())
	{
		texp->addTextureStats(mTexture[ch]->getMaxVirtualSize());
	}

	if (ch == LLRender::DIFFUSE_MAP)
	{
		LLViewerObject* objp = getViewerObject();
		if (objp)
		{
			objp->changeTEImage(mTEOffset, texp);
		}
	}

	setTexture(ch, texp);
	dirtyTexture();
}

void LLFace::switchDiffuseTex(const LLUUID& tex_id)
{
	LLPointer<LLViewerTexture>& diff_texp = mTexture[LLRender::DIFFUSE_MAP];
	if (diff_texp.notNull() && diff_texp->getID() == tex_id)
	{
		return;
	}

	// Make sure the texture will be fetched if not yet in memory.
	LLPointer<LLViewerFetchedTexture> texp =
		LLViewerTextureManager::getFetchedTexture(tex_id, FTT_DEFAULT, true,
												  LLGLTexture::BOOST_NONE,
												  LLViewerTexture::LOD_TEXTURE);
	if (diff_texp.notNull())
	{
		texp->addTextureStats(diff_texp->getMaxVirtualSize());
	}
	else
	{
		texp->addTextureStats(256.f * 256.f);
	}

	LLViewerObject* objp = getViewerObject();
	if (objp)
	{
		objp->changeTEImage(mTEOffset, texp);
	}

	setTexture(LLRender::DIFFUSE_MAP, texp);
	dirtyTexture();
}

void LLFace::setDrawable(LLDrawable* drawablep)
{
	mDrawablep = drawablep;
	mXform = &drawablep->mXform;
}

void LLFace::setSize(U32 num_vertices, U32 num_indices, bool align)
{
	if (align)
	{
		// Allocate vertices in blocks of 4 for alignment
		num_vertices = (num_vertices + 0x3) & ~0x3;
	}

	if (mGeomCount != num_vertices || mIndicesCount != num_indices)
	{
		mGeomCount = num_vertices;
		mIndicesCount = num_indices;
		mVertexBuffer = NULL;
	}

	llassert(verify());
}

void LLFace::setGeomIndex(U16 idx)
{
	if (mGeomIndex != idx)
	{
		mGeomIndex = idx;
		mVertexBuffer = NULL;
	}
}

void LLFace::setTextureIndex(U8 index)
{
	if (index != mTextureIndex)
	{
		mTextureIndex = index;

		if (mTextureIndex != FACE_DO_NOT_BATCH_TEXTURES)
		{
			mDrawablep->setState(LLDrawable::REBUILD_POSITION);
		}
		else if (mDrawInfo && !mDrawInfo->mTextureList.empty())
		{
			llwarns << "Face " << std::hex << (intptr_t)this << std::dec
					<< " with no texture index references indexed texture draw info."
					<< llendl;
		}
	}
}

void LLFace::setIndicesIndex(U32 idx)
{
	if (mIndicesIndex != idx)
	{
		mIndicesIndex = idx;
		mVertexBuffer = NULL;
	}
}

U16 LLFace::getGeometryAvatar(LLStrider<LLVector3>& vertices,
							  LLStrider<LLVector3>& normals,
							  LLStrider<LLVector2>& tex_coords,
							  LLStrider<F32>& vertex_weights,
							  LLStrider<LLVector4a>& clothing_weights)
{
	if (mVertexBuffer.notNull())
	{
		mVertexBuffer->getVertexStrider(vertices, mGeomIndex, mGeomCount);
		mVertexBuffer->getNormalStrider(normals, mGeomIndex, mGeomCount);
		mVertexBuffer->getTexCoord0Strider(tex_coords, mGeomIndex, mGeomCount);
		mVertexBuffer->getWeightStrider(vertex_weights, mGeomIndex,
										mGeomCount);
		mVertexBuffer->getClothWeightStrider(clothing_weights, mGeomIndex,
											 mGeomCount);
	}

	return mGeomIndex;
}

U16 LLFace::getGeometry(LLStrider<LLVector3>& vertices,
						LLStrider<LLVector3>& normals,
					    LLStrider<LLVector2>& tex_coords,
						LLStrider<U16> &indicesp)
{
	if (mVertexBuffer.notNull())
	{
		mVertexBuffer->getVertexStrider(vertices, mGeomIndex, mGeomCount);
		if (mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_NORMAL))
		{
			mVertexBuffer->getNormalStrider(normals, mGeomIndex, mGeomCount);
		}
		if (mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0))
		{
			mVertexBuffer->getTexCoord0Strider(tex_coords, mGeomIndex,
											   mGeomCount);
		}

		mVertexBuffer->getIndexStrider(indicesp, mIndicesIndex, mIndicesCount);
	}

	return mGeomIndex;
}

void LLFace::updateCenterAgent()
{
	if (mDrawablep->isActive())
	{
		mCenterAgent = mCenterLocal * getRenderMatrix();
	}
	else
	{
		mCenterAgent = mCenterLocal;
	}
}

void LLFace::renderSelected(LLViewerTexture* imagep, const LLColor4& color)
{
	if (mDrawablep.isNull() || mDrawablep->getSpatialGroup() == NULL)
	{
		return;
	}

	mDrawablep->getSpatialGroup()->rebuildGeom();
	mDrawablep->getSpatialGroup()->rebuildMesh();

	if (!mGeomCount || !mIndicesCount || mDrawablep.isNull() ||
		mVertexBuffer.isNull())
	{
		return;
	}

	gGL.getTexUnit(0)->bind(imagep);

	gGL.pushMatrix();
	if (mDrawablep->isActive())
	{
		gGL.multMatrix(mDrawablep->getRenderMatrix().getF32ptr());
	}
	else
	{
		gGL.multMatrix(mDrawablep->getRegion()->mRenderMatrix.getF32ptr());
	}

	if (mDrawablep->isState(LLDrawable::RIGGED))
	{
		LLVOVolume* volumep = mDrawablep->getVOVolume();
		// For now, we have no selection outline for rigged meshes in PBR mode
		// (disabled too and marked as "TODO" in LL's PBR viewer). HB
		if (volumep && !gUsePBRShaders)
		{
			// BENTO: called when selecting a face during edit of a mesh object
			LLRiggedVolume* riggedp = volumep->getRiggedVolume();
			if (riggedp)
			{
				LLGLEnable offset(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(-1.f, -1.f);
				gGL.multMatrix(volumep->getRelativeXform().getF32ptr());
				const LLVolumeFace& vol_face =
					riggedp->getVolumeFace(getTEOffset());
				LLVertexBuffer::unbind();
				glVertexPointer(3, GL_FLOAT, 16, vol_face.mPositions);
				if (vol_face.mTexCoords)
				{
					glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					glTexCoordPointer(2, GL_FLOAT, 8, vol_face.mTexCoords);
				}
				gGL.syncMatrices();
				glDrawElements(GL_TRIANGLES, vol_face.mNumIndices,
							   GL_UNSIGNED_SHORT, vol_face.mIndices);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		}
	}
	else if (gUsePBRShaders)
	{
		gGL.diffuseColor4fv(color.mV);
		mVertexBuffer->setBuffer();
		mVertexBuffer->draw(LLRender::TRIANGLES, mIndicesCount, mIndicesIndex);
	}
	else
	{
		gGL.diffuseColor4fv(color.mV);
		LLGLEnable poly_offset(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.f, -1.f);
		// Disable per-vertex color to prevent fixed-function pipeline from
		// using it. We want glColor color, not vertex color !
		mVertexBuffer->setBuffer(mVertexBuffer->getTypeMask() &
								 ~LLVertexBuffer::MAP_COLOR);
		mVertexBuffer->draw(LLRender::TRIANGLES, mIndicesCount, mIndicesIndex);
	}

	gGL.popMatrix();
}

void LLFace::printDebugInfo() const
{
	LLFacePool* poolp = getPool();
	llinfos << "Object: " << getViewerObject()->mID << llendl;
	if (getDrawable().notNull())
	{
		llinfos << "Type: "
				<< LLPrimitive::pCodeToString(getDrawable()->getVObj()->getPCode())
				<< llendl;
	}
	if (getTexture())
	{
		llinfos << "Texture: " << getTexture() << " Comps: "
				<< (U32)getTexture()->getComponents() << llendl;
	}
	else
	{
		llinfos << "No texture: " << llendl;
	}

	llinfos << "Face: " << this << llendl;
	llinfos << "State: " << getState() << llendl;
	llinfos << "Geom Index Data:" << llendl;
	llinfos << "--------------------" << llendl;
	llinfos << "GI: " << mGeomIndex << " Count:" << mGeomCount << llendl;
	llinfos << "Face Index Data:" << llendl;
	llinfos << "--------------------" << llendl;
	llinfos << "II: " << mIndicesIndex << " Count:" << mIndicesCount << llendl;
	llinfos << llendl;

	if (poolp)
	{
		poolp->printDebugInfo();

		S32 pool_references = 0;
		for (std::vector<LLFace*>::iterator iter = poolp->mReferences.begin();
			 iter != poolp->mReferences.end(); iter++)
		{
			LLFace *facep = *iter;
			if (facep == this)
			{
				llinfos << "Pool reference: " << pool_references << llendl;
				pool_references++;
			}
		}

		if (pool_references != 1)
		{
			llinfos << "Incorrect number of pool references!" << llendl;
		}
	}

#if 0
	llinfos << "Indices:" << llendl;
	llinfos << "--------------------" << llendl;

	const U32* indicesp = getRawIndices();
	U32 indices_count = getIndicesCount();
	U32 geom_start = getGeomStart();

	for (U32 i = 0; i < indices_count; ++i)
	{
		llinfos << i << ":" << indicesp[i] << ":"
				<< indicesp[i] - geom_start << llendl;
	}
	llinfos << llendl;

	llinfos << "Vertices:" << llendl;
	llinfos << "--------------------" << llendl;
	for (U16 i = 0; i < mGeomCount; ++i)
	{
		llinfos << mGeomIndex + i << ":" << poolp->getVertex(mGeomIndex + i)
				<< llendl;
	}
	llinfos << llendl;
#endif
}

// Transform the texture coordinates for this face.
static void xform(LLVector2& tex_coord, F32 cos_ang, F32 sin_ang, F32 off_s,
				  F32 off_t, F32 mag_s, F32 mag_t)
{
	// Texture transforms are done about the center of the face.
	F32 s = tex_coord.mV[0] - 0.5f;
	F32 t = tex_coord.mV[1] - 0.5f;

	// Handle rotation
	F32 temp = s;
	s = s * cos_ang + t * sin_ang;
	t = -temp * sin_ang + t * cos_ang;

	// Then scale
	s *= mag_s;
	t *= mag_t;

	// Then offset
	s += off_s + 0.5f;
	t += off_t + 0.5f;

	tex_coord.mV[0] = s;
	tex_coord.mV[1] = t;
}

// Transform the texture coordinates for this face.
static void xform4a(LLVector4a& tex_coord, const LLVector4a& trans,
					const LLVector4Logical& mask, const LLVector4a& rot0,
					const LLVector4a& rot1, const LLVector4a& offset,
					const LLVector4a& scale)
{
	// Tex coord is two coords, <s0, t0, s1, t1>
	LLVector4a st;

	// Texture transforms are done about the center of the face.
	st.setAdd(tex_coord, trans);

	// Handle rotation
	LLVector4a rot_st;

	// <s0 * cos_ang, s0*-sin_ang, s1*cos_ang, s1*-sin_ang>
	LLVector4a s0;
	s0.splat(st, 0.f);
	LLVector4a s1;
	s1.splat(st, 2.f);
	LLVector4a ss;
	ss.setSelectWithMask(mask, s1, s0);

	LLVector4a a;
	a.setMul(rot0, ss);

	// <t0*sin_ang, t0*cos_ang, t1*sin_ang, t1*cos_ang>
	LLVector4a t0;
	t0.splat(st, 1.f);
	LLVector4a t1;
	t1.splat(st, 3.f);
	LLVector4a tt;
	tt.setSelectWithMask(mask, t1, t0);

	LLVector4a b;
	b.setMul(rot1, tt);

	st.setAdd(a, b);

	// Then scale
	st.mul(scale);

	// Then offset
	tex_coord.setAdd(st, offset);
}

#if LL_HAS_ASSERT
// Defined in llspatialpartition.cpp
extern LLVector4a gOctreeMaxMag;

bool less_than_max_mag(const LLVector4a& vec)
{
	LLVector4a val;
	val.setAbs(vec);
	return (val.lessThan(gOctreeMaxMag).getGatheredBits() & 0x7) == 0x7;
}
#endif

bool LLFace::genVolumeBBoxes(const LLVolume& volume, S32 f,
							 const LLMatrix4& mat_vert_in,
							 bool global_volume)
{
	// Get the bounding box
	if (mDrawablep->isState(LLDrawable::REBUILD_VOLUME |
							LLDrawable::REBUILD_POSITION |
							LLDrawable::REBUILD_RIGGED))
	{
		if (f >= volume.getNumVolumeFaces())
		{
			llwarns << "Attempt to generate bounding box for invalid face index !"
					<< llendl;
			return false;
		}

		const LLVolumeFace& face = volume.getVolumeFace(f);
#if 0	// Disabled: it causes rigged meshes not to rez at all !  HB
		// MAINT-8264: stray vertices, especially in low LODs, cause bounding
		// box errors.
		if (face.mNumVertices < 3)
		{
			return false;
		}
#endif
		llassert(less_than_max_mag(face.mExtents[0]));
		llassert(less_than_max_mag(face.mExtents[1]));

		// VECTORIZE THIS
		LLMatrix4a mat_vert;
		mat_vert.loadu(mat_vert_in);
		mat_vert.matMulBoundBox(face.mExtents, mExtents);

		LLVector4a& new_min = mExtents[0];
		LLVector4a& new_max = mExtents[1];

		if (!mDrawablep->isActive())
		{
			// Shift position for region
			LLVector4a offset;
			offset.load3(mDrawablep->getRegion()->getOriginAgent().mV);
			new_min.add(offset);
			new_max.add(offset);
		}

		LLVector4a t;
		t.setAdd(new_min, new_max);
		t.mul(0.5f);

		mCenterLocal.set(t.getF32ptr());

		t.setSub(new_max, new_min);
		mBoundingSphereRadius = t.getLength3().getF32() * 0.5f;

		updateCenterAgent();
	}

	return true;
}

// Converts surface coordinates to texture coordinates, based on the values in
// the texture entry.
// *TODO: VECTORIZE THIS
LLVector2 LLFace::surfaceToTexture(LLVector2 surface_coord,
								   const LLVector4a& position,
								   const LLVector4a& normal)
{
	const LLTextureEntry* tep = getTextureEntry();
	if (!tep)
	{
		// Cannot do much without the texture entry
		return surface_coord;
	}

	LLVector2 tc = surface_coord;

	// See if we have a non-default mapping
	if (tep->getTexGen() == LLTextureEntry::TEX_GEN_PLANAR)
	{
		LLVOVolume* volp = mDrawablep->getVOVolume();
		if (!volp)	// Paranoia
		{
			return surface_coord;
		}

		LLVector4a volume_position;
		LLVector3 v_position(position.getF32ptr());
		volume_position.load3(volp->agentPositionToVolume(v_position).mV);
		if (!volp->isVolumeGlobal())
		{
			LLVector4a scale;
			scale.load3(mVObjp->getScale().mV);
			volume_position.mul(scale);
		}

		LLVector4a& c = *(volp->getVolume()->getVolumeFace(mTEOffset).mCenter);

		LLVector4a volume_normal;
		LLVector3 v_normal(normal.getF32ptr());
		volume_normal.load3(volp->agentDirectionToVolume(v_normal).mV);
		volume_normal.normalize3fast();

		planarProjection(tc, volume_normal, c, volume_position);
	}

	if (mTextureMatrix)	// If we have a texture matrix, use it
	{
		return LLVector2(LLVector3(tc) * *mTextureMatrix);
	}

	// Otherwise use the texture entry parameters
	xform(tc, cosf(tep->getRotation()), sinf(tep->getRotation()),
		  tep->getOffsetS(), tep->getOffsetT(), tep->getScaleS(),
		  tep->getScaleT());

	return tc;
}

// Returns scale compared to default texgen, and face orientation as calculated
// by planarProjection(). This is needed to match planar texgen parameters.
void LLFace::getPlanarProjectedParams(LLQuaternion* face_rot,
									  LLVector3* face_pos, F32* scale) const
{
	LLViewerObject* objp = getViewerObject();
	if (!objp) return;

	const LLVolumeFace& vf = objp->getVolume()->getVolumeFace(mTEOffset);
	if (!vf.mNormals || !vf.mTangents) return;

	const LLVector4a& normal4a = vf.mNormals[0];
	const LLVector4a& tangent = vf.mTangents[0];

	LLVector4a binormal4a;
	binormal4a.setCross3(normal4a, tangent);
	binormal4a.mul(tangent.getF32ptr()[3]);

	LLVector2 projected_binormal;
	planarProjection(projected_binormal, normal4a, *vf.mCenter, binormal4a);

	// This normally happens in xform():
	projected_binormal -= LLVector2(0.5f, 0.5f);

	*scale = projected_binormal.length();

	// Rotate binormal to match what planarProjection() thinks it is, then find
	// rotation from that:
	projected_binormal.normalize();
	F32 ang = acosf(projected_binormal.mV[VY]);
	if (projected_binormal.mV[VX] < 0.f)
	{
		ang = -ang;
	}

	// VECTORIZE THIS
	LLVector3 binormal(binormal4a.getF32ptr());
	LLVector3 normal(normal4a.getF32ptr());
	binormal.rotVec(ang, normal);
	LLQuaternion local_rot(binormal % normal, binormal, normal);

	const LLMatrix4& vol_mat = getWorldMatrix();
	*face_rot = local_rot * vol_mat.quaternion();
	*face_pos = vol_mat.getTranslation();
}

// Returns the necessary texture transform to align this face's TE to align_to's TE
bool LLFace::calcAlignedPlanarTE(const LLFace* align_to,  LLVector2* res_st_offset,
								 LLVector2* res_st_scale, F32* res_st_rot,
								 S32 map) const
{
	if (!align_to)
	{
		return false;
	}
	const LLTextureEntry* orig_tep = align_to->getTextureEntry();
	if (!orig_tep || orig_tep->getTexGen() != LLTextureEntry::TEX_GEN_PLANAR ||
		getTextureEntry()->getTexGen() != LLTextureEntry::TEX_GEN_PLANAR)
	{
		return false;
	}

	LLMaterial* matp = orig_tep->getMaterialParams();
	if (!matp && map != LLRender::DIFFUSE_MAP)
	{
		llwarns_once << "Face " << std::hex << (intptr_t)this << std::dec
					 << " is set to use specular or normal map but has no material, defaulting to diffuse"
					 << llendl;
		map = LLRender::DIFFUSE_MAP;
	}

	F32 map_rot = 0.f;
	F32 map_scl_s = 0.f;
	F32 map_scl_t = 0.f;
	F32 map_off_s = 0.f;
	F32 map_off_t = 0.f;
	switch (map)
	{
		case LLRender::DIFFUSE_MAP:
			map_rot = orig_tep->getRotation();
			map_scl_s = orig_tep->getScaleS();
			map_scl_t = orig_tep->getScaleT();
			map_off_s = orig_tep->getOffsetS();
			map_off_t = orig_tep->getOffsetT();
			break;

		case LLRender::NORMAL_MAP:
			if (matp->getNormalID().isNull())
			{
				return false;
			}
			map_rot = matp->getNormalRotation();
			map_scl_s = matp->getNormalRepeatX();
			map_scl_t = matp->getNormalRepeatY();
			map_off_s = matp->getNormalOffsetX();
			map_off_t = matp->getNormalOffsetY();
			break;

		case LLRender::SPECULAR_MAP:
			if (matp->getSpecularID().isNull())
			{
				return false;
			}
			map_rot = matp->getSpecularRotation();
			map_scl_s = matp->getSpecularRepeatX();
			map_scl_t = matp->getSpecularRepeatY();
			map_off_s = matp->getSpecularOffsetX();
			map_off_t = matp->getSpecularOffsetY();
			break;

		default:
			return false;
	}

	LLVector3 orig_pos, this_pos;
	LLQuaternion orig_face_rot, this_face_rot;
	F32 orig_proj_scale, this_proj_scale;
	align_to->getPlanarProjectedParams(&orig_face_rot, &orig_pos,
									   &orig_proj_scale);
	getPlanarProjectedParams(&this_face_rot, &this_pos, &this_proj_scale);

	// The rotation of "this face's" texture:
	LLQuaternion orig_st_rot = LLQuaternion(map_rot, LLVector3::z_axis) *
							   orig_face_rot;
	LLQuaternion this_st_rot = orig_st_rot * ~this_face_rot;
	F32 x_ang, y_ang, z_ang;
	this_st_rot.getEulerAngles(&x_ang, &y_ang, &z_ang);
	*res_st_rot = z_ang;

	// Offset and scale of "this face's" texture:
	LLVector3 centers_dist = (this_pos - orig_pos) * ~orig_st_rot;
	LLVector3 st_scale(map_scl_s, map_scl_t, 1.f);
	st_scale *= orig_proj_scale;
	centers_dist.scaleVec(st_scale);
	LLVector2 orig_st_offset(map_off_s, map_off_t);

	*res_st_offset = orig_st_offset + (LLVector2)centers_dist;
	res_st_offset->mV[VX] -= (S32)res_st_offset->mV[VX];
	res_st_offset->mV[VY] -= (S32)res_st_offset->mV[VY];

	st_scale /= this_proj_scale;
	*res_st_scale = (LLVector2)st_scale;

	return true;
}

void LLFace::updateRebuildFlags()
{
	if (mDrawablep->isState(LLDrawable::REBUILD_VOLUME))
	{
		// This rebuild is zero overhead (direct consequence of some change
		// that affects this face)
		mLastUpdateTime = gFrameTimeSeconds;
	}
	else
	{
		// This rebuild is overhead (side effect of some change that does not
		// affect this face)
		mLastMoveTime = gFrameTimeSeconds;
	}
}

bool LLFace::canRenderAsMask()
{
	if (isState(LLFace::RIGGED))
	{
		// Never auto alpha-mask rigged faces
		return false;
	}

	const LLTextureEntry* tep = getTextureEntry();
	if (!tep || !getViewerObject() || !getTexture())
	{
		return false;
	}

	if (gUsePBRShaders && tep->getGLTFRenderMaterial())
	{
		return false;
	}

	LLMaterial* matp = tep->getMaterialParams();
	if (matp &&
		matp->getDiffuseAlphaMode() == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND)
	{
		return false;
	}

	const LLVolume* volp = getViewerObject()->getVolumeConst();
	if (!volp)
	{
		return false;
	}

		// Cannot treat as mask if face is part of a flexible object
	if (!volp->isUnique() &&
		// Cannot treat as mask if we have face alpha
		tep->getColor().mV[3] == 1.f &&
		// Glowing masks are hard to implement; do not mask
		!tep->hasGlow() &&
		// HUD attachments are NOT maskable (else they would get affected by
		// day light)
		!getViewerObject()->isHUDAttachment() &&
		// Texture actually qualifies for masking (lazily recalculated but
		// expensive)
		getTexture()->getIsAlphaMask())
	{
		// Fullbright objects are NOT subject to the deferred rendering
		if (LLPipeline::sRenderDeferred && !tep->getFullbright())
		{
			return LLPipeline::sAutoMaskAlphaDeferred;
		}
		return LLPipeline::sAutoMaskAlphaNonDeferred;
	}

	return false;
}

bool LLFace::getGeometryVolume(const LLVolume& volume, S32 f,
							   const LLMatrix4& mat_vert_in,
							   const LLMatrix3& mat_norm_in,
							   const U16& index_offset, bool force_rebuild)
{
	LL_FAST_TIMER(FTM_FACE_GET_GEOM);
	llassert(verify());
	if (f < 0 || f >= volume.getNumVolumeFaces())
	{
		llwarns << "Attempt to get a non-existent volume face: "
				<< volume.getNumVolumeFaces()
				<< " total faces and requested face index = " << f << llendl;
		return false;
	}

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCTREE))
	{
		updateRebuildFlags();
	}

	if (mDrawablep.isNull())
	{
		llwarns << "NULL drawable !" << llendl;
		return false;
	}

	if (!mDrawablep->getVOVolume())
	{
		llwarns << "NULL volume !" << llendl;
		return false;
	}

	if (mVObjp.isNull())
	{
		llwarns << "NULL viewer object !" << llendl;
		return false;
	}

	if (!mVObjp->getVolume())
	{
		llwarns << "NULL viewer object volume !" << llendl;
		return false;
	}

	if (mVertexBuffer.isNull())
	{
		llwarns << "NULL vertex buffer !" << llendl;
		return false;
	}

	bool rigged = isState(RIGGED);
	const LLVolumeFace& vf = volume.getVolumeFace(f);
	U32 num_vertices = llclamp(vf.mNumVertices, 0, mGeomCount);
	U32 num_indices = llclamp(vf.mNumIndices, 0, mIndicesCount);

	if (num_indices + mIndicesIndex > mVertexBuffer->getNumIndices())
	{
		if (gDebugGL)
		{
			llwarns << "Index buffer overflow !  Indices Count: "
					<< mIndicesCount << " - VF Num Indices: " << num_indices
					<< " -  Indices Index: " << mIndicesIndex
					<< " - VB Num Indices: " << mVertexBuffer->getNumIndices()
					<< " - Face Index: " << f << " - Pool Type: " << mPoolType
					<< llendl;
		}
		return false;
	}

	if (num_vertices + mGeomIndex > mVertexBuffer->getNumVerts())
	{
		if (gDebugGL)
		{
			llwarns << "Vertex buffer overflow !" << llendl;
		}
		return false;
	}

	if (!vf.mTexCoords || !vf.mNormals || !vf.mPositions)
	{
		llwarns_sparse << "vf got NULL pointer(s) !" << llendl;
		return false;
	}

	LLStrider<LLVector3> vert, norm, tangent;
	LLStrider<LLVector2> tex_coords0, tex_coords1, tex_coords2;
	LLStrider<LLColor4U> colors;
	LLStrider<U16> indicesp;
	LLStrider<LLVector4a> wght;

	bool full_rebuild = force_rebuild ||
						mDrawablep->isState(LLDrawable::REBUILD_VOLUME);

	LLVector3 scale;
	if (mDrawablep->getVOVolume()->isVolumeGlobal())
	{
		scale.set(1.f, 1.f, 1.f);
	}
	else
	{
		scale = mVObjp->getScale();
	}

	bool rebuild_pos = full_rebuild ||
					   mDrawablep->isState(LLDrawable::REBUILD_POSITION);
	bool rebuild_color = full_rebuild ||
						 mDrawablep->isState(LLDrawable::REBUILD_COLOR);
	bool rebuild_emissive =
		rebuild_color &&
		mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_EMISSIVE);
	bool rebuild_tcoord = full_rebuild ||
						  mDrawablep->isState(LLDrawable::REBUILD_TCOORD);
	bool rebuild_normal =
		rebuild_pos && mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_NORMAL);
	bool rebuild_tangent =
		rebuild_pos &&
		mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TANGENT);
	bool rebuild_weights =
		rebuild_pos &&
		mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_WEIGHT4);

	const LLTextureEntry* tep = mVObjp->getTE(f);
	U8 bump_code = 0;
	LLMaterial* matp = NULL;
	LLFetchedGLTFMaterial* gltfp = NULL;
	LLColor4U color;
	F32 r = 0.f, os = 0.f, ot = 0.f, ms = 0.f, mt = 0.f;
	if (tep)
	{
		bump_code = tep->getBumpmap();
		matp = tep->getMaterialParams().get();
		LLGLTFMaterial* rmatp = tep->getGLTFRenderMaterial();
		gltfp = rmatp ? rmatp->asFetched() : NULL;
		if (rebuild_tcoord)
		{
			if (gltfp && !gUsePBRShaders && isState(USE_FACE_COLOR))
			{
				// We are overriding the diffuse texture with the GLTF base
				// color map, so let's use its own transforms. HB
				r = gltfp->getBaseColorRotation();
				const LLVector2& offset = gltfp->getBaseColorOffset();
				os = offset.mV[0];
				ot = offset.mV[1];
				const LLVector2& scale = gltfp->getBaseColorScale();
				ms = scale.mV[0];
				mt = scale.mV[1];
			}
			else
			{
				r = tep->getRotation();
				os = tep->getOffsetS();
				ot = tep->getOffsetT();
				ms = tep->getScaleS();
				mt = tep->getScaleT();
			}
		}
		if (!gUsePBRShaders)
		{
			gltfp = NULL;	// Do not use the GLTF material in non-PBR mode.
		}
		if (gltfp)
		{
			color = LLColor4U(gltfp->mBaseColor);
		}
		else
		{
			color = LLColor4U(getRenderColor());
		}
	}
	else
	{
		color = LLColor4U::white;
		rebuild_color = false;	// Cannot get color when tep is NULL
	}
//MK
	if (gRLenabled && gRLInterface.mContainsCamTextures &&
		getViewerObject() && !getViewerObject()->isAttachment())
	{
		color = LLColor4::white;
	}
//mk

	if (mDrawablep->isStatic())
	{
		setState(GLOBAL);
	}
	else
	{
		clearState(GLOBAL);
	}

	if (rebuild_color)	// false if tep == NULL
	{
		// Decide if shiny goes in alpha channel of color

		// Alpha channel MUST contain transparency, not shiny:
		if (!isInAlphaPool())
		{
			bool shiny_in_alpha = false;
			if (LLPipeline::sRenderDeferred)
			{
				// Store shiny in alpha if we do not have a specular map
				if (!matp || matp->getSpecularID().isNull())
				{
					shiny_in_alpha = true;
				}
			}
			else if (!matp ||
					 matp->getDiffuseAlphaMode() !=
						LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
			{
				shiny_in_alpha = true;
			}

			if (shiny_in_alpha)
			{
				static const LLColor4U shine_steps(0, 64, 128, 191);
				U8 index = tep->getShiny();
				if (index > 3)
				{
					llwarns << "Shiny index too large (" << index
							<< ") for face " << f << " of object "
							<< mVObjp->getID() << llendl;
					llassert(false);
					index = 3;
				}
				color.mV[3] = shine_steps.mV[tep->getShiny()];
			}
		}
	}

    // INDICES
	bool result;
	if (full_rebuild)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_INDEX);
		result = mVertexBuffer->getIndexStrider(indicesp, mIndicesIndex,
												mIndicesCount);
		if (!result)
		{
			llwarns << "getIndexStrider() failed !" << llendl;
			return false;
		}

		volatile __m128i* dst = (__m128i*)indicesp.get();
		__m128i* src = (__m128i*)vf.mIndices;
		__m128i offset = _mm_set1_epi16(index_offset);

		U32 end = num_indices / 8;

		for (U32 i = 0; i < end; ++i)
		{
			__m128i res = _mm_add_epi16(src[i], offset);
			_mm_storeu_si128((__m128i*)dst++, res);
		}

		U16* idx = (U16*)dst;
		for (U32 i = end * 8; i < num_indices; ++i)
		{
			*idx++ = vf.mIndices[i] + index_offset;
		}
	}

	F32 cos_ang = 0.f, sin_ang = 0.f;
	constexpr S32 XFORM_BLINNPHONG_COLOR = 1;
	constexpr S32 XFORM_BLINNPHONG_NORMAL = 1 << 1;
	constexpr S32 XFORM_BLINNPHONG_SPECULAR = 1 << 2;
	S32 xforms = 0;
	// For PBR material, transforms will be applied later
	if (rebuild_tcoord && tep && !gltfp)
	{
		cos_ang = cosf(r);
		sin_ang = sinf(r);
		if (cos_ang != 1.f || sin_ang != 0.f || os != 0.f || ot != 0.f ||
			ms != 1.f || mt != 1.f)
		{
			xforms = XFORM_BLINNPHONG_COLOR;
		}
		if (matp && !gUsePBRShaders)
		{
			F32 os_norm = 0.f;
			F32 ot_norm = 0.f;
			matp->getNormalOffset(os_norm, ot_norm);
			if (os_norm != 0.f || ot_norm != 0.f)
			{
				xforms |= XFORM_BLINNPHONG_NORMAL;
			}
			else
			{
				F32 ms_norm = 0.f;
				F32 mt_norm = 0.f;
				matp->getNormalRepeat(ms_norm, mt_norm);
				if (ms_norm != 1.f || mt_norm != 1.f)
				{
					xforms |= XFORM_BLINNPHONG_NORMAL;
				}
				else
				{
					F32 r_norm = matp->getNormalRotation();
					if (cosf(r_norm) != 1.f || sinf(r_norm) != 0.f)
					{
						xforms |= XFORM_BLINNPHONG_NORMAL;
					}
				}
			}
			F32 os_spec = 0.f;
			F32 ot_spec = 0.f;
			matp->getSpecularOffset(os_spec, ot_spec);
			if (os_spec != 0.f || ot_spec != 0.f)
			{
				xforms |= XFORM_BLINNPHONG_SPECULAR;
			}
			else
			{
				F32 ms_spec = 0.f;
				F32 mt_spec = 0.f;
				matp->getSpecularRepeat(ms_spec, mt_spec);
				if (ms_spec != 1.f || mt_spec != 1.f)
				{
					xforms |= XFORM_BLINNPHONG_SPECULAR;
				}
				else
				{
					F32 r_spec = matp->getSpecularRotation();
					if (cosf(r_spec) != 1.f || sinf(r_spec) != 0.f)
					{
						xforms |= XFORM_BLINNPHONG_SPECULAR;
					}
				}
			}
		}
	}

	const LLMeshSkinInfo* skinp = rigged ? mSkinInfo.get() : NULL;
	LLMatrix4a mat_vert;
	if (rebuild_pos)
	{
		if (skinp)
		{
			// Override with bind shape matrix if rigged
			mat_vert.loadu(skinp->mBindShapeMatrix);
		}
		else
		{
			mat_vert.loadu(mat_vert_in);
		}
	}
	LLMatrix4a mat_normal;
	if (rebuild_normal || rebuild_tangent)
	{
		if (skinp)
		{
			mat_normal.loadu(skinp->mBindShapeMatrix);
			mat_normal.invert();
			mat_normal.transpose();
		}
		else
		{
			mat_normal.loadu(mat_norm_in);
		}
	}

	if (rebuild_tcoord)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_TEXTURE);

		// Bump setup
		LLVector4a binormal_dir(-sin_ang, cos_ang, 0.f);
		LLVector4a bump_s_prim_light_ray(0.f, 0.f, 0.f);
		LLVector4a bump_t_prim_light_ray(0.f, 0.f, 0.f);

		LLQuaternion bump_quat;
		if (mDrawablep->isActive())
		{
			bump_quat = LLQuaternion(mDrawablep->getRenderMatrix());
		}

		if (bump_code)
		{
			mVObjp->getVolume()->genTangents(f);
			F32 offset_multiple = 1.f / 256.f;
			switch (bump_code)
			{
				case BE_NO_BUMP:
					offset_multiple = 0.f;
					break;

				case BE_BRIGHTNESS:
				case BE_DARKNESS:
				{
					LLViewerTexture* tex =
						mTexture[LLRender::DIFFUSE_MAP].get();
					if (tex && tex->hasGLTexture())
					{
						// Offset by approximately one texel
						S32 cur_discard = tex->getDiscardLevel();
						S32 max_size = llmax(tex->getWidth(),
											 tex->getHeight());
						max_size <<= cur_discard;
						constexpr F32 ARTIFICIAL_OFFSET = 2.f;
						offset_multiple = ARTIFICIAL_OFFSET / (F32)max_size;
					}
					break;
				}

				default:  // Standard bumpmap texture assumed to be 256x256
					break;
			}

			F32 s_scale = 1.f;
			F32 t_scale = 1.f;
			if (tep)
			{
				tep->getScale(&s_scale, &t_scale);
			}

			// Use the nudged south when coming from above Sun angle, such
			// that emboss mapping always shows up on the upward faces of cubes
			// when it is noon (since a lot of builders build with the Sun
			// forced to noon).
			const LLVector3& sun_ray = gSky.mVOSkyp->mBumpSunDir;
			LLVector3 primary_light_ray;
			if (sun_ray.mV[VZ] > 0.f)
			{
				primary_light_ray = sun_ray;
			}
			else
			{
				primary_light_ray = gSky.getMoonDirection();
			}
			bump_s_prim_light_ray.load3((offset_multiple * s_scale *
											primary_light_ray).mV);
			bump_t_prim_light_ray.load3((offset_multiple * t_scale *
											primary_light_ray).mV);
		}

		const LLTextureEntry* te2p = getTextureEntry();
		U8 texgen = te2p ? te2p->getTexGen()
						 : LLTextureEntry::TEX_GEN_DEFAULT;
		if (rebuild_tcoord && texgen != LLTextureEntry::TEX_GEN_DEFAULT)
		{
			// Planar texgen needs binormals
			mVObjp->getVolume()->genTangents(f);
		}

		LLVOVolume* vobj = (LLVOVolume*)((LLViewerObject*)mVObjp);
		U8 tex_mode = vobj->mTexAnimMode;

		// When texture animation is in play, override specular and normal map
		// tex coords with diffuse texcoords.
		bool tex_anim = vobj->mTextureAnimp != NULL;

		if (isState(TEXTURE_ANIM))
		{
			if (!tex_mode)
			{
				clearState(TEXTURE_ANIM);
			}
			else
			{
				os = ot = r = sin_ang = 0.f;
				cos_ang = ms = mt = 1.f;
				xforms = 0;
			}
#if 0		// Performance viewer change (removal of isState(RIGGED) test)
			if (getVirtualSize() >= MIN_TEX_ANIM_SIZE || isState(RIGGED))
#else
			if (getVirtualSize() >= MIN_TEX_ANIM_SIZE)
#endif
			{
				// Do not override texture transform during tc bake
				tex_mode = 0;
			}
		}

		LLVector4a scalea;
		scalea.load3(scale.mV);

		bool vb_has_tc1 =
			mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD1);
		bool do_bump = bump_code && vb_has_tc1;
		if ((matp || gltfp) && !do_bump)
		{
			do_bump = vb_has_tc1 ||
				mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD2);
		}
		bool do_tex_mat = tex_mode && mTextureMatrix;

		if (!do_bump)
		{
			// Not bump mapped, might be able to do a cheap update
			result = mVertexBuffer->getTexCoord0Strider(tex_coords0,
														mGeomIndex,
														mGeomCount);
			if (!result)
			{
				llwarns << "getTexCoord0Strider() failed !" << llendl;
				return false;
			}

			if (texgen != LLTextureEntry::TEX_GEN_PLANAR)
			{
				if (!do_tex_mat)
				{
					if (!xforms)
					{
						S32 tc_size = (num_vertices * 2 * sizeof(F32) + 0xF) &
									  ~0xF;
						LLVector4a::memcpyNonAliased16((F32*)tex_coords0.get(),
													   (F32*)vf.mTexCoords,
													   tc_size);
					}
					else
					{
						F32* dst = (F32*)tex_coords0.get();
						LLVector4a* src = (LLVector4a*)vf.mTexCoords;

						LLVector4a trans;
						trans.splat(-0.5f);

						LLVector4a rot0;
						rot0.set(cos_ang, -sin_ang, cos_ang, -sin_ang);

						LLVector4a rot1;
						rot1.set(sin_ang, cos_ang, sin_ang, cos_ang);

						LLVector4a scale;
						scale.set(ms, mt, ms, mt);

						LLVector4a offset;
						offset.set(os + 0.5f, ot + 0.5f, os + 0.5f, ot + 0.5f);

						LLVector4Logical mask;
						mask.clear();
						mask.setElement<2>();
						mask.setElement<3>();

						U32 count = num_vertices / 2 + num_vertices % 2;
						for (U32 i = 0; i < count; ++i)
						{
							LLVector4a res = *src++;
							xform4a(res, trans, mask, rot0, rot1, offset,
									scale);
							res.store4a(dst);
							dst += 4;
						}
					}
				}
				else
				{
					// Do tex mat, no texgen, no bump
					for (U32 i = 0; i < num_vertices; ++i)
					{
						LLVector2 tc(vf.mTexCoords[i]);
						LLVector3 tmp(tc.mV[0], tc.mV[1], 0.f);
						tmp = tmp * *mTextureMatrix;
						tc.mV[0] = tmp.mV[0];
						tc.mV[1] = tmp.mV[1];
						*tex_coords0++ = tc;
					}
				}
			}
			// No bump, tex gen planar
			else if (do_tex_mat)
			{
				for (U32 i = 0; i < num_vertices; ++i)
				{
					LLVector2 tc(vf.mTexCoords[i]);
					LLVector4a& norm = vf.mNormals[i];
					LLVector4a& center = *(vf.mCenter);
					LLVector4a vec = vf.mPositions[i];
					vec.mul(scalea);
					planarProjection(tc, norm, center, vec);

					LLVector3 tmp(tc.mV[0], tc.mV[1], 0.f);
					tmp = tmp * *mTextureMatrix;
					tc.mV[0] = tmp.mV[0];
					tc.mV[1] = tmp.mV[1];

					*tex_coords0++ = tc;
				}
			}
			else if (xforms || !gUsePBRShaders)
			{
				for (U32 i = 0; i < num_vertices; ++i)
				{
					LLVector2 tc(vf.mTexCoords[i]);
					LLVector4a& norm = vf.mNormals[i];
					LLVector4a& center = *(vf.mCenter);
					LLVector4a vec = vf.mPositions[i];
					vec.mul(scalea);
					planarProjection(tc, norm, center, vec);

					xform(tc, cos_ang, sin_ang, os, ot, ms, mt);

					*tex_coords0++ = tc;
				}
			}
			else 	//  PBR mode, no xforms
			{
				for (U32 i = 0; i < num_vertices; ++i)
				{
					LLVector2 tc(vf.mTexCoords[i]);
					LLVector4a& norm = vf.mNormals[i];
					LLVector4a& center = *(vf.mCenter);
					LLVector4a vec = vf.mPositions[i];
					vec.mul(scalea);
					planarProjection(tc, norm, center, vec);

					*tex_coords0++ = tc;
				}
			}
		}
		else
		{
			// Bump mapped or has material, just do the whole expensive loop
			static std::vector<LLVector2> bump_tc;
			bump_tc.clear();
			bump_tc.reserve(num_vertices);

			if (matp && matp->getNormalID().notNull())
			{
				// Writing out normal and specular texture coordinates, not
				// bump offsets
				do_bump = false;
			}

			LLStrider<LLVector2> dst;

			for (U32 ch = 0; ch < 3; ++ch)
			{
				S32 xform_channel = 0;
				switch (ch)
				{
					case 0:
					{
						result =
							mVertexBuffer->getTexCoord0Strider(dst, mGeomIndex,
															   mGeomCount);
						if (!result)
						{
							llwarns << "getTexCoord0Strider() failed !"
									<< llendl;
							return false;
						}
						xform_channel = XFORM_BLINNPHONG_COLOR;
						break;
					}

					case 1:
					{
						if (!vb_has_tc1)
						{
							continue;
						}
						result =
							mVertexBuffer->getTexCoord1Strider(dst, mGeomIndex,
															   mGeomCount);
						if (!result)
						{
							llwarns << "getTexCoord1Strider() failed !"
									<< llendl;
							return false;
						}
						if (matp && !tex_anim)
						{
							r = matp->getNormalRotation();
							matp->getNormalOffset(os, ot);
							matp->getNormalRepeat(ms, mt);

							cos_ang = cosf(r);
							sin_ang = sinf(r);
						}
						xform_channel = XFORM_BLINNPHONG_NORMAL;
						break;
					}

					case 2:
					{
						if (!mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXCOORD2))
						{
							continue;
						}
						result =
							mVertexBuffer->getTexCoord2Strider(dst, mGeomIndex,
															   mGeomCount);
						if (!result)
						{
							llwarns << "getTexCoord2Strider() failed !"
									<< llendl;
							return false;
						}
						if (matp && !tex_anim)
						{
							r = matp->getSpecularRotation();
							matp->getSpecularOffset(os, ot);
							matp->getSpecularRepeat(ms, mt);

							cos_ang = cosf(r);
							sin_ang = sinf(r);
						}
						xform_channel = XFORM_BLINNPHONG_SPECULAR;
					}
				}

				bool do_xform = (xforms & xform_channel) != 0 ||
								gUsePBRShaders;

				if (texgen == LLTextureEntry::TEX_GEN_PLANAR &&
					!(tex_mode && mTextureMatrix))
				{
					U32 i = 0;
#if defined(__AVX2__)
					if (num_vertices >= 8)
					{
						__m256 cos_vec = _mm256_set1_ps(cos_ang);
						__m256 sin_vec = _mm256_set1_ps(sin_ang);
						__m256 off = _mm256_set1_ps(-0.5f);
						__m256 osoff = _mm256_set1_ps(os + 0.5f);
						__m256 otoff = _mm256_set1_ps(ot + 0.5f);
						__m256 ms_vec = _mm256_set1_ps(ms);
						__m256 mt_vec = _mm256_set1_ps(mt);
						F32 sv[8], tv[8];
						LLVector4a& center = *(vf.mCenter);

						do
						{
							for (S32 j = 0; j < 8; ++j, ++i)
							{
								LLVector2 tcv(vf.mTexCoords[i]);
								LLVector4a vec = vf.mPositions[i];
								vec.mul(scalea);
								planarProjection(tcv, vf.mNormals[i], center,
												 vec);
								sv[j] = tcv.mV[0];
								tv[j] = tcv.mV[1];
							}

							__m256 svv = _mm256_loadu_ps(sv);
							__m256 tvv = _mm256_loadu_ps(tv);

							// Texture transforms are done about the center of
							// the face
							svv = _mm256_add_ps(svv, off);
							tvv = _mm256_add_ps(tvv, off);

							// Transform the texture coordinates for this face.
							__m256 coss = _mm256_mul_ps(svv, cos_vec);
							__m256 sins = _mm256_mul_ps(svv, sin_vec);
							svv = _mm256_fmadd_ps(tvv, sin_vec, coss);
							tvv = _mm256_fmsub_ps(tvv, cos_vec, sins);

							// Then scale and offset
							svv = _mm256_fmadd_ps(svv, ms_vec, osoff);
							tvv = _mm256_fmadd_ps(tvv, mt_vec, otoff);

							_mm256_storeu_ps(sv, svv);
							_mm256_storeu_ps(tv, tvv);

							for (S32 j = 0; j < 8; ++j)
							{
								LLVector2 tc(sv[j], tv[j]);
								*dst++ = tc;

								if (!matp && do_bump)
								{
									bump_tc.emplace_back(tc);
								}
							}
						}
						while (i + 8 <= num_vertices);
					}
#endif
					// SSE2 version
					if (i + 4 <= num_vertices)
					{
						__m128 cos_vec = _mm_set1_ps(cos_ang);
						__m128 sin_vec = _mm_set1_ps(sin_ang);
						__m128 off = _mm_set1_ps(-0.5f);
						__m128 osoff = _mm_set1_ps(os + 0.5f);
						__m128 otoff = _mm_set1_ps(ot + 0.5f);
						__m128 ms_vec = _mm_set1_ps(ms);
						__m128 mt_vec = _mm_set1_ps(mt);
						F32 sv[4], tv[4];
						LLVector4a& center = *(vf.mCenter);

						do
						{
							for (S32 j = 0; j < 4; ++j, ++i)
							{
								LLVector2 tcv(vf.mTexCoords[i]);
								LLVector4a vec = vf.mPositions[i];
								vec.mul(scalea);
								planarProjection(tcv, vf.mNormals[i], center,
												 vec);
								sv[j] = tcv.mV[0];
								tv[j] = tcv.mV[1];
							}

							__m128 svv = _mm_loadu_ps(sv);
							__m128 tvv = _mm_loadu_ps(tv);

							// Texture transforms are done about the center of
							// the face
							svv = _mm_add_ps(svv, off);
							tvv = _mm_add_ps(tvv, off);

							// Transform the texture coordinates for this face.
							__m128 coss = _mm_mul_ps(svv, cos_vec);
							__m128 sins = _mm_mul_ps(svv, sin_vec);
							// No fmadd/fmsub in SSE2: two steps needed...
							svv = _mm_add_ps(_mm_mul_ps(tvv, sin_vec), coss);
							tvv = _mm_sub_ps(_mm_mul_ps(tvv, cos_vec), sins);

							// Then scale and offset
							svv = _mm_add_ps(_mm_mul_ps(svv, ms_vec), osoff);
							tvv = _mm_add_ps(_mm_mul_ps(tvv, mt_vec), otoff);

							_mm_storeu_ps(sv, svv);
							_mm_storeu_ps(tv, tvv);

							for (S32 j = 0; j < 4; ++j)
							{
								LLVector2 tc(sv[j], tv[j]);
								*dst++ = tc;

								if (!matp && do_bump)
								{
									bump_tc.emplace_back(tc);
								}
							}
						}
						while (i + 4 <= num_vertices);
					}

					while (i < num_vertices)
					{
						LLVector2 tc(vf.mTexCoords[i]);
						LLVector4a& norm = vf.mNormals[i];
						LLVector4a& center = *(vf.mCenter);

						LLVector4a vec = vf.mPositions[i++];
						vec.mul(scalea);
						planarProjection(tc, norm, center, vec);

						// Texture transforms are done about the center of the face.
						F32 s = tc.mV[0] - 0.5f;
						F32 t = tc.mV[1] - 0.5f;
							
						// Handle rotation
						F32 temp = s;
						s = s * cos_ang + t * sin_ang;
						t = -temp * sin_ang + t * cos_ang;

						// Then scale
						s *= ms;
						t *= mt;
						// Then offset
						s += os + 0.5f;
						t += ot + 0.5f;
						tc.mV[0] = s;
						tc.mV[1] = t;

						*dst++ = tc;

						if (!matp && do_bump)
						{
							bump_tc.emplace_back(tc);
						}
					}
				}
				else if (tex_mode && mTextureMatrix) 
				{
					for (U32 i = 0; i < num_vertices; ++i)
					{
						LLVector2 tc(vf.mTexCoords[i]);
						if (texgen == LLTextureEntry::TEX_GEN_PLANAR)
						{
							LLVector4a& norm = vf.mNormals[i];
							LLVector4a& center = *(vf.mCenter);
							LLVector4a vec = vf.mPositions[i];
							vec.mul(scalea);
							planarProjection(tc, norm, center, vec);
						}
						LLVector3 tmp(tc.mV[0], tc.mV[1], 0.f);
						tmp = tmp * *mTextureMatrix;

						tc.mV[0] = tmp.mV[0];
						tc.mV[1] = tmp.mV[1];

						*dst++ = tc;

						if (!matp && do_bump)
						{
							bump_tc.emplace_back(tc);
						}
					}
				}
				else if (do_xform)	// Always true in EE rendering mode. HB
				{
					U32 i = 0;
#if defined(__AVX2__)
					if (num_vertices >= 8)
					{
						__m256 cos_vec = _mm256_set1_ps(cos_ang);
						__m256 sin_vec = _mm256_set1_ps(sin_ang);
						__m256 off = _mm256_set1_ps(-0.5f);
						__m256 osoff = _mm256_set1_ps(os + 0.5f);
						__m256 otoff = _mm256_set1_ps(ot + 0.5f);
						__m256 ms_vec = _mm256_set1_ps(ms);
						__m256 mt_vec = _mm256_set1_ps(mt);
						F32 sv[8], tv[8];
						do
						{
							sv[0] = vf.mTexCoords[i].mV[0];
							tv[0] = vf.mTexCoords[i++].mV[1];
							sv[1] = vf.mTexCoords[i].mV[0];
							tv[1] = vf.mTexCoords[i++].mV[1];
							sv[2] = vf.mTexCoords[i].mV[0];
							tv[2] = vf.mTexCoords[i++].mV[1];
							sv[3] = vf.mTexCoords[i].mV[0];
							tv[3] = vf.mTexCoords[i++].mV[1];
							sv[4] = vf.mTexCoords[i].mV[0];
							tv[4] = vf.mTexCoords[i++].mV[1];
							sv[5] = vf.mTexCoords[i].mV[0];
							tv[5] = vf.mTexCoords[i++].mV[1];
							sv[6] = vf.mTexCoords[i].mV[0];
							tv[6] = vf.mTexCoords[i++].mV[1];
							sv[7] = vf.mTexCoords[i].mV[0];
							tv[7] = vf.mTexCoords[i++].mV[1];

							__m256 svv = _mm256_loadu_ps(sv);
							__m256 tvv = _mm256_loadu_ps(tv);

							// Texture transforms are done about the center of
							// the face
							svv = _mm256_add_ps(svv, off);
							tvv = _mm256_add_ps(tvv, off);

							// Transform the texture coordinates for this face.
							__m256 coss = _mm256_mul_ps(svv, cos_vec);
							__m256 sins = _mm256_mul_ps(svv, sin_vec);
							svv = _mm256_fmadd_ps(tvv, sin_vec, coss);
							tvv = _mm256_fmsub_ps(tvv, cos_vec, sins);

							// Then scale and offset
							svv = _mm256_fmadd_ps(svv, ms_vec, osoff);
							tvv = _mm256_fmadd_ps(tvv, mt_vec, otoff);

							_mm256_storeu_ps(sv, svv);
							_mm256_storeu_ps(tv, tvv);

							for (U32 j = 0; j < 8; ++j)
							{
								LLVector2 tc(sv[j], tv[j]);
								*dst++ = tc;

								if (!matp && do_bump)
								{
									bump_tc.emplace_back(tc);
								}
							}
						}
						while (i + 8 <= num_vertices);
					}
#endif
					// SSE2 version
					if (i + 4 <= num_vertices)
					{
						__m128 cos_vec = _mm_set1_ps(cos_ang);
						__m128 sin_vec = _mm_set1_ps(sin_ang);
						__m128 off = _mm_set1_ps(-0.5f);
						__m128 osoff = _mm_set1_ps(os + 0.5f);
						__m128 otoff = _mm_set1_ps(ot + 0.5f);
						__m128 ms_vec = _mm_set1_ps(ms);
						__m128 mt_vec = _mm_set1_ps(mt);
						F32 sv[4], tv[4];
						do
						{
							sv[0] = vf.mTexCoords[i].mV[0];
							tv[0] = vf.mTexCoords[i++].mV[1];
							sv[1] = vf.mTexCoords[i].mV[0];
							tv[1] = vf.mTexCoords[i++].mV[1];
							sv[2] = vf.mTexCoords[i].mV[0];
							tv[2] = vf.mTexCoords[i++].mV[1];
							sv[3] = vf.mTexCoords[i].mV[0];
							tv[3] = vf.mTexCoords[i++].mV[1];
							__m128 svv = _mm_loadu_ps(sv);
							__m128 tvv = _mm_loadu_ps(tv);

							// Texture transforms are done about the center of
							// the face
							svv = _mm_add_ps(svv, off);
							tvv = _mm_add_ps(tvv, off);

							// Transform the texture coordinates for this face.
							__m128 coss = _mm_mul_ps(svv, cos_vec);
							__m128 sins = _mm_mul_ps(svv, sin_vec);
							// No fmadd/fmsub in SSE2: two steps needed...
							svv = _mm_add_ps(_mm_mul_ps(tvv, sin_vec), coss);
							tvv = _mm_sub_ps(_mm_mul_ps(tvv, cos_vec), sins);

							// Then scale and offset
							svv = _mm_add_ps(_mm_mul_ps(svv, ms_vec), osoff);
							tvv = _mm_add_ps(_mm_mul_ps(tvv, mt_vec), otoff);

							_mm_storeu_ps(sv, svv);
							_mm_storeu_ps(tv, tvv);

							for (U32 j = 0; j < 4; ++j)
							{
								LLVector2 tc(sv[j], tv[j]);
								*dst++ = tc;

								if (!matp && do_bump)
								{
									bump_tc.emplace_back(tc);
								}
							}
						}
						while (i + 4 <= num_vertices);
					}

					while (i < num_vertices)
					{
						LLVector2 tc(vf.mTexCoords[i++]);
						xform(tc, cos_ang, sin_ang, os, ot, ms, mt);
						*dst++ = tc;

						if (!matp && do_bump)
						{
							bump_tc.emplace_back(tc);
						}
					}
				}
				else	// PBR rendering mode, no xforms. HB
				{
					U32 i = 0;
					while (i < num_vertices)
					{
						LLVector2 tc(vf.mTexCoords[i++]);
						*dst++ = tc;
						if (do_bump)
						{
							bump_tc.emplace_back(tc);
						}
					}
				}
			}

			if (!matp && !gltfp && do_bump)
			{
				result = mVertexBuffer->getTexCoord1Strider(tex_coords1,
															mGeomIndex,
															mGeomCount);
				if (!result)
				{
					llwarns << "getTexCoord1Strider() failed !" << llendl;
					return false;
				}

				LLMatrix4a tangent_to_object;
				LLVector4a tangent, binorm, t, binormal;
				LLVector3 t2;
				for (U32 i = 0; i < num_vertices; ++i)
				{
					tangent = vf.mTangents[i];

					binorm.setCross3(vf.mNormals[i], tangent);
					binorm.mul(tangent.getF32ptr()[3]);

					tangent_to_object.setRows(tangent, binorm, vf.mNormals[i]);
					tangent_to_object.rotate(binormal_dir, t);

					mat_normal.rotate(t, binormal);
					// VECTORIZE THIS
					if (mDrawablep->isActive())
					{
						t2.set(binormal.getF32ptr());
						t2 *= bump_quat;
						binormal.load3(t2.mV);
					}
					binormal.normalize3fast();

					*tex_coords1++ = bump_tc[i] +
						 LLVector2(bump_s_prim_light_ray.dot3(tangent).getF32(),
								   bump_t_prim_light_ray.dot3(binormal).getF32());
				}

			}
		}
	}

	if (rebuild_pos)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_POSITION);
		llassert(num_vertices > 0);

		result = mVertexBuffer->getVertexStrider(vert, mGeomIndex, mGeomCount);
		if (!result)
		{
			llwarns << "getVertexStrider() failed !" << llendl;
			return false;
		}

		LLVector4a* src = vf.mPositions;
		LLVector4a* end = src + num_vertices;

		S32 index =
			mTextureIndex < FACE_DO_NOT_BATCH_TEXTURES ? mTextureIndex : 0;
		llassert(index <= LLGLSLShader::sIndexedTextureChannels - 1);

		F32 val = 0.f;
		S32* vp = (S32*)&val;
		*vp = index;
		LLVector4a tex_idx(0.f, 0.f, 0.f, val);

		LLVector4Logical mask;
		mask.clear();
		mask.setElement<3>();

		F32* dst = (F32*)vert.get();
		F32* end_f32 = dst + mGeomCount * 4;

		LLVector4a res0, tmp;

		while (src < end)
		{
			mat_vert.affineTransform(*src++, res0);
			tmp.setSelectWithMask(mask, tex_idx, res0);
			tmp.store4a((F32*)dst);
			dst += 4;
		}

		while (dst < end_f32)
		{
			res0.store4a((F32*)dst);
			dst += 4;
		}
	}

	if (rebuild_normal)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_NORMAL);
		result = mVertexBuffer->getNormalStrider(norm, mGeomIndex, mGeomCount);
		if (!result)
		{
			llwarns << "getNormalStrider() failed !" << llendl;
			return false;
		}

		F32* normals = (F32*)norm.get();
		LLVector4a* src = vf.mNormals;
		LLVector4a* end = src + num_vertices;
		LLVector4a normal;
		while (src < end)
		{
			mat_normal.rotate(*src++, normal);
			normal.store4a(normals);
			normals += 4;
		}
	}

	if (rebuild_tangent)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_TANGENT);
		result = mVertexBuffer->getTangentStrider(tangent, mGeomIndex,
												  mGeomCount);
		if (!result)
		{
			llwarns << "getTangentStrider() failed !" << llendl;
			return false;
		}

		F32* tangents = (F32*)tangent.get();

		mVObjp->getVolume()->genTangents(f);

		LLVector4Logical mask;
		mask.clear();
		mask.setElement<3>();

		LLVector4a* src = vf.mTangents;
		LLVector4a* end = vf.mTangents + num_vertices;

		LLVector4a tangent_out;
		while (src < end)
		{
			mat_normal.rotate(*src, tangent_out);
#if 1		// Note: removed from LL's PBR code. Is it safe ?  Kept for now. HB
			tangent_out.normalize3fast();
#endif
			tangent_out.setSelectWithMask(mask, *src++, tangent_out);
			tangent_out.store4a(tangents);
			tangents += 4;
		}
	}

	if (rebuild_weights && vf.mWeights)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_WEIGHTS);
		result = mVertexBuffer->getWeight4Strider(wght, mGeomIndex,
												  mGeomCount);
		if (!result)
		{
			llwarns << "getWeight4Strider() failed !" << llendl;
			return false;
		}
		F32* weights = (F32*)wght.get();
		LLVector4a::memcpyNonAliased16(weights, (F32*)vf.mWeights,
									   num_vertices * 4 * sizeof(F32));
	}

	if (rebuild_color &&
		mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_COLOR))
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_COLOR);
		result = mVertexBuffer->getColorStrider(colors, mGeomIndex,
												mGeomCount);
		if (!result)
		{
			llwarns << "getColorStrider() failed !" << llendl;
			return false;
		}

		U32 vec[4];
		vec[0] = vec[1] = vec[2] = vec[3] = color.asRGBA();

		LLVector4a src;
		src.loadua((F32*)vec);

		F32* dst = (F32*)colors.get();
		U32 num_vecs = (num_vertices + 3) / 4; // Rounded up
		for (U32 i = 0; i < num_vecs; ++i)
		{
			src.store4a(dst);
			dst += 4;
		}
	}

	if (rebuild_emissive)
	{
		LL_FAST_TIMER(FTM_FACE_GEOM_EMISSIVE);
		LLStrider<LLColor4U> emissive;
		result = mVertexBuffer->getEmissiveStrider(emissive, mGeomIndex,
												   mGeomCount);
		if (!result)
		{
			llwarns << "getEmissiveStrider() failed !" << llendl;
			return false;
		}

		F32 glowf = llmax(0.f, getTextureEntry()->getGlow());
		U8 glow = (U8)llmin((S32)(glowf * 255.f), 255);

		LLColor4U glow4u = LLColor4U(0, 0, 0, glow);
		U32 glow32 = glow4u.asRGBA();

		U32 vec[4];
		vec[0] = vec[1] = vec[2] = vec[3] = glow32;

		LLVector4a src;
		src.loadua((F32*)vec);

		F32* dst = (F32*)emissive.get();
		U32 num_vecs = (num_vertices + 3) / 4; // Rounded up
		for (U32 i = 0; i < num_vecs; ++i)
		{
			src.store4a(dst);
			dst += 4;
		}
	}

	if (rebuild_tcoord)
	{
		mTexExtents[0].set(0.f, 0.f);
		mTexExtents[1].set(1.f, 1.f);
		xform(mTexExtents[0], cos_ang, sin_ang, os, ot, ms, mt);
		xform(mTexExtents[1], cos_ang, sin_ang, os, ot, ms, mt);

		F32 es = vf.mTexCoordExtents[1].mV[0] - vf.mTexCoordExtents[0].mV[0];
		F32 et = vf.mTexCoordExtents[1].mV[1] - vf.mTexCoordExtents[0].mV[1];
		mTexExtents[0][0] *= es;
		mTexExtents[1][0] *= es;
		mTexExtents[0][1] *= et;
		mTexExtents[1][1] *= et;
	}

	return true;
}

// Check if the face has a media
bool LLFace::hasMedia() const
{
	if (mHasMedia)
	{
		return true;
	}

	LLViewerTexture* tex = mTexture[LLRender::DIFFUSE_MAP].get();
	if (tex)
	{
		return tex->hasParcelMedia();
	}

	return false; // No media.
}

constexpr F32 LEAST_IMPORTANCE = 0.05f;
constexpr F32 LEAST_IMPORTANCE_FOR_LARGE_IMAGE = 0.3f;

void LLFace::resetVirtualSize()
{
	setVirtualSize(0.f);
	mImportanceToCamera = 0.f;
}

F32 LLFace::getTextureVirtualSize()
{
	F32 radius;
	F32 cos_angle_to_view_dir;
	bool in_frustum = calcPixelArea(cos_angle_to_view_dir, radius);

	if (mPixelArea < F_ALMOST_ZERO || !in_frustum)
	{
		setVirtualSize(0.f);
		return 0.f;
	}

	// Get area of circle in texture space
	LLVector2 tdim = mTexExtents[1] - mTexExtents[0];
	F32 texel_area = (tdim * 0.5f).lengthSquared() * F_PI;
	if (texel_area <= 0)
	{
		// If animated, use minimum (1/64) to avoid blur. HB
		if (isState(TEXTURE_ANIM))
		{
			texel_area = 0.015625f;
		}
		else
		{
			// Take into account the (diffuse) texture scaling to avoid blur,
			// (e.g. on displays with scripted changing text), or on the
			// contrary, to load at an excessive resolution textures with a
			// high repeat per face. HB
			const LLTextureEntry* tep = getTextureEntry();
			if (tep)
			{
				texel_area = tep->getScaleS() * tep->getScaleT();
			}
			else
			{
				texel_area = 1.f;	// Use default.
			}
		}
	}

	F32 face_area;
	if (mVObjp->isSculpted() && texel_area > 1.f)
	{
		// Sculpts can break assumptions about texel area
		face_area = mPixelArea;
	}
	else
	{
		// Apply texel area to face area to get accurate ratio
		// face_area /= llclamp(texel_area, 1.f/64.f, 16.f);
		face_area =  mPixelArea / llclamp(texel_area, 0.015625f, 128.f);
	}

	face_area = adjustPixelArea(mImportanceToCamera, face_area);

	// If it is a large image, shrink face_area by considering the partial
	// overlapping:
	if (mImportanceToCamera < 1.f &&
		face_area > LLViewerTexture::sMinLargeImageSize &&
		mImportanceToCamera > LEAST_IMPORTANCE_FOR_LARGE_IMAGE)
	{
		LLViewerTexture* tex = mTexture[LLRender::DIFFUSE_MAP].get();
		if (tex && tex->isLargeImage())
		{
			face_area *= adjustPartialOverlapPixelArea(cos_angle_to_view_dir,
													   radius);
		}
	}

	setVirtualSize(face_area);

	return face_area;
}

bool LLFace::calcPixelArea(F32& cos_angle_to_view_dir, F32& radius)
{
	// VECTORIZE THIS
	// Get area of circle around face
	LLVector4a center;
	LLVector4a size;
	if (isState(LLFace::RIGGED))
	{
		// Override with avatar bounding box
		LLVOAvatar* avatarp = mVObjp->getAvatar();
		if (!avatarp || avatarp->isDead() || !avatarp->mDrawable)
		{
			return false;
		}
		center.load3(avatarp->getPositionAgent().mV);
		const LLVector4a* exts = avatarp->mDrawable->getSpatialExtents();
		size.setSub(exts[1], exts[0]);
	}
	else
	{
		center.load3(getPositionAgent().mV);
		size.setSub(mExtents[1], mExtents[0]);
	}
	size.mul(0.5f);

	F32 size_squared = size.dot3(size).getF32();
	LLVector4a t;
	t.load3(gViewerCamera.getOrigin().mV);
	LLVector4a look_at;
	look_at.setSub(center, t);

	F32 dist = look_at.getLength3().getF32();
	dist = llmax(dist - size.getLength3().getF32(), 0.001f);
	// Ramp down distance for nearby objects
	if (dist < 16.f)
	{
		dist *= 0.0625f; // /= 16.f;
		dist *= dist;
		dist *= 16.f;
	}

	look_at.normalize3fast();

	// Get area of circle around node
	F32 app_angle = atanf(sqrtf(size_squared) / dist);
	radius = app_angle * LLDrawable::sCurPixelAngle;
	mPixelArea = radius * radius * F_PI;
	LLVector4a x_axis;
	x_axis.load3(gViewerCamera.getXAxis().mV);
	cos_angle_to_view_dir = look_at.dot3(x_axis).getF32();

	// If face has media, check if the face is out of the view frustum.
	if (hasMedia())
	{
		if (!gViewerCamera.AABBInFrustum(center, size))
		{
			mImportanceToCamera = 0.f;
			return false;
		}
		if (cos_angle_to_view_dir > gViewerCamera.getCosHalfFov())
		{
			// The center is within the view frustum
			cos_angle_to_view_dir = 1.f;
		}
		else
		{
			LLVector4a d;
			d.setSub(look_at, x_axis);

			if (dist * dist * d.dot3(d) < size_squared)
			{
				cos_angle_to_view_dir = 1.f;
			}
		}
	}

	if (dist < mBoundingSphereRadius) // Camera is very close
	{
		cos_angle_to_view_dir = 1.f;
		mImportanceToCamera = 1.f;
	}
	else
	{
		mImportanceToCamera = calcImportanceToCamera(cos_angle_to_view_dir,
													 dist);
	}

	return true;
}

// The projection of the face partially overlaps with the screen
F32 LLFace::adjustPartialOverlapPixelArea(F32 cos_angle_to_view_dir,
										  F32 radius)
{
	F32 screen_radius = (F32)llmax(gViewerWindowp->getWindowDisplayWidth(),
								   gViewerWindowp->getWindowDisplayHeight());
	F32 center_angle = acosf(cos_angle_to_view_dir);
	F32 d = center_angle * LLDrawable::sCurPixelAngle;
	if (d + radius <= screen_radius + 5.f)
	{
		return 1.f;
	}

#if 0	// Calculating the intersection area of two circles is too expensive
	F32 radius_square = radius * radius;
	F32 d_square = d * d;
	F32 screen_radius_square = screen_radius * screen_radius;
	face_area = radius_square *
				acosf((d_square + radius_square - screen_radius_square) /
					  (2.f * d * radius)) +
				screen_radius_square *
				acosf((d_square + screen_radius_square - radius_square) /
					  (2.f * d * screen_radius)) -
				0.5f * sqrtf((-d + radius + screen_radius) *
				(d + radius - screen_radius) * (d - radius + screen_radius) *
				(d + radius + screen_radius));
	return face_area;
#else	// This is a good estimation: bounding box of the bounding sphere:
	F32 alpha = llclamp(0.5f * (radius + screen_radius - d) / radius,
						0.f, 1.f);
	return alpha * alpha;
#endif
}

constexpr S8 FACE_IMPORTANCE_LEVEL = 4;

// { distance, importance_weight }
static const F32 FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[FACE_IMPORTANCE_LEVEL][2] =
{
	{ 16.1f, 1.f },
	{ 32.1f, 0.5f },
	{ 48.1f, 0.2f },
	{ 96.1f, 0.05f }
};

// { cosf(angle), importance_weight }
static const F32 FACE_IMPORTANCE_TO_CAMERA_OVER_ANGLE[FACE_IMPORTANCE_LEVEL][2] =
{
	{ 0.985f /*cosf(10 degrees)*/, 1.f },
	{ 0.94f  /*cosf(20 degrees)*/, 0.8f },
	{ 0.866f /*cosf(30 degrees)*/, 0.64f },
	{ 0.f, 0.36f }
};

//static
F32 LLFace::calcImportanceToCamera(F32 cos_angle_to_view_dir, F32 dist)
{
	if (cos_angle_to_view_dir <= gViewerCamera.getCosHalfFov() ||
		dist >= FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[FACE_IMPORTANCE_LEVEL - 1][0])
	{
		return 0.f;
	}

	if (gViewerCamera.getAverageSpeed() > 10.f ||
		gViewerCamera.getAverageAngularSpeed() > 1.f)
	{
		// If camera moves or rotates too fast, ignore the importance factor
		return 0.f;
	}

	S32 i = 0;
	while (i < FACE_IMPORTANCE_LEVEL &&
		   dist > FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[i][0])
	{
		 ++i;
	}
	i = llmin(i, FACE_IMPORTANCE_LEVEL - 1);
	F32 dist_factor = FACE_IMPORTANCE_TO_CAMERA_OVER_DISTANCE[i][1];

	i = 0;
	while (i < FACE_IMPORTANCE_LEVEL &&
		   cos_angle_to_view_dir < FACE_IMPORTANCE_TO_CAMERA_OVER_ANGLE[i][0])
	{
		 ++i;
	}
	i = llmin(i, FACE_IMPORTANCE_LEVEL - 1);

	return dist_factor * FACE_IMPORTANCE_TO_CAMERA_OVER_ANGLE[i][1];
}

//static
F32 LLFace::adjustPixelArea(F32 importance, F32 pixel_area)
{
	if (pixel_area > LLViewerTexture::sMaxSmallImageSize)
	{
		if (importance < LEAST_IMPORTANCE)
		{
			// If the face is not important, do not load hi-res.
			constexpr F32 MAX_LEAST_IMPORTANCE_IMAGE_SIZE = 128.f * 128.f;
			pixel_area = llmin(pixel_area * 0.5f,
							   MAX_LEAST_IMPORTANCE_IMAGE_SIZE);
		}
		else if (pixel_area > LLViewerTexture::sMinLargeImageSize)
		{
			// If is large image, shrink face_area by considering the partial
			// overlapping.
			if (importance < LEAST_IMPORTANCE_FOR_LARGE_IMAGE)
			{
				// If the face is not important, do not load hi-res.
				pixel_area = LLViewerTexture::sMinLargeImageSize;
			}
		}
	}

	return pixel_area;
}

bool LLFace::verify(const U32* indices_array) const
{
	bool ok = true;

	if (mVertexBuffer.isNull())
	{
	 	// No vertex buffer, face is implicitly valid
		return true;
	}

	// First, check whether the face data fits within the pool's range.
	if (mGeomIndex + mGeomCount > mVertexBuffer->getNumVerts())
	{
		ok = false;
		llinfos << "Face references invalid vertices !" << llendl;
	}

	U32 indices_count = getIndicesCount();
	if (!indices_count)
	{
		return true;
	}

	if (indices_count > LL_MAX_INDICES_COUNT)
	{
		ok = false;
		llinfos << "Face has bogus indices count" << llendl;
	}

	if (mIndicesIndex + mIndicesCount > mVertexBuffer->getNumIndices())
	{
		ok = false;
		llinfos << "Face references invalid indices !" << llendl;
	}

#if 0
	U32 geom_start = getGeomStart();
	U32 geom_count = mGeomCount;

	const U32* indicesp = indices_array ? indices_array + mIndicesIndex
										: getRawIndices();

	for (U32 i = 0; i < indices_count; ++i)
	{
		U32 delta = indicesp[i] - geom_start;
		if (0 > delta)
		{
			llwarns << "Face index too low !" << llendl;
			llinfos << "i:" << i << " Index:" << indicesp[i] << " GStart: "
					<< geom_start << llendl;
			ok = false;
		}
		else if (delta >= geom_count)
		{
			llwarns << "Face index too high !" << llendl;
			llinfos << "i:" << i << " Index:" << indicesp[i] << " GEnd: "
					<< geom_start + geom_count << llendl;
			ok = false;
		}
	}
#endif

	if (!ok)
	{
		printDebugInfo();
	}
	return ok;
}

const LLColor4& LLFace::getRenderColor() const
{
	if (isState(USE_FACE_COLOR))
	{
		return mFaceColor; // Face Color
	}
	const LLTextureEntry* tep = getTextureEntry();
	return tep ? tep->getColor() : LLColor4::white;
}

bool LLFace::canBatchTexture() const
{
	const LLTextureEntry* tep = getTextureEntry();
	if (!tep || tep->getBumpmap() || tep->getMaterialParams().notNull())
	{
		// Bump maps and materials do not work with texture batching yet
		return false;
	}

	if (gUsePBRShaders && tep->getGLTFRenderMaterial())
	{
		// PBR materials break indexed texture batching
		return false;
	}

	// Optionally disable glow batching (trying to determine if glow batching
	// would be causing some render glitches, such as bogus glow flickering on
	// no-glow faces). HB
	static LLCachedControl<bool> batch_glow(gSavedSettings, "RenderBatchGlow");
	if (tep->hasGlow() && !batch_glow)
	{
		return false;
	}

	const LLViewerTexture* tex = getTexture();
	if (tex && tex->getPrimaryFormat() == GL_ALPHA)
	{
		// Cannot batch invisiprims
		return false;
	}

	if (isState(TEXTURE_ANIM) && getVirtualSize() >= MIN_TEX_ANIM_SIZE)
	{
		// Texture animation breaks batches
		return false;
	}

	return true;
}

const LLMatrix4& LLFace::getRenderMatrix() const
{
	return mDrawablep->getRenderMatrix();
}

void LLFace::renderIndexed(U32 mask)
{
	if (mVertexBuffer.isNull())
	{
		return;
	}
	if (gUsePBRShaders)
	{
		mVertexBuffer->setBuffer();
	}
	else if (!mDrawPoolp)
	{
		return;
	}
	else
	{
		if (!mask)
		{
			mask = mDrawPoolp->getVertexDataMask();
		}
		mVertexBuffer->setBuffer(mask);
	}
	mVertexBuffer->drawRange(LLRender::TRIANGLES, mGeomIndex,
							 mGeomIndex + mGeomCount - 1, mIndicesCount,
							 mIndicesIndex);
}

S32 LLFace::getColors(LLStrider<LLColor4U>& colors)
{
	if (!mGeomCount)
	{
		return -1;
	}

	mVertexBuffer->getColorStrider(colors, mGeomIndex, mGeomCount);

	return mGeomIndex;
}

S32	LLFace::getIndices(LLStrider<U16>& indicesp)
{
	mVertexBuffer->getIndexStrider(indicesp, mIndicesIndex, mIndicesCount);
	llassert(indicesp[0] != indicesp[1]);
	return mIndicesIndex;
}

LLVector3 LLFace::getPositionAgent() const
{
	if (mDrawablep.isNull() || mDrawablep->isStatic())
	{
		return mCenterAgent;
	}
	return mCenterLocal * getRenderMatrix();
}

LLViewerTexture* LLFace::getTexture(U32 ch) const
{
	if (ch < LLRender::NUM_TEXTURE_CHANNELS)
	{
		return mTexture[ch];
	}
	llassert (false);
	return NULL;
}

void LLFace::setVertexBuffer(LLVertexBuffer* buffer)
{
	mVertexBuffer = buffer;
	llassert(verify());
}

void LLFace::clearVertexBuffer()
{
	if (mVertexBuffer.notNull())
	{
		mVertexBuffer = NULL;
	}
}

S32 LLFace::getRiggedIndex(U32 type) const
{
	if (mRiggedIndex.empty())
	{
		return -1;
	}

	llassert(type < mRiggedIndex.size());

	return mRiggedIndex[type];
}

U64 LLFace::getSkinHash() const
{
	return mSkinInfo.notNull() ? mSkinInfo->mHash : 0;
}

bool LLFace::isInAlphaPool() const
{
	if (mPoolType == LLDrawPool::POOL_ALPHA)
	{
		return true;
	}
	if (gUsePBRShaders &&
		(mPoolType == LLDrawPool::POOL_ALPHA_PRE_WATER ||
		 mPoolType == LLDrawPool::POOL_ALPHA_POST_WATER))
	{
		return true;
	}
	return false;
}
