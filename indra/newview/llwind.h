/**
 * @file llwind.h
 * @brief LLWind class header file
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

#ifndef LL_LLWIND_H
#define LL_LLWIND_H

#include "llbitpack.h"
#include "llmath.h"
#include "llvector3.h"
#include "llvector3d.h"

// Hack to make wind speeds more realistic
constexpr F32 WIND_SCALE_HACK = 2.f;

class LLVector3;
class LLBitPack;
class LLGroupHeader;

class LLWind
{
public:
	LLWind();
	~LLWind();

	void renderVectors();

	// For all three methods below, "location" is region-local
	LLVector3 getVelocity(const LLVector3& location);
	LLVector3 getCloudVelocity(const LLVector3& location);
	LLVector3 getVelocityNoisy(const LLVector3& location, F32 dim);

	void decompress(LLBitPack& bitpack, LLGroupHeader* group_headerp);
	LLVector3 getAverage();

	LL_INLINE void setCloudDensityPointer(F32* d)		{ mCloudDensityp = d; }

	LL_INLINE void setOriginGlobal(const LLVector3d& p)	{ mOriginGlobal = p; }
	// Variable region size support
	LL_INLINE void setRegionWidth(F32 width)			{ mRegionWidth = width; }

private:
	void init();

private:
	S32			mSize;
	F32			mRegionWidth;	// Variable region size support
	F32*		mVelX;
	F32*		mVelY;
	F32*		mCloudVelX;
	F32*		mCloudVelY;
	F32*		mCloudDensityp;
	LLVector3d	mOriginGlobal;
};

#endif
