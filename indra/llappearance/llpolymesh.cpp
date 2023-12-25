/**
 * @file llpolymesh.cpp
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

#include "llpolymesh.h"

#include "llavatarappearance.h"
#include "lldir.h"
#include "llendianswizzle.h"
#include "llfasttimer.h"
#include "llvolume.h"
#include "llwearable.h"
#include "llxmltree.h"

#define HEADER_ASCII "Linden Mesh 1.0"
#define HEADER_BINARY "Linden Binary Mesh 1.0"

LLPolyMorphData* clone_morph_param_duplicate(const LLPolyMorphData* src_data,
											 const std::string& name);
LLPolyMorphData* clone_morph_param_direction(const LLPolyMorphData* src_data,
											 const LLVector3& direction,
											 const std::string& name);
LLPolyMorphData* clone_morph_param_cleavage(const LLPolyMorphData* src_data,
											F32 scale,
											const std::string& name);

//-----------------------------------------------------------------------------
// Global table of loaded LLPolyMeshes
//-----------------------------------------------------------------------------
LLPolyMesh::LLPolyMeshSharedDataTable LLPolyMesh::sGlobalSharedMeshList;

//-----------------------------------------------------------------------------
// LLPolyMeshSharedData class
//-----------------------------------------------------------------------------

LLPolyMeshSharedData::LLPolyMeshSharedData()
{
	mNumVertices = 0;
	mBaseCoords = NULL;
	mBaseNormals = NULL;
	mBaseBinormals = NULL;
	mTexCoords = NULL;
	mDetailTexCoords = NULL;
	mWeights = NULL;
	mHasWeights = false;
	mHasDetailTexCoords = false;

	mNumFaces = 0;
	mFaces = NULL;

	mNumJointNames = 0;
	mJointNames = NULL;

	mTriangleIndices = NULL;
	mNumTriangleIndices = 0;

	mReferenceData = NULL;

	mLastIndexOffset = -1;
}

LLPolyMeshSharedData::~LLPolyMeshSharedData()
{
	freeMeshData();
	for_each(mMorphData.begin(), mMorphData.end(), DeletePointer());
	mMorphData.clear();
}

void LLPolyMeshSharedData::setupLOD(LLPolyMeshSharedData* reference_data)
{
	mReferenceData = reference_data;
	if (reference_data)
	{
		mBaseCoords = reference_data->mBaseCoords;
		mBaseNormals = reference_data->mBaseNormals;
		mBaseBinormals = reference_data->mBaseBinormals;
		mTexCoords = reference_data->mTexCoords;
		mDetailTexCoords = reference_data->mDetailTexCoords;
		mWeights = reference_data->mWeights;
		mHasWeights = reference_data->mHasWeights;
		mHasDetailTexCoords = reference_data->mHasDetailTexCoords;
	}
}

void LLPolyMeshSharedData::freeMeshData()
{
	if (!mReferenceData)
	{
		freeVertexData();
	}

	mNumFaces = 0;
	if (mFaces)
	{
		delete[] mFaces;
		mFaces = NULL;
	}

	mNumJointNames = 0;
	if (mJointNames)
	{
		delete[] mJointNames;
		mJointNames = NULL;
	}

	if (mTriangleIndices)
	{
		delete[] mTriangleIndices;
		mTriangleIndices = NULL;
	}
}

// compate_int is used by the qsort function to sort the index array
S32 compare_int(const void* a, const void* b);

#if 0	// Dead code
void LLPolyMeshSharedData::genIndices(S32 index_offset)
{
	if (index_offset == mLastIndexOffset)
	{
		return;
	}

	if (mTriangleIndices)
	{
		delete[] mTriangleIndices;
	}
	mTriangleIndices = new U32[mNumTriangleIndices];

	S32 cur_index = 0;
	for (S32 i = 0; i < mNumFaces; ++i)
	{
		mTriangleIndices[cur_index++] = mFaces[i][0] + index_offset;
		mTriangleIndices[cur_index++] = mFaces[i][1] + index_offset;
		mTriangleIndices[cur_index++] = mFaces[i][2] + index_offset;
	}

	mLastIndexOffset = index_offset;
}
#endif

U32 LLPolyMeshSharedData::getNumKB()
{
	U32 bytes = sizeof(LLPolyMesh);

	if (!isLOD())
	{
		bytes += mNumVertices * (sizeof(LLVector3) +	// coords
								 sizeof(LLVector3) +	// normals
								 sizeof(LLVector2));	// texCoords
	}

	if (mHasDetailTexCoords && !isLOD())
	{
		bytes += mNumVertices * sizeof(LLVector2);     // detailTexCoords
	}

	if (mHasWeights && !isLOD())
	{
		bytes += mNumVertices * sizeof(float);	 // weights
	}

	bytes += mNumFaces * sizeof(LLPolyFace);       // faces

	return bytes / 1024;
}

bool LLPolyMeshSharedData::allocateVertexData(U32 n_vertices)
{
	mBaseCoords =(LLVector4a*)allocate_volume_mem(n_vertices * sizeof(LLVector4a));
	if (!mBaseCoords)
	{
		freeVertexData();
		return false;
	}

	mBaseNormals = (LLVector4a*)allocate_volume_mem(n_vertices * sizeof(LLVector4a));
	if (!mBaseNormals)
	{
		freeVertexData();
		return false;
	}

	mBaseBinormals = (LLVector4a*)allocate_volume_mem(n_vertices * sizeof(LLVector4a));
	if (!mBaseBinormals)
	{
		freeVertexData();
		return false;
	}

	S32 tex_size = (n_vertices * sizeof(LLVector2) + 0xF) & ~0xF;
	mTexCoords = (LLVector2*)allocate_volume_mem(tex_size);
	if (!mTexCoords)
	{
		freeVertexData();
		return false;
	}

	mDetailTexCoords = (LLVector2*)allocate_volume_mem(n_vertices * sizeof(LLVector2));
	if (!mDetailTexCoords)
	{
		freeVertexData();
		return false;
	}

	mWeights = (F32*)allocate_volume_mem(n_vertices * sizeof(F32));
	if (!mWeights)
	{
		freeVertexData();
		return false;
	}

	for (U32 i = 0; i < n_vertices; ++i)
	{
		mBaseCoords[i].clear();
		mBaseNormals[i].clear();
		mBaseBinormals[i].clear();
		mTexCoords[i].clear();
		mWeights[i] = 0.f;
	}

	mNumVertices = n_vertices;

	return true;
}

void LLPolyMeshSharedData::freeVertexData()
{
	mNumVertices = 0;

	if (mBaseCoords)
	{
		free_volume_mem(mBaseCoords);
		mBaseCoords = NULL;
	}

	if (mBaseNormals)
	{
		free_volume_mem(mBaseNormals);
		mBaseNormals = NULL;
	}

	if (mBaseBinormals)
	{
		free_volume_mem(mBaseBinormals);
		mBaseBinormals = NULL;
	}

	if (mTexCoords)
	{
		free_volume_mem(mTexCoords);
		mTexCoords = NULL;
	}

	if (mDetailTexCoords)
	{
		free_volume_mem(mDetailTexCoords);
		mDetailTexCoords = NULL;
	}

	if (mWeights)
	{
		free_volume_mem(mWeights);
		mWeights = NULL;
	}
}

bool LLPolyMeshSharedData::allocateFaceData(U32 n_faces)
{
	mFaces = new LLPolyFace[n_faces];
	if (!mFaces)
	{
		return false;
	}
	mNumFaces = n_faces;
	mNumTriangleIndices = mNumFaces * 3;
	return true;
}

bool LLPolyMeshSharedData::allocateJointNames(U32 numJointNames)
{
	mJointNames = new std::string[numJointNames];
	if (!mJointNames)
	{
		return false;
	}
	mNumJointNames = numJointNames;
	return true;
}

bool LLPolyMeshSharedData::loadMesh(const std::string& filename)
{
	//-------------------------------------------------------------------------
	// Open the file
	//-------------------------------------------------------------------------
	if (filename.empty())
	{
		llwarns << "Filename is Empty !" << llendl;
		llassert(false);
		return false;
	}
	LLFILE* fp = LLFile::open(filename, "rb");
	if (!fp)
	{
		llwarns << "Cannot open: " << filename << llendl;
		llassert(false);
		return false;
	}

	//-------------------------------------------------------------------------
	// Read a chunk
	//-------------------------------------------------------------------------
	char header[128];
	if (fread(header, 1, 128, fp) != 128)
	{
		llwarns << "Short read" << llendl;
	}

	//-------------------------------------------------------------------------
	// Check for proper binary header
	//-------------------------------------------------------------------------
	bool status = false;
	if (strncmp(header, HEADER_BINARY, strlen(HEADER_BINARY)) == 0)
	{
		LL_DEBUGS("PolyMesh") << "Loading " << filename << LL_ENDL;

		//---------------------------------------------------------------------
		// File Header (seek past it)
		//---------------------------------------------------------------------
		fseek(fp, 24, SEEK_SET);

		//---------------------------------------------------------------------
		// HasWeights
		//---------------------------------------------------------------------
		U8 has_weights;
		size_t num_read = fread(&has_weights, sizeof(U8), 1, fp);
		if (num_read != 1)
		{
			llwarns << "Cannot read HasWeights flag from " << filename
					<< llendl;
			llassert(false);
			goto abortion;
		}
		if (!isLOD())
		{
			mHasWeights = has_weights != 0;
		}

		//---------------------------------------------------------------------
		// HasDetailTexCoords
		//---------------------------------------------------------------------
		U8 has_detail_tcoords;
		num_read = fread(&has_detail_tcoords, sizeof(U8), 1, fp);
		if (num_read != 1)
		{
			llwarns << "Cannot read HasDetailTexCoords flag from " << filename
					<< llendl;
			llassert(false);
			goto abortion;
		}

		//---------------------------------------------------------------------
		// Position
		//---------------------------------------------------------------------
		LLVector3 position;
		num_read = fread(position.mV, sizeof(float), 3, fp);
		llendianswizzle(position.mV, sizeof(float), 3);
		if (num_read != 3)
		{
			llwarns << "Cannot read Position from " << filename << llendl;
			llassert(false);
			goto abortion;
		}
		setPosition(position);

		//---------------------------------------------------------------------
		// Rotation
		//---------------------------------------------------------------------
		LLVector3 rot_angles;
		num_read = fread(rot_angles.mV, sizeof(float), 3, fp);
		llendianswizzle(rot_angles.mV, sizeof(float), 3);
		if (num_read != 3)
		{
			llwarns << "Cannot read RotationAngles from " << filename
					<< llendl;
			llassert(false);
			goto abortion;
		}

		U8 rot_order;
		num_read = fread(&rot_order, sizeof(U8), 1, fp);
		if (num_read != 1)
		{
			llwarns << "Cannot read RotationOrder from " << filename
					<< llendl;
			llassert(false);
			goto abortion;
		}

		rot_order = 0;
		setRotation(mayaQ(rot_angles.mV[0], rot_angles.mV[1], rot_angles.mV[2],
						  (LLQuaternion::Order)rot_order));

		//---------------------------------------------------------------------
		// Scale
		//---------------------------------------------------------------------
		LLVector3 scale;
		num_read = fread(scale.mV, sizeof(float), 3, fp);
		llendianswizzle(scale.mV, sizeof(float), 3);
		if (num_read != 3)
		{
			llwarns << "Cannot read Scale from " << filename << llendl;
			llassert(false);
			goto abortion;
		}
		setScale(scale);

		//---------------------------------------------------------------------
		// Release any existing mesh geometry
		//---------------------------------------------------------------------
		freeMeshData();

		//---------------------------------------------------------------------
		// NumVertices
		//---------------------------------------------------------------------
		U16 n_vertices = 0;
		if (!isLOD())
		{
			num_read = fread(&n_vertices, sizeof(U16), 1, fp);
			llendianswizzle(&n_vertices, sizeof(U16), 1);
			if (num_read != 1)
			{
				llwarns << "Cannot read NumVertices from " << filename
						<< llendl;
				llassert(false);
				goto abortion;
			}

			if (!allocateVertexData(n_vertices))
			{
				llwarns << "Can't allocate vertex data: out of memory ?"
						<< llendl;
				llassert(false);
				goto abortion;
			}

			// Coords
			for (U16 i = 0; i < n_vertices; ++i)
			{
				num_read = fread(&mBaseCoords[i], sizeof(float), 3, fp);
				llendianswizzle(&mBaseCoords[i], sizeof(float), 3);
				if (num_read != 3)
				{
					llwarns << "Cannot read Coordinates from " << filename
							<< llendl;
					llassert(false);
					goto abortion;
				}
			}

			// Normals
			for (U16 i = 0; i < n_vertices; ++i)
			{
				num_read = fread(&mBaseNormals[i], sizeof(float), 3, fp);
				llendianswizzle(&mBaseNormals[i], sizeof(float), 3);
				if (num_read != 3)
				{
					llwarns << "Cannot read Normals from " << filename
							<< llendl;
					llassert(false);
					goto abortion;
				}
			}

			// Binormals
			for (U16 i = 0; i < n_vertices; ++i)
			{
				num_read = fread(&mBaseBinormals[i], sizeof(float), 3, fp);
				llendianswizzle(&mBaseBinormals[i], sizeof(float), 3);
				if (num_read != 3)
				{
					llwarns << "Cannot read Binormals from " << filename
							<< llendl;
					llassert(false);
					goto abortion;
				}
			}

			// TexCoords
			num_read = fread(mTexCoords, 2 * sizeof(float), n_vertices, fp);
			llendianswizzle(mTexCoords, sizeof(float), 2 * n_vertices);
			if (num_read != n_vertices)
			{
				llwarns << "Cannot read TexCoords from " << filename << llendl;
				llassert(false);
				goto abortion;
			}

			// DetailTexCoords
			if (mHasDetailTexCoords)
			{
				num_read = fread(mDetailTexCoords, 2 * sizeof(float),
								 n_vertices, fp);
				llendianswizzle(mDetailTexCoords, sizeof(float),
								2 * n_vertices);
				if (num_read != n_vertices)
				{
					llwarns << "Cannot read DetailTexCoords from " << filename
							<< llendl;
					llassert(false);
					goto abortion;
				}
			}

			// Weights
			if (mHasWeights)
			{
				num_read = fread(mWeights, sizeof(float), n_vertices, fp);
				llendianswizzle(mWeights, sizeof(float), n_vertices);
				if (num_read != n_vertices)
				{
					llwarns << "Cannot read Weights from " << filename
							<< llendl;
					llassert(false);
					goto abortion;
				}
			}
		}

		//---------------------------------------------------------------------
		// NumFaces
		//---------------------------------------------------------------------
		U16 n_faces;
		num_read = fread(&n_faces, sizeof(U16), 1, fp);
		llendianswizzle(&n_faces, sizeof(U16), 1);
		if (num_read != 1)
		{
			llwarns << "Cannot read NumFaces from " << filename << llendl;
			llassert(false);
			goto abortion;
		}
		allocateFaceData(n_faces);


		//---------------------------------------------------------------------
		// Faces
		//---------------------------------------------------------------------
		U32 i;
		U32 n_tris = 0;
		for (i = 0; i < n_faces; ++i)
		{
			S16 face[3];
			num_read = fread(face, sizeof(U16), 3, fp);
			llendianswizzle(face, sizeof(U16), 3);
			if (num_read != 3)
			{
				llwarns << "Cannot read Face[" << i << "] from " << filename
						<< llendl;
				llassert(false);
				goto abortion;
			}
			if (mReferenceData)
			{
				llassert(face[0] < mReferenceData->mNumVertices);
				llassert(face[1] < mReferenceData->mNumVertices);
				llassert(face[2] < mReferenceData->mNumVertices);
			}

			if (isLOD())
			{
				// store largest index in case of LODs
				for (S32 j = 0; j < 3; ++j)
				{
					if (face[j] > mNumVertices - 1)
					{
						mNumVertices = face[j] + 1;
					}
				}
			}
			mFaces[i][0] = face[0];
			mFaces[i][1] = face[1];
			mFaces[i][2] = face[2];

			++n_tris;
		}

		LL_DEBUGS("PolyMesh") << "verts: " << n_vertices << ", faces: "
							  << n_faces << ", tris: " << n_tris << LL_ENDL;

		//---------------------------------------------------------------------
		// NumSkinJoints
		//---------------------------------------------------------------------
		if (!isLOD())
		{
			U16 n_skin_joints = 0;
			if (mHasWeights)
			{
				num_read = fread(&n_skin_joints, sizeof(U16), 1, fp);
				llendianswizzle(&n_skin_joints, sizeof(U16), 1);
				if (num_read != 1)
				{
					llwarns << "Cannot read NumSkinJoints from " << filename
							<< llendl;
					llassert(false);
					goto abortion;
				}
				allocateJointNames(n_skin_joints);
			}

			//----------------------------------------------------------------
			// SkinJoints
			//----------------------------------------------------------------
			char joint_name[65];
			for (i = 0; i < n_skin_joints; ++i)
			{
				num_read = fread(joint_name, 64, 1, fp);
				joint_name[64] = '\0'; // ensure nul-termination
				if (num_read != 1)
				{
					llwarns << "Cannot read Skin[" << i << "].Name from "
							<< filename << llendl;
					llassert(false);
					goto abortion;
				}

				std::string* jn = &mJointNames[i];
				*jn = joint_name;
			}

			//-----------------------------------------------------------------
			// Look for morph section
			//-----------------------------------------------------------------
			char morph_name[65];
			morph_name[64] = '\0'; // Ensure nul-termination
			while (fread(morph_name, 1, 64, fp) == 64)
			{
				if (!strcmp(morph_name, "End Morphs"))
				{
					// We reached the end of the morphs
					break;
				}

				LLPolyMorphData* morph_data = new LLPolyMorphData(morph_name);
				if (!morph_data || !morph_data->isSuccesfullyAllocated())
				{
					llwarns << "Failure to allocate new morph data for "
							<< morph_name << llendl;
					llassert(false);
					goto abortion;
				}

				if (!morph_data->loadBinary(fp, this))
				{
					delete morph_data;
					continue;
				}

				mMorphData.insert(morph_data);

				LLPolyMorphData* cloned_data;
				if (!strcmp(morph_name, "Breast_Female_Cleavage"))
				{
					cloned_data =
						clone_morph_param_cleavage(morph_data, 0.75f,
												   "Breast_Physics_LeftRight_Driven");
					if (!cloned_data || !cloned_data->isSuccesfullyAllocated())
					{
						llwarns << "Failure to clone morph data" << llendl;
						llassert(false);
						goto abortion;
					}
					mMorphData.insert(cloned_data);

					cloned_data =
						clone_morph_param_duplicate(morph_data,
													"Breast_Physics_InOut_Driven");
					if (!cloned_data || !cloned_data->isSuccesfullyAllocated())
					{
						llwarns << "Failure to clone morph data" << llendl;
						llassert(false);
						goto abortion;
					}
					mMorphData.insert(cloned_data);
				}

				if (!strcmp(morph_name, "Breast_Gravity"))
				{
					cloned_data =
						clone_morph_param_duplicate(morph_data,
													"Breast_Physics_UpDown_Driven");
					if (!cloned_data || !cloned_data->isSuccesfullyAllocated())
					{
						llwarns << "Failure to clone morph data" << llendl;
						llassert(false);
						goto abortion;
					}
					mMorphData.insert(cloned_data);
				}

				if (!strcmp(morph_name, "Big_Belly_Torso"))
				{
					cloned_data =
						clone_morph_param_direction(morph_data,
													LLVector3(0, 0, 0.05f),
													"Belly_Physics_Torso_UpDown_Driven");
					if (!cloned_data || !cloned_data->isSuccesfullyAllocated())
					{
						llwarns << "Failure to clone morph data" << llendl;
						llassert(false);
						goto abortion;
					}
					mMorphData.insert(cloned_data);
				}

				if (!strcmp(morph_name, "Big_Belly_Legs"))
				{
					cloned_data =
						clone_morph_param_direction(morph_data,
													LLVector3(0, 0, 0.05f),
													"Belly_Physics_Legs_UpDown_Driven");
					if (!cloned_data || !cloned_data->isSuccesfullyAllocated())
					{
						llwarns << "Failure to clone morph data" << llendl;
						llassert(false);
						goto abortion;
					}
					mMorphData.insert(cloned_data);
				}

				if (!strcmp(morph_name, "skirt_belly"))
				{
					cloned_data =
						clone_morph_param_direction(morph_data,
													LLVector3(0, 0, 0.05f),
													"Belly_Physics_Skirt_UpDown_Driven");
					if (!cloned_data || !cloned_data->isSuccesfullyAllocated())
					{
						llwarns << "Failure to clone morph data" << llendl;
						llassert(false);
						goto abortion;
					}
					mMorphData.insert(cloned_data);
				}

				if (!strcmp(morph_name, "Small_Butt"))
				{
					cloned_data =
						clone_morph_param_direction(morph_data,
													LLVector3(0, 0, 0.05f),
													"Butt_Physics_UpDown_Driven");
					if (!cloned_data || !cloned_data->isSuccesfullyAllocated())
					{
						llwarns << "Failure to clone morph data" << llendl;
						llassert(false);
						goto abortion;
					}
					mMorphData.insert(cloned_data);

					cloned_data =
						clone_morph_param_direction(morph_data,
													LLVector3(0, 0.03f, 0),
													"Butt_Physics_LeftRight_Driven");
					if (!cloned_data || !cloned_data->isSuccesfullyAllocated())
					{
						llwarns << "Failure to clone morph data" << llendl;
						llassert(false);
						goto abortion;
					}
					mMorphData.insert(cloned_data);
				}
			}

			S32 n_remaps;
			if (fread(&n_remaps, sizeof(S32), 1, fp) == 1)
			{
				llendianswizzle(&n_remaps, sizeof(S32), 1);
				for (S32 i = 0; i < n_remaps; ++i)
				{
					S32 remap_src;
					S32 remap_dst;
					if (fread(&remap_src, sizeof(S32), 1, fp) != 1)
					{
						llwarns << "Cannot read source vertex in vertex remap data"
								<< llendl;
						llassert(false);
						goto abortion;
					}
					if (fread(&remap_dst, sizeof(S32), 1, fp) != 1)
					{
						llwarns << "Cannot read destination vertex in vertex remap data"
								<< llendl;
						llassert(false);
						goto abortion;
					}
					llendianswizzle(&remap_src, sizeof(S32), 1);
					llendianswizzle(&remap_dst, sizeof(S32), 1);

					mSharedVerts[remap_src] = remap_dst;
				}
			}
		}

		status = true;
	}
	else
	{
		llwarns << "Invalid mesh file header: " << filename << llendl;
	}

	if (mNumJointNames == 0)
	{
		allocateJointNames(1);
	}

abortion:
	LLFile::close(fp);

	return status;
}

const S32* LLPolyMeshSharedData::getSharedVert(S32 vert)
{
	if (mSharedVerts.count(vert) > 0)
	{
		return &mSharedVerts[vert];
	}
	return NULL;
}

const LLVector2 &LLPolyMeshSharedData::getUVs(U32 index)
{
	// TODO: convert all index variables to S32
	llassert((S32)index < mNumVertices);

	return mTexCoords[index];
}

//-----------------------------------------------------------------------------
// LLPolyMesh class
//-----------------------------------------------------------------------------

LLPolyMesh::LLPolyMesh(LLPolyMeshSharedData* shared_data,
					   LLPolyMesh* reference_mesh)
{
	llassert(shared_data);

	mSharedData = shared_data;
	mReferenceMesh = reference_mesh;
	mAvatarp = NULL;
	mVertexData = NULL;

	mCurVertexCount = 0;
	mFaceIndexCount = 0;
	mFaceIndexOffset = 0;
	mFaceVertexCount = 0;
	mFaceVertexOffset = 0;

	if (shared_data->isLOD() && reference_mesh)
	{
		mCoords = reference_mesh->mCoords;
		mNormals = reference_mesh->mNormals;
		mScaledNormals = reference_mesh->mScaledNormals;
		mBinormals = reference_mesh->mBinormals;
		mScaledBinormals = reference_mesh->mScaledBinormals;
		mTexCoords = reference_mesh->mTexCoords;
		mClothingWeights = reference_mesh->mClothingWeights;
	}
	else
	{
		// Allocate memory without initializing every vector
		// NOTE: This makes assumptions about the size of LLVector[234]
		S32 nverts = mSharedData->mNumVertices;
		// make sure it's an even number of verts for alignment
		nverts += nverts % 2;
		S32 nfloats = nverts * (4 + //coords
								4 + //normals
								4 + //weights
								2 + //coords
								4 + //scaled normals
								4 + //binormals
								4); //scaled binormals

		// use 16 byte aligned vertex data to make LLPolyMesh SSE friendly
		mVertexData = (F32*)allocate_volume_mem(nfloats * 4);
		if (!mVertexData)
		{
			llwarns << "Failure to allocate vertex data buffer !" << llendl;
			mCoords = mNormals = mClothingWeights = mScaledNormals =
					  mBinormals = mScaledBinormals = NULL;
			mTexCoords = NULL;
			return;
		}
		S32 offset = 0;
		mCoords				= (LLVector4a*)(mVertexData + offset); offset += 4 * nverts;
		mNormals			= (LLVector4a*)(mVertexData + offset); offset += 4 * nverts;
		mClothingWeights	= (LLVector4a*)(mVertexData + offset); offset += 4 * nverts;
		mTexCoords			= (LLVector2*)(mVertexData + offset);  offset += 2 * nverts;
		mScaledNormals		= (LLVector4a*)(mVertexData + offset); offset += 4 * nverts;
		mBinormals			= (LLVector4a*)(mVertexData + offset); offset += 4 * nverts;
		mScaledBinormals	= (LLVector4a*)(mVertexData + offset); offset += 4 * nverts;
		initializeForMorph();
	}
}

LLPolyMesh::~LLPolyMesh()
{
	delete_and_clear(mJointRenderData);

	if (mVertexData)
	{
		free_volume_mem(mVertexData);
		mVertexData = NULL;
	}
}

LLPolyMesh* LLPolyMesh::getMesh(const std::string& name,
								LLPolyMesh* reference_mesh)
{
	//-------------------------------------------------------------------------
	// Search for an existing mesh by this name
	//-------------------------------------------------------------------------
	LLPolyMeshSharedData* meshSharedData = get_ptr_in_map(sGlobalSharedMeshList,
														  name);
	if (meshSharedData)
	{
		LL_DEBUGS("PolyMesh") << "Polymesh " << name
							  << " found in global mesh table." << LL_ENDL;
		LLPolyMesh* poly_mesh = new LLPolyMesh(meshSharedData, reference_mesh);
		return poly_mesh;
	}

	//-------------------------------------------------------------------------
	// If not found, create a new one, add it to the list
	//-------------------------------------------------------------------------
	std::string full_path;
	full_path = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER, name);

	LLPolyMeshSharedData* mesh_data = new LLPolyMeshSharedData();
	if (reference_mesh)
	{
		mesh_data->setupLOD(reference_mesh->getSharedData());
	}
	if (!mesh_data->loadMesh(full_path))
	{
		delete mesh_data;
		return NULL;
	}

	LLPolyMesh* poly_mesh = new LLPolyMesh(mesh_data, reference_mesh);

	LL_DEBUGS("PolyMesh") << "Polymesh " << name
						  << " added to global mesh table." << LL_ENDL;
	sGlobalSharedMeshList[name] = poly_mesh->mSharedData;

	return poly_mesh;
}

void LLPolyMesh::freeAllMeshes()
{
	// Delete each item in the global lists
	for_each(sGlobalSharedMeshList.begin(), sGlobalSharedMeshList.end(),
			 DeletePairedPointer());
	sGlobalSharedMeshList.clear();
}

void LLPolyMesh::dumpDiagInfo()
{
	// keep track of totals
	U32 total_verts = 0;
	U32 total_faces = 0;
	U32 total_kb = 0;

	std::string buf;

	llinfos << "-----------------------------------------------------"
			<< llendl;
	llinfos << "       Global PolyMesh Table (DEBUG only)" << llendl;
	llinfos << "   Verts    Faces  Mem(KB) Name" << llendl;
	llinfos << "-----------------------------------------------------"
			<< llendl;

	// print each loaded mesh, and it's memory usage
	for (LLPolyMeshSharedDataTable::iterator iter = sGlobalSharedMeshList.begin();
		 iter != sGlobalSharedMeshList.end(); ++iter)
	{
		const std::string& mesh_name = iter->first;
		LLPolyMeshSharedData* mesh = iter->second;

		S32 num_verts = mesh->mNumVertices;
		S32 num_faces = mesh->mNumFaces;
		U32 num_kb = mesh->getNumKB();

		buf = llformat("%8d %8d %8d %s", num_verts, num_faces, num_kb,
					   mesh_name.c_str());
		llinfos << buf << llendl;

		total_verts += num_verts;
		total_faces += num_faces;
		total_kb += num_kb;
	}

	llinfos << "-----------------------------------------------------"
			<< llendl;
	buf = llformat("%8d %8d %8d TOTAL", total_verts, total_faces, total_kb);
	llinfos << buf << llendl;
	llinfos << "-----------------------------------------------------"
			<< llendl;
}

void LLPolyMesh::initializeForMorph()
{
	S32 num_vertices = mSharedData->mNumVertices;

    LLVector4a::memcpyNonAliased16((F32*)mCoords,
								   (F32*)mSharedData->mBaseCoords,
								   sizeof(LLVector4a) * num_vertices);
	LLVector4a::memcpyNonAliased16((F32*)mNormals,
								   (F32*)mSharedData->mBaseNormals,
								   sizeof(LLVector4a) * num_vertices);
	LLVector4a::memcpyNonAliased16((F32*)mScaledNormals,
								   (F32*)mSharedData->mBaseNormals,
								   sizeof(LLVector4a) * num_vertices);
	LLVector4a::memcpyNonAliased16((F32*)mBinormals,
								   (F32*)mSharedData->mBaseNormals,
								   sizeof(LLVector4a) * num_vertices);
	LLVector4a::memcpyNonAliased16((F32*)mScaledBinormals,
								   (F32*)mSharedData->mBaseNormals,
								   sizeof(LLVector4a) * num_vertices);
	LLVector4a::memcpyNonAliased16((F32*)mTexCoords,
								   (F32*)mSharedData->mTexCoords,
								   sizeof(LLVector2) *
								   (num_vertices + num_vertices % 2));

	for (S32 i = 0; i < num_vertices; ++i)
	{
		mClothingWeights[i].clear();
	}
}

LLPolyMorphData* LLPolyMesh::getMorphData(const std::string& morph_name)
{
	if (mSharedData)
	{
		for (LLPolyMeshSharedData::morphdata_list_t::iterator
				iter = mSharedData->mMorphData.begin(),
				end = mSharedData->mMorphData.end();
			 iter != end; ++iter)
		{
			LLPolyMorphData* morph_data = *iter;
			if (morph_data->getName() == morph_name)
			{
				return morph_data;
			}
		}
	}
	return NULL;
}

#if 0
// Erasing but not deleting seems bad, but fortunately we do not actually use
// this...
void LLPolyMesh::removeMorphData(LLPolyMorphData* morph_target)
{
	if (mSharedData)
	{
		mSharedData->mMorphData.erase(morph_target);
	}
}

void LLPolyMesh::deleteAllMorphData()
{
	if (mSharedData)
	{
		for_each(mSharedData->mMorphData.begin(),
				 mSharedData->mMorphData.end(), DeletePointer());
		mSharedData->mMorphData.clear();
	}
}
#endif
