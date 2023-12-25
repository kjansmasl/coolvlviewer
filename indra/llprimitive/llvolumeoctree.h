/**
 * @file llvolumeoctree.h
 * @brief LLVolume octree classes.
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

#ifndef LL_LLVOLUME_OCTREE_H
#define LL_LLVOLUME_OCTREE_H

#include "lloctree.h"
#include "llvolume.h"

class alignas(16) LLVolumeTriangle : public LLRefCount
{
public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LLVolumeTriangle()
	{
		mBinIndex = -1;
	}

	LLVolumeTriangle(const LLVolumeTriangle& rhs)
	{
		*this = rhs;
	}

	const LLVolumeTriangle& operator=(const LLVolumeTriangle& rhs)
	{
		llerrs << "Illegal operation !" << llendl;
		return *this;
	}

	virtual const LLVector4a& getPositionGroup() const;
	virtual const F32& getBinRadius() const;

	S32 getBinIndex() const				{ return mBinIndex; }
	void setBinIndex(S32 idx) const		{ mBinIndex = idx; }

public:
	// Note: before these variables, we find the 32 bits counter from
	// LLRefCount... Since mPositionGroup will be 16-bytes aligned, fill-up
	// the gap and align in the cache line with other member variables. HB
	F32					mRadius;
	const LLVector4a*	mV[3];

	LLVector4a			mPositionGroup;

	mutable S32			mBinIndex;
	U16					mIndex[3];
};

template <typename T_PTR>
class alignas(16) _LLVolumeOctreeListener
:	public _LLOctreeListener<LLVolumeTriangle, T_PTR>
{
public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	_LLVolumeOctreeListener(_LLOctreeNode<LLVolumeTriangle, T_PTR>* node);

	_LLVolumeOctreeListener(const _LLVolumeOctreeListener& rhs)
	{
		*this = rhs;
	}

	~_LLVolumeOctreeListener() override;

	const _LLVolumeOctreeListener& operator=(const _LLVolumeOctreeListener&)
	{
		llerrs << "Illegal operation !" << llendl;
		return *this;
	}

	 // LISTENER FUNCTIONS
	void handleChildAddition(const _LLOctreeNode<LLVolumeTriangle, T_PTR>* parent,
							 _LLOctreeNode<LLVolumeTriangle, T_PTR>* child) override;

	void handleStateChange(const LLTreeNode<LLVolumeTriangle>* node) override
	{
	}

	void handleChildRemoval(const _LLOctreeNode<LLVolumeTriangle, T_PTR>* parent,
							const _LLOctreeNode<LLVolumeTriangle, T_PTR>* child) override
	{
	}

	void handleInsertion(const LLTreeNode<LLVolumeTriangle>* node,
						 LLVolumeTriangle* tri) override
	{
	}

	void handleRemoval(const LLTreeNode<LLVolumeTriangle>* node,
					   LLVolumeTriangle* tri) override
	{
	}

	void handleDestruction(const LLTreeNode<LLVolumeTriangle>* node) override
	{
	}

public:
	// Bounding box (center, size) of this node and all its children (tight fit
	// to objects)
	alignas(16) LLVector4a mBounds[2];
	// Extents (min, max) of this node and all its children
	alignas(16) LLVector4a mExtents[2];
};

using LLVolumeOctreeListener = _LLVolumeOctreeListener<LLPointer<LLVolumeTriangle> >;
using LLVolumeOctreeListenerNoOwnership = _LLVolumeOctreeListener<LLVolumeTriangle*>;

template<typename T_PTR>
class alignas(16) _LLOctreeTriangleRayIntersect
:	public _LLOctreeTraveler<LLVolumeTriangle, T_PTR>
{
public:
	_LLOctreeTriangleRayIntersect(const LLVector4a& start,
								  const LLVector4a& dir,
								  const LLVolumeFace* face,
								  F32* closest_t,
								  LLVector4a* intersection,
								  LLVector2* tex_coord,
								  LLVector4a* normal,
								  LLVector4a* tangent);

	void traverse(const _LLOctreeNode<LLVolumeTriangle, T_PTR>* node) override;
	void visit(const _LLOctreeNode<LLVolumeTriangle, T_PTR>* node) override;

public:
	LLVector4a			mStart;
	LLVector4a			mDir;
	LLVector4a			mEnd;
	LLVector4a*			mIntersection;
	LLVector2*			mTexCoord;
	LLVector4a*			mNormal;
	LLVector4a*			mTangent;
	F32*				mClosestT;
	const LLVolumeFace*	mFace;
	bool				mHitFace;
};

using LLOctreeTriangleRayIntersect = _LLOctreeTriangleRayIntersect<LLPointer<LLVolumeTriangle> >;
using LLOctreeTriangleRayIntersectNoOwnership = _LLOctreeTriangleRayIntersect<LLVolumeTriangle*>;

template <typename T_PTR>
class _LLVolumeOctreeValidate
:	public _LLOctreeTraveler<LLVolumeTriangle, T_PTR>
{
	void visit(const _LLOctreeNode<LLVolumeTriangle, T_PTR>* branch) override;
};

using LLVolumeOctreeValidate = _LLVolumeOctreeValidate<LLPointer<LLVolumeTriangle> >;
using LLVolumeOctreeValidateNoOwnership = _LLVolumeOctreeValidate<LLVolumeTriangle*>;

#endif
