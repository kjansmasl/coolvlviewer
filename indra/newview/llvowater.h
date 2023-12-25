/** 
 * @file llvowater.h
 * @brief Description of LLVOWater class
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

#ifndef LL_VOWATER_H
#define LL_VOWATER_H

#include "llvector2.h"

#include "llpipeline.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewertexture.h"

constexpr U32 N_RES	= 16; // Number of subdivisions of wave tile
constexpr U8 WAVE_STEP = 8;

class LLSurface;
class LLHeavenBody;
class LLVOSky;
class LLFace;

class LLVOWater : public LLStaticViewerObject
{
public:
	enum 
	{
		VERTEX_DATA_MASK =	(1 << LLVertexBuffer::TYPE_VERTEX) |
							(1 << LLVertexBuffer::TYPE_NORMAL) |
							(1 << LLVertexBuffer::TYPE_TEXCOORD0) 
	};

	LLVOWater(const LLUUID& id, LLViewerRegion* regionp,
			  // This is LL_VO_VOID_WATER for LLVOVoidWater.
			  LLPCode pcode = LL_VO_WATER);

	// Initialize data that's only inited once per class.
	static void initClass()								{}
	static void cleanupClass()							{}

	// Nothing to do.
	LL_INLINE void idleUpdate(F64) override				{}

	LLDrawable* createDrawable() override;
	bool updateGeometry(LLDrawable* drawable) override;
	void updateSpatialExtents(LLVector4a& new_min,
							  LLVector4a& new_max) override;

	LL_INLINE void updateTextures() override			{}

	// Generates accurate apparent angle and area
	void setPixelAreaAndAngle() override;

	LL_INLINE U32 getPartitionType() const override
	{
		return LLViewerRegion::PARTITION_WATER;
	}

	 // Whether this object needs to do an idleUpdate.
	LL_INLINE bool isActive() const override			{ return false; }

	LL_INLINE void setUseTexture(bool b)				{ mUseTexture = b; }
	LL_INLINE void setIsEdgePatch(bool b)				{ mIsEdgePatch = b; }
	LL_INLINE bool getUseTexture() const				{ return mUseTexture; }
	LL_INLINE bool getIsEdgePatch() const				{ return mIsEdgePatch; }

protected:
	bool mUseTexture;
	bool mIsEdgePatch;
	S32 mRenderType;
};

class LLVOVoidWater final : public LLVOWater
{
public:
	LL_INLINE LLVOVoidWater(const LLUUID& id, LLViewerRegion* regionp)
	:	LLVOWater(id, regionp, LL_VO_VOID_WATER)
	{
		mRenderType = LLPipeline::RENDER_TYPE_VOIDWATER;
	}

	LL_INLINE U32 getPartitionType() const override
	{
		return LLViewerRegion::PARTITION_VOIDWATER;
	}
};

#endif // LL_VOSURFACEPATCH_H
