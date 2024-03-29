/**
 * @file llphysshapebuilderutil.h
 * @brief Generic system to convert LL(Physics)VolumeParams to physics shapes
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_PHYSICS_SHAPE_BUILDER_H
#define LL_PHYSICS_SHAPE_BUILDER_H

#include "indra_constants.h"
#include "llpreprocessor.h"
#include "llvolume.h"

#define USE_SHAPE_QUANTIZATION 0

#define SHAPE_BUILDER_DEFAULT_VOLUME_DETAIL 1

#define SHAPE_BUILDER_IMPLICIT_THRESHOLD_HOLLOW 0.10f
#define SHAPE_BUILDER_IMPLICIT_THRESHOLD_HOLLOW_SPHERES 0.90f
#define SHAPE_BUILDER_IMPLICIT_THRESHOLD_PATH_CUT 0.05f
#define SHAPE_BUILDER_IMPLICIT_THRESHOLD_TAPER 0.05f
#define SHAPE_BUILDER_IMPLICIT_THRESHOLD_TWIST 0.09f
#define SHAPE_BUILDER_IMPLICIT_THRESHOLD_SHEAR 0.05f

constexpr F32 COLLISION_TOLERANCE = 0.1f;

constexpr F32 SHAPE_BUILDER_ENTRY_SNAP_SCALE_BIN_SIZE = 0.15f;
constexpr F32 SHAPE_BUILDER_ENTRY_SNAP_PARAMETER_BIN_SIZE = 0.01f;
constexpr F32 SHAPE_BUILDER_MIN_GEOMETRY_SIZE = 0.5f * COLLISION_TOLERANCE;
constexpr F32 SHAPE_BUILDER_CONVEXIFICATION_SIZE = 2.f * COLLISION_TOLERANCE;
constexpr F32 SHAPE_BUILDER_CONVEXIFICATION_SIZE_MESH = 0.5f;

class LLPhysicsVolumeParams : public LLVolumeParams
{
public:

	LL_INLINE LLPhysicsVolumeParams(const LLVolumeParams& params,
									bool force_convex)
	:	LLVolumeParams(params),
		mForceConvex(force_convex)
	{
	}

	LL_INLINE bool operator==(const LLPhysicsVolumeParams& params) const
	{
		return (LLVolumeParams::operator==(params) &&
				mForceConvex == params.mForceConvex);
	}

	LL_INLINE bool operator!=(const LLPhysicsVolumeParams& params) const
	{
		return !operator==(params);
	}

	LL_INLINE bool operator<(const LLPhysicsVolumeParams& params) const
	{
		if (LLVolumeParams::operator!=(params))
		{
			return LLVolumeParams::operator<(params);
		}
		return !params.mForceConvex && mForceConvex;
	}

	LL_INLINE bool shouldForceConvex() const	{ return mForceConvex; }

private:
	bool mForceConvex;
};

// Purely static class
class LLPhysShapeBuilderUtil
{
public:
	LLPhysShapeBuilderUtil() = delete;
	~LLPhysShapeBuilderUtil() = delete;

	class ShapeSpec
	{
		friend class LLPhysShapeBuilderUtil;

	public:
		enum ShapeType
		{
			// Primitive types
			BOX,
			SPHERE,
			CYLINDER,

			// User specified they wanted the convex hull of the volume
			USER_CONVEX,

			// Either a volume that is inherently convex but not a primitive
			// type, or a shape with dimensions such that will convexify it
			// anyway.
			PRIM_CONVEX,

			// Special case for traditional sculpts--they are the convex hull
			// of a single particular set of volume params
 			SCULPT,

			// A user mesh. May or may not contain a convex decomposition.
			USER_MESH,

			// A non-convex volume which we have to represent accurately
			PRIM_MESH,

			INVALID
		};

		LL_INLINE ShapeSpec()
		:	mType(INVALID),
			mScale(0.f, 0.f, 0.f),
			mCenter(0.f, 0.f, 0.f)
		{
		}

		LL_INLINE bool isConvex()
		{
			return mType != USER_MESH && mType != PRIM_MESH &&
				   mType != INVALID;
		}

		LL_INLINE bool isMesh()
		{
			return mType == USER_MESH || mType == PRIM_MESH;
		}

		LL_INLINE ShapeType getType()			{ return mType; }
		LL_INLINE const LLVector3& getScale()	{ return mScale; }
		LL_INLINE const LLVector3& getCenter()	{ return mCenter; }

	private:
		ShapeType mType;

		// Dimensions of an AABB around the shape
		LLVector3 mScale;

		// Offset of shape from origin of primitive's reference frame
		LLVector3 mCenter;
	};

	static void getPhysShape(const LLPhysicsVolumeParams& vol_params,
							 const LLVector3& scale, bool has_decomp,
							 ShapeSpec& spec_out);
};

#endif //LL_PHYSICS_SHAPE_BUILDER_H
