/**
 * @file llavatarjointmesh.h
 * @brief Declaration of LLAvatarJointMesh class
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

#ifndef LL_LLAVATARJOINTMESH_H
#define LL_LLAVATARJOINTMESH_H

#include "llavatarjoint.h"
#include "llgltexture.h"
#include "llpolymesh.h"
#include "llpreprocessor.h"
#include "llcolor4.h"

class LLCharacter;
class LLDrawable;
class LLFace;
class LLTexLayerSet;

typedef enum e_avatar_render_pass
{
	AVATAR_RENDER_PASS_SINGLE,
	AVATAR_RENDER_PASS_CLOTHING_INNER,
	AVATAR_RENDER_PASS_CLOTHING_OUTER
} EAvatarRenderPass;

//-----------------------------------------------------------------------------
// class LLSkinJoint
//-----------------------------------------------------------------------------

class LLSkinJoint
{
protected:
	LOG_CLASS(LLSkinJoint);

public:
	LLSkinJoint();
	~LLSkinJoint();
	bool setupSkinJoint(LLAvatarJoint* joint);

	static LLAvatarJoint* getBaseSkeletonAncestor(LLAvatarJoint* joint);

private:
	static LLVector3 totalSkinOffset(LLAvatarJoint* joint);

public:
	LLAvatarJoint*	mJoint;
	LLVector3		mRootToJointSkinOffset;
	LLVector3		mRootToParentJointSkinOffset;
};

//-----------------------------------------------------------------------------
// class LLAvatarJointMesh
//-----------------------------------------------------------------------------

class LLAvatarJointMesh : public virtual LLAvatarJoint
{
public:
	LLAvatarJointMesh();
	virtual ~LLAvatarJointMesh();

	// Gets the shape color
	void getColor(F32* red, F32* green, F32* blue, F32* alpha);

	// Sets the shape color
	void setColor(F32 red, F32 green, F32 blue, F32 alpha);
	void setColor(const LLColor4& color);

	// Sets the shininess
	LL_INLINE void setSpecular(const LLColor4& color, F32 shiny)
	{
#if 0
		mSpecular = color;
#endif
		mShiny = shiny;
	}

	// Sets the shape texture
	void setTexture(LLGLTexture* texture);

	bool hasGLTexture() const;

	LL_INLINE void setTestTexture(U32 name)				{ mTestImageName = name; }

	// Sets layer set responsible for a dynamic shape texture (takes precedence
	// over normal texture)
	void setLayerSet(LLTexLayerSet* layer_set);

	bool hasComposite() const;

	// Gets the poly mesh
	LL_INLINE LLPolyMesh* getMesh()						{ return mMesh; }

	// Sets the poly mesh
	void setMesh(LLPolyMesh* mesh);

	LL_INLINE LLFace* getFace()							{ return mFace; }

	// Sets up joint matrix data for rendering
	void setupJoint(LLAvatarJoint* current_joint);

	// Render time method to upload batches of joint matrices
	void uploadJointMatrices();

	// Sets ID for picking
	LL_INLINE void setMeshID(S32 id)					{ mMeshID = id; }

	// Gets ID for picking
	LL_INLINE S32 getMeshID()							{ return mMeshID; }

	LL_INLINE void setIsTransparent(bool b)				{ mIsTransparent = b; }

private:
	// Allocate skin data
	bool allocateSkinData(U32 numSkinJoints);

	// Free skin data
	void freeSkinData();

protected:
	LLPointer<LLGLTexture>		mTexture;		// ptr to a global texture
	LLTexLayerSet*				mLayerSet;		// ptr to a layer set owned by the avatar
	LLFace*						mFace;			// ptr to a face w/ AGP copy of mesh
	LLSkinJoint*				mSkinJoints;
	LLPolyMesh*					mMesh;			// ptr to a global polymesh
	LLColor4					mColor;			// color value
	F32							mShiny;			// shiny value
	S32							mMeshID;
	U32 						mTestImageName;	// handle to a temporary texture for previewing uploads
	U32							mFaceIndexCount;
	U32							mNumSkinJoints;

#if 0	// Not used
 	LLColor4					mSpecular;		// specular color (always white for now)
	bool						mCullBackFaces;	// true by default
#endif

public:
	// RN: this is here for testing purposes
	static U32					sClothingMaskImageName;
	static LLColor4				sClothingInnerColor;
};

#endif // LL_LLAVATARJOINTMESH_H
