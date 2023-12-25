/**
 * @file llvotree.h
 * @brief LLVOTree class header file
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLVOTREE_H
#define LL_LLVOTREE_H

#include "hbfastmap.h"
#include "llxform.h"

#include "lldrawpooltree.h"
#include "llviewerobject.h"

class LLFace;
class LLDrawPool;
class LLSelectNode;
class LLViewerFetchedTexture;

constexpr U32 MAX_NUM_TREE_LOD_LEVELS = 4;
constexpr F32 TRUNK_STIFF = 22.f * DEG_TO_RAD;

class LLVOTree final : public LLViewerObject
{
	friend class LLDrawPoolTree;

protected:
	LOG_CLASS(LLVOTree);

	~LLVOTree() override;

public:
	enum
	{
		VERTEX_DATA_MASK =	(1 << LLVertexBuffer::TYPE_VERTEX) |
							(1 << LLVertexBuffer::TYPE_NORMAL) |
							(1 << LLVertexBuffer::TYPE_TEXCOORD0)
	};

	LLVOTree(const LLUUID& id, LLViewerRegion* regionp);

	// Call these only once:
	static void initClass();
	static void cleanupClass();
	// Call this whenever needed:
	static void updateSettings();

	static bool isTreeRenderingStopped();

	U32 processUpdateMessage(LLMessageSystem* mesgsys, void** user_data,
							 U32 block_num, EObjectUpdateType upd_type,
							 LLDataPacker* dp) override;

	void idleUpdate(F64 time) override;

	void setPixelAreaAndAngle() override;
	void updateTextures() override;

	LLDrawable* createDrawable() override;
	bool updateGeometry(LLDrawable* drawable) override;
	void updateSpatialExtents(LLVector4a& min, LLVector4a& max) override;

	U32 getPartitionType() const override;

	LL_INLINE void resetVertexBuffers() override
	{
		mReferenceBuffer = mUpdateMeshBuffer = NULL;
	}

	LL_INLINE LLViewerFetchedTexture* getTreeTexture()	{ return mTreeImagep.get(); }

	void updateRadius() override;

	void calcNumVerts(U32& vert_count, U32& index_count, S32 trunk_LOD,
					  S32 stop_level, U16 depth, U16 trunk_depth,
					  F32 branches);

	void updateMesh();

	void appendMesh(LLStrider<LLVector3>& vertices,
					LLStrider<LLVector3>& normals,
					LLStrider<LLVector2>& tex_coords,
					LLStrider<LLColor4U>& colors,
					LLStrider<U16>& indices, U16& idx_offset,
					const LLMatrix4& matrix, const LLMatrix4& norm_mat,
					S32 vertex_offset, S32 vertex_count,
					S32 index_count, S32 index_offset);

	void genBranchPipeline(LLStrider<LLVector3>& vertices,
						   LLStrider<LLVector3>& normals,
						   LLStrider<LLVector2>& tex_coords,
						   LLStrider<LLColor4U>& colors,
						   LLStrider<U16>& indices, U16& index_offset,
						   const LLMatrix4& matrix,
						   S32 trunk_LOD, S32 stop_level,
						   U16 depth, U16 trunk_depth,
						   F32 scale, F32 twist, F32 droop,
						   F32 branches, F32 alpha);

	U32 drawBranchPipeline(LLMatrix4& matrix, U16* indicesp, S32 trunk_LOD,
						   S32 stop_level, U16 depth, U16 trunk_depth,
						   F32 scale, F32 twist, F32 droop,  F32 branches,
						   F32 alpha);

	 bool lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
							   // Which face to check, -1=ALL_SIDES
							   S32 face = -1,
							   bool pick_transparent = false,
							   bool pick_rigged = false,
							   // Which face was hit
							   S32* face_hit = NULL,
							   // Intersection point
							   LLVector4a* intersection = NULL,
							   // Texture coordinates of the intersection point
							   LLVector2* tex_coord = NULL,
							   // Surface normal at the intersection point
							   LLVector4a* normal = NULL,
							   // Surface tangent at the intersection point
							   LLVector4a* tangent = NULL) override;

	void generateSilhouette(LLSelectNode* nodep);

private:
	void generateSilhouetteVertices(std::vector<LLVector3>& vertices,
									std::vector<LLVector3>& normals,
									const LLVector3& view_vec,
									const LLMatrix4& mat,
									const LLMatrix3& norm_mat);

public:
	struct TreeSpeciesData
	{
		LLUUID			mTextureID;

		F32				mBranchLength;  	// Scale (length) of tree branches
		F32				mDroop;         	// Droop from vertical (degrees) at each branch recursion
		F32				mTwist;         	// Twist
		F32				mBranches;      	// Number of branches emitted at each recursion level
		U8				mDepth;         	// Number of recursions to tips of branches
		F32				mScaleStep;     	// Multiplier for scale at each recursion level
		U8				mTrunkDepth;

		F32				mLeafScale;     	// Scales leaf texture when rendering
		F32				mTrunkLength;   	// Scales branch diameters when rendering
		F32				mBillboardScale; 	// Scales the billboard representation
		F32				mBillboardRatio; 	// Height to width aspect ratio
		F32				mTrunkAspect;
		F32				mBranchAspect;
		F32				mRandomLeafRotate;
		F32				mNoiseScale;    	// Scaling of noise function in perlin space (norm = 1.0)
		F32				mNoiseMag;      	// amount of perlin noise to deform by (0 = none)
		F32				mTaper;         	// amount of perlin noise to deform by (0 = none)
		F32				mRepeatTrunkZ;  	// Times to repeat the trunk texture vertically along trunk
	};

	static F32			sTreeFactor;		// Tree level of detail factor

	static S32			sMaxTreeSpecies;

	static F32			sTreeAnimationDamping;
	static F32			sTreeTrunkStiffness;
	static F32			sTreeWindSensitivity;
	static bool			sRenderAnimateTrees;

	typedef std::map<std::string, S32> species_list_t;
	static species_list_t sSpeciesNames;

protected:
	TreeSpeciesData*	mSpeciesData;
	// Reference geometry for generating tree mesh
	LLPointer<LLVertexBuffer>			mReferenceBuffer;
	// Auxilliary buffer used when updating tree mesh
	LLPointer<LLVertexBuffer>			mUpdateMeshBuffer;
	// Pointer to proper tree image
	LLPointer<LLViewerFetchedTexture>	mTreeImagep;

	// Accumulated wind (used for blowing trees)
	LLVector3			mTrunkBend;
	LLVector3			mTrunkVel;
	LLVector3			mWind;

	// Complete rebuild when not animating
	LLVector3			mLastPosition;
	LLQuaternion		mLastRotation;
	U32					mFrameCount;

	F32					mBranchLength;		// Scale (length) of tree branches
	F32					mTrunkLength;		// Trunk length (first recursion)
	F32					mDroop;				// Droop from vertical (degrees) at each branch recursion
	F32					mTwist;				// Twist
	F32					mBranches;			// Number of branches emitted at each recursion level
	F32					mScaleStep;			// Multiplier for scale at each recursion level
	U32					mTrunkLOD;
	F32					mLeafScale;			// Scales leaf texture when rendering

	F32					mBillboardScale;	//  How big to draw the billboard?
	F32					mBillboardRatio;	//  Height to width ratio of billboard
	F32					mTrunkAspect;		//  Ratio between width/length of trunk
	F32					mBranchAspect;		//  Ratio between width/length of branch
	F32					mRandomLeafRotate;	//	How much to randomly rotate leaves about arbitrary axis

	U8					mSpecies;			// Species of tree
	U8					mDepth;				// Number of recursions to tips of branches
	U8					mTrunkDepth;

	typedef flat_hmap<U32, TreeSpeciesData*> data_map_t;
	static data_map_t	sSpeciesTable;

	static S32			sLODIndexOffset[4];
	static S32			sLODIndexCount[4];
	static S32			sLODVertexOffset[4];
	static S32			sLODVertexCount[4];
	static S32			sLODSlices[4];
	static F32			sLODAngles[4];
};

#endif
