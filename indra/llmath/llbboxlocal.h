/**
 * @file llbboxlocal.h
 * @brief General purpose bounding box class.
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

#ifndef LL_BBOXLOCAL_H
#define LL_BBOXLOCAL_H

#include "llmatrix4.h"
#include "llvector3.h"

class LLMatrix4;

class LLBBoxLocal
{
	friend LLBBoxLocal operator*(const LLBBoxLocal& a, const LLMatrix4& b);

public:
	LLBBoxLocal() = default;

	LL_INLINE LLBBoxLocal(const LLVector3& min, const LLVector3& max)
	:	mMin(min),
		mMax(max)
	{
	}

	// Default copy constructor is OK.

	LL_INLINE const LLVector3& getMin() const			{ return mMin; }
	LL_INLINE void setMin(const LLVector3& min)			{ mMin = min; }

	LL_INLINE const LLVector3& getMax() const			{ return mMax; }
	LL_INLINE void setMax(const LLVector3& max)			{ mMax = max; }

	LL_INLINE LLVector3 getCenter() const				{ return (mMax - mMin) * 0.5f + mMin; }
	LL_INLINE LLVector3 getExtent() const				{ return mMax - mMin; }

	LL_INLINE void addPoint(const LLVector3& p)
	{
		mMin.mV[VX] = llmin(p.mV[VX], mMin.mV[VX]);
		mMin.mV[VY] = llmin(p.mV[VY], mMin.mV[VY]);
		mMin.mV[VZ] = llmin(p.mV[VZ], mMin.mV[VZ]);
		mMax.mV[VX] = llmax(p.mV[VX], mMax.mV[VX]);
		mMax.mV[VY] = llmax(p.mV[VY], mMax.mV[VY]);
		mMax.mV[VZ] = llmax(p.mV[VZ], mMax.mV[VZ]);
	}

	LL_INLINE void addBBox(const LLBBoxLocal& b)
	{
		addPoint(b.mMin);
		addPoint(b.mMax);
	}

	LL_INLINE void expand(F32 delta)
	{
		mMin.mV[VX] -= delta;
		mMin.mV[VY] -= delta;
		mMin.mV[VZ] -= delta;
		mMax.mV[VX] += delta;
		mMax.mV[VY] += delta;
		mMax.mV[VZ] += delta;
	}

private:
	LLVector3 mMin;
	LLVector3 mMax;
};

LL_INLINE LLBBoxLocal operator*(const LLBBoxLocal& a, const LLMatrix4& b)
{
	return LLBBoxLocal(a.mMin * b, a.mMax * b);
}

#endif  // LL_BBOXLOCAL_H
