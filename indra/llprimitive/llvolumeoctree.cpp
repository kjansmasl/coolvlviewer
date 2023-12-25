/** 
 * @file llvolumeoctree.cpp
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2010, Linden Research, Inc.
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

#include "llvolumeoctree.h"

bool LLLineSegmentBoxIntersect(const LLVector4a& start,
							   const LLVector4a& end,
							   const LLVector4a& center,
							   const LLVector4a& size)
{
	LLVector4a fAWdU;
	LLVector4a dir;
	LLVector4a diff;

	dir.setSub(end, start);
	dir.mul(0.5f);

	diff.setAdd(end,start);
	diff.mul(0.5f);
	diff.sub(center);
	fAWdU.setAbs(dir); 

	LLVector4a rhs;
	rhs.setAdd(size, fAWdU);

	LLVector4a lhs;
	lhs.setAbs(diff);

	U32 grt = lhs.greaterThan(rhs).getGatheredBits();

	if (grt & 0x7)
	{
		return false;
	}
	
	LLVector4a f;
	f.setCross3(dir, diff);
	f.setAbs(f);

	LLVector4a v0, v1;

	v0 = _mm_shuffle_ps(size, size,_MM_SHUFFLE(3, 0, 0, 1));
	v1 = _mm_shuffle_ps(fAWdU, fAWdU, _MM_SHUFFLE(3, 1, 2, 2));
	lhs.setMul(v0, v1);

	v0 = _mm_shuffle_ps(size, size, _MM_SHUFFLE(3, 1, 2, 2));
	v1 = _mm_shuffle_ps(fAWdU, fAWdU, _MM_SHUFFLE(3, 0, 0, 1));
	rhs.setMul(v0, v1);
	rhs.add(lhs);
	
	grt = f.greaterThan(rhs).getGatheredBits();

	return (grt & 0x7) == 0;
}

template<typename T_PTR>
_LLVolumeOctreeListener<T_PTR>::_LLVolumeOctreeListener(_LLOctreeNode<LLVolumeTriangle, T_PTR>* node)
{
	node->addListener(this);
}

template<typename T_PTR>
_LLVolumeOctreeListener<T_PTR>::~_LLVolumeOctreeListener()
{
}

template<typename T_PTR>
void _LLVolumeOctreeListener<T_PTR>::handleChildAddition(const _LLOctreeNode<LLVolumeTriangle, T_PTR>* parent,
														 _LLOctreeNode<LLVolumeTriangle, T_PTR>* child)
{
	new _LLVolumeOctreeListener<T_PTR>(child);
}

template class _LLVolumeOctreeListener<LLVolumeTriangle*>;
template class _LLVolumeOctreeListener<LLPointer<LLVolumeTriangle> >;

template<typename T_PTR>
_LLOctreeTriangleRayIntersect<T_PTR>::_LLOctreeTriangleRayIntersect(const LLVector4a& start,
																	const LLVector4a& dir,
																	const LLVolumeFace* face,
																	F32* closest_t,
																	LLVector4a* intersection,
																	LLVector2* tex_coord,
																	LLVector4a* normal,
																	LLVector4a* tangent)
:	mFace(face),
	mStart(start),
	mDir(dir),
	mIntersection(intersection),
	mTexCoord(tex_coord),
	mNormal(normal),
	mTangent(tangent),
	mClosestT(closest_t),
	mHitFace(false)
{
	mEnd.setAdd(mStart, mDir);
}

template<typename T_PTR>
void _LLOctreeTriangleRayIntersect<T_PTR>::traverse(const _LLOctreeNode<LLVolumeTriangle, T_PTR>* node)
{
	_LLVolumeOctreeListener<T_PTR>* vl =
		(_LLVolumeOctreeListener<T_PTR>*)node->getListener(0);

	if (LLLineSegmentBoxIntersect(mStart, mEnd, vl->mBounds[0],
								  vl->mBounds[1]))
	{
		node->accept(this);
		for (U32 i = 0; i < node->getChildCount(); ++i)
		{
			traverse(node->getChild(i));
		}
	}
}

template<typename T_PTR>
void _LLOctreeTriangleRayIntersect<T_PTR>::visit(const _LLOctreeNode<LLVolumeTriangle, T_PTR>* node)
{
	for (typename _LLOctreeNode<LLVolumeTriangle, T_PTR>::const_element_iter
			iter = node->getDataBegin();
		 iter != node->getDataEnd(); ++iter)
	{
		const LLVolumeTriangle* tri = *iter;

		F32 a, b, t;
		
		if (LLTriangleRayIntersect(*tri->mV[0], *tri->mV[1], *tri->mV[2],
								   mStart, mDir, a, b, t))
		{
			if (t >= 0.f &&		// if hit is after start
				t <= 1.f &&		// and before end
				t < *mClosestT)	// and this hit is closer
			{
				*mClosestT = t;
				mHitFace = true;

				if (mIntersection)
				{
					LLVector4a intersect = mDir;
					intersect.mul(*mClosestT);
					intersect.add(mStart);
					*mIntersection = intersect;
				}

				U32 idx0 = tri->mIndex[0];
				U32 idx1 = tri->mIndex[1];
				U32 idx2 = tri->mIndex[2];

				if (mTexCoord)
				{
					LLVector2* tc = (LLVector2*)mFace->mTexCoords;
					*mTexCoord = (1.f - a - b) * tc[idx0] + a * tc[idx1] +
								 b * tc[idx2];

				}

				if (mNormal)
				{
					LLVector4a* norm = mFace->mNormals;

					LLVector4a n1 = norm[idx0];
					n1.mul(1.f - a - b);
					LLVector4a n2 = norm[idx1];
					n2.mul(a);
					LLVector4a n3 = norm[idx2];
					n3.mul(b);
					n1.add(n2);
					n1.add(n3);

					*mNormal = n1;
				}

				if (mTangent)
				{
					LLVector4a* tangents = mFace->mTangents;

					LLVector4a t1 = tangents[idx0];
					t1.mul(1.f - a - b);
					LLVector4a t2 = tangents[idx1];
					t2.mul(a);
					LLVector4a t3 = tangents[idx2];
					t3.mul(b);
					t1.add(t2);
					t1.add(t3);

					*mTangent = t1;
				}
			}
		}
	}
}

template class _LLOctreeTriangleRayIntersect<LLVolumeTriangle*>;
template class _LLOctreeTriangleRayIntersect<LLPointer<LLVolumeTriangle> >;

const LLVector4a& LLVolumeTriangle::getPositionGroup() const
{
	return mPositionGroup;
}

const F32& LLVolumeTriangle::getBinRadius() const
{
	return mRadius;
}

// TEST CODE

template<typename T_PTR>
void _LLVolumeOctreeValidate<T_PTR>::visit(const _LLOctreeNode<LLVolumeTriangle, T_PTR>* branch)
{
	_LLVolumeOctreeListener<T_PTR>* node =
		(_LLVolumeOctreeListener<T_PTR>*)branch->getListener(0);

	// Make sure bounds matches extents
	LLVector4a& min = node->mExtents[0];
	LLVector4a& max = node->mExtents[1];

	LLVector4a& center = node->mBounds[0];
	LLVector4a& size = node->mBounds[1];

	LLVector4a test_min, test_max;
	test_min.setSub(center, size);
	test_max.setAdd(center, size);

	if (!test_min.equals3(min, 0.001f) || !test_max.equals3(max, 0.001f))
	{
		llerrs << "Bad bounding box data found." << llendl;
	}

	test_min.sub(LLVector4a(0.001f));
	test_max.add(LLVector4a(0.001f));

	for (U32 i = 0; i < branch->getChildCount(); ++i)
	{
		_LLVolumeOctreeListener<T_PTR>* child =
			(_LLVolumeOctreeListener<T_PTR>*)branch->getChild(i)->getListener(0);

		// Make sure all children fit inside this node
		if (child->mExtents[0].lessThan(test_min).areAnySet(LLVector4Logical::MASK_XYZ) ||
			child->mExtents[1].greaterThan(test_max).areAnySet(LLVector4Logical::MASK_XYZ))
		{
			llerrs << "Child protrudes from bounding box." << llendl;
		}
	}

	// Children fit, check data
	for (typename _LLOctreeNode<LLVolumeTriangle, T_PTR>::const_element_iter
			iter = branch->getDataBegin(); 
		 iter != branch->getDataEnd(); ++iter)
	{
		const LLVolumeTriangle* tri = *iter;

		// validate triangle
		for (U32 i = 0; i < 3; i++)
		{
			if (tri->mV[i]->greaterThan(test_max).areAnySet(LLVector4Logical::MASK_XYZ) ||
				tri->mV[i]->lessThan(test_min).areAnySet(LLVector4Logical::MASK_XYZ))
			{
				llerrs << "Triangle protrudes from node." << llendl;
			}
		}
	}
}

template class _LLVolumeOctreeValidate<LLPointer<LLVolumeTriangle> >;
template class _LLVolumeOctreeValidate<LLVolumeTriangle*>;
