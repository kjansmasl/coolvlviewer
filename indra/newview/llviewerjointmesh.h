/**
 * @file llviewerjointmesh.h
 * @brief Declaration of LLViewerJointMesh class
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

#ifndef LL_LLVIEWERJOINTMESH_H
#define LL_LLVIEWERJOINTMESH_H

#include "llavatarjointmesh.h"
#include "llpolymesh.h"
#include "llcolor4.h"

#include "llviewerjoint.h"
#include "llviewertexture.h"

#if LL_MSVC
# pragma warning(disable : 4250)	// LLViewerJoint::asViewerJoint inheritance
#endif

class LLCharacter;
class LLDrawable;
class LLFace;
class LLViewerTexLayerSet;

//-----------------------------------------------------------------------------
// class LLViewerJointMesh
//-----------------------------------------------------------------------------

class LLViewerJointMesh final : public LLAvatarJointMesh, public LLViewerJoint
{
protected:
	LOG_CLASS(LLViewerJointMesh);

public:
	LLViewerJointMesh();

	// This lifts any ambiguity since LLAvatarJointMesh is not an LLViewerJoint
	LL_INLINE LLViewerJoint* asViewerJoint() override	{ return this; }
	// This lifts any ambiguity since LLViewerJoint is not an LLAvatarJointMesh
	LL_INLINE LLAvatarJoint* asAvatarJoint() override	{ return this; }

	// Render time method to upload batches of joint matrices
	void uploadJointMatrices();

	// Overloaded from base class
	U32 drawShape(F32 pixelArea, bool first_pass = true,
				  bool is_dummy = false) override;

	// Necessary because MS's compiler warns on function inheritance via
	// dominance in the diamond inheritance here. Warns even though
	// LLViewerJoint holds the only non virtual implementation.
	U32 render(F32 pixelArea, bool first_pass = true,
			   bool is_dummy = false) override
	{
		return LLViewerJoint::render(pixelArea, first_pass, is_dummy);
	}

	void updateFaceSizes(U32& num_vertices, U32& num_indices,
						 F32 pixel_area) override;
	void updateFaceData(LLFace* face, F32 pixel_area, bool damp_wind = false,
						bool terse_update = false) override;
	bool updateLOD(F32 pixel_area, bool activate) override;
	void updateJointGeometry() override;
	void dump() override;

	LL_INLINE bool isAnimatable() const override	{ return false; }

private:
	// Copy mesh into given face's vertex buffer, applying current animation
	// pose
	static void updateGeometry(LLFace* face, LLPolyMesh* mesh);
};

#endif // LL_LLVIEWERJOINTMESH_H
