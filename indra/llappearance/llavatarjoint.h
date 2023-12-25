/**
 * @file llavatarjoint.h
 * @brief Implementation of LLAvatarJoint class
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

#ifndef LL_LLAVATARJOINT_H
#define LL_LLAVATARJOINT_H

#include <vector>

#include "lljoint.h"
#include "llpreprocessor.h"

class LLAvatarJointMesh;
class LLFace;

constexpr F32 DEFAULT_AVATAR_JOINT_LOD = 0.f;

// Sets the OpenGL selection stack name that is pushed and popped with this
// joint state. The default value indicates that no name should be pushed or
// popped.
enum LLJointPickName
{
	PN_DEFAULT = -1,
	PN_0 = 0,
	PN_1 = 1,
	PN_2 = 2,
	PN_3 = 3,
	PN_4 = 4,
	PN_5 = 5
};

typedef std::vector<LLAvatarJointMesh*> avatar_joint_mesh_list_t;

class LLAvatarJoint : public LLJoint
{
public:
	LLAvatarJoint();
	// *TODO: Only used for LLVOAvatarSelf::mScreenp.
	//        *DOES NOT INITIALIZE mResetAfterRestoreOldXform*
	LLAvatarJoint(const std::string& name, LLJoint* parent = NULL);

	LL_INLINE LLAvatarJoint* asAvatarJoint() override	{ return this; }

	// Gets the validity of this joint
	LL_INLINE bool getValid()							{ return mValid; }

	// Sets the validity of this joint
	virtual void setValid(bool valid, bool recursive = false);

	// Returns true if this object is transparent.
	// This is used to determine in which order to draw objects.
	LL_INLINE virtual bool isTransparent()				{ return mIsTransparent; }

	// Returns true if this object should inherit scale modifiers from its
	// immediate parent
	LL_INLINE virtual bool inheritScale()				{ return false; }

	enum Components
	{
		SC_BONE		= 1,
		SC_JOINT	= 2,
		SC_AXES		= 4
	};

	// Selects which skeleton components to draw
	void setSkeletonComponents(U32 comp, bool recursive = true);

	// Returns which skeleton components are enables for drawing
	LL_INLINE U32 getSkeletonComponents()				{ return mComponents; }

	// Sets the level of detail for this node as a minimum pixel area
	// threshold. If the current pixel area for this object is less than the
	// specified threshold, the node is not traversed. In addition, if a value
	// is specified (not default of 0.0), and the pixel area is larger than the
	// specified minimum, the node is rendered, but no other siblings of this
	// node under the same parent will be.
	LL_INLINE F32 getLOD()								{ return mMinPixelArea; }
	LL_INLINE void setLOD(F32 pixelArea)				{ mMinPixelArea = pixelArea; }

	LL_INLINE void setPickName(LLJointPickName name)	{ mPickName = name; }
	LL_INLINE LLJointPickName getPickName()				{ return mPickName; }

	void setVisible(bool visible, bool recursive);

	// Takes meshes in mMeshParts and sets each one as a child joint
	void setMeshesToChildren();

	// LLViewerJoint interface
	virtual U32 render(F32 pixelArea, bool first_pass = true,
					   bool is_dummy = false) = 0;
	virtual void updateFaceSizes(U32& num_vertices, U32& num_indices,
								 F32 pixel_area);
	virtual void updateFaceData(LLFace* face, F32 pixel_area,
								bool damp_wind = false,
								bool terse_update = false);
	virtual bool updateLOD(F32 pixel_area, bool activate);
	virtual void updateJointGeometry();
	virtual void dump();

	LL_INLINE void setMeshID(S32 id)					{ mMeshID = id; }

protected:
	void init();

public:
	avatar_joint_mesh_list_t	mMeshParts;	// LLViewerJointMesh*

	static bool					sDisableLOD;

protected:
	S32							mMeshID;
	U32							mComponents;
	F32							mMinPixelArea;
	LLJointPickName				mPickName;
	bool						mValid;
	bool						mIsTransparent;
	bool						mVisible;
};

class LLAvatarJointCollisionVolume : public LLAvatarJoint
{
public:
	LLAvatarJointCollisionVolume();

	LL_INLINE bool inheritScale() override				{ return true; }
	U32 render(F32 pixelArea, bool first_pass = true,
			   bool is_dummy = false) override;

	void renderCollision();

	LLVector3 getVolumePos(const LLVector3& offset);
};

#endif // LL_LLAVATARJOINT_H
