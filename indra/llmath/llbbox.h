/**
 * @file llbbox.h
 * @brief General purpose bounding box class
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

#ifndef LL_BBOX_H
#define LL_BBOX_H

#include "llvector3.h"
#include "llquaternion.h"

// Note: "local space" for an LLBBox is defined relative to agent space in
// terms of a translation followed by a rotation.  There is no scale term since
// the LLBBox's min and max are not necessarily symetrical and define their own
// extents.

class LLBBox
{
#if 0
	friend LLBBox operator*(const LLBBox& a, const LLMatrix4& b);
#endif

public:
	LLBBox()												{ mEmpty = true; }

	LLBBox(const LLVector3& pos_agent, const LLQuaternion& rot,
		   const LLVector3& min_local, const LLVector3& max_local)
	:	mMinLocal(min_local),
		mMaxLocal(max_local),
		mPosAgent(pos_agent),
		mRotation(rot),
		mEmpty(true)
	{
	}

	// Default copy constructor is OK.

	LL_INLINE const LLVector3& getPositionAgent() const		{ return mPosAgent; }
	LL_INLINE const LLQuaternion& getRotation() const		{ return mRotation; }

	LL_INLINE LLVector3 getMinAgent() const					{ return localToAgent(mMinLocal); }
	LL_INLINE const LLVector3& getMinLocal() const			{ return mMinLocal; }
	LL_INLINE void setMinLocal(const LLVector3& min)		{ mMinLocal = min; }

	LL_INLINE LLVector3 getMaxAgent() const					{ return localToAgent(mMaxLocal); }
	LL_INLINE const LLVector3& getMaxLocal() const			{ return mMaxLocal; }
	LL_INLINE void setMaxLocal(const LLVector3& max)		{ mMaxLocal = max; }

	LL_INLINE LLVector3 getCenterLocal() const				{ return (mMaxLocal - mMinLocal) * 0.5f + mMinLocal; }
	LL_INLINE LLVector3 getCenterAgent() const				{ return localToAgent(getCenterLocal()); }

	LL_INLINE LLVector3 getExtentLocal() const				{ return mMaxLocal - mMinLocal; }

	LL_INLINE bool containsPointLocal(const LLVector3& p) const
	{
		return !(p.mV[VX] < mMinLocal.mV[VX] || p.mV[VX] > mMaxLocal.mV[VX] ||
				 p.mV[VY] < mMinLocal.mV[VY] || p.mV[VY] > mMaxLocal.mV[VY] ||
				 p.mV[VZ] < mMinLocal.mV[VZ] || p.mV[VZ] > mMaxLocal.mV[VZ]);
	}

	LL_INLINE bool containsPointAgent(const LLVector3& p) const
	{
		return containsPointLocal(agentToLocal(p));
	}

	void addPointAgent(LLVector3 p);
	void addBBoxAgent(const LLBBox& b);

	void addPointLocal(const LLVector3& p);

	LL_INLINE void addBBoxLocal(const LLBBox& b)
	{
		addPointLocal(b.mMinLocal);
		addPointLocal(b.mMaxLocal);
	}

	LL_INLINE void expand(F32 delta)
	{
		mMinLocal.mV[VX] -= delta;
		mMinLocal.mV[VY] -= delta;
		mMinLocal.mV[VZ] -= delta;
		mMaxLocal.mV[VX] += delta;
		mMaxLocal.mV[VY] += delta;
		mMaxLocal.mV[VZ] += delta;
	}

	LLVector3 localToAgent(const LLVector3& v) const;
	LLVector3 agentToLocal(const LLVector3& v) const;

	// Changes rotation but not position
	LLVector3 localToAgentBasis(const LLVector3& v) const;
	LLVector3 agentToLocalBasis(const LLVector3& v) const;

	// Get the smallest possible axis aligned bbox that contains this bbox
	LLBBox getAxisAligned() const;

private:
	LLQuaternion	mRotation;
	LLVector3		mMinLocal;
	LLVector3		mMaxLocal;
	LLVector3		mPosAgent;  // Position relative to Agent's Region
	bool			mEmpty;		// Nothing has been added to this bbox yet
};

#if 0	// Not used
LL_INLINE LLBBox operator*(const LLBBox& a, const LLMatrix4& b);
{
	return LLBBox(a.mMin * b, a.mMax * b);
}
#endif

#endif  // LL_BBOX_H
