/**
 * @file llpolymesh.h
 * @brief Implementation of LLPolyMesh class
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

#ifndef LL_LLPOLYMESHINTERFACE_H
#define LL_LLPOLYMESHINTERFACE_H

#include "linden_common.h"

#include "lljoint.h"
#include "llpolymorph.h"
#include "llquaternion.h"
#include "llstl.h"
#include "llvector2.h"
#include "llvector3.h"

class LLAvatarAppearance;
class LLSkinJoint;
class LLWearable;

//-----------------------------------------------------------------------------
// LLPolyFace
// A set of 4 vertex indices.
// An LLPolyFace can represent either a triangle or quad.
// If the last index is -1, it's a triangle.
//-----------------------------------------------------------------------------
typedef S32 LLPolyFace[3];

//struct PrimitiveGroup;

//-----------------------------------------------------------------------------
// LLPolyMesh
// A polyhedra consisting of any number of triangles and quads.
// All instances contain a set of faces, and optionally may include
// faces grouped into named face sets.
//-----------------------------------------------------------------------------
class LLPolyMorphTarget;

class LLPolyMeshSharedData
{
	friend class LLPolyMesh;

protected:
	LOG_CLASS(LLPolyMeshSharedData);

public:
	LLPolyMeshSharedData();
	~LLPolyMeshSharedData();

private:
	void setupLOD(LLPolyMeshSharedData* reference_data);

	// Frees all mesh memory resources
	void freeMeshData();

	LL_INLINE void setPosition(const LLVector3& pos)	{ mPosition = pos; }
	LL_INLINE void setRotation(const LLQuaternion& rot)	{ mRotation = rot; }
	LL_INLINE void setScale(const LLVector3& scale)		{ mScale = scale; }

	bool allocateVertexData(U32 numVertices);

	// Free the vertex data
	void freeVertexData();

	bool allocateFaceData(U32 numFaces);

	bool allocateJointNames(U32 numJointNames);

	// Retrieve the number of KB of memory used by this instance
	U32 getNumKB();

	// Load mesh data from file
	bool loadMesh(const std::string& fileName);

public:
#if 0	// Dead code
	void genIndices(S32 offset);
#endif

	const LLVector2& getUVs(U32 index);

	const S32* getSharedVert(S32 vert);

	LL_INLINE bool isLOD()								{ return mReferenceData != NULL; }

private:
	// Transform data
	LLQuaternion			mRotation;
	LLVector3				mPosition;
	LLVector3				mScale;

	// Vertex data
	LLVector4a*				mBaseCoords;
	LLVector4a*				mBaseNormals;
	LLVector4a*				mBaseBinormals;
	LLVector2*				mTexCoords;
	LLVector2*				mDetailTexCoords;
	F32*					mWeights;
	S32						mNumVertices;

	bool					mHasWeights;
	bool					mHasDetailTexCoords;

	// Face data
	LLPolyFace*				mFaces;
	S32						mNumFaces;

	// Face set data
	U32						mNumJointNames;
	std::string*			mJointNames;

	// Morph targets
	typedef std::set<LLPolyMorphData*> morphdata_list_t;
	morphdata_list_t		mMorphData;

	std::map<S32, S32> 		mSharedVerts;

	LLPolyMeshSharedData*	mReferenceData;
	S32						mLastIndexOffset;

public:
	// Temporarily...
	// Triangle indices
	U32						mNumTriangleIndices;
	U32*					mTriangleIndices;
};

class LLJointRenderData
{
public:
	LLJointRenderData(const LLMatrix4* world_matrix, LLSkinJoint* skin_joint)
	:	mWorldMatrix(world_matrix),
		mSkinJoint(skin_joint)
	{
	}

public:
	const LLMatrix4*		mWorldMatrix;
	LLSkinJoint*			mSkinJoint;
};

class LLPolyMesh
{
protected:
	LOG_CLASS(LLPolyMesh);

public:
	// Constructor
	LLPolyMesh(LLPolyMeshSharedData* shared_data, LLPolyMesh* reference_mesh);

	// Destructor
	~LLPolyMesh();

	// Requests a mesh by name.
	// If the mesh already exists in the global mesh table, it is returned,
	// otherwise it is loaded from file, added to the table, and returned.
	static LLPolyMesh* getMesh(const std::string& name,
							   LLPolyMesh* reference_mesh = NULL);

	// Frees all loaded meshes.
	// This should only be called once you know there are no outstanding
	// references to these objects.  Generally, upon exit of the application.
	static void freeAllMeshes();

	//--------------------------------------------------------------------
	// Transform Data Access
	//--------------------------------------------------------------------
	// Get position
	LL_INLINE const LLVector3& getPosition()
	{
		llassert (mSharedData);
		return mSharedData->mPosition;
	}

	// Get rotation
	LL_INLINE const LLQuaternion& getRotation()
	{
		llassert (mSharedData);
		return mSharedData->mRotation;
	}

	// Get scale
	LL_INLINE const LLVector3& getScale()
	{
		llassert (mSharedData);
		return mSharedData->mScale;
	}

	//--------------------------------------------------------------------
	// Vertex Data Access
	//--------------------------------------------------------------------
	// Get number of vertices
	LL_INLINE U32 getNumVertices()
	{
		llassert (mSharedData);
		return mSharedData->mNumVertices;
	}

	// Returns whether or not the mesh has detail texture coords
	LL_INLINE bool hasDetailTexCoords()
	{
		llassert (mSharedData);
		return mSharedData->mHasDetailTexCoords;
	}

	// Returns whether or not the mesh has vertex weights
	LL_INLINE bool hasWeights() const
	{
		llassert (mSharedData);
		return mSharedData->mHasWeights;
	}

	// Get coords
	LL_INLINE const LLVector4a* getCoords() const		{ return mCoords; }

	// non const version
	LL_INLINE LLVector4a* getWritableCoords()			{ return mCoords; }

	// Get normals
	LL_INLINE const LLVector4a* getNormals() const		{ return mNormals; }

	// Get binormals
	LL_INLINE const LLVector4a* getBinormals() const	{ return mBinormals; }

	// Get base mesh coords
	LL_INLINE const LLVector4a* getBaseCoords() const
	{
		llassert(mSharedData);
		return mSharedData->mBaseCoords;
	}

	// Get base mesh normals
	LL_INLINE const LLVector4a* getBaseNormals() const
	{
		llassert(mSharedData);
		return mSharedData->mBaseNormals;
	}

	// Get base mesh binormals
	LL_INLINE const LLVector4a* getBaseBinormals() const
	{
		llassert(mSharedData);
		return mSharedData->mBaseBinormals;
	}

	// intermediate morphed normals and output normals
	LL_INLINE LLVector4a* getWritableNormals()			{ return mNormals; }

	LL_INLINE LLVector4a* getScaledNormals()			{ return mScaledNormals; }

	LL_INLINE LLVector4a* getWritableBinormals()		{ return mBinormals; }

	LL_INLINE LLVector4a* getScaledBinormals()			{ return mScaledBinormals; }

	// Get texCoords
	LL_INLINE const LLVector2* getTexCoords() const
	{
		return mTexCoords;
	}

	// non const version
	LL_INLINE LLVector2* getWritableTexCoords()			{ return mTexCoords; }

	// Get detailTexCoords
	LL_INLINE const LLVector2* getDetailTexCoords() const
	{
		llassert (mSharedData);
		return mSharedData->mDetailTexCoords;
	}

	// Get weights
	LL_INLINE const F32* getWeights() const
	{
		llassert (mSharedData);
		return mSharedData->mWeights;
	}

	LL_INLINE F32* getWritableWeights() const
	{
		llassert (mSharedData);
		return mSharedData->mWeights;
	}

	LL_INLINE LLVector4a* getWritableClothingWeights()	{ return mClothingWeights; }

	LL_INLINE const LLVector4a* getClothingWeights()
	{
		return mClothingWeights;
	}

	//--------------------------------------------------------------------
	// Face Data Access
	//--------------------------------------------------------------------
	// Get number of faces
	LL_INLINE S32 getNumFaces()
	{
		llassert (mSharedData);
		return mSharedData->mNumFaces;
	}

	// Get faces
	LL_INLINE LLPolyFace* getFaces()
	{
		llassert (mSharedData);
		return mSharedData->mFaces;
	}

	LL_INLINE U32 getNumJointNames()
	{
		llassert (mSharedData);
		return mSharedData->mNumJointNames;
	}

	LL_INLINE std::string* getJointNames()
	{
		llassert (mSharedData);
		return mSharedData->mJointNames;
	}

	LLPolyMorphData* getMorphData(const std::string& morph_name);
#if 0
 	void removeMorphData(LLPolyMorphData* morph_target);
 	void deleteAllMorphData();
#endif

	LL_INLINE LLPolyMeshSharedData* getSharedData() const
	{
		return mSharedData;
	}

	LL_INLINE LLPolyMesh* getReferenceMesh()			{ return mReferenceMesh ? mReferenceMesh : this; }

	// Get indices
	LL_INLINE U32* getIndices()							{ return mSharedData ? mSharedData->mTriangleIndices : NULL; }

	LL_INLINE bool isLOD()								{ return mSharedData && mSharedData->isLOD(); }

	LL_INLINE void setAvatar(LLAvatarAppearance* av)	{ mAvatarp = av; }
	LL_INLINE LLAvatarAppearance* getAvatar()			{ return mAvatarp; }

	std::vector<LLJointRenderData*>	mJointRenderData;

	U32 mFaceVertexOffset;
	U32 mFaceVertexCount;
	U32 mFaceIndexOffset;
	U32 mFaceIndexCount;
	U32 mCurVertexCount;

private:
	void initializeForMorph();

	// Dumps diagnostic information about the global mesh table
	static void dumpDiagInfo();

protected:
	// mesh data shared across all instances of a given mesh
	LLPolyMeshSharedData*	mSharedData;
	// Single array of floats for allocation / deletion
	F32*					mVertexData;
	// deformed vertices (resulting from application of morph targets)
	LLVector4a*				mCoords;
	// deformed normals (resulting from application of morph targets)
	LLVector4a*				mScaledNormals;
	// output normals (after normalization)
	LLVector4a*				mNormals;
	// deformed binormals (resulting from application of morph targets)
	LLVector4a*				mScaledBinormals;
	// output binormals (after normalization)
	LLVector4a*				mBinormals;
	// weight values that mark verts as clothing/skin
	LLVector4a*				mClothingWeights;
	// output texture coordinates
	LLVector2*				mTexCoords;

	LLPolyMesh*				mReferenceMesh;

	// global mesh list
	typedef std::map<std::string, LLPolyMeshSharedData*> LLPolyMeshSharedDataTable;
	static LLPolyMeshSharedDataTable sGlobalSharedMeshList;

	// Backlink only; don't make this an LLPointer.
	LLAvatarAppearance*		mAvatarp;
};

#endif // LL_LLPOLYMESHINTERFACE_H
