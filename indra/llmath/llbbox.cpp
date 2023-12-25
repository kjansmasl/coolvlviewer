/**
 * @file llbbox.cpp
 * @brief General purpose bounding box class (Not axis aligned)
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

#include "linden_common.h"

#include "llbbox.h"

#include "llmatrix4.h"

void LLBBox::addPointLocal(const LLVector3& p)
{
	if (mEmpty)
	{
		mMinLocal = p;
		mMaxLocal = p;
		mEmpty = false;
	}
	else
	{
		mMinLocal.mV[VX] = llmin(p.mV[VX], mMinLocal.mV[VX]);
		mMinLocal.mV[VY] = llmin(p.mV[VY], mMinLocal.mV[VY]);
		mMinLocal.mV[VZ] = llmin(p.mV[VZ], mMinLocal.mV[VZ]);
		mMaxLocal.mV[VX] = llmax(p.mV[VX], mMaxLocal.mV[VX]);
		mMaxLocal.mV[VY] = llmax(p.mV[VY], mMaxLocal.mV[VY]);
		mMaxLocal.mV[VZ] = llmax(p.mV[VZ], mMaxLocal.mV[VZ]);
	}
}

void LLBBox::addPointAgent(LLVector3 p)
{
	p -= mPosAgent;
	p.rotVec(~mRotation);
	addPointLocal(p);
}

void LLBBox::addBBoxAgent(const LLBBox& b)
{
	if (mEmpty)
	{
		mPosAgent = b.mPosAgent;
		mRotation = b.mRotation;
		mMinLocal.clear();
		mMaxLocal.clear();
	}
	LLVector3 vertex[8];
	vertex[0].set(b.mMinLocal.mV[VX], b.mMinLocal.mV[VY], b.mMinLocal.mV[VZ]);
	vertex[1].set(b.mMinLocal.mV[VX], b.mMinLocal.mV[VY], b.mMaxLocal.mV[VZ]);
	vertex[2].set(b.mMinLocal.mV[VX], b.mMaxLocal.mV[VY], b.mMinLocal.mV[VZ]);
	vertex[3].set(b.mMinLocal.mV[VX], b.mMaxLocal.mV[VY], b.mMaxLocal.mV[VZ]);
	vertex[4].set(b.mMaxLocal.mV[VX], b.mMinLocal.mV[VY], b.mMinLocal.mV[VZ]);
	vertex[5].set(b.mMaxLocal.mV[VX], b.mMinLocal.mV[VY], b.mMaxLocal.mV[VZ]);
	vertex[6].set(b.mMaxLocal.mV[VX], b.mMaxLocal.mV[VY], b.mMinLocal.mV[VZ]);
	vertex[7].set(b.mMaxLocal.mV[VX], b.mMaxLocal.mV[VY], b.mMaxLocal.mV[VZ]);

	LLMatrix4 m(b.mRotation);
	m.translate(b.mPosAgent);
	m.translate(-mPosAgent);
	m.rotate(~mRotation);

	for(S32 i = 0; i < 8; ++i)
	{
		addPointLocal(vertex[i] * m);
	}
}

LLBBox LLBBox::getAxisAligned() const
{
	// No rotation = axis aligned rotation
	LLBBox aligned(mPosAgent, LLQuaternion(), LLVector3(), LLVector3());

	// Add the center point so that it's not empty
	aligned.addPointAgent(mPosAgent);

	// Add our BBox
	aligned.addBBoxAgent(*this);

	return aligned;
}

LLVector3 LLBBox::localToAgent(const LLVector3& v) const
{
	LLMatrix4 m(mRotation);
	m.translate(mPosAgent);
	return v * m;
}

LLVector3 LLBBox::agentToLocal(const LLVector3& v) const
{
	LLMatrix4 m;
	m.translate(-mPosAgent);
	m.rotate(~mRotation);  // inverse rotation
	return v * m;
}

LLVector3 LLBBox::localToAgentBasis(const LLVector3& v) const
{
	return v * LLMatrix4(mRotation);
}

LLVector3 LLBBox::agentToLocalBasis(const LLVector3& v) const
{
	return v * LLMatrix4(~mRotation);  // Inverse rotation
}
