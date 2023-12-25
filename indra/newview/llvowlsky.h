/**
 * @file llvowlsky.h
 * @brief LLVOWLSky class definition
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_VOWLSKY_H
#define LL_VOWLSKY_H

#include "llsettingssky.h"		// For LL_VARIABLE_SKY_DOME_SIZE

#include "llviewerobject.h"

class LLVOWLSky final : public LLStaticViewerObject
{
protected:
	LOG_CLASS(LLVOWLSky);

public:
	LLVOWLSky(const LLUUID& id, LLViewerRegion* regionp);

	// Nothing to do.
	LL_INLINE void idleUpdate(F64) override				{}

	LL_INLINE bool isActive() const override			{ return false; }

	LLDrawable* createDrawable() override;
	bool updateGeometry(LLDrawable* drawable) override;

	void drawStars();
	void drawDome();

	void resetVertexBuffers() override;

	void cleanupGL();
	void restoreGL();

	static void initClass();
	static void updateSettings();
	static void cleanupClass();

private:
	LL_INLINE static U32 getNumStacks()
	{
		return sWLSkyDetail;
	}

	LL_INLINE static U32 getNumSlices()
	{
		return 2 * sWLSkyDetail;
	}

	LL_INLINE static U32 getFanNumVerts()
	{
		return getNumSlices() + 1;
	}

	LL_INLINE static U32 getFanNumIndices()
	{
		return getNumSlices() * 3;
	}

	// Gets the dome radius, based on whether we render Windlight or extended
	// environment settings.
#if LL_VARIABLE_SKY_DOME_SIZE
	static F32 getDomeRadius();
#else
	// In fact, Windlight always had it fixed to 15000m, and it is also the
	// value for the current extended environment code... So, why bothering ?
	LL_INLINE static F32 getDomeRadius()				{ return 15000.f; }
#endif

	// A tiny helper method for controlling the sky dome tesselation.
	static F32 calcPhi(U32 i);

	// Helper method for initializing the stars.
	void initStars();

	// Helper method for building the strips vertex buffer. Note: begin_stack
	// and end_stack follow stl iterator conventions, begin_stack is the first
	// stack to be included, end_stack is the first stack not to be included.
	static void buildStripsBuffer(U32 begin_stack, U32 end_stack,
								  LLStrider<LLVector3>& vertices,
								  LLStrider<LLVector2>& texCoords,
								  LLStrider<U16>& indices);

	// Helper method for updating the stars colors.
	void updateStarColors();

	// Helper method for updating the stars geometry.
	bool updateStarGeometry(LLDrawable* drawable);

private:
	LLPointer<LLVertexBuffer>	mStarsVerts;

	typedef std::vector<LLPointer<LLVertexBuffer> > strips_verts_vec_t;
	strips_verts_vec_t			mStripsVerts;

	std::vector<LLVector3>		mStarVertices;
	std::vector<LLColor4>		mStarColors;
	std::vector<F32>			mStarIntensities;

	U32							mLastWLSkyDetail;

	static U32					sWLSkyDetail;
};

#endif // LL_VOWLSKY_H
