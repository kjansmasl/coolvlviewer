/**
 * @file llvolume.cpp
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "linden_common.h"

#if !LL_WINDOWS
# include <stdint.h>
#endif
#include <utility>				// For std::swap()

#include "mikktspace/mikktspace.h"

#include "meshoptimizer.h"

#include "llmemory.h"

#include "llvolume.h"

#include "llmatrix4.h"
#include "llmatrix3.h"
#include "llmatrix3a.h"
#include "llmeshoptimizer.h"
#include "lloctree.h"
#include "llsdserialize.h"
#include "lltimer.h"
#include "llvolumemgr.h"
#include "llvolumeoctree.h"
#if LL_OPENMP
# include "llatomic.h"
# include "llthread.h"			// For is_main_thread()
#endif
#include "indra_constants.h"

// Insert mikktspace implementation into llvolume object file
#include "mikktspace/mikktspace.c"

static const F32 sTableScale[] = {
	1.f, 1.f, 1.f, 0.5f, 0.707107f, 0.53f, 0.525f, 0.5f
};

// This avoids having to import llrender headers in llprimitive
extern bool gDebugGL;
extern bool gUsePBRShaders;

bool LLLineSegmentBoxIntersect(const LLVector3& start, const LLVector3& end,
							   const LLVector3& center, const LLVector3& size)
{
	return LLLineSegmentBoxIntersect(start.mV, end.mV, center.mV, size.mV);
}

bool LLLineSegmentBoxIntersect(const F32* start, const F32* end,
							   const F32* center, const F32* size)
{
	F32 fAWdU[3];
	F32 dir[3];
	F32 diff[3];

	for (U32 i = 0; i < 3; ++i)
	{
		dir[i] = 0.5f * (end[i] - start[i]);
		diff[i] = (0.5f * (end[i] + start[i])) - center[i];
		fAWdU[i] = fabsf(dir[i]);
		if (fabsf(diff[i]) > size[i] + fAWdU[i])
		{
			return false;
		}
	}

	float f;
	f = dir[1] * diff[2] - dir[2] * diff[1];
	if (fabsf(f) > size[1] * fAWdU[2] + size[2] * fAWdU[1])
	{
		return false;
	}
	f = dir[2] * diff[0] - dir[0] * diff[2];
	if (fabsf(f) > size[0] * fAWdU[2] + size[2] * fAWdU[0])
	{
		return false;
	}
	f = dir[0] * diff[1] - dir[1] * diff[0];
	if (fabsf(f) > size[0] * fAWdU[1] + size[1] * fAWdU[0])
	{
		return false;
	}

	return true;
}

// Finds tangent vector based on three vertices with texture coordinates.
// Fills in dummy values if the triangle has degenerate texture coordinates.
void calc_tangent_from_triangle(LLVector4a& normal, LLVector4a& tangent_out,
								const LLVector4a& v1, const LLVector2&  w1,
								const LLVector4a& v2, const LLVector2&  w2,
								const LLVector4a& v3, const LLVector2&  w3)
{
	const F32* v1ptr = v1.getF32ptr();
	const F32* v2ptr = v2.getF32ptr();
	const F32* v3ptr = v3.getF32ptr();

	F32 x1 = v2ptr[0] - v1ptr[0];
	F32 x2 = v3ptr[0] - v1ptr[0];
	F32 y1 = v2ptr[1] - v1ptr[1];
	F32 y2 = v3ptr[1] - v1ptr[1];
	F32 z1 = v2ptr[2] - v1ptr[2];
	F32 z2 = v3ptr[2] - v1ptr[2];

	F32 s1 = w2.mV[0] - w1.mV[0];
	F32 s2 = w3.mV[0] - w1.mV[0];
	F32 t1 = w2.mV[1] - w1.mV[1];
	F32 t2 = w3.mV[1] - w1.mV[1];

	F32 rd = s1 * t2 - s2 * t1;

	F32 r = rd * rd > FLT_EPSILON ? 1.f / rd
								  : (rd > 0.f ? 1024.f : -1024.f);

	llassert(llfinite(r));
	llassert(!llisnan(r));

	LLVector4a sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
					(t2 * z1 - t1 * z2) * r);

	LLVector4a tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
					(s1 * z2 - s2 * z1) * r);

	LLVector4a n = normal;
	LLVector4a t = sdir;

	LLVector4a ncrosst;
	ncrosst.setCross3(n, t);

	// Gram-Schmidt orthogonalize
	n.mul(n.dot3(t).getF32());

	LLVector4a tsubn;
	tsubn.setSub(t, n);

	if (tsubn.dot3(tsubn).getF32() > F_APPROXIMATELY_ZERO)
	{
		tsubn.normalize3fast_checked();

		// Calculate handedness
		F32 handedness = ncrosst.dot3(tdir).getF32() < 0.f ? -1.f : 1.f;

		tsubn.getF32ptr()[3] = handedness;

		tangent_out = tsubn;
	}
	else
	{
		// Degenerate, make up a value
		tangent_out.set(0.f, 0.f, 1.f, 1.f);
	}
}

// Intersect test between triangle vert0, vert1, vert2 and a ray from orig in
// direction dir. Returns true if intersecting and returns barycentric
// coordinates in intersection_a, intersection_b, and returns the intersection
// point along dir in intersection_t.
// Moller-Trumbore algorithm
bool LLTriangleRayIntersect(const LLVector4a& vert0, const LLVector4a& vert1,
							const LLVector4a& vert2, const LLVector4a& orig,
							const LLVector4a& dir, F32& intersection_a,
							F32& intersection_b, F32& intersection_t)
{
	// Find vectors for two edges sharing vert0
	LLVector4a edge1;
	edge1.setSub(vert1, vert0);

	LLVector4a edge2;
	edge2.setSub(vert2, vert0);

	// Begin calculating determinant - also used to calculate U parameter
	LLVector4a pvec;
	pvec.setCross3(dir, edge2);

	// If determinant is near zero, ray lies in plane of triangle
	LLVector4a det;
	det.setAllDot3(edge1, pvec);

	if (det.greaterEqual(LLVector4a::getEpsilon()).getGatheredBits() & 0x7)
	{
		// Calculate distance from vert0 to ray origin
		LLVector4a tvec;
		tvec.setSub(orig, vert0);

		// Calculate U parameter and test bounds
		LLVector4a u;
		u.setAllDot3(tvec, pvec);

		if ((u.greaterEqual(LLVector4a::getZero()).getGatheredBits() & 0x7) &&
			(u.lessEqual(det).getGatheredBits() & 0x7))
		{
			// Prepare to test V parameter
			LLVector4a qvec;
			qvec.setCross3(tvec, edge1);

			// Calculate V parameter and test bounds
			LLVector4a v;
			v.setAllDot3(dir, qvec);

			LLVector4a sum_uv;
			sum_uv.setAdd(u, v);

			S32 v_gequal =
				v.greaterEqual(LLVector4a::getZero()).getGatheredBits() & 0x7;
			S32 sum_lequal = sum_uv.lessEqual(det).getGatheredBits() & 0x7;

			if (v_gequal  && sum_lequal)
			{
				// Calculate t, scale parameters, ray intersects triangle
				LLVector4a t;
				t.setAllDot3(edge2, qvec);

				t.div(det);
				u.div(det);
				v.div(det);

				intersection_a = u[0];
				intersection_b = v[0];
				intersection_t = t[0];
				return true;
			}
		}
	}

	return false;
}

bool LLTriangleRayIntersectTwoSided(const LLVector4a& vert0,
									const LLVector4a& vert1,
									const LLVector4a& vert2,
									const LLVector4a& orig,
									const LLVector4a& dir,
									F32& intersection_a,
									F32& intersection_b,
									F32& intersection_t)
{
	F32 u, v, t;

	// Find vectors for two edges sharing vert0
	LLVector4a edge1;
	edge1.setSub(vert1, vert0);

	LLVector4a edge2;
	edge2.setSub(vert2, vert0);

	// Begin calculating determinant - also used to calculate U parameter
	LLVector4a pvec;
	pvec.setCross3(dir, edge2);

	// If determinant is near zero, ray lies in plane of triangle
	F32 det = edge1.dot3(pvec).getF32();

	if (det > -F_APPROXIMATELY_ZERO && det < F_APPROXIMATELY_ZERO)
	{
		return false;
	}

	F32 inv_det = 1.f / det;

	// Calculate distance from vert0 to ray origin
	LLVector4a tvec;
	tvec.setSub(orig, vert0);

	// Calculate U parameter and test bounds
	u = tvec.dot3(pvec).getF32() * inv_det;
	if (u < 0.f || u > 1.f)
	{
		return false;
	}

	// Prepare to test V parameter
	tvec.sub(edge1);

	// Calculate V parameter and test bounds
	v = dir.dot3(tvec).getF32() * inv_det;

	if (v < 0.f || u + v > 1.f)
	{
		return false;
	}

	// Calculate t, ray intersects triangle
	t = edge2.dot3(tvec).getF32() * inv_det;

	intersection_a = u;
	intersection_b = v;
	intersection_t = t;

	return true;
}

// helper for non-aligned vectors
bool LLTriangleRayIntersect(const LLVector3& vert0, const LLVector3& vert1,
							const LLVector3& vert2, const LLVector3& orig,
							const LLVector3& dir, F32& intersection_a,
							F32& intersection_b, F32& intersection_t,
							bool two_sided)
{
	LLVector4a vert0a, vert1a, vert2a, origa, dira;
	vert0a.load3(vert0.mV);
	vert1a.load3(vert1.mV);
	vert2a.load3(vert2.mV);
	origa.load3(orig.mV);
	dira.load3(dir.mV);

	if (two_sided)
	{
		return LLTriangleRayIntersectTwoSided(vert0a, vert1a, vert2a, origa,
											  dira, intersection_a,
											  intersection_b, intersection_t);
	}
	else
	{
		return LLTriangleRayIntersect(vert0a, vert1a, vert2a, origa, dira,
									  intersection_a, intersection_b,
									  intersection_t);
	}
}

// Finds the point on a triangle closest to a given target point algorithm
// derived from:
// http://www.geometrictools.com/Documentation/DistancePoint3Triangle3.pdf
// (returns distance squared and barycentric coordinates)
F32 LLTriangleClosestPoint(const LLVector3& vert0, const LLVector3& vert1,
						   const LLVector3& vert2, const LLVector3& target,
						   F32& closest_a, F32& closest_b)
{
	// Edges of triangle
	LLVector3 edge0 = vert1 - vert0;
	LLVector3 edge1 = vert2 - vert0;

	LLVector3 delta = vert0 - target;

	// Length of triangle edges
	F32 a00 = edge0.lengthSquared();
	F32 a01 = edge0 * edge1;
	F32 a11 = edge1.lengthSquared();

	F32 b0 = delta * edge0;
	F32 b1 = delta * edge1;

	F32 c = delta.lengthSquared();

	F32 det = fabs(a00 * a11 - a01 * a01);

	F32 s = a01 * b1 - a11 * b0;
	F32 t = a01 * b0 - a00 * b1;

	F32 dist_squared;

	if (s + t <= det)
	{
		if (s < 0.f)
		{
			if (t < 0.f)		// region 4
			{
				if (b0 < 0.f)
				{
					t = 0.f;
					if (-b0 >= a00)
					{
						s = 1.f;
						dist_squared = a00 + 2.f * b0 + c;
					}
					else
					{
						s = -b0 / a00;
						dist_squared = b0 * s + c;
					}
				}
				else
				{
					s = 0.f;
					if (b1 >= 0.f)
					{
						t = 0.f;
						dist_squared = c;
					}
					else if (-b1 >= a11)
					{
						t = 1.f;
						dist_squared = a11 + 2.f * b1 + c;
					}
					else
					{
						t = -b1 / a11;
						dist_squared = b1 * t + c;
					}
				}
			}
			else				// region 3
			{
				s = 0.f;
				if (b1 >= 0.f)
				{
					t = 0.f;
					dist_squared = c;
				}
				else if (-b1 >= a11)
				{
					t = 1.f;
					dist_squared = a11 + 2.f * b1 + c;
				}
				else
				{
					t = -b1 / a11;
					dist_squared = b1 * t + c;
				}
			}
		}
		else if (t < 0.f)		// region 5
		{
			t = 0.f;
			if (b0 >= 0.f)
			{
				s = 0.f;
				dist_squared = c;
			}
			else if (-b0 >= a00)
			{
				s = 1.f;
				dist_squared = a00 + 2.f * b0 + c;
			}
			else
			{
				s = -b0 / a00;
				dist_squared = b0 * s + c;
			}
		}
		else					// region 0
		{
			// Minimum at interior point
			F32 det_inv = 1.f / det;
			s *= det_inv;
			t *= det_inv;
			dist_squared = s * (a00 * s + a01 * t + 2.f * b0) +
						   t * (a01 * s + a11 * t + 2.f * b1) + c;
		}
	}
	else
	{
		F32 tmp0, tmp1, numerator, denominator;

		if (s < 0.f)			// region 2
		{
			tmp0 = a01 + b0;
			tmp1 = a11 + b1;
			if (tmp1 > tmp0)
			{
				numerator = tmp1 - tmp0;
				denominator = a00 - 2.f * a01 + a11;
				if (numerator >= denominator)
				{
					s = 1.f;
					t = 0.f;
					dist_squared = a00 + 2.f * b0 + c;
				}
				else
				{
					s = numerator / denominator;
					t = 1.f - s;
					dist_squared = s * (a00 * s + a01 * t + 2.f * b0) +
								   t * (a01 * s + a11 * t + 2.f * b1) + c;
				}
			}
			else
			{
				s = 0.f;
				if (tmp1 <= 0.f)
				{
					t = 1.f;
					dist_squared = a11 + 2.f * b1 + c;
				}
				else if (b1 >= 0.f)
				{
					t = 0.f;
					dist_squared = c;
				}
				else
				{
					t = -b1 / a11;
					dist_squared = b1 * t + c;
				}
			}
		}
		else if (t < 0.f)		// region 6
		{
			tmp0 = a01 + b1;
			tmp1 = a00 + b0;
			if (tmp1 > tmp0)
			{
				numerator = tmp1 - tmp0;
				denominator = a00 - 2.f * a01 + a11;
				if (numerator >= denominator)
				{
					t = 1.f;
					s = 0.f;
					dist_squared = a11 + 2.f * b1 + c;
				}
				else
				{
					t = numerator / denominator;
					s = 1.f - t;
					dist_squared = s * (a00 * s + a01 * t + 2.f * b0) +
								   t * (a01 * s + a11 * t + 2.f * b1) + c;
				}
			}
			else
			{
				t = 0.f;
				if (tmp1 <= 0.f)
				{
					s = 1.f;
					dist_squared = a00 + 2.f * b0 + c;
				}
				else if (b0 >= 0.f)
				{
					s = 0.f;
					dist_squared = c;
				}
				else
				{
					s = -b0 / a00;
					dist_squared = b0 * s + c;
				}
			}
		}
		else					// region 1
		{
			numerator = a11 + b1 - a01 - b0;
			if ( numerator <= 0.f )
			{
				s = 0.f;
				t = 1.f;
				dist_squared = a11 + 2.f * b1 + c;
			}
			else
			{
				denominator = a00 - 2.f * a01 + a11;
				if (numerator >= denominator)
				{
					s = 1.f;
					t = 0.f;
					dist_squared = a00 + 2.f * b0 + c;
				}
				else
				{
					s = numerator / denominator;
					t = 1.f - s;
					dist_squared = s * (a00 * s + a01 * t + 2.f * b0) +
								   t * (a01 * s + a11 * t + 2.f * b1) + c;
				}
			}
		}
	}

	closest_a = s;
	closest_b = t;

	return fabs(dist_squared);
}

class LLVolumeOctreeRebound
:	public LLOctreeTravelerDepthFirstNoOwnership<LLVolumeTriangle>
{
protected:
	LOG_CLASS(LLVolumeOctreeRebound);

public:
	LLVolumeOctreeRebound(const LLVolumeFace* face)
	:	mFace(face)
	{
	}

	void visit(const LLOctreeNodeNoOwnership<LLVolumeTriangle>* branch) override
	{
		// This is a depth first traversal, so it's safe to assume all children
		// have complete bounding data

		LLVolumeOctreeListenerNoOwnership* node =
			(LLVolumeOctreeListenerNoOwnership*)branch->getListener(0);

		LLVector4a& min = node->mExtents[0];
		LLVector4a& max = node->mExtents[1];

		if (!branch->isEmpty())
		{
			// Node has data, find AABB that binds data set
			const LLVolumeTriangle* tri = *(branch->getDataBegin());

			// Initialize min/max to first available vertex
			min = *(tri->mV[0]);
			max = *(tri->mV[0]);

			// For each triangle in node stretch by triangles in node
			for (LLOctreeNodeNoOwnership<LLVolumeTriangle>::const_element_iter
					iter = branch->getDataBegin();
				 iter != branch->getDataEnd(); ++iter)
			{
				tri = *iter;

				min.setMin(min, *tri->mV[0]);
				min.setMin(min, *tri->mV[1]);
				min.setMin(min, *tri->mV[2]);

				max.setMax(max, *tri->mV[0]);
				max.setMax(max, *tri->mV[1]);
				max.setMax(max, *tri->mV[2]);
			}
		}
		else if (branch->getChildCount())
		{
			// No data, but child nodes exist
			LLVolumeOctreeListenerNoOwnership* child =
				(LLVolumeOctreeListenerNoOwnership*)branch->getChild(0)->getListener(0);

			// Initialize min/max to extents of first child
			min = child->mExtents[0];
			max = child->mExtents[1];
		}
		else if (branch->isLeaf())
		{
			llwarns << "Empty leaf" << llendl;
			return;
		}

		for (S32 i = 0, count = branch->getChildCount(); i < count; ++i)
		{
			// Stretch by child extents
			LLVolumeOctreeListenerNoOwnership* child =
				(LLVolumeOctreeListenerNoOwnership*)branch->getChild(i)->getListener(0);
			min.setMin(min, child->mExtents[0]);
			max.setMax(max, child->mExtents[1]);
		}

		node->mBounds[0].setAdd(min, max);
		node->mBounds[0].mul(0.5f);

		node->mBounds[1].setSub(max, min);
		node->mBounds[1].mul(0.5f);
	}

public:
	const LLVolumeFace* mFace;
};

LLProfile::Face* LLProfile::addCap(S16 face_id)
{
	size_t count = mFaces.size();
	mFaces.resize(count + 1);
	Face* facep = &(mFaces[count]);
	if (LL_UNLIKELY(!facep))
	{
		LLMemory::allocationFailed();
		llwarns << "Out of memory, face not added !" << llendl;
		return NULL;
	}
	facep->mIndex = 0;
	facep->mCount = mTotal;
	facep->mScaleU = 1.f;
	facep->mCap = true;
	facep->mFaceID = face_id;
	return facep;
}

LLProfile::Face* LLProfile::addFace(S32 i, S32 count, F32 u_scale, S16 face_id,
									bool flat)
{
	size_t faces = mFaces.size();
	mFaces.resize(faces + 1);
	Face* facep = &(mFaces[faces]);
	if (LL_UNLIKELY(!facep))
	{
		LLMemory::allocationFailed();
		llwarns << "Out of memory, face not added !" << llendl;
		return NULL;
	}
	facep->mIndex = i;
	facep->mCount = count;
	facep->mScaleU = u_scale;
	facep->mFlat = flat;
	facep->mCap = false;
	facep->mFaceID = face_id;
	return facep;
}

// This is basically LLProfile::genNGon stripped down to only the operations
// that influence the number of points
//static
S32 LLProfile::getNumNGonPoints(const LLProfileParams& params, S32 sides,
								F32 ang_scale, S32 split)
{
	// Generate an n-sided "circular" path. 0 is (1,0), and we go counter-
	// clockwise along a circular path from there.

	F32 begin = params.getBegin();
	F32 end = params.getEnd();

	F32 t_step = 1.f / (F32)sides;

	F32 t_first = floor(begin * sides) / (F32)sides;

	// pt1 is the first point on the fractional face. Starting t and ang values
	// for the first face. Increment to the next point. pt2 is the end point on
	// the fractional face.
	F32 t = t_first + t_step;

	F32 t_fraction = (begin - t_first) * sides;

	// Only use if it is not almost exactly on an edge.
	S32 np = 0;
	if (t_fraction < 0.9999f)
	{
		++np;
	}

	// There's lots of potential here for floating point error to generate
	// unneeded extra points - DJS 04/05/02
	while (t < end)
	{
		// Iterate through all the integer steps of t.
		++np;
		t += t_step;
	}

	// Find the fraction that we need to add to the end point.
	t_fraction = (end - t + t_step) * sides;
	if (t_fraction > 0.0001f)
	{
		++np;
	}

	// If we are sliced, the profile is open.
	if ((end - begin) * ang_scale < 0.99f)
	{
		if (params.getHollow() <= 0)
		{
			// Put center point if not hollow.
			++np;
		}
	}

	return np;
}

void LLProfile::genNGon(const LLProfileParams& params, S32 sides, F32 offset,
						F32 ang_scale, S32 split)
{
	// Generate an n-sided "circular" path. 0 is (1,0), and we go counter-
	// clockwise along a circular path from there
	F32 begin = params.getBegin();
	F32 end = params.getEnd();
	F32 t_step = 1.f / sides;
	F32 ang_step = 2.f * F_PI * t_step * ang_scale;

	// Scale to have size "match" scale. Compensates to get object to generally
	// fill bounding box.

	// Total number of sides all around:
	S32 total_sides = ll_roundp(sides / ang_scale);

	F32 scale = 0.5f;
	if (total_sides < 8)
	{
		scale = sTableScale[total_sides];
	}

	F32 t_first = floor(begin * sides) / (F32)sides;

	// pt1 is the first point on the fractional face.
	// Starting t and ang values for the first face
	F32 t = t_first;
	F32 ang = 2.f * F_PI * (t * ang_scale + offset);
	LLVector4a pt1;
	pt1.set(cosf(ang) * scale, sinf(ang) * scale, t);

	// Increment to the next point. pt2 is the end point on the fractional face
	t += t_step;
	ang += ang_step;
	LLVector4a pt2;
	pt2.set(cosf(ang) * scale, sinf(ang) * scale, t);

	F32 t_fraction = (begin - t_first) * sides;

	// Only use if it is not almost exactly on an edge.
	if (t_fraction < 0.9999f)
	{
		LLVector4a new_pt;
		new_pt.setLerp(pt1, pt2, t_fraction);
		mVertices.push_back(new_pt);
	}

	// There is lots of potential here for floating point error to generate
	// unneeded extra points - DJS 04/05/02
	while (t < end)
	{
		// Iterate through all the integer steps of t.
		pt1.set(cosf(ang) * scale, sinf(ang) * scale, t);

		if (mVertices.size() > 0)
		{
			LLVector4a p = mVertices[mVertices.size() - 1];
			LLVector4a new_pt;
			for (S32 i = 0; i < split && mVertices.size() > 0; ++i)
			{
				new_pt.setSub(pt1, p);
				new_pt.mul(1.f / (F32)(split + 1) * (F32)(i + 1));
				new_pt.add(p);
				mVertices.push_back(new_pt);
			}
		}
		mVertices.push_back(pt1);

		t += t_step;
		ang += ang_step;
	}

	// pt1 is the first point on the fractional face
	// pt2 is the end point on the fractional face
	pt2.set(cosf(ang) * scale, sinf(ang) * scale, t);

	// Find the fraction that we need to add to the end point.
	t_fraction = (end - t + t_step) * sides;
	if (t_fraction > 0.0001f)
	{
		LLVector4a new_pt;
		new_pt.setLerp(pt1, pt2, t_fraction);

		if (mVertices.size() > 0)
		{
			LLVector4a p = mVertices[mVertices.size() - 1];
			for (S32 i = 0; i < split && mVertices.size() > 0; ++i)
			{
				LLVector4a pt1;
				pt1.setSub(new_pt, p);
				pt1.mul(1.f / (F32)(split + 1) * (F32)(i + 1));
				pt1.add(p);
				mVertices.push_back(pt1);
			}
		}
		mVertices.push_back(new_pt);
	}

	// If we are sliced, the profile is open.
	if ((end - begin) * ang_scale < 0.99f)
	{
		if ((end - begin) * ang_scale > 0.5f)
		{
			mConcave = true;
		}
		else
		{
			mConcave = false;
		}
		mOpen = true;
		if (params.getHollow() <= 0)
		{
			// Put center point if not hollow.
			mVertices.push_back(LLVector4a(0.f, 0.f, 0.f));
		}
	}
	else
	{
		// The profile is not open.
		mOpen = false;
		mConcave = false;
	}

	mTotal = mVertices.size();
}

// Hollow is percent of the original bounding box, not of this particular
// profile's geometry. Thus, a swept triangle needs lower hollow values than a
// swept square. Note that addHole will NOT work for non-"circular" profiles,
// if we ever decide to use them.
LLProfile::Face* LLProfile::addHole(const LLProfileParams& params, bool flat,
									F32 sides, F32 offset, F32 box_hollow,
									F32 ang_scale, S32 split)
{
	// Total add has number of vertices on outside.
	mTotalOut = mTotal;

	genNGon(params, llfloor(sides), offset, ang_scale, split);

	Face* face = addFace(mTotalOut, mTotal - mTotalOut, 0, LL_FACE_INNER_SIDE,
						 flat);

	// thread_local and not just static, because this method can be indirectly
	// called (via the generate(...) method) by both the main thread and the
	// mesh repository thread. HB
	thread_local LLAlignedArray<LLVector4a, 64> pt;
	pt.resize(mTotal);

	for (S32 i = mTotalOut; i < mTotal; ++i)
	{
		pt[i] = mVertices[i];
		pt[i].mul(box_hollow);
	}

	S32 j = mTotal - 1;
	for (S32 i = mTotalOut; i < mTotal; ++i)
	{
		mVertices[i] = pt[j--];
	}

	for (S32 i = 0, count = mFaces.size(); i < count; ++i)
	{
		if (mFaces[i].mCap)
		{
			mFaces[i].mCount *= 2;
		}
	}

	return face;
}

// This is basically LLProfile::generate stripped down to only operations that
// influence the number of points
//static
S32 LLProfile::getNumPoints(const LLProfileParams& params, bool path_open,
							F32 detail, S32 split, bool is_sculpted,
							S32 sculpt_size)
{
	if (detail < 0.f)
	{
		detail = 0.f;
	}

	// Generate the face data
	F32 hollow = params.getHollow();

	S32 np = 0;

	switch (params.getCurveType() & LL_PCODE_PROFILE_MASK)
	{
		case LL_PCODE_PROFILE_SQUARE:
		{
			np = getNumNGonPoints(params, 4, 1.f, split);

			if (hollow)
			{
				np *= 2;
			}
			break;
		}

		case LL_PCODE_PROFILE_ISOTRI:
		case LL_PCODE_PROFILE_RIGHTTRI:
		case LL_PCODE_PROFILE_EQUALTRI:
		{
			np = getNumNGonPoints(params, 3, 1.f, split);

			if (hollow)
			{
				np *= 2;
			}
			break;
		}

		case LL_PCODE_PROFILE_CIRCLE:
		{
			// If this has a square hollow, we should adjust the number of
			// faces a bit so that the geometry lines up.
			U8 hole_type = 0;
			F32 circle_detail = MIN_DETAIL_FACES * detail;
			if (hollow)
			{
				hole_type = params.getCurveType() & LL_PCODE_HOLE_MASK;
				if (hole_type == LL_PCODE_HOLE_SQUARE)
				{
					// Snap to the next multiple of four sides, so that corners
					// line up
					circle_detail = llceil(circle_detail * 0.25f) * 4.f;
				}
			}

			S32 sides = (S32)circle_detail;

			if (is_sculpted)
			{
				sides = sculpt_size;
			}

			np = getNumNGonPoints(params, sides);

			if (hollow)
			{
				np *= 2;
			}
			break;
		}

		case LL_PCODE_PROFILE_CIRCLE_HALF:
		{
			// If this has a square hollow, we should adjust the number of
			// faces a bit so that the geometry lines up.
			U8 hole_type = 0;

			// Number of faces is cut in half because it's only a half-circle.
			F32 circle_detail = MIN_DETAIL_FACES * detail * 0.5f;
			if (hollow)
			{
				hole_type = params.getCurveType() & LL_PCODE_HOLE_MASK;
				if (hole_type == LL_PCODE_HOLE_SQUARE)
				{
					// Snap to the next multiple of four sides (div 2), so that
					// corners line up.
					circle_detail = llceil(circle_detail * 0.5f) * 2.f;
				}
			}
			np = getNumNGonPoints(params, llfloor(circle_detail), 0.5f);

			if (hollow)
			{
				np *= 2;
			}

			// Special case for openness of sphere
			if (params.getEnd() - params.getBegin() < 1.f)
			{
			}
			else if (!hollow)
			{
				np++;
			}
			break;
		}

		default:
		   break;
	}

	return np;
}

bool LLProfile::generate(const LLProfileParams& params, bool path_open,
						 F32 detail, S32 split, bool is_sculpted,
						 S32 sculpt_size)
{
	// We need a mutex here, because this code can be called both from the main
	// thread (via sculpt() which is called from LLVOVolume) and from the mesh
	// repository thread (via LLVolume() -> LLVolume::generate()), when a new
	// LOD is created (by LLMeshRepoThread::lodReceived())... HB
	mMutex.lock();
	if (!mDirty && !is_sculpted)
	{
		mMutex.unlock();
		return false;
	}
	mDirty = false;

	if (detail < 0.f)
	{
		llwarns << "Attempt to generate profile with negative LOD: " << detail
				<< ". Clamping it to 0." << llendl;
		detail = 0.f;
	}

	mVertices.resize(0);
	mFaces.resize(0);

	// Generate the face data
	S32 i;
	F32 begin = params.getBegin();
	F32 end = params.getEnd();
	F32 hollow = params.getHollow();

	// Quick validation to eliminate some server crashes.
	if (begin > end - 0.01f)
	{
		mMutex.unlock();
		llwarns << "Assertion 'begin >= end' failed; aborting." << llendl;
		return false;
	}

	S32 face_num = 0;

	switch (params.getCurveType() & LL_PCODE_PROFILE_MASK)
	{
		case LL_PCODE_PROFILE_SQUARE:
		{
			genNGon(params, 4, -0.375f, 1.f, split);
			if (path_open)
			{
				addCap (LL_FACE_PATH_BEGIN);
			}

			for (i = llfloor(begin * 4.f); i < llfloor(end * 4.f + .999f); ++i)
			{
				addFace((face_num++) * (split + 1), split + 2, 1,
						LL_FACE_OUTER_SIDE_0 << i, true);
			}

			LLVector4a scale(1, 1, 4, 1);
			S32 count = mVertices.size();
			for (i = 0; i < count; ++i)
			{
				// Scale by 4 to generate proper tex coords.
				mVertices[i].mul(scale);
				llassert(mVertices[i].isFinite3());
			}

			if (hollow)
			{
				switch (params.getCurveType() & LL_PCODE_HOLE_MASK)
				{
					case LL_PCODE_HOLE_TRIANGLE:
						// This offset is not correct, but we cannot change it
						// now... DK 11/17/04
					  	addHole(params, true, 3, -0.375f, hollow, 1.f, split);
						break;

					case LL_PCODE_HOLE_CIRCLE:
						// *TODO: Compute actual detail levels for cubes
					  	addHole(params, false, MIN_DETAIL_FACES * detail,
								-0.375f, hollow, 1.f);
						break;

					default:	// LL_PCODE_HOLE_SAME, LL_PCODE_HOLE_SQUARE
						addHole(params, true, 4, -0.375f, hollow, 1.f, split);
				}
			}

			if (path_open)
			{
				mFaces[0].mCount = mTotal;
			}
			break;
		}

		case LL_PCODE_PROFILE_ISOTRI:
		case LL_PCODE_PROFILE_RIGHTTRI:
		case LL_PCODE_PROFILE_EQUALTRI:
		{
			genNGon(params, 3, 0.f, 1.f, split);
			LLVector4a scale(1, 1, 3, 1);
			S32 count = mVertices.size();
			for (i = 0; i < count; ++i)
			{
				// Scale by 3 to generate proper tex coords.
				mVertices[i].mul(scale);
				llassert(mVertices[i].isFinite3());
			}

			if (path_open)
			{
				addCap(LL_FACE_PATH_BEGIN);
			}

			for (i = llfloor(begin * 3.f); i < llfloor(end * 3.f + .999f); ++i)
			{
				addFace((face_num++) * (split + 1), split + 2, 1,
						LL_FACE_OUTER_SIDE_0 << i, true);
			}
			if (hollow)
			{
				// Swept triangles need smaller hollowness values, because the
				// triangle doesn't fill the bounding box.
				F32 triangle_hollow = hollow * 0.5f;

				switch (params.getCurveType() & LL_PCODE_HOLE_MASK)
				{
					case LL_PCODE_HOLE_CIRCLE:
						// *TODO: Actually generate level of detail for
						// triangles
						addHole(params, false, MIN_DETAIL_FACES * detail, 0,
								triangle_hollow, 1.f);
						break;

					case LL_PCODE_HOLE_SQUARE:
						addHole(params, true, 4, 0, triangle_hollow, 1.f,
								split);
						break;

					default:	// LL_PCODE_HOLE_SAME, LL_PCODE_HOLE_TRIANGLE
						addHole(params, true, 3, 0, triangle_hollow, 1.f,
								split);
				}
			}
			break;
		}

		case LL_PCODE_PROFILE_CIRCLE:
		{
			// If this has a square hollow, we should adjust the number of
			// faces a bit so that the geometry lines up.
			U8 hole_type = 0;
			F32 circle_detail = MIN_DETAIL_FACES * detail;
			if (hollow)
			{
				hole_type = params.getCurveType() & LL_PCODE_HOLE_MASK;
				if (hole_type == LL_PCODE_HOLE_SQUARE)
				{
					// Snap to the next multiple of four sides, so that corners
					// line up.
					circle_detail = llceil(circle_detail * 0.25f) * 4.f;
				}
			}

			S32 sides = (S32)circle_detail;

			if (is_sculpted)
			{
				sides = sculpt_size;
			}

			if (sides > 0)
			{
				genNGon(params, sides);
			}

			if (path_open)
			{
				addCap (LL_FACE_PATH_BEGIN);
			}

			if (mOpen && !hollow)
			{
				addFace(0, mTotal - 1, 0, LL_FACE_OUTER_SIDE_0, false);
			}
			else
			{
				addFace(0, mTotal, 0, LL_FACE_OUTER_SIDE_0, false);
			}

			if (hollow)
			{
				switch (hole_type)
				{
					case LL_PCODE_HOLE_SQUARE:
						addHole(params, true, 4, 0, hollow, 1.f, split);
						break;

					case LL_PCODE_HOLE_TRIANGLE:
						addHole(params, true, 3, 0, hollow, 1.f, split);
						break;

					default:	// LL_PCODE_HOLE_SAME, LL_PCODE_HOLE_CIRCLE
						addHole(params, false, circle_detail, 0, hollow, 1.f);
				}
			}
			break;
		}

		case LL_PCODE_PROFILE_CIRCLE_HALF:
		{
			// If this has a square hollow, we should adjust the number of
			// faces a bit so that the geometry lines up.
			U8 hole_type = 0;
			// Number of faces is cut in half because it's only a half-circle.
			F32 circle_detail = MIN_DETAIL_FACES * detail * 0.5f;
			if (hollow)
			{
				hole_type = params.getCurveType() & LL_PCODE_HOLE_MASK;
				if (hole_type == LL_PCODE_HOLE_SQUARE)
				{
					// Snap to the next multiple of four sides (div 2),
					// so that corners line up.
					circle_detail = llceil(circle_detail * 0.5f) * 2.f;
				}
			}
			genNGon(params, llfloor(circle_detail), 0.5f, 0.5f);
			if (path_open)
			{
				addCap(LL_FACE_PATH_BEGIN);
			}
			if (mOpen && !params.getHollow())
			{
				addFace(0, mTotal - 1, 0, LL_FACE_OUTER_SIDE_0, false);
			}
			else
			{
				addFace(0, mTotal, 0, LL_FACE_OUTER_SIDE_0, false);
			}

			if (hollow)
			{
				switch (hole_type)
				{
					case LL_PCODE_HOLE_SQUARE:
						addHole(params, true, 2, 0.5f, hollow, 0.5f, split);
						break;

					case LL_PCODE_HOLE_TRIANGLE:
						addHole(params, true, 3,  0.5f, hollow, 0.5f, split);
						break;

					default:	// LL_PCODE_HOLE_SAME, LL_PCODE_HOLE_CIRCLE
						addHole(params, false, circle_detail,  0.5f, hollow,
								0.5f);
				}
			}

			// Special case for openness of sphere
			if (params.getEnd() - params.getBegin() < 1.f)
			{
				mOpen = true;
			}
			else if (!hollow)
			{
				mOpen = false;
				mVertices.push_back(mVertices[0]);
				llassert(mVertices[0].isFinite3());
				++mTotal;
			}
			break;
		}

		default:
		    llerrs << "Unknown profile: getCurveType() = "
				   << params.getCurveType() << llendl;
	}

	if (path_open)
	{
		addCap(LL_FACE_PATH_END); // bottom
	}

	if (mOpen) // Interior edge caps
	{
		addFace(mTotal - 1, 2, 0.5, LL_FACE_PROFILE_BEGIN, true);

		if (hollow)
		{
			addFace(mTotalOut - 1, 2, 0.5, LL_FACE_PROFILE_END, true);
		}
		else
		{
			addFace(mTotal - 2, 2, 0.5, LL_FACE_PROFILE_END, true);
		}
	}

	mMutex.unlock();
	return true;
}

bool LLProfileParams::importFile(LLFILE* fp)
{
	constexpr S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];
	// *NOTE: changing the size or type of these buffers would require changing
	// the sscanf below.
	char keyword[256];
	char valuestr[256];
	keyword[0] = 0;
	valuestr[0] = 0;
	F32 tempF32;
	U32 tempU32;

	while (!feof(fp))
	{
		if (fgets(buffer, BUFSIZE, fp) == NULL)
		{
			buffer[0] = '\0';
		}

		sscanf(buffer, " %255s %255s", keyword, valuestr);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}", keyword))
		{
			break;
		}
		else if (!strcmp("curve", keyword))
		{
			sscanf(valuestr, "%d", &tempU32);
			setCurveType((U8)tempU32);
		}
		else if (!strcmp("begin", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setBegin(tempF32);
		}
		else if (!strcmp("end", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setEnd(tempF32);
		}
		else if (!strcmp("hollow", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setHollow(tempF32);
		}
		else
		{
			llwarns << "Unknown keyword '" << keyword << "' in profile import."
					<< llendl;
		}
	}

	return true;
}

bool LLProfileParams::exportFile(LLFILE* fp) const
{
	fprintf(fp, "\t\tprofile 0\n");
	fprintf(fp, "\t\t{\n");
	fprintf(fp, "\t\t\tcurve\t%d\n", getCurveType());
	fprintf(fp, "\t\t\tbegin\t%g\n", getBegin());
	fprintf(fp, "\t\t\tend\t%g\n", getEnd());
	fprintf(fp, "\t\t\thollow\t%g\n", getHollow());
	fprintf(fp, "\t\t}\n");
	return true;
}

bool LLProfileParams::importLegacyStream(std::istream& input_stream)
{
	constexpr S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];
	// *NOTE: changing the size or type of these buffers would require changing
	// the sscanf below.
	char keyword[256];
	char valuestr[256];
	keyword[0] = 0;
	valuestr[0] = 0;
	F32 tempF32;
	U32 tempU32;

	while (input_stream.good())
	{
		input_stream.getline(buffer, BUFSIZE);
		sscanf(buffer, " %255s %255s", keyword, valuestr);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}", keyword))
		{
			break;
		}
		else if (!strcmp("curve", keyword))
		{
			sscanf(valuestr, "%d", &tempU32);
			setCurveType((U8)tempU32);
		}
		else if (!strcmp("begin", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setBegin(tempF32);
		}
		else if (!strcmp("end", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setEnd(tempF32);
		}
		else if (!strcmp("hollow", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setHollow(tempF32);
		}
		else
		{
 			llwarns << "Unknown keyword " << keyword << " in profile import"
					<< llendl;
		}
	}

	return true;
}

bool LLProfileParams::exportLegacyStream(std::ostream& output_stream) const
{
	output_stream <<"\t\tprofile 0\n";
	output_stream <<"\t\t{\n";
	output_stream <<"\t\t\tcurve\t" << (S32) getCurveType() << "\n";
	output_stream <<"\t\t\tbegin\t" << getBegin() << "\n";
	output_stream <<"\t\t\tend\t" << getEnd() << "\n";
	output_stream <<"\t\t\thollow\t" << getHollow() << "\n";
	output_stream << "\t\t}\n";
	return true;
}

LLSD LLProfileParams::asLLSD() const
{
	LLSD sd;

	sd["curve"] = getCurveType();
	sd["begin"] = getBegin();
	sd["end"] = getEnd();
	sd["hollow"] = getHollow();
	return sd;
}

bool LLProfileParams::fromLLSD(LLSD& sd)
{
	setCurveType(sd["curve"].asInteger());
	setBegin((F32)sd["begin"].asReal());
	setEnd((F32)sd["end"].asReal());
	setHollow((F32)sd["hollow"].asReal());
	return true;
}

void LLProfileParams::copyParams(const LLProfileParams& params)
{
	setCurveType(params.getCurveType());
	setBegin(params.getBegin());
	setEnd(params.getEnd());
	setHollow(params.getHollow());
}

// This is basically LLPath::genNGon stripped down to only operations that
// influence the number of points added
S32 LLPath::getNumNGonPoints(const LLPathParams& params, S32 sides)
{
	F32 step = 1.f / sides;
	F32 t = params.getBegin() + step;

	// Snap to a quantized parameter, so that cut does not affect most sample
	// points.
	t = ((S32)(t * sides)) / (F32)sides;

	S32 ret = 1;

	// Run through the non-cut dependent points.
	while (t < params.getEnd())
	{
		++ret;
		t += step;
	}

	return ++ret;
}

// Generates a circular path, starting at (1, 0, 0), counter-clockwise along
// the xz plane.
void LLPath::genNGon(const LLPathParams& params, S32 sides, F32 end_scale,
					 F32 twist_scale)
{
	F32 revolutions = params.getRevolutions();
	F32 skew = params.getSkew();
	F32 skew_mag = fabsf(skew);
	F32 hole_x = params.getScaleX() * (1.f - skew_mag);
	F32 hole_y = params.getScaleY();

	// Calculate taper begin/end for x,y (Negative means taper the beginning)
	F32 taper_x_begin	= 1.f;
	F32 taper_x_end		= 1.f - params.getTaperX();
	F32	taper_y_begin	= 1.f;
	F32	taper_y_end		= 1.f - params.getTaperY();

	if (taper_x_end > 1.f)
	{
		// Flip tapering.
		taper_x_begin	= 2.f - taper_x_end;
		taper_x_end		= 1.f;
	}
	if (taper_y_end > 1.f)
	{
		// Flip tapering.
		taper_y_begin	= 2.f - taper_y_end;
		taper_y_end		= 1.f;
	}

	// For spheres, the radius is usually zero.
	F32 radius_start = 0.5f;
	if (sides < 8)
	{
		radius_start = sTableScale[sides];
	}

	// Scale the radius to take the hole size into account.
	radius_start *= 1.f - hole_y;

	// Now check the radius offset to calculate the start,end radius
	// (negative means decrease the start radius instead).
	F32 radius_end = radius_start;
	F32 radius_offset = params.getRadiusOffset();
	if (radius_offset < 0.f)
	{
		radius_start *= 1.f + radius_offset;
	}
	else
	{
		radius_end *= 1.f - radius_offset;
	}

	// Is the path NOT a closed loop ?
	mOpen = params.getEnd() * end_scale - params.getBegin() < 1.f ||
		    skew_mag > 0.001f || fabsf(taper_x_end - taper_x_begin) > 0.001f ||
			fabsf(taper_y_end - taper_y_begin) > 0.001f ||
			fabsf(radius_end - radius_start) > 0.001f;

	LLVector3 path_axis(1.f, 0.f, 0.f);
	F32 twist_begin = params.getTwistBegin() * twist_scale;
	F32 twist_end = params.getTwistEnd() * twist_scale;

	// We run through this once before the main loop, to make sure the path
	// begins at the correct cut.
	F32 step = 1.f / sides;
	F32 t = params.getBegin();
	PathPt* pt = mPath.append(1);
	F32 ang = 2.f * F_PI * revolutions * t;
	F32 s = sinf(ang) * lerp(radius_start, radius_end, t);
	F32 c = cosf(ang) * lerp(radius_start, radius_end, t);

	pt->mPos.set(lerp(0, params.getShear().mV[0], s) +
				 lerp(-skew, skew, t) * 0.5f,
				 c + lerp(0, params.getShear().mV[1], s), s);
	pt->mScale.set(hole_x * lerp(taper_x_begin, taper_x_end, t),
				   hole_y * lerp(taper_y_begin, taper_y_end, t), 0, 1);
	pt->mTexT = t;

	// Twist rotates the path along the x,y plane (I think) - DJS 04/05/02
	LLQuaternion twist;
	twist.setAngleAxis(lerp(twist_begin, twist_end, t) * 2.f * F_PI - F_PI,
					   0.f, 0.f, 1.f);
	// Rotate the point around the circle's center.
	LLQuaternion qang;
	qang.setAngleAxis(ang, path_axis);
	LLMatrix3 rot(twist * qang);
	pt->mRot.loadu(rot);

	t += step;

	// Snap to a quantized parameter, so that cut does not affect most sample
	// points.
	t = ((S32)(t * sides)) / (F32)sides;

	// Run through the non-cut dependent points.
	while (t < params.getEnd())
	{
		pt = mPath.append(1);

		ang = 2.f * F_PI * revolutions * t;
		c   = cosf(ang) * lerp(radius_start, radius_end, t);
		s   = sinf(ang) * lerp(radius_start, radius_end, t);

		pt->mPos.set(lerp(0, params.getShear().mV[0], s) +
					 lerp(-skew, skew, t) * 0.5f,
					 c + lerp(0, params.getShear().mV[1], s), s);

		pt->mScale.set(hole_x * lerp(taper_x_begin, taper_x_end, t),
					   hole_y * lerp(taper_y_begin, taper_y_end, t), 0, 1);
		pt->mTexT = t;

		// Twist rotates the path along the x,y plane (I think) - DJS 04/05/02
		twist.setAngleAxis(lerp(twist_begin, twist_end, t) * 2.f * F_PI - F_PI,
								0.f, 0.f, 1.f);
		// Rotate the point around the circle's center.
		qang.setAngleAxis(ang, path_axis);
		LLMatrix3 tmp(twist * qang);
		pt->mRot.loadu(tmp);

		t += step;
	}

	// Make one final pass for the end cut.
	t = params.getEnd();
	pt = mPath.append(1);
	ang = 2.f * F_PI * revolutions * t;
	c = cosf(ang) * lerp(radius_start, radius_end, t);
	s = sinf(ang) * lerp(radius_start, radius_end, t);

	pt->mPos.set(lerp(0, params.getShear().mV[0], s) +
				 lerp(-skew, skew, t) * 0.5f,
				  c + lerp(0, params.getShear().mV[1], s), s);
	pt->mScale.set(hole_x * lerp(taper_x_begin, taper_x_end, t),
				   hole_y * lerp(taper_y_begin, taper_y_end, t), 0, 1);
	pt->mTexT  = t;

	// Twist rotates the path along the x,y plane (I think) - DJS 04/05/02
	twist.setAngleAxis(lerp(twist_begin, twist_end, t) * 2.f * F_PI - F_PI,
					   0.f, 0.f, 1.f);
	// Rotate the point around the circle's center.
	qang.setAngleAxis(ang, path_axis);
	LLMatrix3 tmp(twist * qang);
	pt->mRot.loadu(tmp);

	mTotal = mPath.size();
}

LLVector2 LLPathParams::getBeginScale() const
{
	LLVector2 begin_scale(1.f, 1.f);
	if (getScaleX() > 1)
	{
		begin_scale.mV[0] = 2.f - getScaleX();
	}
	if (getScaleY() > 1)
	{
		begin_scale.mV[1] = 2.f - getScaleY();
	}
	return begin_scale;
}

LLVector2 LLPathParams::getEndScale() const
{
	LLVector2 end_scale(1.f, 1.f);
	if (getScaleX() < 1)
	{
		end_scale.mV[0] = getScaleX();
	}
	if (getScaleY() < 1)
	{
		end_scale.mV[1] = getScaleY();
	}
	return end_scale;
}

// This is basically LLPath::generate stripped down to only the operations
// that influence the number of points
S32 LLPath::getNumPoints(const LLPathParams& params, F32 detail)
{
	if (detail < 0.f)
	{
		detail = 0.f;
	}

	S32 np = 2; // Hardcode for line

	// Is this 0xf0 mask really necessary?  DK 03/02/05

	switch (params.getCurveType() & 0xf0)
	{
		case LL_PCODE_PATH_CIRCLE:
		{
			// Increase the detail as the revolutions and twist increase.
			F32 twist_mag = fabsf(params.getTwistBegin() -
								  params.getTwistEnd());

			S32 sides = (S32)llfloor(llfloor((MIN_DETAIL_FACES * detail +
									 twist_mag * 3.5f * (detail - 0.5f))) *
									 params.getRevolutions());
			np = sides;
			break;
		}

		case LL_PCODE_PATH_CIRCLE2:
		{
			np = getNumNGonPoints(params, llfloor(MIN_DETAIL_FACES * detail));
			break;
		}

		case LL_PCODE_PATH_TEST:
		{
			np = 5;
			break;
		}

		//case LL_PCODE_PATH_LINE:
		default:
		{
			// Take the begin/end twist into account for detail.
			np = llfloor(fabsf(params.getTwistBegin() - params.getTwistEnd()) *
						 3.5f * (detail - 0.5f)) + 2;
		}
	}

	return np;
}

bool LLPath::generate(const LLPathParams& params, F32 detail, S32 split,
					  bool is_sculpted, S32 sculpt_size)
{
	if (!mDirty && !is_sculpted)
	{
		return false;
	}

	if (detail < 0.f)
	{
		llwarns << "Attempt to generating path with negative LOD: " << detail
				<< ". Clamping it to 0." << llendl;
		detail = 0.f;
	}

	mDirty = false;
	S32 np = 2; // hardcode for line

	mPath.resize(0);
	mOpen = true;

	// Is this 0xf0 mask really necessary ?  DK 03/02/05
	switch (params.getCurveType() & 0xf0)
	{
		case LL_PCODE_PATH_CIRCLE:
		{
			// Increase the detail as the revolutions and twist increase.
			F32 twist_mag = fabsf(params.getTwistBegin() -
								  params.getTwistEnd());

			S32 sides = (S32)llfloor(llfloor((MIN_DETAIL_FACES * detail +
											 twist_mag * 3.5f *
											 (detail - 0.5f))) *
									 params.getRevolutions());

			if (is_sculpted)
			{
				sides = llmax(sculpt_size, 1);
			}

			if (sides > 0)
			{
				genNGon(params, sides);
			}
			break;
		}

		case LL_PCODE_PATH_CIRCLE2:
		{
			if (params.getEnd() - params.getBegin() >= 0.99f &&
				params.getScaleX() >= .99f)
			{
				mOpen = false;
			}

			genNGon(params, llfloor(MIN_DETAIL_FACES * detail));

			F32 toggle = 0.5f;
			for (S32 i = 0, count = (S32)mPath.size(); i < count; ++i)
			{
				mPath[i].mPos.getF32ptr()[0] = toggle;
				if (toggle == 0.5f)
				{
					toggle = -0.5f;
				}
				else
				{
					toggle = 0.5f;
				}
			}
			break;
		}

		case LL_PCODE_PATH_TEST:
		{
			np = 5;
			mStep = 1.f / (F32)(np - 1);

			mPath.resize(np);

			LLQuaternion quat;
			for (S32 i = 0; i < np; ++i)
			{
				F32 t = F32(i) * mStep;
				F32 twist_angle = F_PI * params.getTwistEnd() * t;
				mPath[i].mPos.set(0.f,
								  lerp(0.f, -sinf(twist_angle) * 0.5f, t),
								  lerp(-0.5, cosf(twist_angle) * 0.5f, t));
				mPath[i].mScale.set(lerp(1.f, params.getScale().mV[0], t),
									lerp(1.f, params.getScale().mV[1], t),
									0.f, 1.f);
				mPath[i].mTexT = t;
				quat.setAngleAxis(twist_angle, 1.f, 0.f, 0.f);
				LLMatrix3 tmp(quat);
				mPath[i].mRot.loadu(tmp);
			}

			break;
		}

		//case LL_PCODE_PATH_LINE:
		default:
		{
			// Take the begin/end twist into account for detail.
			np = llfloor(fabsf(params.getTwistBegin() - params.getTwistEnd()) *
						 3.5f * (detail - 0.5f)) + 2;
			if (np < split + 2)
			{
				np = split + 2;
			}

			mStep = 1.f / (np - 1);

			mPath.resize(np);

			LLVector2 start_scale = params.getBeginScale();
			LLVector2 end_scale = params.getEndScale();

			for (S32 i = 0; i < np; ++i)
			{
				F32 t = lerp(params.getBegin(), params.getEnd(),
							 (F32)i * mStep);
				mPath[i].mPos.set(lerp(0, params.getShear().mV[0], t),
								  lerp(0, params.getShear().mV[1], t),
								  t - 0.5f);
				LLQuaternion quat;
				quat.setAngleAxis(lerp(F_PI * params.getTwistBegin(),
									   F_PI * params.getTwistEnd(), t),
								  0.f, 0.f, 1.f);
				LLMatrix3 tmp(quat);
				mPath[i].mRot.loadu(tmp);
				mPath[i].mScale.set(lerp(start_scale.mV[0],
										 end_scale.mV[0], t),
									lerp(start_scale.mV[1],
										 end_scale.mV[1], t),
									0.f, 1.f);
				mPath[i].mTexT = t;
			}
		}
	}

	if (params.getTwistEnd() != params.getTwistBegin())
	{
		mOpen = true;
	}

#if 0
	if ((S32(fabsf(params.getTwistEnd() -
				   params.getTwistBegin()) * 100)) % 100 != 0)
	{
		mOpen = true;
	}
#endif

	return true;
}

bool LLDynamicPath::generate(const LLPathParams& params, F32 detail, S32 split,
							 bool is_sculpted, S32 sculpt_size)
{
	mOpen = true; // Draw end caps
	if (getPathLength() == 0)
	{
		// Path has not been generated yet. Some algorithms later assume at
		// least TWO path points.
		resizePath(2);

		LLQuaternion quat;
		quat.setEulerAngles(0.f, 0.f, 0.f);
		LLMatrix3 tmp(quat);
		for (U32 i = 0; i < 2; ++i)
		{
			mPath[i].mPos.set(0.f, 0.f, 0.f);
			mPath[i].mRot.loadu(tmp);
			mPath[i].mScale.set(1.f, 1.f, 0.f, 1.f);
			mPath[i].mTexT = 0.f;
		}
	}

	return true;
}

bool LLPathParams::importFile(LLFILE* fp)
{
	constexpr S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];
	// *NOTE: changing the size or type of these buffers would require changing
	// the sscanf below.
	char keyword[256];
	char valuestr[256];
	keyword[0] = 0;
	valuestr[0] = 0;

	F32 tempF32;
	F32 x, y;
	U32 tempU32;

	while (!feof(fp))
	{
		if (fgets(buffer, BUFSIZE, fp) == NULL)
		{
			buffer[0] = '\0';
		}

		sscanf(buffer, " %255s %255s", keyword, valuestr);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}", keyword))
		{
			break;
		}
		else if (!strcmp("curve", keyword))
		{
			sscanf(valuestr, "%d", &tempU32);
			setCurveType((U8)tempU32);
		}
		else if (!strcmp("begin", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setBegin(tempF32);
		}
		else if (!strcmp("end", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setEnd(tempF32);
		}
		else if (!strcmp("scale", keyword))
		{
			// Legacy for one dimensional scale per path
			sscanf(valuestr, "%g", &tempF32);
			setScale(tempF32, tempF32);
		}
		else if (!strcmp("scale_x", keyword))
		{
			sscanf(valuestr, "%g", &x);
			setScaleX(x);
		}
		else if (!strcmp("scale_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setScaleY(y);
		}
		else if (!strcmp("shear_x", keyword))
		{
			sscanf(valuestr, "%g", &x);
			setShearX(x);
		}
		else if (!strcmp("shear_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setShearY(y);
		}
		else if (!strcmp("twist", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setTwistEnd(tempF32);
		}
		else if (!strcmp("twist_begin", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTwistBegin(y);
		}
		else if (!strcmp("radius_offset", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setRadiusOffset(y);
		}
		else if (!strcmp("taper_x", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTaperX(y);
		}
		else if (!strcmp("taper_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTaperY(y);
		}
		else if (!strcmp("revolutions", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setRevolutions(y);
		}
		else if (!strcmp("skew", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setSkew(y);
		}
		else
		{
			llwarns << "Unknown keyword '" << keyword << "' in path import."
					<< llendl;
		}
	}
	return true;
}

bool LLPathParams::exportFile(LLFILE* fp) const
{
	fprintf(fp, "\t\tpath 0\n");
	fprintf(fp, "\t\t{\n");
	fprintf(fp, "\t\t\tcurve\t%d\n", getCurveType());
	fprintf(fp, "\t\t\tbegin\t%g\n", getBegin());
	fprintf(fp, "\t\t\tend\t%g\n", getEnd());
	fprintf(fp, "\t\t\tscale_x\t%g\n", getScaleX());
	fprintf(fp, "\t\t\tscale_y\t%g\n", getScaleY());
	fprintf(fp, "\t\t\tshear_x\t%g\n", getShearX());
	fprintf(fp, "\t\t\tshear_y\t%g\n", getShearY());
	fprintf(fp, "\t\t\ttwist\t%g\n", getTwistEnd());

	fprintf(fp, "\t\t\ttwist_begin\t%g\n", getTwistBegin());
	fprintf(fp, "\t\t\tradius_offset\t%g\n", getRadiusOffset());
	fprintf(fp, "\t\t\ttaper_x\t%g\n", getTaperX());
	fprintf(fp, "\t\t\ttaper_y\t%g\n", getTaperY());
	fprintf(fp, "\t\t\trevolutions\t%g\n", getRevolutions());
	fprintf(fp, "\t\t\tskew\t%g\n", getSkew());

	fprintf(fp, "\t\t}\n");
	return true;
}

bool LLPathParams::importLegacyStream(std::istream& input_stream)
{
	constexpr S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];
	// *NOTE: changing the size or type of these buffers would require changing
	// the sscanf below.
	char keyword[256];
	char valuestr[256];
	keyword[0] = 0;
	valuestr[0] = 0;

	F32 tempF32;
	F32 x, y;
	U32 tempU32;

	while (input_stream.good())
	{
		input_stream.getline(buffer, BUFSIZE);
		sscanf(buffer, " %255s %255s", keyword, valuestr);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}", keyword))
		{
			break;
		}
		else if (!strcmp("curve", keyword))
		{
			sscanf(valuestr, "%d", &tempU32);
			setCurveType((U8)tempU32);
		}
		else if (!strcmp("begin", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setBegin(tempF32);
		}
		else if (!strcmp("end", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setEnd(tempF32);
		}
		else if (!strcmp("scale", keyword))
		{
			// Legacy for one dimensional scale per path
			sscanf(valuestr, "%g", &tempF32);
			setScale(tempF32, tempF32);
		}
		else if (!strcmp("scale_x", keyword))
		{
			sscanf(valuestr, "%g", &x);
			setScaleX(x);
		}
		else if (!strcmp("scale_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setScaleY(y);
		}
		else if (!strcmp("shear_x", keyword))
		{
			sscanf(valuestr, "%g", &x);
			setShearX(x);
		}
		else if (!strcmp("shear_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setShearY(y);
		}
		else if (!strcmp("twist", keyword))
		{
			sscanf(valuestr, "%g", &tempF32);
			setTwistEnd(tempF32);
		}
		else if (!strcmp("twist_begin", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTwistBegin(y);
		}
		else if (!strcmp("radius_offset", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setRadiusOffset(y);
		}
		else if (!strcmp("taper_x", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTaperX(y);
		}
		else if (!strcmp("taper_y", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setTaperY(y);
		}
		else if (!strcmp("revolutions", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setRevolutions(y);
		}
		else if (!strcmp("skew", keyword))
		{
			sscanf(valuestr, "%g", &y);
			setSkew(y);
		}
		else
		{
			llwarns << "Unknown keyword '" << keyword << "' in path import."
					<< llendl;
		}
	}
	return true;
}

bool LLPathParams::exportLegacyStream(std::ostream& output_stream) const
{
	output_stream << "\t\tpath 0\n";
	output_stream << "\t\t{\n";
	output_stream << "\t\t\tcurve\t" << (S32) getCurveType() << "\n";
	output_stream << "\t\t\tbegin\t" << getBegin() << "\n";
	output_stream << "\t\t\tend\t" << getEnd() << "\n";
	output_stream << "\t\t\tscale_x\t" << getScaleX()  << "\n";
	output_stream << "\t\t\tscale_y\t" << getScaleY()  << "\n";
	output_stream << "\t\t\tshear_x\t" << getShearX()  << "\n";
	output_stream << "\t\t\tshear_y\t" << getShearY()  << "\n";
	output_stream <<"\t\t\ttwist\t" << getTwistEnd() << "\n";

	output_stream <<"\t\t\ttwist_begin\t" << getTwistBegin() << "\n";
	output_stream <<"\t\t\tradius_offset\t" << getRadiusOffset() << "\n";
	output_stream <<"\t\t\ttaper_x\t" << getTaperX() << "\n";
	output_stream <<"\t\t\ttaper_y\t" << getTaperY() << "\n";
	output_stream <<"\t\t\trevolutions\t" << getRevolutions() << "\n";
	output_stream <<"\t\t\tskew\t" << getSkew() << "\n";

	output_stream << "\t\t}\n";
	return true;
}

LLSD LLPathParams::asLLSD() const
{
	LLSD sd = LLSD();
	sd["curve"] = getCurveType();
	sd["begin"] = getBegin();
	sd["end"] = getEnd();
	sd["scale_x"] = getScaleX();
	sd["scale_y"] = getScaleY();
	sd["shear_x"] = getShearX();
	sd["shear_y"] = getShearY();
	sd["twist"] = getTwistEnd();
	sd["twist_begin"] = getTwistBegin();
	sd["radius_offset"] = getRadiusOffset();
	sd["taper_x"] = getTaperX();
	sd["taper_y"] = getTaperY();
	sd["revolutions"] = getRevolutions();
	sd["skew"] = getSkew();

	return sd;
}

bool LLPathParams::fromLLSD(LLSD& sd)
{
	setCurveType(sd["curve"].asInteger());
	setBegin((F32)sd["begin"].asReal());
	setEnd((F32)sd["end"].asReal());
	setScaleX((F32)sd["scale_x"].asReal());
	setScaleY((F32)sd["scale_y"].asReal());
	setShearX((F32)sd["shear_x"].asReal());
	setShearY((F32)sd["shear_y"].asReal());
	setTwistEnd((F32)sd["twist"].asReal());
	setTwistBegin((F32)sd["twist_begin"].asReal());
	setRadiusOffset((F32)sd["radius_offset"].asReal());
	setTaperX((F32)sd["taper_x"].asReal());
	setTaperY((F32)sd["taper_y"].asReal());
	setRevolutions((F32)sd["revolutions"].asReal());
	setSkew((F32)sd["skew"].asReal());
	return true;
}

void LLPathParams::copyParams(const LLPathParams& params)
{
	setCurveType(params.getCurveType());
	setBegin(params.getBegin());
	setEnd(params.getEnd());
	setScale(params.getScaleX(), params.getScaleY());
	setShear(params.getShearX(), params.getShearY());
	setTwistEnd(params.getTwistEnd());
	setTwistBegin(params.getTwistBegin());
	setRadiusOffset(params.getRadiusOffset());
	setTaper(params.getTaperX(), params.getTaperY());
	setRevolutions(params.getRevolutions());
	setSkew(params.getSkew());
}

// Static member variables
U32 LLVolume::sLODCacheHit = 0;
U32 LLVolume::sLODCacheMiss = 0;
S32 LLVolume::sNumMeshPoints = 0;
bool LLVolume::sOptimizeCache = true;

LLVolume::LLVolume(const LLVolumeParams& params, F32 detail,
				   bool generate_single_face, bool is_unique)
:	mParams(params),
	mUnique(is_unique),
	mGenerateSingleFace(generate_single_face),
	mFaceMask(0x0),
	mDetail(detail),
	mSculptLevel(-2),
	mSurfaceArea(1.f), // Only calculated for sculpts (1 for all other prims)
	mIsMeshAssetLoaded(false),
	mHullPoints(NULL),
	mHullIndices(NULL),
	mNumHullPoints(0),
	mNumHullIndices(0),
	mTrianglesCache(NULL)
{
	mLODScaleBias.set(1.f, 1.f, 1.f);

	// Set defaults
	if (mParams.getPathParams().getCurveType() == LL_PCODE_PATH_FLEXIBLE)
	{
		mPathp = new LLDynamicPath();
	}
	else
	{
		mPathp = new LLPath();
	}

	generate();

	if (mParams.getSculptID().isNull() &&
		(mParams.getSculptType() == LL_SCULPT_TYPE_NONE ||
		 mParams.getSculptType() == LL_SCULPT_TYPE_MESH))
	{
		createVolumeFaces();
	}
}

void LLVolume::resizePath(S32 length)
{
	mPathp->resizePath(length);
	mVolumeFaces.clear();
	setDirty();
}

void LLVolume::regen()
{
	generate();
	createVolumeFaces();
}

void LLVolume::genTangents(S32 face)
{
	mVolumeFaces[face].createTangents();
}

LLVolume::~LLVolume()
{
	sNumMeshPoints -= mMesh.size();
	delete mPathp;
	mPathp = NULL;

	if (mTrianglesCache)
	{
		delete mTrianglesCache;
		mTrianglesCache = NULL;
	}

	mVolumeFaces.clear();

	if (mHullPoints)
	{
		free_volume_mem(mHullPoints);
		mHullPoints = NULL;
	}
	if (mHullIndices)
	{
		free_volume_mem(mHullIndices);
		mHullIndices = NULL;
	}
}

bool LLVolume::generate()
{
	U8 path_type = mParams.getPathParams().getCurveType();
	U8 profile_type = mParams.getProfileParams().getCurveType();

	// Added 10.03.05 Dave Parks
	// Split is a parameter to LLProfile::generate that tesselates edges on
	// the profile to prevent lighting and texture interpolation errors on
	// triangles that are stretched due to twisting or scaling on the path.
	S32 split = (S32)(mDetail * 0.66f);
	if (path_type == LL_PCODE_PATH_LINE &&
		(mParams.getPathParams().getScale().mV[0] != 1.f ||
		 mParams.getPathParams().getScale().mV[1] != 1.f) &&
		(profile_type == LL_PCODE_PROFILE_SQUARE ||
		 profile_type == LL_PCODE_PROFILE_ISOTRI ||
		 profile_type == LL_PCODE_PROFILE_EQUALTRI ||
		 profile_type == LL_PCODE_PROFILE_RIGHTTRI))
	{
		split = 0;
	}

	if ((mParams.getSculptType() & LL_SCULPT_TYPE_MASK) != LL_SCULPT_TYPE_MESH)
	{
		if (path_type == LL_PCODE_PATH_LINE &&
			profile_type == LL_PCODE_PROFILE_CIRCLE)
		{
			// Cylinders do not care about Z-Axis
			mLODScaleBias.set(0.6f, 0.6f, 0.f);
		}
		else if (path_type == LL_PCODE_PATH_CIRCLE)
		{
			mLODScaleBias.set(0.6f, 0.6f, 0.6f);
		}
	}
	else
	{
		mLODScaleBias.set(0.5f, 0.5f, 0.5f);
	}

	F32 profile_detail = mDetail;
	F32 path_detail = mDetail;
	bool regen_path = mPathp->generate(mParams.getPathParams(), path_detail,
									   split);
	bool regen_prof = mProfile.generate(mParams.getProfileParams(),
										mPathp->isOpen(), profile_detail,
										split);
	if (regen_path || regen_prof)
	{
		S32 s_size = mPathp->mPath.size();
		S32 t_size = mProfile.mVertices.size();

		sNumMeshPoints -= mMesh.size();
		mMesh.resize(t_size * s_size);
		sNumMeshPoints += mMesh.size();

		// Generate vertex positions

		// Run along the path.
		LLMatrix4a rot_mat;
		LLVector4a tmp;
		LLVector4a* dst = mMesh.mArray;
		for (S32 s = 0; s < s_size; ++s)
		{
			F32* scale = mPathp->mPath[s].mScale.getF32ptr();

			F32 sc [] = { scale[0], 0, 0, 0,
						  0, scale[1], 0, 0,
						  0, 0, scale[2], 0,
						  0, 0, 0, 1 };

			LLMatrix4 rot(mPathp->mPath[s].mRot.getF32ptr());
			LLMatrix4 scale_mat(sc);

			scale_mat *= rot;

			rot_mat.loadu(scale_mat);

			LLVector4a* profile = mProfile.mVertices.mArray;
			LLVector4a* end_profile = profile + t_size;
			LLVector4a offset = mPathp->mPath[s].mPos;
			if (!offset.isFinite3())
			{
				llwarns_sparse << "Path with non-finite points. Resetting offset to 0."
							   << llendl;
				offset.clear();
			}

			// Run along the profile.
			while (profile < end_profile)
			{
				rot_mat.rotate(*profile++, tmp);
				dst->setAdd(tmp, offset);
				llassert(dst->isFinite3());
				++dst;
			}
		}

		for (std::vector<LLProfile::Face>::const_iterator
				iter = mProfile.mFaces.begin(),
				end = mProfile.mFaces.end();
			 iter != end; ++iter)
		{
			LLFaceID id = iter->mFaceID;
			mFaceMask |= id;
		}

		return true;
	}

	return false;
}

#if LL_JEMALLOC
// Initialize with sane values, in case our allocators get called before the
// jemalloc arena for them is set.
U32 LLVolumeFace::sMallocxFlags16 = MALLOCX_ALIGN(16) | MALLOCX_TCACHE_NONE;
U32 LLVolumeFace::sMallocxFlags64 = MALLOCX_ALIGN(64) | MALLOCX_TCACHE_NONE;
#endif

//static
void LLVolumeFace::initClass()
{
#if LL_JEMALLOC
	static unsigned int arena = 0;
	if (!arena)
	{
		size_t sz = sizeof(arena);
		if (mallctl("arenas.create", &arena, &sz, NULL, 0))
		{
			llwarns << "Failed to create a new jemalloc arena" << llendl;
		}
	}
	llinfos << "Using jemalloc arena " << arena << " for volume faces memory"
			<< llendl;

	sMallocxFlags16 = MALLOCX_ARENA(arena) | MALLOCX_ALIGN(16) |
					  MALLOCX_TCACHE_NONE;
	sMallocxFlags64 = MALLOCX_ARENA(arena) | MALLOCX_ALIGN(64) |
					  MALLOCX_TCACHE_NONE;
#endif
}

void LLVolumeFace::VertexData::init()
{
	if (!mData)
	{
		mData = (LLVector4a*)allocate_volume_mem(sizeof(LLVector4a) * 2);
	}
}

const LLVolumeFace::VertexData& LLVolumeFace::VertexData::operator=(const LLVolumeFace::VertexData& rhs)
{
	if (this != &rhs)
	{
		init();
		LLVector4a::memcpyNonAliased16((F32*)mData, (F32*)rhs.mData,
									   2 * sizeof(LLVector4a));
		mTexCoord = rhs.mTexCoord;
	}
	return *this;
}

LLVolumeFace::VertexData::~VertexData()
{
	if (mData)
	{
		free_volume_mem(mData);
		mData = NULL;
	}
}

bool LLVolumeFace::VertexData::operator<(const LLVolumeFace::VertexData& rhs)const
{
	const F32* lp = this->getPosition().getF32ptr();
	const F32* rp = rhs.getPosition().getF32ptr();

	if (lp[0] != rp[0])
	{
		return lp[0] < rp[0];
	}

	if (rp[1] != lp[1])
	{
		return lp[1] < rp[1];
	}

	if (rp[2] != lp[2])
	{
		return lp[2] < rp[2];
	}

	lp = getNormal().getF32ptr();
	rp = rhs.getNormal().getF32ptr();

	if (lp[0] != rp[0])
	{
		return lp[0] < rp[0];
	}

	if (rp[1] != lp[1])
	{
		return lp[1] < rp[1];
	}

	if (rp[2] != lp[2])
	{
		return lp[2] < rp[2];
	}

	if (mTexCoord.mV[0] != rhs.mTexCoord.mV[0])
	{
		return mTexCoord.mV[0] < rhs.mTexCoord.mV[0];
	}

	return mTexCoord.mV[1] < rhs.mTexCoord.mV[1];
}

bool LLVolumeFace::VertexData::compareNormal(const LLVolumeFace::VertexData& rhs,
											 F32 angle_cutoff) const
{
	bool retval = false;

	constexpr F32 epsilon = 0.00001f;

	if (rhs.mData[POSITION].equals3(mData[POSITION], epsilon) &&
		fabs(rhs.mTexCoord[0]-mTexCoord[0]) < epsilon &&
		fabs(rhs.mTexCoord[1]-mTexCoord[1]) < epsilon)
	{
		if (angle_cutoff > 1.f)
		{
			retval = (mData[NORMAL].equals3(rhs.mData[NORMAL], epsilon));
		}
		else
		{
			F32 cur_angle = rhs.mData[NORMAL].dot3(mData[NORMAL]).getF32();
			retval = cur_angle > angle_cutoff;
		}
	}

	return retval;
}

bool LLVolume::unpackVolumeFaces(std::istream& is, S32 size)
{
	// Input stream is now pointing at a zlib compressed block of LLSD.
	// Decompress block.
	LLSD mdl;
	if (!unzip_llsd(mdl, is, size))
	{
		LL_DEBUGS("MeshVolume") << "Failed to unzip LLSD blob for LoD, will probably fetch from sim again."
								<< LL_ENDL;
		return false;
	}

	return unpackVolumeFaces(mdl);
}

bool LLVolume::unpackVolumeFaces(const U8* in, S32 size)
{
	// 'in' is now pointing at a zlib compressed block of LLSD.
	// Decompress block.
	LLSD mdl;
	if (!unzip_llsd(mdl, in, size))
	{
		LL_DEBUGS("MeshVolume") << "Failed to unzip LLSD blob for LoD, will probably fetch from sim again."
								<< LL_ENDL;
		return false;
	}

	return unpackVolumeFaces(mdl);
}

bool LLVolume::unpackVolumeFaces(const LLSD& mdl)
{
	size_t face_count = mdl.size();
	if (face_count == 0)
	{
		// No faces unpacked, treat as failed decode
		llwarns << "No face found !" << llendl;
		return false;
	}

	mVolumeFaces.resize(face_count);

	LLVector3 minp, maxp;
	LLVector2 min_tc, max_tc;
	LLVector4a min_pos, max_pos, tc_range;
	for (size_t i = 0; i < face_count; ++i)
	{
		LLVolumeFace& face = mVolumeFaces[i];
		if (mdl[i].has("NoGeometry"))
		{
			// Face has no geometry, continue
			face.resizeIndices(3);
			face.resizeVertices(1);
			memset((void*)face.mPositions, 0, sizeof(LLVector4a));
			memset((void*)face.mNormals, 0, sizeof(LLVector4a));
			memset((void*)face.mTexCoords, 0, sizeof(LLVector2));
			memset((void*)face.mIndices, 0, sizeof(U16) * 3);
			continue;
		}

		LLSD::Binary pos = mdl[i]["Position"];
		LLSD::Binary norm = mdl[i]["Normal"];
		LLSD::Binary tc = mdl[i]["TexCoord0"];
		LLSD::Binary idx = mdl[i]["TriangleList"];
#if LL_USE_TANGENTS
		LLSD::Binary tangent = mdl[i]["Tangent"];
#endif

		// Copy out indices
		U32 num_indices = idx.size() / 2;
		const U32 indices_to_discard = num_indices % 3;
		if (indices_to_discard)
		{
			llwarns << "Incomplete triangle discarded from face. Indices count: "
					<< num_indices << " was not divisible by 3 at face index: "
					<< i << "/" << face_count << llendl;
			num_indices -= indices_to_discard;
		}
		if (!face.resizeIndices(num_indices))
		{
			llwarns << "Failed to allocate " << num_indices
					<< " indices for face index: " << i << " Total: "
					<< face_count << llendl;
			continue;
		}

		if (idx.empty() || face.mNumIndices < 3)
		{
			// Why is there an empty index list ?
			llwarns << "Empty face present. Face index: " << i
					<< " - Faces count: " << face_count << llendl;
			continue;
		}

		U16* indices = (U16*)&(idx[0]);
		for (U32 j = 0; j < num_indices; ++j)
		{
			face.mIndices[j] = indices[j];
		}

		// Copy out vertices
		U32 num_verts = pos.size() / 6;
		if (!face.resizeVertices(num_verts))
		{
			llwarns << "Failed to allocate " << num_verts
					<< " vertices for face index: " << i << " Total: "
					<< face_count << llendl;
			face.resizeIndices(0);
			continue;
		}

		minp.setValue(mdl[i]["PositionDomain"]["Min"]);
		maxp.setValue(mdl[i]["PositionDomain"]["Max"]);

		min_pos.load3(minp.mV);
		max_pos.load3(maxp.mV);

		min_tc.setValue(mdl[i]["TexCoord0Domain"]["Min"]);
		max_tc.setValue(mdl[i]["TexCoord0Domain"]["Max"]);

		// Unpack normalized scale/translation
		if (mdl[i].has("NormalizedScale"))
		{
			face.mNormalizedScale.setValue(mdl[i]["NormalizedScale"]);
		}
		else
		{
			face.mNormalizedScale.set(1.f, 1.f, 1.f);
		}

		LLVector4a pos_range;
		pos_range.setSub(max_pos, min_pos);
		LLVector2 tc_range2 = max_tc - min_tc;
		tc_range.set(tc_range2[0], tc_range2[1], tc_range2[0], tc_range2[1]);
		LLVector4a min_tc4(min_tc[0], min_tc[1], min_tc[0], min_tc[1]);

		LLVector4a* pos_out = face.mPositions;
		LLVector4a* norm_out = face.mNormals;
		LLVector4a* tc_out = (LLVector4a*)face.mTexCoords;

		U16* v = (U16*)&(pos[0]);
		for (U32 j = 0; j < num_verts; ++j)
		{
			pos_out->set((F32)v[0], (F32)v[1], (F32)v[2]);
			pos_out->div(65535.f);
			pos_out->mul(pos_range);
			pos_out->add(min_pos);
			++pos_out;
			v += 3;
		}

		if (!norm.empty())
		{
			U16* n = (U16*)&(norm[0]);
			for (U32 j = 0; j < num_verts; ++j)
			{
				norm_out->set((F32)n[0], (F32)n[1], (F32)n[2]);
				norm_out->div(65535.f);
				norm_out->mul(2.f);
				norm_out->sub(1.f);
				++norm_out;
				n += 3;
			}
		}
		else
		{
			memset((void*)norm_out, 0, sizeof(LLVector4a) * num_verts);
		}

		if (!tc.empty())
		{
			U16* t = (U16*)&(tc[0]);
			for (U32 j = 0; j < num_verts; j += 2)
			{
				if (j < num_verts - 1)
				{
					tc_out->set((F32)t[0], (F32)t[1], (F32)t[2], (F32)t[3]);
				}
				else
				{
					tc_out->set((F32)t[0], (F32)t[1], 0.f, 0.f);
				}

				t += 4;

				tc_out->div(65535.f);
				tc_out->mul(tc_range);
				tc_out->add(min_tc4);
				++tc_out;
			}
		}
		else
		{
			memset((void*)tc_out, 0, sizeof(LLVector2) * num_verts);
		}

#if LL_USE_TANGENTS
		if (!tangent.empty())
		{
			face.allocateTangents(face.mNumVertices);
			U16* t = (U16*)&(tangent[0]);
			// Note: tangents coming from the asset may not be mikkt space, but
			// they should always be used by the GLTF shaders to maintain
			// compliance with the GLTF spec
			LLVector4a* t_out = face.mTangents; 
			for (U32 j = 0; j < num_verts; ++j)
			{
				t_out->set((F32)t[0], (F32)t[1], (F32)t[2], (F32)t[3]);
				t_out->div(65535.f);
				t_out->mul(2.f);
				t_out->sub(1.f);

				F32* tp = t_out->getF32ptr();
				tp[3] = tp[3] < 0.f ? -1.f : 1.f;
				++t_out;
				t += 4;
			}
		}
#endif	// LL_USE_TANGENTS

		if (mdl[i].has("Weights"))
		{
			if (!face.allocateWeights(num_verts))
			{
				llwarns << "Failed to allocate " << num_verts
						<< " weights for face index: " << i << " Total: "
						<< face_count << llendl;
				face.resizeIndices(0);
				face.resizeVertices(0);
				continue;
			}

			LLSD::Binary weights = mdl[i]["Weights"];

			U32 idx = 0;
			U32 cur_vertex = 0;
			bool fp_prec_error = false;
			while (idx < weights.size() && cur_vertex < num_verts)
			{
				constexpr U8 END_INFLUENCES = 0xFF;
				U8 joint = weights[idx++];

				U32 cur_influence = 0;
				LLVector4 wght(0, 0, 0, 0);
				U32 joints[4] = { 0, 0, 0, 0 };
				LLVector4 joints_with_weights(0, 0, 0, 0);

				while (joint != END_INFLUENCES && idx < weights.size())
				{
					U16 influence = weights[idx++];
					influence |= ((U16)weights[idx++] << 8);

					F32 w = llclamp((F32)influence / 65535.f, 0.001f, 0.999f);
					wght.mV[cur_influence] = w;
					joints[cur_influence++] = joint;

					if (cur_influence >= 4)
					{
						joint = END_INFLUENCES;
					}
					else
					{
						joint = weights[idx++];
					}
				}
				F32 wsum = wght.mV[VX] + wght.mV[VY] + wght.mV[VZ] +
						   wght.mV[VW];
				if (wsum <= 0.f)
				{
					wght = LLVector4(0.999f, 0.f, 0.f, 0.f);
				}
				for (U32 k = 0; k < 4; ++k)
				{
					F32 f_combined = (F32)joints[k] + wght[k];
					joints_with_weights[k] = f_combined;
					if (k < cur_influence &&
						f_combined - (S32)f_combined <= 0.f)
					{
						// Any weights we added above should wind up non-zero
						// and applied to a specific bone.
						fp_prec_error = true;
					}
				}
				face.mWeights[cur_vertex++].loadua(joints_with_weights.mV);
			}

			if (cur_vertex != num_verts || idx != weights.size())
			{
				llwarns << "Vertex weight count does not match vertex count !"
						<< llendl;
			}
			if (fp_prec_error)
			{
				LL_DEBUGS("MeshVolume") << "Floating point precision error detected."
										<< LL_ENDL;
			}
		}

		// Translate modifier flags into actions:
		bool do_reflect_x = false;
		bool do_reverse_triangles = false;
		bool do_invert_normals = false;

		bool do_mirror = (mParams.getSculptType() & LL_SCULPT_FLAG_MIRROR);
		if (do_mirror)
		{
			do_reflect_x = true;
			do_reverse_triangles = !do_reverse_triangles;
		}

		bool do_invert = (mParams.getSculptType() & LL_SCULPT_FLAG_INVERT);
		if (do_invert)
		{
			do_invert_normals = true;
			do_reverse_triangles = !do_reverse_triangles;
		}

		// Now do the work

		if (do_reflect_x)
		{
			LLVector4a* p = (LLVector4a*)face.mPositions;
			LLVector4a* n = (LLVector4a*)face.mNormals;
			for (S32 i = 0; i < face.mNumVertices; ++i)
			{
				p[i].mul(-1.f);
				n[i].mul(-1.f);
			}
		}

		if (do_invert_normals)
		{
			LLVector4a* n = (LLVector4a*)face.mNormals;
			for (S32 i = 0; i < face.mNumVertices; ++i)
			{
				n[i].mul(-1.f);
			}
		}

		if (do_reverse_triangles)
		{
			for (S32 j = 0; j < face.mNumIndices; j += 3)
			{
				// Swap the 2nd and 3rd index
				S32 swap = face.mIndices[j + 1];
				face.mIndices[j + 1] = face.mIndices[j + 2];
				face.mIndices[j + 2] = swap;
			}
		}

		// Calculate bounding box
		LLVector4a& min = face.mExtents[0];
		LLVector4a& max = face.mExtents[1];

		if (face.mNumVertices < 3)
		{
			// Empty face, use a dummy 1cm (at 1m scale) bounding box
			min.splat(-0.005f);
			max.splat(0.005f);
		}
		else
		{
			min = max = face.mPositions[0];

			for (S32 i = 1; i < face.mNumVertices; ++i)
			{
				min.setMin(min, face.mPositions[i]);
				max.setMax(max, face.mPositions[i]);
			}

			if (face.mTexCoords)
			{
				LLVector2& min_tc = face.mTexCoordExtents[0];
				LLVector2& max_tc = face.mTexCoordExtents[1];

				min_tc = face.mTexCoords[0];
				max_tc = face.mTexCoords[0];

				for (S32 j = 1; j < face.mNumVertices; ++j)
				{
					update_min_max(min_tc, max_tc, face.mTexCoords[j]);
				}
			}
			else
			{
				face.mTexCoordExtents[0].set(0, 0);
				face.mTexCoordExtents[1].set(1, 1);
			}
		}
	}

	if (sOptimizeCache && !cacheOptimize(gUsePBRShaders))
	{
		llwarns << "Failed to optimize cache." << llendl;
		mVolumeFaces.clear();
		return false;
	}

	mSculptLevel = 0;  // Success !

	return true;
}

bool LLVolume::cacheOptimize(bool gen_tangents)
{
	const S32 count = mVolumeFaces.size();

#if LL_OPENMP
	// NOTE: we cannot use OpenMP when called from the mesh repository which is
	// itself a (p)thread (pthread and OpenMP threads are incompatible)... HB
	if (is_main_thread())
	{
		LLAtomicBool success(true);

#		pragma omp parallel for
		for (S32 i = 0; success && i < count; ++i)
		{
			if (!mVolumeFaces[i].cacheOptimize(gen_tangents))
			{
				success = false;
			}
		}

		return success;
	}
#endif
	for (S32 i = 0; i < count; ++i)
	{
		if (!mVolumeFaces[i].cacheOptimize())
		{
			return false;
		}
	}
	return true;
}

void LLVolume::createVolumeFaces()
{
	if (mGenerateSingleFace)
	{
		// Do nothing
		return;
	}

	S32 num_faces = getNumFaces();
	bool partial_build = true;
	if (num_faces != (S32)mVolumeFaces.size())
	{
		partial_build = false;
		mVolumeFaces.resize(num_faces);
	}
	// Initialize volume faces with parameter data
	for (S32 i = 0, count = (S32)mVolumeFaces.size(); i < count; ++i)
	{
		LLVolumeFace& vf = mVolumeFaces[i];
		LLProfile::Face& face = mProfile.mFaces[i];
		vf.mBeginS = face.mIndex;
		vf.mNumS = face.mCount;
		if (vf.mNumS < 0)
		{
			llerrs << "Volume face corruption detected." << llendl;
		}

		vf.mBeginT = 0;
		vf.mNumT = getPath().mPath.size();
		vf.mID = i;

		// Set the type mask bits correctly
		if (mParams.getProfileParams().getHollow() > 0)
		{
			vf.mTypeMask |= LLVolumeFace::HOLLOW_MASK;
		}
		if (mProfile.isOpen())
		{
			vf.mTypeMask |= LLVolumeFace::OPEN_MASK;
		}
		if (face.mCap)
		{
			vf.mTypeMask |= LLVolumeFace::CAP_MASK;
			if (face.mFaceID == LL_FACE_PATH_BEGIN)
			{
				vf.mTypeMask |= LLVolumeFace::TOP_MASK;
			}
			else
			{
				llassert(face.mFaceID == LL_FACE_PATH_END);
				vf.mTypeMask |= LLVolumeFace::BOTTOM_MASK;
			}
		}
		else if (face.mFaceID & (LL_FACE_PROFILE_BEGIN | LL_FACE_PROFILE_END))
		{
			vf.mTypeMask |= LLVolumeFace::FLAT_MASK | LLVolumeFace::END_MASK;
		}
		else
		{
			vf.mTypeMask |= LLVolumeFace::SIDE_MASK;
			if (face.mFlat)
			{
				vf.mTypeMask |= LLVolumeFace::FLAT_MASK;
			}
			if (face.mFaceID & LL_FACE_INNER_SIDE)
			{
				vf.mTypeMask |= LLVolumeFace::INNER_MASK;
				if (face.mFlat && vf.mNumS > 2)
				{
					// Flat inner faces have to copy vert normals
					vf.mNumS = vf.mNumS * 2;
					if (vf.mNumS < 0)
					{
						llerrs << "Volume face corruption detected." << llendl;
					}
				}
			}
			else
			{
				vf.mTypeMask |= LLVolumeFace::OUTER_MASK;
			}
		}
	}

	for (face_list_t::iterator iter = mVolumeFaces.begin();
		 iter != mVolumeFaces.end(); ++iter)
	{
		iter->create(this, partial_build);
	}
}

LL_INLINE LLVector4a sculpt_rgb_to_vector(U8 r, U8 g, U8 b)
{
	// maps RGB values to vector values [0..255] -> [-0.5..0.5]
	LLVector4a value;
	LLVector4a sub(0.5f, 0.5f, 0.5f);

	value.set(r, g, b);
	value.mul(1.f / 255.f);
	value.sub(sub);

	return value;
}

LL_INLINE U32 sculpt_xy_to_index(U32 x, U32 y, U16 sculpt_width,
							  U16 sculpt_height, S8 sculpt_components)
{
	U32 index = (x + y * sculpt_width) * sculpt_components;
	return index;
}

LL_INLINE U32 sculpt_st_to_index(S32 s, S32 t, S32 siz_s, S32 siz_t,
							  U16 sculpt_width, U16 sculpt_height,
							  S8 sculpt_components)
{
	U32 x = (U32) ((F32)s / (siz_s) * (F32) sculpt_width);
	U32 y = (U32) ((F32)t / (siz_t) * (F32) sculpt_height);

	return sculpt_xy_to_index(x, y, sculpt_width, sculpt_height,
							  sculpt_components);
}

LL_INLINE LLVector4a sculpt_index_to_vector(U32 index, const U8* sculpt_data)
{
	LLVector4a v = sculpt_rgb_to_vector(sculpt_data[index],
										sculpt_data[index + 1],
										sculpt_data[index + 2]);
	return v;
}

LL_INLINE LLVector4a sculpt_st_to_vector(S32 s, S32 t, S32 siz_s, S32 siz_t,
									 U16 sculpt_width, U16 sculpt_height,
									 S8 sculpt_components,
									 const U8* sculpt_data)
{
	U32 index = sculpt_st_to_index(s, t, siz_s, siz_t, sculpt_width,
								   sculpt_height, sculpt_components);

	return sculpt_index_to_vector(index, sculpt_data);
}

LL_INLINE LLVector4a sculpt_xy_to_vector(U32 x, U32 y, U16 sculpt_width,
										 U16 sculpt_height,
										 S8 sculpt_components,
										 const U8* sculpt_data)
{
	U32 index = sculpt_xy_to_index(x, y, sculpt_width, sculpt_height,
								   sculpt_components);

	return sculpt_index_to_vector(index, sculpt_data);
}

F32 LLVolume::sculptGetSurfaceArea()
{
	// Test to see if image has enough variation to create non-degenerate
	// geometry

	F32 area = 0;

	S32 s_size = mPathp->mPath.size();
	S32 t_size = mProfile.mVertices.size();

	LLVector4a v0, v1, v2, v3, cross1, cross2;
	for (S32 s = 0; s < s_size - 1; ++s)
	{
		for (S32 t = 0; t < t_size - 1; ++t)
		{
			// Get four corners of quad
			LLVector4a& p1 = mMesh[s * t_size + t];
			LLVector4a& p2 = mMesh[(s + 1) * t_size + t];
			LLVector4a& p3 = mMesh[s * t_size + t + 1];
			LLVector4a& p4 = mMesh[(s + 1) * t_size + t + 1];

			// Compute the area of the quad by taking the length of the cross
			// product of the two triangles
			v0.setSub(p1, p2);
			v1.setSub(p1, p3);
			v2.setSub(p4, p2);
			v3.setSub(p4, p3);

			cross1.setCross3(v0, v1);
			cross2.setCross3(v2, v3);

			area += (cross1.getLength3() +
					 cross2.getLength3()).getF32() * 0.5f;
		}
	}

	return area;
}

// Create empty placeholder shape
void LLVolume::sculptGenerateEmptyPlaceholder()
{
	S32 s_size = mPathp->mPath.size();
	S32 t_size = mProfile.mVertices.size();
	S32 line = 0;
	for (S32 s = 0; s < s_size; ++s)
	{
		for (S32 t = 0; t < t_size; ++t)
		{
			S32 i = t + line;
			LLVector4a& pt = mMesh[i];
			F32* p = pt.getF32ptr();
			p[0] = p[1] = p[2] = 0.f;
		}
		line += t_size;
	}
}

void LLVolume::sculptGenerateSpherePlaceholder()
{
	S32 s_size = mPathp->mPath.size();
	S32 t_size = mProfile.mVertices.size();
	S32 line = 0;
	constexpr F32 RADIUS = 0.3f;
	for (S32 s = 0; s < s_size; ++s)
	{
		for (S32 t = 0; t < t_size; ++t)
		{
			S32 i = t + line;
			F32 u = (F32)s / (s_size - 1) * 2.f * F_PI;
			F32 v = (F32)t / (t_size - 1) * F_PI;

			LLVector4a& pt = mMesh[i];
			F32* p = pt.getF32ptr();
			p[0] = sinf(v) * cosf(u) * RADIUS;
			p[1] = sinf(v) * sinf(u) * RADIUS;
			p[2] = cosf(v) * RADIUS;
		}
		line += t_size;
	}
}

// Creates the vertices from the map
void LLVolume::sculptGenerateMapVertices(U16 sculpt_width, U16 sculpt_height,
										 S8 sculpt_components,
										 const U8* sculpt_data, U8 sculpt_type)
{
	U8 sculpt_stitching = sculpt_type & LL_SCULPT_TYPE_MASK;
	bool sculpt_invert = (sculpt_type & LL_SCULPT_FLAG_INVERT) != 0;
	bool sculpt_mirror = (sculpt_type & LL_SCULPT_FLAG_MIRROR) != 0;
	bool reverse_horizontal = sculpt_invert ? !sculpt_mirror : sculpt_mirror;

	S32 s_size = mPathp->mPath.size();
	S32 t_size = mProfile.mVertices.size();

	S32 line = 0;
	for (S32 s = 0; s < s_size; ++s)
	{
		// Run along the profile.
		for (S32 t = 0; t < t_size; ++t)
		{
			S32 i = t + line;
			LLVector4a& pt = mMesh[i];

			S32 reversed_t = t;

			if (reverse_horizontal)
			{
				reversed_t = t_size - t - 1;
			}

			U32 x = (U32)((F32)reversed_t / (t_size - 1) * (F32)sculpt_width);
			U32 y = (U32)((F32)s / (s_size - 1) * (F32)sculpt_height);

			if (y == 0)  // top row stitching
			{
				// Pinch ?
				if (sculpt_stitching == LL_SCULPT_TYPE_SPHERE)
				{
					x = sculpt_width / 2;
				}
			}

			if (y == sculpt_height)  // bottom row stitching
			{
				// Wrap ?
				if (sculpt_stitching == LL_SCULPT_TYPE_TORUS)
				{
					y = 0;
				}
				else
				{
					y = sculpt_height - 1;
				}

				// Pinch ?
				if (sculpt_stitching == LL_SCULPT_TYPE_SPHERE)
				{
					x = sculpt_width / 2;
				}
			}

			if (x == sculpt_width)   // side stitching
			{
				// Wrap ?
				if (sculpt_stitching == LL_SCULPT_TYPE_SPHERE ||
					sculpt_stitching == LL_SCULPT_TYPE_TORUS ||
					sculpt_stitching == LL_SCULPT_TYPE_CYLINDER)
				{
					x = 0;
				}
				else
				{
					x = sculpt_width - 1;
				}
			}

			pt = sculpt_xy_to_vector(x, y, sculpt_width, sculpt_height,
									 sculpt_components, sculpt_data);

			if (sculpt_mirror)
			{
				static const LLVector4a scale(-1.f, 1.f, 1.f, 1.f);
				pt.mul(scale);
			}

			llassert(pt.isFinite3());
		}

		line += t_size;
	}
}
// Changed from 4 to 6 - 6 looks round whereas 4 looks square:
constexpr S32 SCULPT_REZ_1 = 6;
constexpr S32 SCULPT_REZ_2 = 8;
constexpr S32 SCULPT_REZ_3 = 16;
constexpr S32 SCULPT_REZ_4 = 32;

S32 sculpt_sides(F32 detail)
{

	// detail is usually one of: 1, 1.5, 2.5, 4.0.

	if (detail <= 1.f)
	{
		return SCULPT_REZ_1;
	}
	if (detail <= 2.f)
	{
		return SCULPT_REZ_2;
	}
	if (detail <= 3.f)
	{
		return SCULPT_REZ_3;
	}
	else
	{
		return SCULPT_REZ_4;
	}
}

// Determine the number of vertices in both s and t direction for this sculpt
void sculpt_calc_mesh_resolution(U16 width, U16 height, U8 type, F32 detail,
								 S32& s, S32& t)
{
	// this code has the following properties:
	// 1) the aspect ratio of the mesh is as close as possible to the ratio of
	//    the map while still using all available verts
	// 2) the mesh cannot have more verts than is allowed by LOD
	// 3) the mesh cannot have more verts than is allowed by the map

	S32 max_vertices_lod = (S32)powf((F32)sculpt_sides(detail), 2.f);
	S32 max_vertices_map = width * height / 4;

	S32 vertices;
	if (max_vertices_map > 0)
	{
		vertices = llmin(max_vertices_lod, max_vertices_map);
	}
	else
	{
		vertices = max_vertices_lod;
	}

	F32 ratio;
	if (width == 0 || height == 0)
	{
		ratio = 1.f;
	}
	else
	{
		ratio = (F32) width / (F32) height;
	}

	s = (S32)sqrtf((F32)vertices / ratio);

	s = llmax(s, 4);              // No degenerate sizes, please
	t = vertices / s;

	t = llmax(t, 4);              // No degenerate sizes, please
	s = vertices / t;
}

// This method replaces generate() for sculpted surfaces
void LLVolume::sculpt(U16 sculpt_width, U16 sculpt_height,
					  S8 sculpt_components, const U8* sculpt_data,
					  S32 sculpt_level, bool visible_placeholder)
{
    U8 sculpt_type = mParams.getSculptType();

	bool data_is_empty = false;

	if (sculpt_width == 0 || sculpt_height == 0 || sculpt_components < 3 ||
		!sculpt_data)
	{
		sculpt_level = -1;
		data_is_empty = true;
	}

	S32 requested_s_size = 0;
	S32 requested_t_size = 0;

	// Always create oblong sculpties with high LOD
	F32 sculpt_detail = mDetail;
	if (sculpt_detail < 4.f && sculpt_width != sculpt_height)
	{
		sculpt_detail = 4.f;
	}

	sculpt_calc_mesh_resolution(sculpt_width, sculpt_height, sculpt_type,
								sculpt_detail, requested_s_size,
								requested_t_size);

	mPathp->generate(mParams.getPathParams(), mDetail, 0, true,
					 requested_s_size);
	mProfile.generate(mParams.getProfileParams(), mPathp->isOpen(), mDetail, 0,
					  true, requested_t_size);

	/// We requested a specific size, now see what we really got
	S32 s_size = mPathp->mPath.size();
	S32 t_size = mProfile.mVertices.size();

	// weird crash bug - DEV-11158 - trying to collect more data:
	if (s_size == 0 || t_size == 0)
	{
		llwarns << "Sculpt bad mesh size " << s_size << " " << t_size
				<< llendl;
	}

	sNumMeshPoints -= mMesh.size();
	mMesh.resize(s_size * t_size);
	sNumMeshPoints += mMesh.size();

	// Generate vertex positions
	if (!data_is_empty)
	{
		sculptGenerateMapVertices(sculpt_width, sculpt_height,
								  sculpt_components, sculpt_data,
								  sculpt_type);

		// Do not test lowest LOD to support legacy content DEV-33670
		if (mDetail > SCULPT_MIN_AREA_DETAIL)
		{
			F32 area = sculptGetSurfaceArea();
			mSurfaceArea = area;

			constexpr F32 SCULPT_MAX_AREA = 384.f;
			if (area < SCULPT_MIN_AREA || area > SCULPT_MAX_AREA)
			{
				data_is_empty = visible_placeholder = true;
			}
		}
	}

	if (data_is_empty)
	{
		if (visible_placeholder)
		{
			sculptGenerateSpherePlaceholder();
		}
		else
		{
			sculptGenerateEmptyPlaceholder();
		}
	}

	for (S32 i = 0; i < (S32)mProfile.mFaces.size(); ++i)
	{
		mFaceMask |= mProfile.mFaces[i].mFaceID;
	}

	mSculptLevel = sculpt_level;

	// Delete any existing faces so that they get regenerated
	mVolumeFaces.clear();

	createVolumeFaces();
}

bool LLVolumeParams::operator==(const LLVolumeParams& params) const
{
	return getPathParams() == params.getPathParams() &&
		   getProfileParams() == params.getProfileParams() &&
		   mSculptID == params.mSculptID &&
		   mSculptType == params.mSculptType;
}

bool LLVolumeParams::operator!=(const LLVolumeParams& params) const
{
	return getPathParams() != params.getPathParams() ||
		   getProfileParams() != params.getProfileParams() ||
		   mSculptID != params.mSculptID ||
		   mSculptType != params.mSculptType;
}

bool LLVolumeParams::operator<(const LLVolumeParams& params) const
{
	if (getPathParams() != params.getPathParams())
	{
		return getPathParams() < params.getPathParams();
	}

	if (getProfileParams() != params.getProfileParams())
	{
		return getProfileParams() < params.getProfileParams();
	}

	if (mSculptID != params.mSculptID)
	{
		return mSculptID < params.mSculptID;
	}

	return mSculptType < params.mSculptType;
}

void LLVolumeParams::copyParams(const LLVolumeParams& params)
{
	mProfileParams.copyParams(params.mProfileParams);
	mPathParams.copyParams(params.mPathParams);
	mSculptID = params.getSculptID();
	mSculptType = params.getSculptType();
}

// Less restricitve approx 0 for volumes
constexpr F32 APPROXIMATELY_ZERO = 0.001f;
LL_INLINE static bool approx_zero(F32 f, F32 tolerance)
{
	return f >= -tolerance && f <= tolerance;
}

// Returns true if in range (or nearly so)
static bool limit_range(F32& v, F32 min, F32 max,
						F32 tolerance = APPROXIMATELY_ZERO)
{
	if (v < min)
	{
		LL_DEBUGS("VolumeMessage") << "Wrong value = " << v << " - min = "
								   << min << ". Clamping." << LL_ENDL;
		v = min;
		if (!approx_zero(v - min, tolerance))
		{
			return false;
		}
	}
	if (v > max)
	{
		LL_DEBUGS("VolumeMessage") << "Wrong value = " << v << " - max = "
								   << max << ". Clamping." << LL_ENDL;
		v = max;
		if (!approx_zero(max - v, tolerance))
		{
			return false;
		}
	}
	return true;
}

bool LLVolumeParams::setBeginAndEndS(F32 b, F32 e)
{
	bool valid = true;

	// First, clamp to valid ranges.
	F32 begin = b;
	valid &= limit_range(begin, 0.f, 1.f - OBJECT_MIN_CUT_INC);

	F32 end = e;
	if (end >= .0149f && end < OBJECT_MIN_CUT_INC)
	{
		// Eliminate warning for common rounding error
		end = OBJECT_MIN_CUT_INC;
	}
	valid &= limit_range(end, OBJECT_MIN_CUT_INC, 1.f);

	valid &= limit_range(begin, 0.f, end - OBJECT_MIN_CUT_INC, .01f);

	// Now set them.
	mProfileParams.setBegin(begin);
	mProfileParams.setEnd(end);

	return valid;
}

bool LLVolumeParams::setBeginAndEndT(F32 b, F32 e)
{
	bool valid = true;

	// First, clamp to valid ranges.
	F32 begin = b;
	valid &= limit_range(begin, 0.f, 1.f - OBJECT_MIN_CUT_INC);

	F32 end = e;
	valid &= limit_range(end, OBJECT_MIN_CUT_INC, 1.f);

	valid &= limit_range(begin, 0.f, end - OBJECT_MIN_CUT_INC, .01f);

	// Now set them.
	mPathParams.setBegin(begin);
	mPathParams.setEnd(end);

	return valid;
}

bool LLVolumeParams::setHollow(F32 h)
{
	// Validate the hollow based on path and profile.
	U8 profile = mProfileParams.getCurveType() & LL_PCODE_PROFILE_MASK;
	U8 hole_type = mProfileParams.getCurveType() & LL_PCODE_HOLE_MASK;

	F32 max_hollow = OBJECT_HOLLOW_MAX;

	// Only square holes have trouble.
	if (hole_type == LL_PCODE_HOLE_SQUARE &&
		(profile == LL_PCODE_PROFILE_CIRCLE ||
		 profile == LL_PCODE_PROFILE_CIRCLE_HALF ||
		 profile == LL_PCODE_PROFILE_EQUALTRI))
	{
		max_hollow = OBJECT_HOLLOW_MAX_SQUARE;
	}

	F32 hollow = h;
	bool valid = limit_range(hollow, OBJECT_HOLLOW_MIN, max_hollow);
	mProfileParams.setHollow(hollow);

	return valid;
}

bool LLVolumeParams::setTwistBegin(F32 b)
{
	F32 twist_begin = b;
	bool valid = limit_range(twist_begin, OBJECT_TWIST_MIN, OBJECT_TWIST_MAX);
	mPathParams.setTwistBegin(twist_begin);
	return valid;
}

bool LLVolumeParams::setTwistEnd(F32 e)
{
	F32 twist_end = e;
	bool valid = limit_range(twist_end, OBJECT_TWIST_MIN, OBJECT_TWIST_MAX);
	mPathParams.setTwistEnd(twist_end);
	return valid;
}

bool LLVolumeParams::setRatio(F32 x, F32 y)
{
	F32 min_x = RATIO_MIN;
	F32 max_x = RATIO_MAX;
	F32 min_y = RATIO_MIN;
	F32 max_y = RATIO_MAX;
	// If this is a circular path (and not a sphere) then 'ratio' is actually
	// hole size.
	U8 path_type = mPathParams.getCurveType();
	U8 profile_type = mProfileParams.getCurveType() & LL_PCODE_PROFILE_MASK;
	if (LL_PCODE_PATH_CIRCLE == path_type &&
		LL_PCODE_PROFILE_CIRCLE_HALF != profile_type)
	{
		// Holes are more restricted...
		min_x = OBJECT_MIN_HOLE_SIZE;
		max_x = OBJECT_MAX_HOLE_SIZE_X;
		min_y = OBJECT_MIN_HOLE_SIZE;
		max_y = OBJECT_MAX_HOLE_SIZE_Y;
	}

	F32 ratio_x = x;
	bool valid = limit_range(ratio_x, min_x, max_x);
	F32 ratio_y = y;
	valid &= limit_range(ratio_y, min_y, max_y);

	mPathParams.setScale(ratio_x, ratio_y);

	return valid;
}

bool LLVolumeParams::setShear(F32 x, F32 y)
{
	F32 shear_x = x;
	bool valid = limit_range(shear_x, SHEAR_MIN, SHEAR_MAX);
	F32 shear_y = y;
	valid &= limit_range(shear_y, SHEAR_MIN, SHEAR_MAX);
	mPathParams.setShear(shear_x, shear_y);
	return valid;
}

bool LLVolumeParams::setTaperX(F32 v)
{
	F32 taper = v;
	bool valid = limit_range(taper, TAPER_MIN, TAPER_MAX);
	mPathParams.setTaperX(taper);
	return valid;
}

bool LLVolumeParams::setTaperY(F32 v)
{
	F32 taper = v;
	bool valid = limit_range(taper, TAPER_MIN, TAPER_MAX);
	mPathParams.setTaperY(taper);
	return valid;
}

bool LLVolumeParams::setRevolutions(F32 r)
{
	F32 revolutions = r;
	bool valid = limit_range(revolutions, OBJECT_REV_MIN, OBJECT_REV_MAX);
	mPathParams.setRevolutions(revolutions);
	return valid;
}

bool LLVolumeParams::setRadiusOffset(F32 offset)
{
	bool valid = true;

	// If this is a sphere, just set it to 0 and get out.
	U8 path_type 	= mPathParams.getCurveType();
	U8 profile_type = mProfileParams.getCurveType() & LL_PCODE_PROFILE_MASK;
	if (profile_type == LL_PCODE_PROFILE_CIRCLE_HALF ||
		path_type != LL_PCODE_PATH_CIRCLE)
	{
		mPathParams.setRadiusOffset(0.f);
		return true;
	}

	// Limit radius offset, based on taper and hole size y.
	F32 radius_offset	= offset;
	F32 taper_y    		= getTaperY();
	F32 radius_mag		= fabs(radius_offset);
	F32 hole_y_mag 		= fabs(getRatioY());
	F32 taper_y_mag		= fabs(taper_y);
	// Check to see if the taper effects us.
	if ((radius_offset > 0.f && taper_y < 0.f) ||
			(radius_offset < 0.f && taper_y > 0.f))
	{
		// The taper does not help increase the radius offset range.
		taper_y_mag = 0.f;
	}
	F32 max_radius_mag = 1.f - hole_y_mag * (1.f - taper_y_mag) /
						 (1.f - hole_y_mag);

	// Enforce the maximum magnitude.
	F32 delta = max_radius_mag - radius_mag;
	if (delta < 0.f)
	{
		// Check radius offset sign.
		if (radius_offset < 0.f)
		{
			radius_offset = -max_radius_mag;
		}
		else
		{
			radius_offset = max_radius_mag;
		}
		valid = approx_zero(delta, .1f);
	}

	mPathParams.setRadiusOffset(radius_offset);
	return valid;
}

bool LLVolumeParams::setSkew(F32 skew_value)
{
	bool valid = true;

	// Check the skew value against the revolutions.
	F32 skew = llclamp(skew_value, SKEW_MIN, SKEW_MAX);
	F32 skew_mag = fabs(skew);
	F32 revolutions = getRevolutions();
	F32 scale_x = getRatioX();
	F32 min_skew_mag = 1.f - 1.f / (revolutions * scale_x + 1.f);
	// Discontinuity; A revolution of 1 allows skews below 0.5.
	if (fabs(revolutions - 1.f) < 0.001)
	{
		min_skew_mag = 0.f;
	}

	// Clip skew.
	F32 delta = skew_mag - min_skew_mag;
	if (delta < 0.f)
	{
		// Check skew sign.
		if (skew < 0.f)
		{
			skew = -min_skew_mag;
		}
		else
		{
			skew = min_skew_mag;
		}
		valid = approx_zero(delta, .01f);
	}

	mPathParams.setSkew(skew);
	return valid;
}

bool LLVolumeParams::setSculptID(const LLUUID& sculpt_id, U8 sculpt_type)
{
	mSculptID = sculpt_id;
	mSculptType = sculpt_type;
	return true;
}

bool LLVolumeParams::setType(U8 profile, U8 path)
{
	bool result = true;
	// First, check profile and path for validity.
	U8 profile_type	= profile & LL_PCODE_PROFILE_MASK;
	U8 hole_type 	= (profile & LL_PCODE_HOLE_MASK) >> 4;
	U8 path_type	= path >> 4;

	if (profile_type > LL_PCODE_PROFILE_MAX)
	{
		// Bad profile.  Make it square.
		profile = LL_PCODE_PROFILE_SQUARE;
		result = false;
		llwarns << "Changing bad profile type (" << (S32)profile_type
				<< ") to be LL_PCODE_PROFILE_SQUARE" << llendl;
	}
	else if (hole_type > LL_PCODE_HOLE_MAX)
	{
		// Bad hole.  Make it the same.
		profile = profile_type;
		result = false;
		llwarns << "Changing bad hole type (" << (S32)hole_type
				<< ") to be LL_PCODE_HOLE_SAME" << llendl;
	}

	if (path_type < LL_PCODE_PATH_MIN ||
		path_type > LL_PCODE_PATH_MAX)
	{
		// Bad path.  Make it linear.
		result = false;
		llwarns << "Changing bad path (" << (S32)path
				<< ") to be LL_PCODE_PATH_LINE" << llendl;
		path = LL_PCODE_PATH_LINE;
	}

	mProfileParams.setCurveType(profile);
	mPathParams.setCurveType(path);
	return result;
}

// static
bool LLVolumeParams::validate(U8 prof_curve, F32 prof_begin, F32 prof_end,
							  F32 hollow, U8 path_curve, F32 path_begin,
							  F32 path_end, F32 scx, F32 scy, F32 shx, F32 shy,
							  F32 twistend, F32 twistbegin, F32 radiusoffset,
							  F32 tx, F32 ty, F32 revolutions, F32 skew)
{
	LLVolumeParams test_params;
	return test_params.setType(prof_curve, path_curve) &&
		   test_params.setBeginAndEndS(prof_begin, prof_end) &&
		   test_params.setBeginAndEndT(path_begin, path_end) &&
		   test_params.setHollow(hollow) &&
		   test_params.setTwistBegin(twistbegin) &&
		   test_params.setTwistEnd(twistend) &&
		   test_params.setRatio(scx, scy) &&
		   test_params.setShear(shx, shy) &&
		   test_params.setTaper(tx, ty) &&
		   test_params.setRevolutions(revolutions) &&
		   test_params.setRadiusOffset(radiusoffset) &&
		   test_params.setSkew(skew);
}

// Attempt to approximate the number of triangles that will result from
// generating a volume LoD set for the supplied LLVolumeParams: inaccurate, but
// a close enough approximation for determining streaming cost
void LLVolume::getLoDTriangleCounts(S32* counts)
{
	const LLPathParams& path = mParams.getPathParams();
	const LLProfileParams& prof = mParams.getProfileParams();

	if (mTrianglesCache && mTrianglesCache->mPathParams == path &&
		mTrianglesCache->mProfileParams == prof)
	{
		counts[0] = mTrianglesCache->mTriangles[0];
		counts[1] = mTrianglesCache->mTriangles[1];
		counts[2] = mTrianglesCache->mTriangles[2];
		counts[3] = mTrianglesCache->mTriangles[3];
		++sLODCacheHit;
		return;
	}
	++sLODCacheMiss;

	if (!mTrianglesCache)
	{
		mTrianglesCache = new TrianglesPerLODCache;
	}
	mTrianglesCache->mPathParams = path;
	mTrianglesCache->mProfileParams = prof;

	static const F32 details[] = { 1.f, 1.5f, 2.5f, 4.f };
#if LL_GNUC && GCC_VERSION >= 80000
# pragma GCC unroll 4
#elif LL_CLANG
# pragma clang loop unroll(full)
#endif
	for (S32 i = 0; i < 4; ++i)
	{
		const F32& detail = details[i];
		S32 path_points = LLPath::getNumPoints(path, detail);
		S32 profile_points = LLProfile::getNumPoints(prof, false, detail);
		S32 count = (profile_points - 1) * 2 * (path_points - 1);
		count += profile_points * 2;

		counts[i] = mTrianglesCache->mTriangles[i] = count;
	}
}

S32 LLVolume::getNumTriangles(S32* vcount) const
{
	U32 triangle_count = 0;
	U32 vertex_count = 0;

	for (S32 i = 0; i < getNumVolumeFaces(); ++i)
	{
		const LLVolumeFace& face = getVolumeFace(i);
		triangle_count += face.mNumIndices / 3;
		vertex_count += face.mNumVertices;
	}

	if (vcount)
	{
		*vcount = vertex_count;
	}

	return triangle_count;
}

void LLVolume::generateSilhouetteVertices(std::vector<LLVector3>& vertices,
										  std::vector<LLVector3>& normals,
										  const LLVector3& obj_cam_vec_in,
										  const LLMatrix4& mat_in,
										  const LLMatrix3& norm_mat_in,
										  S32 face_mask)
{
	vertices.clear();
	normals.clear();

	if ((mParams.getSculptType() & LL_SCULPT_TYPE_MASK) == LL_SCULPT_TYPE_MESH)
	{
		return;
	}

	LLMatrix4a mat;
	mat.loadu(mat_in);

	LLMatrix4a norm_mat;
	norm_mat.loadu(norm_mat_in);

	LLVector4a obj_cam_vec;
	obj_cam_vec.load3(obj_cam_vec_in.mV);

	LLVector4a c1, c2, t, norm, view;
	std::vector<U8> f_facing;

	S32 cur_index = 0;
	// For each face
	for (face_list_t::iterator iter = mVolumeFaces.begin();
		 iter != mVolumeFaces.end(); ++iter)
	{
		LLVolumeFace& face = *iter;

		if (!(face_mask & (0x1 << cur_index++)) || face.mNumIndices == 0 ||
			face.mEdge.empty())
		{
			continue;
		}

		if ((face.mTypeMask & LLVolumeFace::CAP_MASK))
		{
			LLVector4a* v = (LLVector4a*)face.mPositions;
			LLVector4a* n = (LLVector4a*)face.mNormals;

			for (S32 j = 0, count = face.mNumIndices / 3; j < count; ++j)
			{
				for (S32 k = 0; k < 3; ++k)
				{
					S32 index = face.mEdge[j * 3 + k];
					if (index == -1)
					{
						// Silhouette edge, currently only cubes, so no other
						// conditions
						S32 v1 = face.mIndices[j * 3 + k];
						S32 v2 = face.mIndices[j * 3 + ((k + 1) % 3)];

						mat.affineTransform(v[v1], t);
						vertices.emplace_back(t[0], t[1], t[2]);

						norm_mat.rotate(n[v1], t);

						t.normalize3fast();
						normals.emplace_back(t[0], t[1], t[2]);

						mat.affineTransform(v[v2], t);
						vertices.emplace_back(t[0], t[1], t[2]);

						norm_mat.rotate(n[v2], t);
						t.normalize3fast();
						normals.emplace_back(t[0], t[1], t[2]);
					}
				}
			}
		}
		else
		{
			constexpr U8 AWAY = 0x01;
			constexpr U8 TOWARDS = 0x02;

			// For each triangle
			f_facing.clear();
			f_facing.resize(face.mNumIndices / 3);

			LLVector4a* v = (LLVector4a*)face.mPositions;
			LLVector4a* n = (LLVector4a*)face.mNormals;

			for (S32 j = 0, count = face.mNumIndices / 3; j < count; ++j)
			{
				// Approximate normal
				S32 v1 = face.mIndices[j * 3];
				S32 v2 = face.mIndices[j * 3 + 1];
				S32 v3 = face.mIndices[j * 3 + 2];

				c1.setSub(v[v1], v[v2]);
				c2.setSub(v[v2], v[v3]);

				norm.setCross3(c1, c2);

				if (norm.dot3(norm) < 0.00000001f)
				{
					f_facing[j] = AWAY | TOWARDS;
				}
				else
				{
					// Get view vector
					view.setSub(obj_cam_vec, v[v1]);
					bool away = view.dot3(norm) > 0.f;
					if (away)
					{
						f_facing[j] = AWAY;
					}
					else
					{
						f_facing[j] = TOWARDS;
					}
				}
			}

			// For each triangle
			for (S32 j = 0, count = face.mNumIndices / 3; j < count; ++j)
			{
				if (f_facing[j] == (AWAY | TOWARDS))
				{
					// This is a degenerate triangle. Take neighbor facing
					// (degenerate faces get facing of one of their neighbors)
					// *FIX IF NEEDED: this does not deal with neighboring
					// degenerate faces
					for (S32 k = 0; k < 3; ++k)
					{
						S32 index = face.mEdge[j * 3 + k];
						if (index != -1)
						{
							f_facing[j] = f_facing[index];
							break;
						}
					}
					continue; // Skip degenerate face
				}

				// For each edge
				for (S32 k = 0; k < 3; ++k)
				{
					S32 index = face.mEdge[j * 3 + k];
					if (index != -1 && f_facing[index] == (AWAY | TOWARDS))
					{
						// Our neighbor is degenerate, make him face our direction
						f_facing[face.mEdge[j * 3 + k]] = f_facing[j];
						continue;
					}

					// index == -1 ==> no neighbor, MUST be a silhouette edge
					if (index == -1 || (f_facing[index] & f_facing[j]) == 0)
					{
						// We found a silhouette edge
						S32 v1 = face.mIndices[j * 3 + k];
						S32 v2 = face.mIndices[j * 3 + (k + 1) % 3];

						mat.affineTransform(v[v1], t);
						vertices.emplace_back(t[0], t[1], t[2]);

						norm_mat.rotate(n[v1], t);

						t.normalize3fast();
						normals.emplace_back(t[0], t[1], t[2]);

						mat.affineTransform(v[v2], t);
						vertices.emplace_back(t[0], t[1], t[2]);

						norm_mat.rotate(n[v2], t);
						t.normalize3fast();
						normals.emplace_back(t[0], t[1], t[2]);
					}
				}
			}
		}
	}
}

S32 LLVolume::lineSegmentIntersect(const LLVector4a& start,
								   const LLVector4a& end,
								   S32 face,
								   LLVector4a* intersection,
								   LLVector2* tex_coord,
								   LLVector4a* normal,
								   LLVector4a* tangent_out)
{
	S32 hit_face = -1;

	S32 start_face;
	S32 end_face;
	if (face == -1) // ALL_SIDES
	{
		start_face = 0;
		end_face = getNumVolumeFaces() - 1;
	}
	else
	{
		start_face = face;
		end_face = face;
	}

	LLVector4a dir;
	dir.setSub(end, start);

	F32 closest_t = 2.f; // must be larger than 1

	end_face = llmin(end_face, getNumVolumeFaces() - 1);

	LLVector4a box_center, box_size, intersect, n1, n2, n3, t1, t2, t3;

	for (S32 i = start_face; i <= end_face; ++i)
	{
		LLVolumeFace& face = mVolumeFaces[i];

		box_center.setAdd(face.mExtents[0], face.mExtents[1]);
		box_center.mul(0.5f);

		box_size.setSub(face.mExtents[1], face.mExtents[0]);

        if (LLLineSegmentBoxIntersect(start, end, box_center, box_size))
		{
			// If the caller wants tangents, we may need to generate them
			if (tangent_out != NULL)
			{
				genTangents(i);
			}

			if (isUnique())
			{
				// Do not bother with an octree for flexi volumes
				S32 tri_count = face.mNumIndices / 3;

				for (S32 j = 0; j < tri_count; ++j)
				{
					U16 idx0 = face.mIndices[j * 3];
					U16 idx1 = face.mIndices[j * 3 + 1];
					U16 idx2 = face.mIndices[j * 3 + 2];

					const LLVector4a& v0 = face.mPositions[idx0];
					const LLVector4a& v1 = face.mPositions[idx1];
					const LLVector4a& v2 = face.mPositions[idx2];

					F32 a, b, t;

					if (LLTriangleRayIntersect(v0, v1, v2, start, dir,
											   a, b, t))
					{
						if (t >= 0.f &&		// if hit is after start
							t <= 1.f &&		// and before end
							t < closest_t)	// and this hit is closer
						{
							closest_t = t;
							hit_face = i;

							if (intersection != NULL)
							{
								intersect = dir;
								intersect.mul(closest_t);
								intersect.add(start);
								*intersection = intersect;
							}

							if (tex_coord != NULL)
							{
								LLVector2* tc = (LLVector2*) face.mTexCoords;
								*tex_coord = (1.f - a - b) * tc[idx0] +
											 a * tc[idx1] + b * tc[idx2];
							}

							if (normal != NULL)
							{
								LLVector4a* norm = face.mNormals;
								n1 = norm[idx0];
								n1.mul(1.f - a - b);
								n2 = norm[idx1];
								n2.mul(a);
								n3 = norm[idx2];
								n3.mul(b);
								n1.add(n2);
								n1.add(n3);
								*normal = n1;
							}

							if (tangent_out != NULL)
							{
								LLVector4a* tangents = face.mTangents;
								t1 = tangents[idx0];
								t1.mul(1.f - a - b);
								t2 = tangents[idx1];
								t2.mul(a);
								t3 = tangents[idx2];
								t3.mul(b);
								t1.add(t2);
								t1.add(t3);
								*tangent_out = t1;
							}
						}
					}
				}
			}
			else
			{
				if (!face.mOctree)
				{
					face.createOctree();
				}

				LLOctreeTriangleRayIntersectNoOwnership intersect(start,
																  dir, &face,
																  &closest_t,
																  intersection,
																  tex_coord,
																  normal,
																  tangent_out);
				intersect.traverse(face.mOctree);
				if (intersect.mHitFace)
				{
					hit_face = i;
				}
			}
		}
	}

	return hit_face;
}

class LLVertexIndexPair
{
public:
	LL_INLINE LLVertexIndexPair(const LLVector3& vertex, S32 index)
	:	mVertex(vertex),
		mIndex(index)
	{
	}

public:
	LLVector3	mVertex;
	S32			mIndex;
};

constexpr F32 VERTEX_SLOP = 0.00001f;

struct lessVertex
{
	bool operator()(const LLVertexIndexPair* a, const LLVertexIndexPair* b)
	{
		constexpr F32 slop = VERTEX_SLOP;

		if (a->mVertex.mV[0] + slop < b->mVertex.mV[0])
		{
			return true;
		}
		if (a->mVertex.mV[0] - slop > b->mVertex.mV[0])
		{
			return false;
		}

		if (a->mVertex.mV[1] + slop < b->mVertex.mV[1])
		{
			return true;
		}
		if (a->mVertex.mV[1] - slop > b->mVertex.mV[1])
		{
			return false;
		}

		return a->mVertex.mV[2] + slop < b->mVertex.mV[2];
	}
};

struct lessTriangle
{
	bool operator()(const S32* a, const S32* b)
	{
		if (*a < *b)
		{
			return true;
		}
		else if (*a > *b)
		{
			return false;
		}

		if (*(a + 1) < *(b + 1))
		{
			return true;
		}
		else if (*(a + 1) > *(b + 1))
		{
			return false;
		}

		return *(a + 2) < *(b + 2);
	}
};

bool LLVolumeParams::importFile(LLFILE* fp)
{
	constexpr S32 BUFSIZE = 16384;
	char buffer[BUFSIZE];
	// *NOTE: changing the size or type of this buffer would require changing
	// the sscanf below.
	char keyword[256];
	keyword[0] = 0;

	while (!feof(fp))
	{
		if (fgets(buffer, BUFSIZE, fp) == NULL)
		{
			buffer[0] = '\0';
		}

		sscanf(buffer, " %255s", keyword);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}", keyword))
		{
			break;
		}
		else if (!strcmp("profile", keyword))
		{
			mProfileParams.importFile(fp);
		}
		else if (!strcmp("path", keyword))
		{
			mPathParams.importFile(fp);
		}
		else
		{
			llwarns << "Unknown keyword " << keyword << " in volume import."
					<< llendl;
		}
	}

	return true;
}

bool LLVolumeParams::exportFile(LLFILE* fp) const
{
	fprintf(fp, "\tshape 0\n");
	fprintf(fp, "\t{\n");
	mPathParams.exportFile(fp);
	mProfileParams.exportFile(fp);
	fprintf(fp, "\t}\n");
	return true;
}

bool LLVolumeParams::importLegacyStream(std::istream& input_stream)
{
	constexpr S32 BUFSIZE = 16384;
	// *NOTE: changing the size or type of this buffer would require changing
	// the sscanf below.
	char buffer[BUFSIZE];
	char keyword[256];
	keyword[0] = 0;

	while (input_stream.good())
	{
		input_stream.getline(buffer, BUFSIZE);
		sscanf(buffer, " %255s", keyword);
		if (!strcmp("{", keyword))
		{
			continue;
		}
		if (!strcmp("}", keyword))
		{
			break;
		}
		if (!strcmp("profile", keyword))
		{
			mProfileParams.importLegacyStream(input_stream);
		}
		else if (!strcmp("path", keyword))
		{
			mPathParams.importLegacyStream(input_stream);
		}
		else
		{
			llwarns << "Unknown keyword " << keyword << " in volume import."
					<< llendl;
		}
	}

	return true;
}

bool LLVolumeParams::exportLegacyStream(std::ostream& output_stream) const
{
	output_stream <<"\tshape 0\n";
	output_stream <<"\t{\n";
	mPathParams.exportLegacyStream(output_stream);
	mProfileParams.exportLegacyStream(output_stream);
	output_stream << "\t}\n";
	return true;
}

LLSD LLVolumeParams::sculptAsLLSD() const
{
	LLSD sd = LLSD();
	sd["id"] = getSculptID();
	sd["type"] = getSculptType();
	return sd;
}

bool LLVolumeParams::sculptFromLLSD(LLSD& sd)
{
	setSculptID(sd["id"].asUUID(), (U8)sd["type"].asInteger());
	return true;
}

LLSD LLVolumeParams::asLLSD() const
{
	LLSD sd = LLSD();
	sd["path"] = mPathParams;
	sd["profile"] = mProfileParams;
	sd["sculpt"] = sculptAsLLSD();
	return sd;
}

bool LLVolumeParams::fromLLSD(LLSD& sd)
{
	mPathParams.fromLLSD(sd["path"]);
	mProfileParams.fromLLSD(sd["profile"]);
	sculptFromLLSD(sd["sculpt"]);

	return true;
}

void LLVolumeParams::reduceS(F32 begin, F32 end)
{
	begin = llclampf(begin);
	end = llclampf(end);
	if (begin > end)
	{
		F32 temp = begin;
		begin = end;
		end = temp;
	}
	F32 a = mProfileParams.getBegin();
	F32 b = mProfileParams.getEnd();
	mProfileParams.setBegin(a + begin * (b - a));
	mProfileParams.setEnd(a + end * (b - a));
}

void LLVolumeParams::reduceT(F32 begin, F32 end)
{
	begin = llclampf(begin);
	end = llclampf(end);
	if (begin > end)
	{
		F32 temp = begin;
		begin = end;
		end = temp;
	}
	F32 a = mPathParams.getBegin();
	F32 b = mPathParams.getEnd();
	mPathParams.setBegin(a + begin * (b - a));
	mPathParams.setEnd(a + end * (b - a));
}

constexpr F32 MIN_CONCAVE_PROFILE_WEDGE = 0.125f;	// 1/8 unity
constexpr F32 MIN_CONCAVE_PATH_WEDGE = 0.111111f;	// 1/9 unity

// Returns true if the shape can be approximated with a convex shape for
// collison purposes
bool LLVolumeParams::isConvex() const
{
	if (!getSculptID().isNull())
	{
		// Cannot determine, be safe and say no:
		return false;
	}

	F32 path_length = mPathParams.getEnd() - mPathParams.getBegin();
	F32 hollow = mProfileParams.getHollow();

	U8 path_type = mPathParams.getCurveType();
	if (path_length > MIN_CONCAVE_PATH_WEDGE &&
		(mPathParams.getTwistEnd() != mPathParams.getTwistBegin() ||
		 (hollow > 0.f && LL_PCODE_PATH_LINE != path_type)))
	{
		// Twist along a "not too short" path is concave
		return false;
	}

	F32 profile_length = mProfileParams.getEnd() - mProfileParams.getBegin();
	bool same_hole = hollow == 0.f ||
					 (mProfileParams.getCurveType() &
					  LL_PCODE_HOLE_MASK) == LL_PCODE_HOLE_SAME;

	F32 min_profile_wedge = MIN_CONCAVE_PROFILE_WEDGE;
	U8 profile_type = mProfileParams.getCurveType() & LL_PCODE_PROFILE_MASK;
	if (profile_type == LL_PCODE_PROFILE_CIRCLE_HALF)
	{
		// It is a sphere and spheres get twice the minimum profile wedge
		min_profile_wedge = 2.f * MIN_CONCAVE_PROFILE_WEDGE;
	}

	bool convex_profile = // trivially convex
						  ((profile_length == 1.f ||
							profile_length <= 0.5f) && hollow == 0.f)
							// effectvely convex (even when hollow)
						   || (profile_length <= min_profile_wedge &&
							   same_hole);
	if (!convex_profile)
	{
		// Profile is concave
		return false;
	}

	if (path_type == LL_PCODE_PATH_LINE)
	{
		// Straight paths with convex profile
		return true;
	}

	if (path_length < 1.f && path_length > 0.5f)
	{
		// Profile is concave
		return false;
	}

	// We are left with spheres, toroids and tubes
	if (profile_type == LL_PCODE_PROFILE_CIRCLE_HALF)
	{
		// At this stage all spheres must be convex
		return true;
	}

	// If it is a toroid or tube, effectively convex
	return path_length <= MIN_CONCAVE_PATH_WEDGE;
}

// Debug
void LLVolumeParams::setCube()
{
	mProfileParams.setCurveType(LL_PCODE_PROFILE_SQUARE);
	mProfileParams.setBegin(0.f);
	mProfileParams.setEnd(1.f);
	mProfileParams.setHollow(0.f);

	mPathParams.setBegin(0.f);
	mPathParams.setEnd(1.f);
	mPathParams.setScale(1.f, 1.f);
	mPathParams.setShear(0.f, 0.f);
	mPathParams.setCurveType(LL_PCODE_PATH_LINE);
	mPathParams.setTwistBegin(0.f);
	mPathParams.setTwistEnd(0.f);
	mPathParams.setRadiusOffset(0.f);
	mPathParams.setTaper(0.f, 0.f);
	mPathParams.setRevolutions(0.f);
	mPathParams.setSkew(0.f);
}

LLFaceID LLVolume::generateFaceMask()
{
	LLFaceID new_mask = 0x0000;

	switch (mParams.getProfileParams().getCurveType() & LL_PCODE_PROFILE_MASK)
	{
		case LL_PCODE_PROFILE_CIRCLE:
		case LL_PCODE_PROFILE_CIRCLE_HALF:
			new_mask |= LL_FACE_OUTER_SIDE_0;
			break;

		case LL_PCODE_PROFILE_SQUARE:
		{
			for (S32 side = mParams.getProfileParams().getBegin() * 4.f,
					 count = llceil(mParams.getProfileParams().getEnd() * 4.f);
				 side < count; ++side)
			{
				new_mask |= LL_FACE_OUTER_SIDE_0 << side;
			}
			break;
		}

		case LL_PCODE_PROFILE_ISOTRI:
		case LL_PCODE_PROFILE_EQUALTRI:
		case LL_PCODE_PROFILE_RIGHTTRI:
		{
			for (S32 side = mParams.getProfileParams().getBegin() * 3.f,
					 count = llceil(mParams.getProfileParams().getEnd() * 3.f);
				 side < count; ++side)
			{
				new_mask |= LL_FACE_OUTER_SIDE_0 << side;
			}
			break;
		}

		default:
			llerrs << "Unknown profile !" << llendl;
	}

	// Handle hollow objects
	if (mParams.getProfileParams().getHollow() > 0)
	{
		new_mask |= LL_FACE_INNER_SIDE;
	}

	// Handle open profile curves
	if (mProfile.isOpen())
	{
		new_mask |= LL_FACE_PROFILE_BEGIN | LL_FACE_PROFILE_END;
	}

	// Handle open path curves
	if (mPathp->isOpen())
	{
		new_mask |= LL_FACE_PATH_BEGIN | LL_FACE_PATH_END;
	}

	return new_mask;
}

bool LLVolume::isFaceMaskValid(LLFaceID face_mask)
{
	LLFaceID test_mask = 0;
	for (S32 i = 0, count = getNumFaces(); i < count; ++i)
	{
		test_mask |= mProfile.mFaces[i].mFaceID;
	}

	return test_mask == face_mask;
}

std::ostream& operator<<(std::ostream& s, const LLProfileParams& prof_params)
{
	s << "{type=" << (U32)prof_params.mCurveType;
	s << ", begin=" << prof_params.mBegin;
	s << ", end=" << prof_params.mEnd;
	s << ", hollow=" << prof_params.mHollow;
	s << "}";
	return s;
}

std::ostream& operator<<(std::ostream& s, const LLPathParams& path_params)
{
	s << "{type=" << (U32)path_params.mCurveType;
	s << ", begin=" << path_params.mBegin;
	s << ", end=" << path_params.mEnd;
	s << ", twist=" << path_params.mTwistEnd;
	s << ", scale=" << path_params.mScale;
	s << ", shear=" << path_params.mShear;
	s << ", twist_begin=" << path_params.mTwistBegin;
	s << ", radius_offset=" << path_params.mRadiusOffset;
	s << ", taper=" << path_params.mTaper;
	s << ", revolutions=" << path_params.mRevolutions;
	s << ", skew=" << path_params.mSkew;
	s << "}";
	return s;
}

std::ostream& operator<<(std::ostream& s, const LLVolumeParams& volume_params)
{
	s << "{profileparams = " << volume_params.mProfileParams;
	s << ", pathparams = " << volume_params.mPathParams;
	s << "}";
	return s;
}

std::ostream& operator<<(std::ostream& s, const LLProfile& profile)
{
	s << " {open=" << (U32)profile.mOpen;
	s << ", dirty=" << profile.mDirty;
	s << ", totalout=" << profile.mTotalOut;
	s << ", total=" << profile.mTotal;
	s << "}";
	return s;
}

std::ostream& operator<<(std::ostream& s, const LLPath& path)
{
	s << "{open=" << (U32)path.mOpen;
	s << ", dirty=" << path.mDirty;
	s << ", step=" << path.mStep;
	s << ", total=" << path.mTotal;
	s << "}";
	return s;
}

std::ostream& operator<<(std::ostream& s, const LLVolume& volume)
{
	s << "{params = " << volume.getParams();
	s << ", path = " << *volume.mPathp;
	s << ", profile = " << volume.mProfile;
	s << "}";
	return s;
}

std::ostream& operator<<(std::ostream& s, const LLVolume* volumep)
{
	s << "{params = " << volumep->getParams();
	s << ", path = " << *(volumep->mPathp);
	s << ", profile = " << volumep->mProfile;
	s << "}";
	return s;
}

LLVolumeFace::LLVolumeFace()
:	mID(0),
	mTypeMask(0),
	mBeginS(0),
	mBeginT(0),
	mNumS(0),
	mNumT(0),
	mNumVertices(0),
	mNumAllocatedVertices(0),
	mNumIndices(0),
	mPositions(NULL),
	mNormals(NULL),
	mTangents(NULL),
	mTexCoords(NULL),
	mIndices(NULL),
	mWeights(NULL),
	mNormalizedScale(1.f, 1.f, 1.f),
	mOctree(NULL),
	mOctreeTriangles(NULL),
	mOptimized(false),
	mWeightsScrubbed(false)
{
	mExtents = (LLVector4a*)allocate_volume_mem(sizeof(LLVector4a) * 3);
	if (mExtents)
	{
		mExtents[0].splat(-0.5f);
		mExtents[1].splat(0.5f);
		mCenter = mExtents + 2;
	}
	else
	{
		mCenter = NULL;
	}
}

LLVolumeFace::LLVolumeFace(const LLVolumeFace& src)
:	mID(0),
	mTypeMask(0),
	mBeginS(0),
	mBeginT(0),
	mNumS(0),
	mNumT(0),
	mNumVertices(0),
	mNumAllocatedVertices(0),
	mNumIndices(0),
	mPositions(NULL),
	mNormals(NULL),
	mTangents(NULL),
	mTexCoords(NULL),
	mIndices(NULL),
	mWeights(NULL),
	mNormalizedScale(1.f, 1.f, 1.f),
	mOctree(NULL),
	mOctreeTriangles(NULL),
	mOptimized(false),
	mWeightsScrubbed(false)
{
	mExtents = (LLVector4a*)allocate_volume_mem(sizeof(LLVector4a) * 3);
	if (mExtents)
	{
		mCenter = mExtents + 2;
	}
	else
	{
		mCenter = NULL;
	}

	*this = src;
}

LLVolumeFace& LLVolumeFace::operator=(const LLVolumeFace& src)
{
	if (&src == this)
	{
		// Self assignment, do nothing
		return *this;
	}

	mID = src.mID;
	mTypeMask = src.mTypeMask;
	mBeginS = src.mBeginS;
	mBeginT = src.mBeginT;
	mNumS = src.mNumS;
	mNumT = src.mNumT;

	mExtents[0] = src.mExtents[0];
	mExtents[1] = src.mExtents[1];
	*mCenter = *src.mCenter;

	mNumVertices = 0;
	mNumIndices = 0;

	freeData();

	resizeVertices(src.mNumVertices);
	resizeIndices(src.mNumIndices);

	if (mNumVertices)
	{
		S32 vert_size = mNumVertices * sizeof(LLVector4a);
		S32 tc_size = (mNumVertices * sizeof(LLVector2) + 0xF) & ~0xF;

		LLVector4a::memcpyNonAliased16((F32*)mPositions, (F32*)src.mPositions,
									   vert_size);

		if (src.mNormals)
		{
			LLVector4a::memcpyNonAliased16((F32*)mNormals, (F32*)src.mNormals,
										   vert_size);
		}

		if (src.mTexCoords)
		{
			LLVector4a::memcpyNonAliased16((F32*)mTexCoords,
										   (F32*)src.mTexCoords, tc_size);
		}

		if (src.mTangents)
		{
			if (allocateTangents(src.mNumVertices))
			{
				LLVector4a::memcpyNonAliased16((F32*)mTangents,
											   (F32*)src.mTangents, vert_size);
			}
		}
		else if (mTangents)
		{
			free_volume_mem(mTangents);
			mTangents = NULL;
		}

		if (src.mWeights)
		{
			if (allocateWeights(src.mNumVertices))
			{
				LLVector4a::memcpyNonAliased16((F32*)mWeights,
											   (F32*)src.mWeights, vert_size);
			}
		}
		else if (mWeights)
		{
			free_volume_mem(mWeights);
			mWeights = NULL;
		}

		mWeightsScrubbed = src.mWeightsScrubbed;
	}

	if (mNumIndices)
	{
		S32 idx_size = (mNumIndices * sizeof(U16) + 0xF) & ~0xF;

		LLVector4a::memcpyNonAliased16((F32*)mIndices, (F32*)src.mIndices,
									   idx_size);
	}

	mOptimized = src.mOptimized;
	mNormalizedScale = src.mNormalizedScale;

	// delete
	return *this;
}

LLVolumeFace::~LLVolumeFace()
{
	if (mExtents)
	{
		free_volume_mem(mExtents);
		mExtents = mCenter = NULL;
	}

	freeData();
}

void LLVolumeFace::freeData()
{
	if (mPositions)
	{
		free_volume_mem_64(mPositions);
		mPositions = NULL;
	}

	// Normals and texture coordinates are part of the same buffer as
	// mPositions, do not free them separately
	mNormals = NULL;
	mTexCoords = NULL;

	if (mIndices)
	{
		free_volume_mem(mIndices);
		mIndices = NULL;
	}
	if (mTangents)
	{
		free_volume_mem(mTangents);
		mTangents = NULL;
	}
	if (mWeights)
	{
		free_volume_mem(mWeights);
		mWeights = NULL;
	}

	mJointRiggingInfoTab.clear();

	destroyOctree();
}

bool LLVolumeFace::create(LLVolume* volume, bool partial_build)
{
	// Tree for this face is no longer valid
	destroyOctree();

	bool ret = false;
	if (mTypeMask & CAP_MASK)
	{
		ret = createCap(volume, partial_build);
	}
	else if ((mTypeMask & END_MASK) || (mTypeMask & SIDE_MASK))
	{
		ret = createSide(volume, partial_build);
	}
	else
	{
		llerrs << "Unknown/uninitialized face type !" << llendl;
	}

	return ret;
}

void LLVolumeFace::getVertexData(U16 index, LLVolumeFace::VertexData& cv)
{
	cv.setPosition(mPositions[index]);
	if (mNormals)
	{
		cv.setNormal(mNormals[index]);
	}
	else
	{
		cv.getNormal().clear();
	}

	if (mTexCoords)
	{
		cv.mTexCoord = mTexCoords[index];
	}
	else
	{
		cv.mTexCoord.clear();
	}
}

bool LLVolumeFace::VertexMapData::operator==(const LLVolumeFace::VertexData& rhs) const
{
	return getPosition().equals3(rhs.getPosition()) &&
		   mTexCoord == rhs.mTexCoord && getNormal().equals3(rhs.getNormal());
}

bool LLVolumeFace::VertexMapData::ComparePosition::operator()(const LLVector3& a,
															  const LLVector3& b) const
{
	if (a.mV[0] != b.mV[0])
	{
		return a.mV[0] < b.mV[0];
	}

	if (a.mV[1] != b.mV[1])
	{
		return a.mV[1] < b.mV[1];
	}

	return a.mV[2] < b.mV[2];
}

void LLVolumeFace::remap()
{
	// Generate a remap buffer
	std::vector<U32> remap(mNumVertices);
	// Remap with the U32 indices
	S32 vert_count = LLMeshOptimizer::generateRemapMulti16(remap.data(),
														   mIndices,
														   mNumIndices,
														   mPositions,
														   mNormals,
														   mTexCoords,
														   mNumVertices);
	if (vert_count < 3)
	{
		return;	// Nothing to remap or remap failed.
	}

	// Allocate new buffers
	S32 size = ((mNumIndices * sizeof(U16)) + 0xF) & ~0xF;
	U16* remap_idx = (U16*)allocate_volume_mem(size);
	if (!remap_idx)
	{
		LLMemory::allocationFailed();
		llwarns << "Out of memory trying to remap vertices (2)" << llendl;
		return;
	}
	size_t tc_bytes = (vert_count * sizeof(LLVector2) + 0xF) & ~0xF;
	LLVector4a* remap_pos =
		(LLVector4a*)allocate_volume_mem_64(sizeof(LLVector4a) * 2 *
											vert_count + tc_bytes);
	if (!remap_pos)
	{
		LLMemory::allocationFailed();
		llwarns << "Out of memory trying to remap vertices (3)" << llendl;
		free_volume_mem(remap_idx);
		return;
	}
	LLVector4a* remap_norm = remap_pos + vert_count;
	LLVector2* remap_tc = (LLVector2*)(remap_norm + vert_count);

	// Fill the buffers
	LLMeshOptimizer::remapIndexBuffer16(remap_idx, mIndices, mNumIndices,
										remap.data());
	LLMeshOptimizer::remapVertsBuffer(remap_pos, mPositions, mNumVertices,
									  remap.data());
	LLMeshOptimizer::remapVertsBuffer(remap_norm, mNormals, mNumVertices,
									  remap.data());
	LLMeshOptimizer::remapTexCoordsBuffer(remap_tc, mTexCoords, mNumVertices,
										  remap.data());

	// Free old buffers
	free_volume_mem(mIndices);
	free_volume_mem_64(mPositions);
	// Tangets are now invalid
	free_volume_mem(mTangents);

	// Update volume face using new buffers
	mNumVertices = mNumAllocatedVertices = vert_count;
	mIndices = remap_idx;
	mPositions = remap_pos;
	mNormals = remap_norm;
	mTexCoords = remap_tc;
	mTangents = NULL;
}

void LLVolumeFace::optimize(F32 angle_cutoff)
{
	LLVolumeFace new_face;

	// Map of points to vector of vertices at that point
	std::map<U64, std::vector<VertexMapData> > point_map;

	LLVector4a range;
	range.setSub(mExtents[1], mExtents[0]);

	// Remove redundant vertices
	std::map<U64, std::vector<VertexMapData> >::iterator point_iter;
	LLVector4a pos;
	for (S32 i = 0; i < mNumIndices; ++i)
	{
		U16 index = mIndices[i];
		if (index >= mNumVertices)
		{
			// Invalid index: replace with a valid one to avoid a crash.
			llwarns_once << "Invalid vextex index in volume face "
						 << std::hex << (intptr_t)this << std::dec << llendl;
			index = mNumVertices - 1;
			mIndices[i] = index;
		}

		LLVolumeFace::VertexData cv;
		getVertexData(index, cv);

		bool found = false;

		pos.setSub(mPositions[index], mExtents[0]);
		pos.div(range);

		U64 pos64 = (U16)(pos[0] * 65535);
		pos64 = pos64 | (((U64)(pos[1] * 65535)) << 16);
		pos64 = pos64 | (((U64)(pos[2] * 65535)) << 32);

		point_iter = point_map.find(pos64);
		if (point_iter != point_map.end())
		{
			// Duplicate point might exist
			for (S32 j = 0, count = point_iter->second.size(); j < count; ++j)
			{
				LLVolumeFace::VertexData& tv = (point_iter->second)[j];
				if (tv.compareNormal(cv, angle_cutoff))
				{
					found = true;
					new_face.pushIndex((point_iter->second)[j].mIndex);
					break;
				}
			}
		}

		if (!found)
		{
			new_face.pushVertex(cv, mNumIndices);
			U16 index = (U16)new_face.mNumVertices - 1;
			new_face.pushIndex(index);

			VertexMapData d;
			d.setPosition(cv.getPosition());
			d.mTexCoord = cv.mTexCoord;
			d.setNormal(cv.getNormal());
			d.mIndex = index;
			if (point_iter != point_map.end())
			{
				point_iter->second.emplace_back(d);
			}
			else
			{
				point_map[pos64].emplace_back(d);
			}
		}
	}

	if (angle_cutoff > 1.f && !mNormals && new_face.mNormals)
	{
		// NOTE: normals are part of the same buffer as mPositions, do not free
		// them separately.
		new_face.mNormals = NULL;
	}

	if (!mTexCoords && new_face.mTexCoords)
	{
		// NOTE: texture coordinates are part of the same buffer as mPositions,
		// do not free them separately.
		new_face.mTexCoords = NULL;
	}

	// Only swap data if we have actually optimized the mesh
	if (new_face.mNumVertices < mNumVertices &&
		new_face.mNumIndices == mNumIndices)
	{
		LL_DEBUGS("MeshVolume") << "Optimization reached for volume face "
								<< std::hex << (intptr_t)this << std::dec
								<< " = " << new_face.mNumVertices << "/"
								<< mNumVertices << " new/old vertices."
								<< LL_ENDL;
		swapData(new_face);
	}
	else
	{
		LL_DEBUGS("MeshVolume") << "No optimization possible for volume face "
								<< std::hex << (intptr_t)this << std::dec
								<< LL_ENDL;
	}
}

// Data structure for tangent generation

class MikktData
{
protected:
	LOG_CLASS(MikktData);

public:
	MikktData(LLVolumeFace* f)
	:	face(f)
	{
		U32 count = face->mNumIndices;

		p.resize(count);
		n.resize(count);
		tc.resize(count);
		t.resize(count);
		bool has_weights = face->mWeights != NULL;
		if (has_weights)
		{
			w.resize(count);
		}

		LLVector3 inv_scale(1.f / face->mNormalizedScale.mV[0],
							1.f / face->mNormalizedScale.mV[1],
							1.f / face->mNormalizedScale.mV[2]);

		for (S32 i = 0, count = face->mNumIndices; i < count; ++i)
		{
			S32 idx = face->mIndices[i];

			p[i].set(face->mPositions[idx].getF32ptr());
			// Put mesh in original coordinate frame when reconstructing
			// tangents.
			p[i].scaleVec(face->mNormalizedScale);

			n[i].set(face->mNormals[idx].getF32ptr());
			n[i].scaleVec(inv_scale);
			n[i].normalize();

			tc[i].set(face->mTexCoords[idx]);

			if (idx >= face->mNumVertices)
			{
				// Invalid index: replace with a valid index to avoid crashes.
				LL_DEBUGS("MeshVolume") << "Invalid index: " << idx << LL_ENDL;
				idx = face->mNumVertices - 1;
				face->mIndices[i] = idx;
			}

			if (has_weights)
			{
				w[i].set(face->mWeights[idx].getF32ptr());
			}
		}
	}

public:
	LLVolumeFace*			face;
	std::vector<LLVector3>	p;
	std::vector<LLVector3>	n;
	std::vector<LLVector2>	tc;
	std::vector<LLVector4>	w;
	std::vector<LLVector4>	t;
};

bool LLVolumeFace::cacheOptimize(bool gen_tangents)
{
	if (mOptimized)
	{
		llwarns << "Already optimized, ignoring." << llendl;
		llassert(false);
		return true;
	}
	mOptimized = true;

	if (!mIndices)
	{
		llwarns << "NULL mIndices, aborting." << llendl;
		// Bad mesh data: report a failure.
		return false;
	}

	// New PBR viewer code, used when gen_tangents (= gUsePBRShaders) is true.
	if (gen_tangents && mNormals && mTexCoords)
	{
		// Generate mikkt space tangents before cache optimizing since the
		// index buffer may change; a bit of a hack to do this here, but this
		// method gets called exactly once for the lifetime of a mesh and is
		// executed on a background thread.
		SMikkTSpaceInterface ms;
		ms.m_getNumFaces =
			[](const SMikkTSpaceContext* contextp)
			{
				MikktData* data = (MikktData*)contextp->m_pUserData;
				LLVolumeFace* face = data->face;
				return face->mNumIndices / 3;
			};
		ms.m_getNumVerticesOfFace =
			[](const SMikkTSpaceContext*, const int)
			{
				return 3;
			};
		ms.m_getPosition =
			[](const SMikkTSpaceContext* contextp, float pos_out[],
			   const int face_idx, const int vert_idx)
			{
				MikktData* data = (MikktData*)contextp->m_pUserData;
				F32* p = data->p[face_idx * 3 + vert_idx].mV;
				pos_out[0] = p[0];
				pos_out[1] = p[1];
				pos_out[2] = p[2];
			};
		ms.m_getNormal =
			[](const SMikkTSpaceContext* contextp, float norm_out[],
			   const int face_idx, const int vert_idx)
			{
				MikktData* data = (MikktData*)contextp->m_pUserData;
				F32* n = data->n[face_idx * 3 + vert_idx].mV;
				norm_out[0] = n[0];
				norm_out[1] = n[1];
				norm_out[2] = n[2];
			};
		ms.m_getTexCoord =
			[](const SMikkTSpaceContext* contextp, float tc_out[],
			   const int face_idx, const int vert_idx)
			{
				MikktData* data = (MikktData*)contextp->m_pUserData;
				F32* tc = data->tc[face_idx * 3 + vert_idx].mV;
				tc_out[0] = tc[0];
				tc_out[1] = tc[1];
			};
		ms.m_setTSpaceBasic =
			[](const SMikkTSpaceContext* contextp, const float tangent[],
			   const float sign, const int face_idx, const int vert_idx)
			{
				MikktData* data = (MikktData*)contextp->m_pUserData;
				S32 i = face_idx * 3 + vert_idx;
				data->t[i].set(tangent);
				data->t[i].mV[3] = sign;
			};
		ms.m_setTSpace = NULL;

		MikktData data(this);
		SMikkTSpaceContext ctx = { &ms, &data };
		genTangSpaceDefault(&ctx);

		// Re-weld
		meshopt_Stream mos[] =
		{
			{ &data.p[0], sizeof(LLVector3), sizeof(LLVector3) },
			{ &data.n[0], sizeof(LLVector3), sizeof(LLVector3) },
			{ &data.t[0], sizeof(LLVector4), sizeof(LLVector4) },
			{ &data.tc[0], sizeof(LLVector2), sizeof(LLVector2) },
			{ data.w.empty() ? NULL : &data.w[0],
			  sizeof(LLVector4), sizeof(LLVector4) }
		};

		std::vector<U32> remap;
		try
		{
			remap.resize(data.p.size());
		}
		catch (const std::bad_alloc&)
		{
			LLMemory::allocationFailed();
			llwarns << "Out of memory trying to generate tangents" << llendl;
			return false;
		}

		U32 stream_count = data.w.empty() ? 4 : 5;
		U32 vert_count = meshopt_generateVertexRemapMulti(&remap[0], NULL,
														  data.p.size(),
														  data.p.size(), mos,
														  stream_count);
		if (vert_count < 65535)
		{
			bool success = true;
			std::vector<U32> indices;
			try
			{
				indices.resize(mNumIndices);
			}
			catch (const std::bad_alloc&)
			{
				success = false;
			}
			// Copy results back into volume
			if (success)
			{
				success = resizeVertices(vert_count);
			}
			if (success && !data.w.empty())
			{
				success = allocateWeights(vert_count);
			}
			if (success)
			{
				success = allocateTangents(mNumVertices);
			}
			if (!success)
			{
				LLMemory::allocationFailed();
				llwarns << "Out of memory trying to generate tangents"
						<< llendl;
				return false;
			}

			for (S32 i = 0; i < mNumIndices; ++i)
			{
				U32 src_idx = i;
				U32 dst_idx = remap[i];
				mIndices[i] = dst_idx;

				mPositions[dst_idx].load3(data.p[src_idx].mV);
				mNormals[dst_idx].load3(data.n[src_idx].mV);
				mTexCoords[dst_idx] = data.tc[src_idx];
				mTangents[dst_idx].loadua(data.t[src_idx].mV);
				if (mWeights)
				{
					mWeights[dst_idx].loadua(data.w[src_idx].mV);
				}
			}

			// Put back in normalized coordinate frame
			LLVector4a inv_scale(1.f / mNormalizedScale.mV[0],
								 1.f / mNormalizedScale.mV[1],
								 1.f / mNormalizedScale.mV[2]);
			LLVector4a scale;
			scale.load3(mNormalizedScale.mV);
			scale.getF32ptr()[3] = 1.f;
			for (S32 i = 0; i < mNumVertices; ++i)
			{
				mPositions[i].mul(inv_scale);
				mNormals[i].mul(scale);
				mNormals[i].normalize3();
				F32 w = mTangents[i].getF32ptr()[3];
				mTangents[i].mul(scale);
				mTangents[i].normalize3();
				mTangents[i].getF32ptr()[3] = w;
			}
		}
		else
		{
			// Blew past the max vertex size limit, use legacy tangent
			// generation which never adds verts.
			createTangents();
		}

		// Cache-optimize index buffer; meshopt needs scratch space, do some
		// pointer shuffling to avoid an extra index buffer copy.
		U16* src_indices = mIndices;
		mIndices = NULL;
		resizeIndices(mNumIndices);
		meshopt_optimizeVertexCache<U16>(mIndices, src_indices, mNumIndices,
										 mNumVertices);
		free_volume_mem(src_indices);
		return true;
	}

	// Pre-PBR code.

	if (mNumVertices < 3 || mNumIndices < 3)
	{
		// Nothing to do
		return true;
	}

	// Check indices validity and "fix" bogus ones if needed, since otherwise
	// meshoptimizer would likely assert and thus crash in case of an issue
	// with them... HB
	for (S32 i = 0; i < mNumIndices; ++i)
	{
		if (mIndices[i] >= mNumVertices)
		{
			// Invalid index: replace with a valid one to avoid a crash.
			llwarns_once << "Invalid vextex index in volume face "
						 << std::hex << (intptr_t)this << std::dec << llendl;
			mIndices[i] = mNumVertices - 1;
		}
	}

	struct buffer_data_t
	{
		// Double pointer to volume attribute data. Avoids fixup after
		// reallocating buffers on resize.
		void** dst;
		// Scratch buffer. Allocated with vertices count from
		// meshopt_generateVertexRemapMulti()
		void* scratch;
		// Stride between contiguous attributes
		size_t stride;
	};
	// Contains data needed by meshopt_generateVertexRemapMulti()
	std::vector<buffer_data_t> buffers;

	// Contains data needed by meshopt_remapVertexBuffer()
	std::vector<meshopt_Stream> streams;

	static struct
	{
		size_t offs;
		size_t size;
		size_t stride;
	} ref_streams[] =
	{
		{
			offsetof(LLVolumeFace, mPositions),
			sizeof(float) * 3,
			sizeof(mPositions[0])
		},
		{
			offsetof(LLVolumeFace, mNormals),
			sizeof(float) * 3,
			sizeof(mNormals[0])
		},
		{
			offsetof(LLVolumeFace, mTexCoords),
			sizeof(float) * 2,
			sizeof(mTexCoords[0])
		},
		{
			offsetof(LLVolumeFace, mWeights),
			sizeof(float) * 3,
			sizeof(mWeights[0])
		},
		{
			offsetof(LLVolumeFace, mTangents),
			sizeof(float) * 3,
			sizeof(mTangents[0])
		},
	};
	constexpr size_t ref_streams_elements = LL_ARRAY_SIZE(ref_streams);

	for (size_t i = 0; i < ref_streams_elements; ++i)
	{
		void** ptr =
			reinterpret_cast<void**>((char*)this + ref_streams[i].offs);
		if (*ptr)
		{
			streams.push_back({ *ptr, ref_streams[i].size,
								ref_streams[i].stride });
			buffers.push_back({ ptr, NULL, ref_streams[i].stride });
		}
	}

	std::vector<unsigned int> remap;
	try
	{
		remap.reserve(mNumIndices);
	}
	catch (const std::bad_alloc&)
	{
		LLMemory::allocationFailed();
		llwarns << "Out of memory trying to optimize vertices" << llendl;
		return false;
	}
	size_t total_verts = meshopt_generateVertexRemapMulti(remap.data(),
														  mIndices,
														  mNumIndices,
														  mNumVertices,
														  streams.data(),
														  streams.size());
	meshopt_remapIndexBuffer(mIndices, mIndices, mNumIndices, remap.data());
	bool failed = false;
	for (S32 i = 0, count = buffers.size(); i < count; ++i)
	{
		buffer_data_t& entry = buffers[i];
		void* buf_tmp = allocate_volume_mem(entry.stride * total_verts);
		if (!buf_tmp)
		{
			failed = true;
			break;
		}
		entry.scratch = buf_tmp;
		// Write to scratch buffer
		meshopt_remapVertexBuffer(entry.scratch, *entry.dst, mNumVertices,
								  entry.stride, remap.data());
	}
	if (failed)
	{
		for (S32 i = 0, count = buffers.size(); i < count; ++i)
		{
			buffer_data_t& entry = buffers[i];
			if (entry.scratch)
			{
				free_volume_mem(entry.scratch);
			}
		}
		LLMemory::allocationFailed();
		llwarns << "Out of memory trying to optimize vertices" << llendl;
	}
	else if (mNumAllocatedVertices != (S32)total_verts)
	{
		if (!resizeVertices(total_verts))
		{
			failed = true;
		}
		else if (mWeights && !allocateWeights(total_verts))
		{
			failed = true;
		}
		else if (mTangents && !allocateTangents(total_verts))
		{
			failed = true;
		}
	}
	if (failed)
	{
		for (S32 i = 0, count = buffers.size(); i < count; ++i)
		{
			buffer_data_t& entry = buffers[i];
			if (entry.scratch)
			{
				free_volume_mem(entry.scratch);
			}
		}
		return false;
	}

	meshopt_optimizeVertexCache(mIndices, mIndices, mNumIndices, total_verts);
#if 0	// Do not do that: it causes rendering glitches with some meshes. HB
	meshopt_optimizeOverdraw(mIndices, mIndices, mNumIndices,
							 (float*)buffers[0].scratch, total_verts,
							 buffers[0].stride, 1.05f);
#endif
	meshopt_optimizeVertexFetchRemap(remap.data(), mIndices, mNumIndices,
									 total_verts);
	meshopt_remapIndexBuffer(mIndices, mIndices, mNumIndices, remap.data());

	for (S32 i = 0, count = buffers.size(); i < count; ++i)
	{
		buffer_data_t& entry = buffers[i];
		// Write to LLVolume attribute buffer
		meshopt_remapVertexBuffer(*entry.dst, entry.scratch, total_verts,
								   entry.stride, remap.data());
		// Release scratch buffer
		if (entry.scratch)
		{
			free_volume_mem(entry.scratch);
		}
	}

	mNumVertices = total_verts;

	return true;
}

void LLVolumeFace::createOctree(F32 scaler, const LLVector4a& center0,
								const LLVector4a& size0)
{
	if (mOctree)
	{
		return;
	}

	mOctree = new LLOctreeRootNoOwnership<LLVolumeTriangle>(center0, size0,
															NULL);
	new LLVolumeOctreeListenerNoOwnership(mOctree);
	// Initialize all the triangles we need
	const U32 num_triangles = mNumIndices / 3;
	mOctreeTriangles = new LLVolumeTriangle[num_triangles];

	LLVector4a min, max, center, size;
	for (U32 tri_idx = 0; tri_idx < num_triangles; ++tri_idx)
	{
		// For each triangle
		LLVolumeTriangle* tri = &mOctreeTriangles[tri_idx];

		const U32 index = 3 * tri_idx;

		const LLVector4a& v0 = mPositions[mIndices[index]];
		const LLVector4a& v1 = mPositions[mIndices[index + 1]];
		const LLVector4a& v2 = mPositions[mIndices[index + 2]];

		// Store pointers to vertex data
		tri->mV[0] = &v0;
		tri->mV[1] = &v1;
		tri->mV[2] = &v2;

		// Store indices
		tri->mIndex[0] = mIndices[index];
		tri->mIndex[1] = mIndices[index + 1];
		tri->mIndex[2] = mIndices[index + 2];

		// Get minimum point
		min = v0;
		min.setMin(min, v1);
		min.setMin(min, v2);

		// Get maximum point
		max = v0;
		max.setMax(max, v1);
		max.setMax(max, v2);

		// Compute center
		center.setAdd(min, max);
		center.mul(0.5f);

		tri->mPositionGroup = center;

		// Compute "radius"
		size.setSub(max, min);

		tri->mRadius = size.getLength3().getF32() * scaler;

		// Insert
		mOctree->insert(tri);
	}

	// Remove unneeded octree layers
	while (!mOctree->balance()) ;

	// Calculate AABB for each node
	LLVolumeOctreeRebound rebound(this);
	rebound.traverse(mOctree);

	if (gDebugGL)
	{
		LLVolumeOctreeValidateNoOwnership validate;
		validate.traverse(mOctree);
	}
}

void LLVolumeFace::destroyOctree()
{
	if (mOctree)
	{
		delete mOctree;
		mOctree = NULL;
	}
	if (mOctreeTriangles)
	{
		delete[] mOctreeTriangles;
		mOctreeTriangles = NULL;
	}
}

void LLVolumeFace::swapData(LLVolumeFace& rhs)
{
	std::swap(rhs.mPositions, mPositions);
	std::swap(rhs.mNormals, mNormals);
	std::swap(rhs.mTangents, mTangents);
	std::swap(rhs.mTexCoords, mTexCoords);
	std::swap(rhs.mIndices, mIndices);
	std::swap(rhs.mNumVertices, mNumVertices);
	std::swap(rhs.mNumIndices, mNumIndices);
}

void lerp_planar_vert(LLVolumeFace::VertexData& v0,
					  LLVolumeFace::VertexData& v1,
					  LLVolumeFace::VertexData& v2,
					  LLVolumeFace::VertexData& vout,
					  F32 coef01, F32 coef02)
{

	LLVector4a lhs;
	lhs.setSub(v1.getPosition(), v0.getPosition());
	lhs.mul(coef01);
	LLVector4a rhs;
	rhs.setSub(v2.getPosition(), v0.getPosition());
	rhs.mul(coef02);

	rhs.add(lhs);
	rhs.add(v0.getPosition());

	vout.setPosition(rhs);

	vout.mTexCoord = v0.mTexCoord + (v1.mTexCoord - v0.mTexCoord) * coef01 +
					(v2.mTexCoord - v0.mTexCoord) * coef02;
	vout.setNormal(v0.getNormal());
}

bool LLVolumeFace::createUnCutCubeCap(LLVolume* volume, bool partial_build)
{
	const LLAlignedArray<LLVector4a, 64>& mesh = volume->getMesh();
	const LLAlignedArray<LLVector4a, 64>& profile = volume->getProfile().mVertices;
	S32 max_s = volume->getProfile().getTotal();
	S32 max_t = volume->getPath().mPath.size();

	S32	grid_size = (profile.size() - 1) / 4;

	LLVector4a& min = mExtents[0];
	LLVector4a& max = mExtents[1];

	S32 offset = 0;
	if (mTypeMask & TOP_MASK)
	{
		offset = (max_t - 1) * max_s;
	}
	else
	{
		offset = mBeginS;
	}

	{
		VertexData corners[4];
		VertexData base_vert;

		for (S32 t = 0; t < 4; ++t)
		{
			corners[t].getPosition().load3(mesh[offset +
												grid_size * t].getF32ptr());
			corners[t].mTexCoord.mV[0] = profile[grid_size * t][0] + 0.5f;
			corners[t].mTexCoord.mV[1] = 0.5f - profile[grid_size * t][1];
		}

		{
			LLVector4a lhs;
			lhs.setSub(corners[1].getPosition(), corners[0].getPosition());
			LLVector4a rhs;
			rhs.setSub(corners[2].getPosition(), corners[1].getPosition());
			base_vert.getNormal().setCross3(lhs, rhs);
			base_vert.getNormal().normalize3fast();
		}

		if (!(mTypeMask & TOP_MASK))
		{
			base_vert.getNormal().mul(-1.f);
		}
		else
		{
			// Swap the UVs on the U(X) axis for top face
			LLVector2 swap;
			swap = corners[0].mTexCoord;
			corners[0].mTexCoord = corners[3].mTexCoord;
			corners[3].mTexCoord = swap;
			swap = corners[1].mTexCoord;
			corners[1].mTexCoord = corners[2].mTexCoord;
			corners[2].mTexCoord = swap;
		}

		S32 size = (grid_size + 1) * (grid_size + 1);
		resizeVertices(size);

		LLVector4a* pos = (LLVector4a*)mPositions;
		LLVector4a* norm = (LLVector4a*)mNormals;
		LLVector2* tc = (LLVector2*)mTexCoords;

		VertexData new_vert;
		for (S32 gx = 0; gx <= grid_size; ++gx)
		{
			for (S32 gy = 0; gy <= grid_size; ++gy)
			{
				lerp_planar_vert(corners[0], corners[1], corners[3], new_vert,
								 (F32)gx / (F32)grid_size,
								 (F32)gy / (F32)grid_size);

				*pos++ = new_vert.getPosition();
				*norm++ = base_vert.getNormal();
				*tc++ = new_vert.mTexCoord;

				if (gx == 0 && gy == 0)
				{
					min = new_vert.getPosition();
					max = min;
				}
				else
				{
					min.setMin(min, new_vert.getPosition());
					max.setMax(max, new_vert.getPosition());
				}
			}
		}

		mCenter->setAdd(min, max);
		mCenter->mul(0.5f);
	}

	if (!partial_build)
	{
		size_t num_indices = grid_size * grid_size * 6;
		resizeIndices(num_indices);
		if (!volume->isMeshAssetLoaded() || mEdge.size() < num_indices)
		{
			mEdge.resize(num_indices);
		}

		U16* out = mIndices;
		S32 cur_edge = 0;

		S32 idxs[] = { 0, 1, grid_size + 2, grid_size + 2, grid_size + 1, 0 };
		for (S32 gx = 0; gx < grid_size; ++gx)
		{

			for (S32 gy = 0; gy < grid_size; ++gy)
			{
				if (mTypeMask & TOP_MASK)
				{
					for (S32 i = 5; i >= 0; --i)
					{
						*out++ = gy * (grid_size + 1) + gx + idxs[i];
					}
					S32 edge_value = grid_size * 2 * gy + gx * 2;
					if (gx > 0)
					{
						mEdge[cur_edge++] = edge_value;
					}
					else
					{
						mEdge[cur_edge++] = -1; // Mark face to higlight it
					}
					if (gy < grid_size - 1)
					{
						mEdge[cur_edge++] = edge_value;
					}
					else
					{
						mEdge[cur_edge++] = -1;
					}
					mEdge[cur_edge++] = edge_value;
					if (gx < grid_size - 1)
					{
						mEdge[cur_edge++] = edge_value;
					}
					else
					{
						mEdge[cur_edge++] = -1;
					}
					if (gy > 0)
					{
						mEdge[cur_edge++] = edge_value;
					}
					else
					{
						mEdge[cur_edge++] = -1;
					}
					mEdge[cur_edge++] = edge_value;
				}
				else
				{
					for (S32 i = 0; i < 6; ++i)
					{
						*out++ = gy * (grid_size + 1) + gx + idxs[i];
					}
					S32 edge_value = grid_size * 2 * gy + gx * 2;
					if (gy > 0)
					{
						mEdge[cur_edge++] = edge_value;
					}
					else
					{
						mEdge[cur_edge++] = -1;
					}
					if (gx < grid_size - 1)
					{
						mEdge[cur_edge++] = edge_value;
					}
					else
					{
						mEdge[cur_edge++] = -1;
					}
					mEdge[cur_edge++] = edge_value;
					if (gy < grid_size - 1)
					{
						mEdge[cur_edge++] = edge_value;
					}
					else
					{
						mEdge[cur_edge++] = -1;
					}
					if (gx > 0)
					{
						mEdge[cur_edge++] = edge_value;
					}
					else
					{
						mEdge[cur_edge++] = -1;
					}
					mEdge[cur_edge++] = edge_value;
				}
			}
		}
	}

	return true;
}

bool LLVolumeFace::createCap(LLVolume* volume, bool partial_build)
{
	constexpr U32 HOLLOW_OR_OPEN_MASK = HOLLOW_MASK | OPEN_MASK;

	const LLPathParams& params = volume->getParams().getPathParams();
	if (!(mTypeMask & HOLLOW_OR_OPEN_MASK) &&
		params.getBegin() == 0.f && params.getEnd()== 1.f &&
		params.getCurveType() == LL_PCODE_PATH_LINE &&
		volume->getParams().getProfileParams().getCurveType() ==
			LL_PCODE_PROFILE_SQUARE)
	{
		return createUnCutCubeCap(volume, partial_build);
	}

	S32 num_vertices = 0, num_indices = 0;

	const LLAlignedArray<LLVector4a, 64>& mesh = volume->getMesh();
	const LLAlignedArray<LLVector4a, 64>& profile =
		volume->getProfile().mVertices;

	// All types of caps have the same number of vertices and indices
	num_vertices = profile.size();
	num_indices = (num_vertices - 2) * 3;

	if (!(mTypeMask & HOLLOW_OR_OPEN_MASK))
	{
		resizeVertices(num_vertices + 1);
#if 0
		if (!partial_build)
#endif
		{
			resizeIndices(num_indices + 3);
		}
	}
	else
	{
		resizeVertices(num_vertices);
#if 0
		if (!partial_build)
#endif
		{
			resizeIndices(num_indices);
		}
	}

	S32 max_s = volume->getProfile().getTotal();
	S32 max_t = volume->getPath().mPath.size();

	mCenter->clear();

	S32 offset = 0;
	if (mTypeMask & TOP_MASK)
	{
		offset = (max_t - 1) * max_s;
	}
	else
	{
		offset = mBeginS;
	}

	// Figure out the normal, assume all caps are flat faces. Cross product to
	// get normals.

	LLVector2 cuv, min_uv, max_uv;

	LLVector4a& min = mExtents[0];
	LLVector4a& max = mExtents[1];

	LLVector2* tc = (LLVector2*)mTexCoords;
	LLVector4a* pos = (LLVector4a*)mPositions;
	LLVector4a* norm = (LLVector4a*)mNormals;

	// Copy the vertices into the array

	const LLVector4a* src = mesh.mArray + offset;
	const LLVector4a* end = src + num_vertices;
	min = *src;
	max = min;
	const LLVector4a* p = profile.mArray;
	if (mTypeMask & TOP_MASK)
	{
		min_uv.set((*p)[0] + 0.5f, (*p)[1] + 0.5f);
		max_uv = min_uv;

		while (src < end)
		{
			tc->mV[0] = (*p)[0] + 0.5f;
			tc->mV[1] = (*p++)[1] + 0.5f;

			llassert(src->isFinite3());
			update_min_max(min, max, *src);
			update_min_max(min_uv, max_uv, *tc++);

			*pos++ = *src++;
		}
	}
	else
	{
		min_uv.set((*p)[0] + 0.5f, 0.5f - (*p)[1]);
		max_uv = min_uv;

		while (src < end)
		{
			// Mirror for underside.
			tc->mV[0] = (*p)[0] + 0.5f;
			tc->mV[1] = 0.5f - (*p++)[1];

			llassert(src->isFinite3());
			update_min_max(min, max, *src);
			update_min_max(min_uv, max_uv, *tc++);

			*pos++ = *src++;
		}
	}

	mCenter->setAdd(min, max);
	mCenter->mul(0.5f);

	cuv = (min_uv + max_uv) * 0.5f;

	VertexData vd;
	vd.setPosition(*mCenter);
	vd.mTexCoord = cuv;

	if (!(mTypeMask & HOLLOW_OR_OPEN_MASK))
	{
		*pos++ = *mCenter;
		*tc++ = cuv;
		++num_vertices;
	}

#if 0
	if (partial_build)
	{
		return true;
	}
#endif

	if (mTypeMask & HOLLOW_MASK)
	{
		if (mTypeMask & TOP_MASK)
		{
			// HOLLOW TOP
			// Does it matter if it's open or closed ? - djs

			S32 pt1 = 0, pt2 = num_vertices - 1;
			S32 i = 0;
			while (pt2 - pt1 > 1)
			{
				// Use the profile points instead of the mesh, since you want
				// the un-transformed profile distances.
				const LLVector4a& p1 = profile[pt1];
				const LLVector4a& p2 = profile[pt2];
				const LLVector4a& pa = profile[pt1 + 1];
				const LLVector4a& pb = profile[pt2 - 1];

				const F32* p1V = p1.getF32ptr();
				const F32* p2V = p2.getF32ptr();
				const F32* paV = pa.getF32ptr();
				const F32* pbV = pb.getF32ptr();

				// Use area of triangle to determine backfacing
				F32 area_1a2, area_1ba, area_21b, area_2ab;
				area_1a2 =  p1V[0] * paV[1] - paV[0] * p1V[1] +
							paV[0] * p2V[1] - p2V[0] * paV[1] +
							p2V[0] * p1V[1] - p1V[0] * p2V[1];

				area_1ba =  p1V[0] * pbV[1] - pbV[0] * p1V[1] +
							pbV[0] * paV[1] - paV[0] * pbV[1] +
							paV[0] * p1V[1] - p1V[0] * paV[1];

				area_21b =  p2V[0] * p1V[1] - p1V[0] * p2V[1] +
							p1V[0] * pbV[1] - pbV[0] * p1V[1] +
							pbV[0] * p2V[1] - p2V[0] * pbV[1];

				area_2ab =  p2V[0] * paV[1] - paV[0] * p2V[1] +
							paV[0] * pbV[1] - pbV[0] * paV[1] +
							pbV[0] * p2V[1] - p2V[0] * pbV[1];

				bool use_tri1a2 = true;
				bool tri_1a2 = true;
				bool tri_21b = true;

				if (area_1a2 < 0)
				{
					tri_1a2 = false;
				}
				if (area_2ab < 0)
				{
					// Cannot use, because it contains point b
					tri_1a2 = false;
				}
				if (area_21b < 0)
				{
					tri_21b = false;
				}
				if (area_1ba < 0)
				{
					// Cannot use, because it contains point b
					tri_21b = false;
				}

				if (!tri_1a2)
				{
					use_tri1a2 = false;
				}
				else if (!tri_21b)
				{
					use_tri1a2 = true;
				}
				else
				{
					LLVector4a d1;
					d1.setSub(p1, pa);

					LLVector4a d2;
					d2.setSub(p2, pb);

					use_tri1a2 = d1.dot3(d1) < d2.dot3(d2);
				}

				if (use_tri1a2)
				{
					mIndices[i++] = pt1;
					mIndices[i++] = ++pt1;
					mIndices[i++] = pt2;
				}
				else
				{
					mIndices[i++] = pt1;
					mIndices[i++] = pt2 - 1;
					mIndices[i++] = pt2--;
				}
			}
		}
		else
		{
			// HOLLOW BOTTOM
			// Does it matter if it's open or closed? - djs

			llassert(mTypeMask & BOTTOM_MASK);
			S32 pt1 = 0, pt2 = num_vertices - 1;

			S32 i = 0;
			while (pt2 - pt1 > 1)
			{
				// Use the profile points instead of the mesh, since you want
				// the un-transformed profile distances.
				const LLVector4a& p1 = profile[pt1];
				const LLVector4a& p2 = profile[pt2];
				const LLVector4a& pa = profile[pt1 + 1];
				const LLVector4a& pb = profile[pt2 - 1];

				const F32* p1V = p1.getF32ptr();
				const F32* p2V = p2.getF32ptr();
				const F32* paV = pa.getF32ptr();
				const F32* pbV = pb.getF32ptr();

				// Use area of triangle to determine backfacing
				F32 area_1a2, area_1ba, area_21b, area_2ab;
				area_1a2 =  p1V[0] * paV[1] - paV[0] * p1V[1] +
							paV[0] * p2V[1] - p2V[0] * paV[1] +
							p2V[0] * p1V[1] - p1V[0] * p2V[1];

				area_1ba =  p1V[0] * pbV[1] - pbV[0] * p1V[1] +
							pbV[0] * paV[1] - paV[0] * pbV[1] +
							paV[0] * p1V[1] - p1V[0] * paV[1];

				area_21b =  p2V[0] * p1V[1] - p1V[0] * p2V[1] +
							p1V[0] * pbV[1] - pbV[0] * p1V[1] +
							pbV[0] * p2V[1] - p2V[0] * pbV[1];

				area_2ab =  p2V[0] * paV[1] - paV[0] * p2V[1] +
							paV[0] * pbV[1] - pbV[0] * paV[1] +
							pbV[0] * p2V[1] - p2V[0] * pbV[1];

				bool use_tri1a2 = true;
				bool tri_1a2 = true;
				bool tri_21b = true;

				if (area_1a2 < 0)
				{
					tri_1a2 = false;
				}
				if (area_2ab < 0)
				{
					// Cannot use, because it contains point b
					tri_1a2 = false;
				}
				if (area_21b < 0)
				{
					tri_21b = false;
				}
				if (area_1ba < 0)
				{
					// Cannot use, because it contains point b
					tri_21b = false;
				}

				if (!tri_1a2)
				{
					use_tri1a2 = false;
				}
				else if (!tri_21b)
				{
					use_tri1a2 = true;
				}
				else
				{
					LLVector4a d1;
					d1.setSub(p1, pa);
					LLVector4a d2;
					d2.setSub(p2, pb);

					use_tri1a2 = d1.dot3(d1) < d2.dot3(d2);
				}

				// Flipped backfacing from top
				if (use_tri1a2)
				{
					mIndices[i++] = pt1;
					mIndices[i++] = pt2;
					mIndices[i++] = ++pt1;
				}
				else
				{
					mIndices[i++] = pt1;
					mIndices[i++] = pt2;
					mIndices[i++] = --pt2;
				}
			}
		}
	}
	else
	{
		// Not hollow, generate the triangle fan.
		U16 v1 = 2;
		U16 v2 = 1;

		if (mTypeMask & TOP_MASK)
		{
			v1 = 1;
			v2 = 2;
		}

		for (S32 i = 0; i < num_vertices - 2; ++i)
		{
			mIndices[3 * i] = num_vertices - 1;
			mIndices[3 * i + v1] = i;
			mIndices[3 * i + v2] = i + 1;
		}
	}

	LLVector4a d0, d1;

	d0.setSub(mPositions[mIndices[1]], mPositions[mIndices[0]]);
	d1.setSub(mPositions[mIndices[2]], mPositions[mIndices[0]]);

	LLVector4a normal;
	normal.setCross3(d0, d1);

	if (normal.dot3(normal).getF32() > F_APPROXIMATELY_ZERO)
	{
		normal.normalize3fast();
	}
	// Degenerate, make up a value
	else if (normal.getF32ptr()[2] >= 0)
	{
		normal.set(0.f, 0.f, 1.f);
	}
	else
	{
		normal.set(0.f, 0.f, -1.f);
	}

	llassert(llfinite(normal.getF32ptr()[0]));
	llassert(llfinite(normal.getF32ptr()[1]));
	llassert(llfinite(normal.getF32ptr()[2]));

	llassert(!llisnan(normal.getF32ptr()[0]));
	llassert(!llisnan(normal.getF32ptr()[1]));
	llassert(!llisnan(normal.getF32ptr()[2]));

	for (S32 i = 0; i < num_vertices; ++i)
	{
		norm[i].load4a(normal.getF32ptr());
	}

	return true;
}

// Adapted from Lengyel, Eric. "Computing Tangent Space Basis Vectors for an
// Arbitrary Mesh". Terathon Software 3D Graphics Library, 2001.
// http://www.terathon.com/code/tangent.html
bool CalculateTangentArray(const U32 vertex_count,
						   const LLVector4a* vertex,
						   const LLVector4a* normal,
						   const LLVector2* texcoord,
						   U32 triangle_count,
						   const U16* index_array,
						   LLVector4a* tangent)
{
	const size_t size = vertex_count * 2 * sizeof(LLVector4a);
	LLVector4a* tan1 = (LLVector4a*)allocate_volume_mem(size);
	if (!tan1) return false;

	LLVector4a* tan2 = tan1 + vertex_count;
	if (size > 0)
	{
		memset((void*)tan1, 0, size);
	}

	for (U32 a = 0; a < triangle_count; ++a)
	{
		U32 i1 = *index_array++;
		U32 i2 = *index_array++;
		U32 i3 = *index_array++;

		const LLVector4a& v1 = vertex[i1];
		const LLVector4a& v2 = vertex[i2];
		const LLVector4a& v3 = vertex[i3];

		const LLVector2& w1 = texcoord[i1];
		const LLVector2& w2 = texcoord[i2];
		const LLVector2& w3 = texcoord[i3];

		const F32* v1ptr = v1.getF32ptr();
		const F32* v2ptr = v2.getF32ptr();
		const F32* v3ptr = v3.getF32ptr();

		F32 x1 = v2ptr[0] - v1ptr[0];
		F32 x2 = v3ptr[0] - v1ptr[0];
		F32 y1 = v2ptr[1] - v1ptr[1];
		F32 y2 = v3ptr[1] - v1ptr[1];
		F32 z1 = v2ptr[2] - v1ptr[2];
		F32 z2 = v3ptr[2] - v1ptr[2];

        F32 s1 = w2.mV[0] - w1.mV[0];
        F32 s2 = w3.mV[0] - w1.mV[0];
        F32 t1 = w2.mV[1] - w1.mV[1];
        F32 t2 = w3.mV[1] - w1.mV[1];

		F32 rd = s1 * t2 - s2 * t1;
		F32 r = rd * rd > FLT_EPSILON ? 1.f / rd
									  : (rd > 0.f ? 1024.f : -1024.f);
		llassert(llfinite(r) && !llisnan(r));

		LLVector4a sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r,
						(t2 * z1 - t1 * z2) * r);
		LLVector4a tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r,
						(s1 * z2 - s2 * z1) * r);

		tan1[i1].add(sdir);
		tan1[i2].add(sdir);
		tan1[i3].add(sdir);

		tan2[i1].add(tdir);
		tan2[i2].add(tdir);
		tan2[i3].add(tdir);
	}

	LLVector4a n, ncrosst, tsubn;
	for (U32 a = 0; a < vertex_count; ++a)
	{
		n = normal[a];
		const LLVector4a& t = tan1[a];

		ncrosst.setCross3(n, t);

		// Gram-Schmidt orthogonalize
		n.mul(n.dot3(t).getF32());

		tsubn.setSub(t, n);

		if (tsubn.dot3(tsubn).getF32() > F_APPROXIMATELY_ZERO)
		{
			tsubn.normalize3fast();

			// Calculate handedness
			F32 handedness = ncrosst.dot3(tan2[a]).getF32() < 0.f ? -1.f : 1.f;

			tsubn.getF32ptr()[3] = handedness;

			tangent[a] = tsubn;
		}
		else
		{
			// Degenerate, make up a value
			tangent[a].set(0.f, 0.f, 1.f, 1.f);
		}
	}

	free_volume_mem(tan1);

	return true;
}

void LLVolumeFace::createTangents()
{
	if (!mTangents)
	{
		if (!allocateTangents(mNumVertices))
		{
			LLMemory::allocationFailed();
			llwarns << "Out of memory error while calculating tangents !"
					<< llendl;
			return;
		}

		// Generate tangents
		LLVector4a* ptr = (LLVector4a*)mTangents;
		LLVector4a* end = mTangents + mNumVertices;
		while (ptr < end)
		{
			(*ptr++).clear();
		}

		if (!CalculateTangentArray(mNumVertices, mPositions, mNormals,
								   mTexCoords, mNumIndices / 3, mIndices,
								   mTangents))
		{
			LLMemory::allocationFailed();
			llwarns << "Out of memory error while calculating tangents !"
					<< llendl;
			return;
		}

		// Normalize normals
		for (S32 i = 0; i < mNumVertices; ++i)
		{
			// Bump map/planar projection code requires normals to be
			// normalized
			mNormals[i].normalize3fast();
		}
	}
}

bool LLVolumeFace::resizeVertices(S32 num_verts)
{
	if (mPositions)
	{
		free_volume_mem_64(mPositions);
		mPositions = NULL;
	}

	// NOTE: mNormals and mTexCoords are part of mPositions: do not free them !
	mNormals = NULL;
	mTexCoords = NULL;

	if (mTangents)
	{
		free_volume_mem(mTangents);
		mTangents = NULL;
	}

	mNumVertices = num_verts > 0 ? num_verts : 0;
	mNumAllocatedVertices = mNumVertices;
	if (mNumVertices)
	{
		// Pad texture coordinate block end to allow for QWORD reads
		size_t size = (num_verts * sizeof(LLVector2) + 0xF) & ~0xF;
		S32 bytes = sizeof(LLVector4a) * 2 * num_verts + size;
		mPositions = (LLVector4a*)allocate_volume_mem_64(bytes);
		if (!mPositions)
		{
			LLMemory::allocationFailed(bytes);
			llwarns << "Out of memory while resizing vertex positions !"
					<< llendl;
			mNumVertices = mNumAllocatedVertices = 0;
			return false;
		}
		ll_assert_aligned(mPositions, 64);

		mNormals = mPositions + num_verts;
		mTexCoords = (LLVector2*)(mNormals + num_verts);
	}

	// Force update
	mJointRiggingInfoTab.clear();
	return true;
}

void LLVolumeFace::pushVertex(const LLVolumeFace::VertexData& cv,
							  S32 max_indice)
{
	pushVertex(cv.getPosition(), cv.getNormal(), cv.mTexCoord, max_indice);
}

void LLVolumeFace::pushVertex(const LLVector4a& pos, const LLVector4a& norm,
							  const LLVector2& tc, S32 max_indice)
{
	S32 new_verts = mNumVertices + 1;
	if (new_verts > mNumAllocatedVertices)
	{
		if (new_verts < max_indice)
		{
			if (new_verts < max_indice / 2)
			{
				// It is very unlikely that we will manage to optimize beyond
				// the point of halving the number of vertices...
				new_verts = max_indice / 2;
			}
			else
			{
				S32 delta = llmin((max_indice -  new_verts) / 2, 2);
				new_verts = new_verts + delta < max_indice ? new_verts + delta
														   : max_indice;
			}
		}
		S32 new_tc_size = ((new_verts * 8) + 0xF) & ~0xF;
		S32 old_tc_size = ((mNumVertices * 8) + 0xF) & ~0xF;
		S32 old_vsize = mNumVertices * 16;
		S32 new_size = new_verts * 16 * 2 + new_tc_size;
		LLVector4a* old_buf = mPositions;
		mPositions = (LLVector4a*)allocate_volume_mem_64(new_size);
		if (!mPositions && new_verts != mNumVertices + 1)
		{
			LLMemory::allocationFailed(new_size);
			// out of memory: try to allocate the exact required amount instead
			new_verts = mNumVertices + 1;
			new_tc_size = ((new_verts * 8) + 0xF) & ~0xF;
			old_tc_size = ((mNumVertices * 8) + 0xF) & ~0xF;
			old_vsize = mNumVertices * 16;
			new_size = new_verts * 16 * 2 + new_tc_size;
			mPositions = (LLVector4a*)allocate_volume_mem_64(new_size);
		}
		if (!mPositions)
		{
			LLMemory::allocationFailed();
			mPositions = old_buf;
			llwarns << "Out of memory while reallocating vertex data !"
					<< llendl;
			return;
		}

		mNormals = mPositions + new_verts;
		mTexCoords = (LLVector2*)(mNormals + new_verts);

		if (mNumVertices && old_buf)
		{
			LLVector4a::memcpyNonAliased16((F32*)mPositions, (F32*)old_buf,
										   old_vsize);

			LLVector4a::memcpyNonAliased16((F32*)mNormals,
										   (F32*)(old_buf + mNumVertices),
										   old_vsize);

			LLVector4a::memcpyNonAliased16((F32*)mTexCoords,
										   (F32*)(old_buf + mNumVertices * 2),
										   old_tc_size);
		}

		// Just clear tangents
		if (mTangents)
		{
			free_volume_mem(mTangents);
			mTangents = NULL;
		}

		mNumAllocatedVertices = new_verts;
	}

	mPositions[mNumVertices] = pos;
	mNormals[mNumVertices] = norm;
	mTexCoords[mNumVertices++] = tc;
}

bool LLVolumeFace::allocateTangents(S32 num_verts)
{
	if (mTangents)
	{
		free_volume_mem(mTangents);
	}
	mTangents = (LLVector4a*)allocate_volume_mem(sizeof(LLVector4a) *
												 num_verts);
	if (mTangents)
	{
		return true;
	}
	LLMemory::allocationFailed();
	llwarns << "Out of memory trying to allocate " << num_verts << " tangents"
			<< llendl;
	return false;
}

bool LLVolumeFace::allocateWeights(S32 num_verts)
{
	if (mWeights)
	{
		free_volume_mem(mWeights);
	}
	mWeights = (LLVector4a*)allocate_volume_mem(sizeof(LLVector4a) *
												num_verts);
	if (mWeights)
	{
		return true;
	}
	LLMemory::allocationFailed();
	llwarns << "Out of memory trying to allocate " << num_verts << " weigths"
			<< llendl;
	return false;
}

bool LLVolumeFace::resizeIndices(S32 num_indices)
{
	if (mNumIndices == num_indices)
	{
		return true;
	}

	if (mIndices)
	{
		free_volume_mem(mIndices);
	}

	if (num_indices < 0)
	{
		llwarns << "Negative number of indices passed (" << num_indices
				<< "). Zeored." << llendl;
		return false;
	}
	if (num_indices == 0)
	{
		mIndices = NULL;
		mNumIndices = 0;
		return true;
	}

	// Pad index block end to allow for QWORD reads
	S32 size = ((num_indices * sizeof(U16)) + 0xF) & ~0xF;
	mIndices = (U16*)allocate_volume_mem(size);
	if (mIndices)
	{
		mNumIndices = num_indices;
		return true;
	}

	mNumIndices = 0;
	LLMemory::allocationFailed();
	llwarns << "Out of memory trying to allocate " << num_indices << " indices"
			<< llendl;
	return false;
}

void LLVolumeFace::pushIndex(const U16& idx)
{
	S32 new_count = mNumIndices + 1;
	S32 new_size = (new_count * 2 + 0xF) & ~0xF;
	S32 old_size = (mNumIndices * 2 + 0xF) & ~0xF;
	if (new_size != old_size)
	{
		mIndices = (U16*)realloc_volume_mem(mIndices, new_size, old_size);
		ll_assert_aligned(mIndices, 16);
	}

	mIndices[mNumIndices++] = idx;
}

void LLVolumeFace::fillFromLegacyData(std::vector<LLVolumeFace::VertexData>& v,
									  std::vector<U16>& idx)
{
	resizeVertices(v.size());
	resizeIndices(idx.size());

	for (S32 i = 0, count = v.size(); i < count; ++i)
	{
		mPositions[i] = v[i].getPosition();
		mNormals[i] = v[i].getNormal();
		mTexCoords[i] = v[i].mTexCoord;
	}

	for (S32 i = 0, count = idx.size(); i < count; ++i)
	{
		mIndices[i] = idx[i];
	}
}

bool LLVolumeFace::createSide(LLVolume* volume, bool partial_build)
{
	bool flat = (mTypeMask & FLAT_MASK) != 0;

	U8 sculpt_type = volume->getParams().getSculptType();
	U8 sculpt_stitching = sculpt_type & LL_SCULPT_TYPE_MASK;
	bool sculpt_invert = (sculpt_type & LL_SCULPT_FLAG_INVERT) != 0;
	bool sculpt_mirror = (sculpt_type & LL_SCULPT_FLAG_MIRROR) != 0;
	bool sculpt_reverse_horizontal = sculpt_invert ? !sculpt_mirror
												   : sculpt_mirror;

	S32 num_vertices, num_indices;

	const LLAlignedArray<LLVector4a, 64>& mesh = volume->getMesh();
	const LLAlignedArray<LLVector4a, 64>& profile =
		volume->getProfile().mVertices;
	const LLAlignedArray<LLPath::PathPt, 64>& path_data =
		volume->getPath().mPath;

	S32 max_s = volume->getProfile().getTotal();

	S32 s, t, i;
	F32 ss, tt;

	num_vertices = mNumS * mNumT;
	num_indices = (mNumS - 1) * (mNumT - 1) * 6;

	if (num_vertices > mNumVertices || num_indices > mNumIndices)
	{
		partial_build = false;
	}
	if (!partial_build)
	{
		resizeVertices(num_vertices);
		resizeIndices(num_indices);

		if (!volume->isMeshAssetLoaded())
		{
			mEdge.resize(num_indices);
		}
	}

	LLVector4a* pos = (LLVector4a*)mPositions;
	LLVector2* tc = (LLVector2*)mTexCoords;
	F32 begin_stex = llfloor(profile[mBeginS][2]);
	S32 num_s = ((mTypeMask & INNER_MASK) &&
				 (mTypeMask & FLAT_MASK) && mNumS > 2) ? mNumS / 2 : mNumS;

	S32 cur_vertex = 0;
	S32 end_t = mBeginT + mNumT;
	bool test = (mTypeMask & INNER_MASK) && (mTypeMask & FLAT_MASK) &&
				mNumS > 2;

	// Copy the vertices into the array
	for (t = mBeginT; t < end_t; ++t)
	{
		tt = path_data[t].mTexT;
		for (s = 0; s < num_s; ++s)
		{
			if (mTypeMask & END_MASK)
			{
				if (s)
				{
					ss = 1.f;
				}
				else
				{
					ss = 0.f;
				}
			}
			// Get s value for tex-coord.
			else
			{
				S32 index = mBeginS + s;
				if (index >= (S32)profile.size())
				{
					ss = flat ? 1.f - begin_stex : 1.f;
				}
				else if (flat)
				{
					ss = profile[index][2] - begin_stex;
				}
				else
				{
					ss = profile[index][2];
				}
			}

			if (sculpt_reverse_horizontal)
			{
				ss = 1.f - ss;
			}

			// Check to see if this triangle wraps around the array.
			if (mBeginS + s >= max_s)
			{
				// We are wrapping
				i = mBeginS + s + max_s * (t - 1);
			}
			else
			{
				i = mBeginS + s + max_s * t;
			}

			mesh[i].store4a((F32*)(pos + cur_vertex));
			tc[cur_vertex++].set(ss, tt);

			if (test && s > 0)
			{
				mesh[i].store4a((F32*)(pos + cur_vertex));
				tc[cur_vertex++].set(ss, tt);
			}
		}

		if (test)
		{
			if (mTypeMask & OPEN_MASK)
			{
				s = num_s - 1;
			}
			else
			{
				s = 0;
			}

			i = mBeginS + s + max_s * t;
			ss = profile[mBeginS + s][2] - begin_stex;
			mesh[i].store4a((F32*)(pos + cur_vertex));
			tc[cur_vertex++].set(ss, tt);
		}
	}

	mCenter->clear();

	LLVector4a* cur_pos = pos;
	LLVector4a* end_pos = pos + mNumVertices;

	// Get bounding box for this side
	LLVector4a face_min;
	LLVector4a face_max;

	face_min = face_max = *cur_pos++;

	while (cur_pos < end_pos)
	{
		update_min_max(face_min, face_max, *cur_pos++);
	}

	mExtents[0] = face_min;
	mExtents[1] = face_max;

	U32 tc_count = mNumVertices;
	if (tc_count % 2 == 1)
	{
		// Odd number of texture coordinates, duplicate last entry to padded
		// end of array
		++tc_count;
		mTexCoords[mNumVertices] = mTexCoords[mNumVertices - 1];
	}

	LLVector4a* cur_tc = (LLVector4a*)mTexCoords;
	LLVector4a* end_tc = (LLVector4a*)(mTexCoords + tc_count);

	LLVector4a tc_min;
	LLVector4a tc_max;

	tc_min = tc_max = *cur_tc++;

	while (cur_tc < end_tc)
	{
		update_min_max(tc_min, tc_max, *cur_tc++);
	}

	F32* minp = tc_min.getF32ptr();
	F32* maxp = tc_max.getF32ptr();

	mTexCoordExtents[0].mV[0] = llmin(minp[0], minp[2]);
	mTexCoordExtents[0].mV[1] = llmin(minp[1], minp[3]);
	mTexCoordExtents[1].mV[0] = llmax(maxp[0], maxp[2]);
	mTexCoordExtents[1].mV[1] = llmax(maxp[1], maxp[3]);

	mCenter->setAdd(face_min, face_max);
	mCenter->mul(0.5f);

	S32 cur_index = 0;
	S32 cur_edge = 0;
	bool flat_face = (mTypeMask & FLAT_MASK) != 0;

	if (!partial_build)
	{
		// Now we generate the indices.
		for (t = 0; t < mNumT - 1; ++t)
		{
			for (s = 0; s < mNumS - 1; ++s)
			{
				S32 bottom_left = s + mNumS * t;
				mIndices[cur_index++] = bottom_left;
				S32 top_right = s + 1 + mNumS * (t + 1);
				mIndices[cur_index++] = top_right;
				mIndices[cur_index++] = s + mNumS * (t + 1); // top left
				mIndices[cur_index++] = bottom_left;
				mIndices[cur_index++] = s + 1 + mNumS * t; // bottom right
				mIndices[cur_index++] = top_right;

				// Bottom left/top right neighbor face
				mEdge[cur_edge++] = (mNumS - 1) * 2 * t + s * 2 + 1;

				if (t < mNumT - 2)
				{
					// Top right/top left neighbor face
					mEdge[cur_edge++] = (mNumS - 1) * 2 * (t + 1) + s * 2 + 1;
				}
				else if (mNumT <= 3 || volume->getPath().isOpen())
				{
					// No neighbor
					mEdge[cur_edge++] = -1;
				}
				else
				{
					// Wrap on T
					mEdge[cur_edge++] = s * 2 + 1;
				}
				if (s > 0)
				{
					// Top left/bottom left neighbor face
					mEdge[cur_edge++] = (mNumS - 1) * 2 * t + s * 2 - 1;
				}
				else if (flat_face || volume->getProfile().isOpen())
				{
					// No neighbor
					mEdge[cur_edge++] = -1;
				}
				else
				{
					// Wrap on S
					mEdge[cur_edge++] = (mNumS - 1) * 2 * t + (mNumS - 2) * 2 +
										1;
				}

				if (t > 0)
				{
					// bottom left/bottom right neighbor face
					mEdge[cur_edge++] = (mNumS - 1) * 2 * (t - 1) + s * 2;
				}
				else if (mNumT <= 3 || volume->getPath().isOpen())
				{
					// No neighbor
					mEdge[cur_edge++] = -1;
				}
				else
				{
					// Wrap on T
					mEdge[cur_edge++] = (mNumS - 1) * 2 * (mNumT - 2) + s * 2;
				}
				if (s < mNumS - 2)
				{
					// Bottom right/top right neighbor face
					mEdge[cur_edge++] = (mNumS - 1) * 2 * t + (s + 1) * 2;
				}
				else if (flat_face || volume->getProfile().isOpen())
				{
					// No neighbor
					mEdge[cur_edge++] = -1;
				}
				else
				{
					// Wrap on S
					mEdge[cur_edge++] = (mNumS - 1) * 2 * t;
				}
				// Top right/bottom left neighbor face
				mEdge[cur_edge++] = (mNumS - 1) * 2 * t + s * 2;
			}
		}
	}

	// Clear normals
	F32* dst = (F32*)mNormals;
	F32* end = (F32*)(mNormals + mNumVertices);
	LLVector4a zero = LLVector4a::getZero();
	while (dst < end)
	{
		zero.store4a(dst);
		dst += 4;
	}

	// Generate normals
	U32 count = mNumIndices / 3;
	LLVector4a* norm = mNormals;
	// thread_local instead of static, in case this method would be called by
	// another thread than the main thread in the future (I do not think it is
	// the case for now, but what happened with LLProfile::addHole() makes me
	// wary). HB
	thread_local LLAlignedArray<LLVector4a, 64> triangle_normals;
	triangle_normals.resize(count);
	LLVector4a* output = triangle_normals.mArray;
	LLVector4a* end_output = output + count;
	U16* idx = mIndices;
	LLVector4a b, v1, v2;
	while (output < end_output)
	{
		b.load4a((F32*)(pos + idx[0]));
		v1.load4a((F32*)(pos + idx[1]));
		v2.load4a((F32*)(pos + idx[2]));

		// Calculate triangle normal
		LLVector4a a;
		a.setSub(b, v1);
		b.sub(v2);

		LLQuad& vector1 = *((LLQuad*)&v1);
		LLQuad& vector2 = *((LLQuad*)&v2);

		LLQuad& amQ = *((LLQuad*)&a);
		LLQuad& bmQ = *((LLQuad*)&b);

		// Vectors are stored in memory in w, z, y, x order from high to low
		// Set vector1 = { a[W], a[X], a[Z], a[Y] }
		vector1 = _mm_shuffle_ps(amQ, amQ, _MM_SHUFFLE(3, 0, 2, 1));

		// Set vector2 = { b[W], b[Y], b[X], b[Z] }
		vector2 = _mm_shuffle_ps(bmQ, bmQ, _MM_SHUFFLE(3, 1, 0, 2));

		// mQ = { a[W]*b[W], a[X]*b[Y], a[Z]*b[X], a[Y]*b[Z] }
		vector2 = _mm_mul_ps(vector1, vector2);

		// vector3 = { a[W], a[Y], a[X], a[Z] }
		amQ = _mm_shuffle_ps( amQ, amQ, _MM_SHUFFLE(3, 1, 0, 2));

		// vector4 = { b[W], b[X], b[Z], b[Y] }
		bmQ = _mm_shuffle_ps(bmQ, bmQ, _MM_SHUFFLE(3, 0, 2, 1));

		// mQ = { 0, a[X]*b[Y] - a[Y]*b[X], a[Z]*b[X] - a[X]*b[Z],
		//		  a[Y]*b[Z] - a[Z]*b[Y] }
		vector1 = _mm_sub_ps(vector2, _mm_mul_ps(amQ, bmQ));

		llassert(v1.isFinite3());

		v1.store4a((F32*)output++);
		idx += 3;
	}

	idx = mIndices;

	LLVector4a* src = triangle_normals.mArray;

	LLVector4a c, n0, n1, n2;
	for (U32 i = 0; i < count; ++i) // For each triangle
	{
		c.load4a((F32*)src++);

		LLVector4a* n0p = norm + idx[0];
		LLVector4a* n1p = norm + idx[1];
		LLVector4a* n2p = norm + idx[2];

		idx += 3;

		n0.load4a((F32*)n0p);
		n1.load4a((F32*)n1p);
		n2.load4a((F32*)n2p);

		n0.add(c);
		n1.add(c);
		n2.add(c);

		llassert(c.isFinite3());

		// Even out quad contributions
		switch (i % 2 + 1)
		{
			case 0: n0.add(c); break;
			case 1: n1.add(c); break;
			case 2: n2.add(c); break;
		}

		n0.store4a((F32*)n0p);
		n1.store4a((F32*)n1p);
		n2.store4a((F32*)n2p);
	}

	// Adjust normals based on wrapping and stitching

	LLVector4a top;
	top.setSub(pos[0], pos[mNumS * (mNumT - 2)]);
	bool s_bottom_converges = top.dot3(top) < 0.000001f;

	top.setSub(pos[mNumS - 1], pos[mNumS * (mNumT - 2) + mNumS - 1]);
	bool s_top_converges = top.dot3(top) < 0.000001f;

	// Logic for non-sculpt volumes:
	if (sculpt_stitching == LL_SCULPT_TYPE_NONE)
	{
		if (!volume->getPath().isOpen())
		{
			// Wrap normals on T
			LLVector4a n;
			for (S32 i = 0; i < mNumS; ++i)
			{
				n.setAdd(norm[i], norm[mNumS * (mNumT - 1) + i]);
				norm[i] = n;
				norm[mNumS * (mNumT - 1) + i] = n;
			}
		}

		if (!s_bottom_converges && !volume->getProfile().isOpen())
		{
			// Wrap normals on S
			LLVector4a n;
			for (S32 i = 0; i < mNumT; ++i)
			{
				n.setAdd(norm[mNumS * i], norm[mNumS * i + mNumS - 1]);
				norm[mNumS * i] = n;
				norm[mNumS * i + mNumS - 1] = n;
			}
		}

		if (volume->getPathType() == LL_PCODE_PATH_CIRCLE &&
			(volume->getProfileType() &
			 LL_PCODE_PROFILE_MASK) == LL_PCODE_PROFILE_CIRCLE_HALF)
		{
			if (s_bottom_converges)
			{
				// All lower S have same normal
				for (S32 i = 0; i < mNumT; ++i)
				{
					norm[mNumS * i].set(1, 0, 0);
				}
			}

			if (s_top_converges)
			{
				// All upper S have same normal
				for (S32 i = 0; i < mNumT; ++i)
				{
					norm[mNumS * i + mNumS - 1].set(-1, 0, 0);
				}
			}
		}
	}
	else  // Logic for sculpt volumes
	{
		bool average_poles = false;
		bool wrap_s = false;
		bool wrap_t = false;

		if (sculpt_stitching == LL_SCULPT_TYPE_SPHERE)
		{
			average_poles = true;
		}

		if (sculpt_stitching == LL_SCULPT_TYPE_SPHERE ||
			sculpt_stitching == LL_SCULPT_TYPE_TORUS ||
			sculpt_stitching == LL_SCULPT_TYPE_CYLINDER)
		{
			wrap_s = true;
		}

		if (sculpt_stitching == LL_SCULPT_TYPE_TORUS)
		{
			wrap_t = true;
		}

		if (average_poles)
		{
			// Average normals for north pole
			LLVector4a average;
			average.clear();

			for (S32 i = 0; i < mNumS; ++i)
			{
				average.add(norm[i]);
			}

			// Set average
			for (S32 i = 0; i < mNumS; ++i)
			{
				norm[i] = average;
			}

			// Average normals for south pole

			average.clear();

			for (S32 i = 0; i < mNumS; ++i)
			{
				average.add(norm[i + mNumS * (mNumT - 1)]);
			}

			// Set average
			for (S32 i = 0; i < mNumS; ++i)
			{
				norm[i + mNumS * (mNumT - 1)] = average;
			}
		}

		if (wrap_s)
		{
			LLVector4a n;
			for (S32 i = 0; i < mNumT; ++i)
			{
				n.setAdd(norm[mNumS * i], norm[mNumS * i + mNumS - 1]);
				norm[mNumS * i] = n;
				norm[mNumS * i + mNumS - 1] = n;
			}
		}

		if (wrap_t)
		{
			LLVector4a n;
			for (S32 i = 0; i < mNumS; ++i)
			{
				n.setAdd(norm[i], norm[mNumS * (mNumT - 1) + i]);
				norm[i] = n;
				norm[mNumS * (mNumT - 1) + i] = n;
			}
		}
	}

	return true;
}

// Used to be a validate_face(const LLVolumeFace& face) global function in
// llmodel.cpp. HB
bool LLVolumeFace::validate(bool check_nans) const
{
	// Note: this check does not exist in LL's viewer, but it allows to prevent
	// crashes when attempting to load an invalid model from a file. It however
	// may cause the mesh upload floater to abort during a LOD optimization
	// process, so I added check_nans to make this check non-fatal and only
	// warn about NaNs when it is false. HB
	for (S32 v = 0; v < mNumVertices; ++v)
	{
		if (mPositions && !mPositions[v].isFinite3())
		{
			llwarns << "NaN position data in face found !" << llendl;
			if (check_nans)
			{
				return false;
			}
			break;
		}

		if (mNormals && !mNormals[v].isFinite3())
		{
			llwarns << "NaN normal data in face found !" << llendl;
			if (check_nans)
			{
				return false;
			}
			break;
		}
	}

	for (S32 i = 0; i < mNumIndices; ++i)
	{
		if (mIndices[i] >= mNumVertices)
		{
			llwarns << "Face has invalid index." << llendl;
			return false;
		}
	}

	if (mNumIndices % 3 != 0 || mNumIndices == 0)
	{
		llwarns << "Face has invalid number of indices." << llendl;
		return false;
	}

#if 0
	const LLVector4a scale(0.5f);

	for (U32 i = 0; i < mNumIndices; i+=3)
	{
		U16 idx1 = mIndices[i];
		U16 idx2 = mIndices[i + 1];
		U16 idx3 = mIndices[i+2];

		LLVector4a v1; v1.setMul(mPositions[idx1], scale);
		LLVector4a v2; v2.setMul(mPositions[idx2], scale);
		LLVector4a v3; v3.setMul(mPositions[idx3], scale);

		if (isDegenerate(v1, v2, v3))
		{
			llwarns << "Degenerate face found !" << llendl;
			return false;
		}
	}
#endif

	return true;

}

LL_INLINE static F32 dot3fpu(const LLVector4a& a, const LLVector4a& b)
{
	F32 p0 = a[0] * b[0];
	F32 p1 = a[1] * b[1];
	F32 p2 = a[2] * b[2];
	return p0 + p1 + p2;
}

// Used to be a ll_is_degenerate() global function in llmodel.h. HB
#define LL_DEGENERACY_TOLERANCE  1e-7f
//static
bool LLVolumeFace::isDegenerate(const LLVector4a& a, const LLVector4a& b,
								const LLVector4a& c)
{
	// Small area check

	LLVector4a edge1;
	edge1.setSub(a, b);

	LLVector4a edge2;
	edge2.setSub(a, c);

	//////////////////////////////////////////////////////////////////////////
	/// Linden modified

	// If no one edge is more than 10x longer than any other edge, we weaken
	// the tolerance by a factor of 1e-4f.
	F32 tolerance = LL_DEGENERACY_TOLERANCE;
	LLVector4a edge3; edge3.setSub(c, b);
	F32 len1sq = edge1.dot3(edge1).getF32();
	F32 len2sq = edge2.dot3(edge2).getF32();
	F32 len3sq = edge3.dot3(edge3).getF32();
	bool ab_ok = len1sq <= 100.f * len2sq && len1sq <= 100.f * len3sq;
	bool ac_ok = len2sq <= 100.f * len1sq && len1sq <= 100.f * len3sq;
	bool cb_ok = len3sq <= 100.f * len1sq && len1sq <= 100.f * len2sq;
	if (ab_ok && ac_ok && cb_ok)
	{
		tolerance *= 1e-4f;
	}
	/// End of modifications
	//////////////////////////////////////////////////////////////////////////

	LLVector4a cross;
	cross.setCross3(edge1, edge2);

	LLVector4a edge1b;
	edge1b.setSub(b, a);

	LLVector4a edge2b;
	edge2b.setSub(b, c);

	LLVector4a crossb;
	crossb.setCross3(edge1b, edge2b);

	if (cross.dot3(cross).getF32() < tolerance ||
		crossb.dot3(crossb).getF32() < tolerance)
	{
		return true;
	}

	// Point triangle distance check

	LLVector4a q;
	q.setSub(a, b);

	LLVector4a r;
	r.setSub(c, b);

	F32 qq = dot3fpu(q, q);
	F32 rr = dot3fpu(r, r);
	F32 qr = dot3fpu(r, q);
	F32 qqrr = qq * rr;
	F32 qrqr = qr * qr;

	return qqrr == qrqr;
}

// ----------------------------------------------------------------------------
// LLJointRiggingInfo class.
// ----------------------------------------------------------------------------

LLJointRiggingInfo::LLJointRiggingInfo()
:	mIsRiggedTo(false)
{
	mRiggedExtents[0].clear();
	mRiggedExtents[1].clear();
}

void LLJointRiggingInfo::merge(const LLJointRiggingInfo& other)
{
	if (other.mIsRiggedTo)
	{
		if (mIsRiggedTo)
		{
			// Combine existing boxes
			update_min_max(mRiggedExtents[0], mRiggedExtents[1],
						   other.mRiggedExtents[0]);
			update_min_max(mRiggedExtents[0], mRiggedExtents[1],
						   other.mRiggedExtents[1]);
		}
		else
		{
			// Initialize box
			mIsRiggedTo = true;
			mRiggedExtents[0] = other.mRiggedExtents[0];
			mRiggedExtents[1] = other.mRiggedExtents[1];
		}
	}
}

// ----------------------------------------------------------------------------
// LLJointRiggingInfoTab class.
// ----------------------------------------------------------------------------

LLJointRiggingInfoTab::LLJointRiggingInfoTab()
:	mRigInfoPtr(NULL),
	mSize(0),
	mNeedsUpdate(true)
{
}

LLJointRiggingInfoTab::~LLJointRiggingInfoTab()
{
	clear();
}

void LLJointRiggingInfoTab::clear()
{
	if (mRigInfoPtr)
	{
		delete[] mRigInfoPtr;
		mRigInfoPtr = NULL;
		mSize = 0;
	}
}

void LLJointRiggingInfoTab::resize(U32 size)
{
	if (size == mSize)
	{
		return;
	}
	if (!size)
	{
		clear();
		return;
	}

	LLJointRiggingInfo* new_info_ptr = new LLJointRiggingInfo[size];
	if (mSize)
	{
		U32 min_size = llmin(size, mSize);
		for (U32 i = 0; i < min_size; ++i)
		{
			LLVector4a* old_extents = mRigInfoPtr[i].getRiggedExtents();
			LLVector4a* new_extents = new_info_ptr[i].getRiggedExtents();
			new_extents[0] = old_extents[0];
			new_extents[1] = old_extents[1];
		}
		delete[] mRigInfoPtr;
	}
	mRigInfoPtr = new_info_ptr;
	mSize = size;
}

void LLJointRiggingInfoTab::merge(const LLJointRiggingInfoTab& src)
{
	if (src.size() > size())
	{
		resize(src.size());
	}

	U32 min_size = llmin(size(), src.size());
	for (U32 i = 0; i < min_size; ++i)
	{
		mRigInfoPtr[i].merge(src[i]);
	}
}
