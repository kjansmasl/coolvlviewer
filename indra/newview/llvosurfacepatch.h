/**
 * @file llvosurfacepatch.h
 * @brief Description of LLVOSurfacePatch class
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

#ifndef LL_VOSURFACEPATCH_H
#define LL_VOSURFACEPATCH_H

#include "llstrider.h"

#include "llface.h"
#include "llviewerobject.h"
#include "llviewerregion.h"

class LLDrawPool;
class LLFace;
class LLFacePool;
class LLSurfacePatch;
class LLVector2;

class LLVOSurfacePatch final : public LLStaticViewerObject
{
protected:
	LOG_CLASS(LLVOSurfacePatch);

public:
	enum
	{
		VERTEX_DATA_MASK =	(1 << LLVertexBuffer::TYPE_VERTEX) |
							(1 << LLVertexBuffer::TYPE_NORMAL) |
							(1 << LLVertexBuffer::TYPE_TEXCOORD0) |
							(1 << LLVertexBuffer::TYPE_TEXCOORD1)
	};

	LLVOSurfacePatch(const LLUUID& id, LLViewerRegion* regionp);

	void markDead() override;

	static void initClass();

	LL_INLINE U32 getPartitionType() const override		{ return LLViewerRegion::PARTITION_TERRAIN; }

	LLDrawable* createDrawable() override;

	void updateGL() override;
	bool updateGeometry(LLDrawable* drawable) override;
	LL_INLINE bool updateLOD() override					{ return true; }
	void updateFaceSize(S32 idx) override;

	void getGeometry(LLStrider<LLVector3>& verticesp,
					 LLStrider<LLVector3>& normalsp,
					 LLStrider<LLVector2>& texCoords0p,
					 LLStrider<LLVector2>& texCoords1p,
					 LLStrider<U16>& indicesp);

	LL_INLINE void updateTextures() override			{}

	// Generates accurate apparent angle and area:
	void setPixelAreaAndAngle() override;

	void updateSpatialExtents(LLVector4a& new_min,
							  LLVector4a& new_max) override;

	// Whether this object needs to do an idleUpdate:
	LL_INLINE bool isActive() const override			{ return false; }

	void setPatch(LLSurfacePatch* patchp);
	LL_INLINE LLSurfacePatch* getPatch() const			{ return mPatchp; }

	void dirtyPatch();
	void dirtyGeom();

	bool lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
							  // Which face to check, -1=ALL_SIDES
							  S32 face = -1,
							  bool pick_transparent = false,
							  bool pick_rigged = false,
							  // Which face was hit
							  S32* face_hit = NULL,
							  // Intersection point
							  LLVector4a* intersection = NULL,
							  // Texture coordinates of the intersection point
							  LLVector2* tex_coord = NULL,
							  // Surface normal at the intersection point
							  LLVector4a* normal = NULL,
							  // Surface tangent at the intersection point
							  LLVector4a* tangent = NULL) override;

protected:
	~LLVOSurfacePatch() override;

	LLFacePool* getPool();

	void getGeomSizesMain(S32 stride, S32& num_vertices, S32& num_indices);
	void getGeomSizesNorth(S32 stride, S32 north_stride, S32& num_vertices,
						   S32& num_indices);
	void getGeomSizesEast(S32 stride, S32 east_stride, S32& num_vertices,
						  S32& num_indices);

	void updateMainGeometry(LLFace* facep,
							LLStrider<LLVector3>& verticesp,
							LLStrider<LLVector3>& normalsp,
							LLStrider<LLVector2>& texCoords0p,
							LLStrider<LLVector2>& texCoords1p,
							LLStrider<U16>& indicesp,
							U32& index_offset);
	void updateNorthGeometry(LLFace* facep,
							 LLStrider<LLVector3>& verticesp,
							 LLStrider<LLVector3>& normalsp,
							 LLStrider<LLVector2>& texCoords0p,
							 LLStrider<LLVector2>& texCoords1p,
							 LLStrider<U16>& indicesp,
							 U32& index_offset);
	void updateEastGeometry(LLFace* facep,
							LLStrider<LLVector3>& verticesp,
							LLStrider<LLVector3>& normalsp,
							LLStrider<LLVector2>& texCoords0p,
							LLStrider<LLVector2>& texCoords1p,
							LLStrider<U16>& indicesp,
							U32& index_offset);

protected:
	LLFacePool*		mPool;
	LLSurfacePatch* mPatchp;
	S32				mBaseComp;

	S32				mLastNorthStride;
	S32				mLastEastStride;
	S32				mLastStride;
	S32				mLastLength;

	bool			mDirtyTexture;
	bool			mDirtyTerrain;

public:
	bool			mDirtiedPatch;
	static F32		sLODFactor;
};

#endif // LL_VOSURFACEPATCH_H
