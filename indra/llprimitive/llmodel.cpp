/**
 * @file llmodel.cpp
 * @brief Model handling implementation
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

#include <memory>

#include "zlib.h"

#include "llmodel.h"

#include "llconvexdecomposition.h"
#include "lljoint.h"
#include "llmath.h"
#include "llsdserialize.h"
#include "hbxxh.h"

///////////////////////////////////////////////////////////////////////////////
// LLMeshSkinInfo class
///////////////////////////////////////////////////////////////////////////////

LLMeshSkinInfo::LLMeshSkinInfo()
:	mHash(0),
	mPelvisOffset(0.f),
	mLockScaleIfJointPosition(false),
	mInvalidJointsScrubbed(false)
{
}

LLMeshSkinInfo::LLMeshSkinInfo(const LLSD& skin)
:	mHash(0),
	mPelvisOffset(0.f),
	mLockScaleIfJointPosition(false),
	mInvalidJointsScrubbed(false)
{
	fromLLSD(skin);
}

LLMeshSkinInfo::LLMeshSkinInfo(const LLSD& skin, const LLUUID& mesh_id)
:	mHash(0),
	mMeshID(mesh_id),
	mPelvisOffset(0.f),
	mLockScaleIfJointPosition(false),
	mInvalidJointsScrubbed(false)
{
	fromLLSD(skin);
}

void LLMeshSkinInfo::clone(const LLMeshSkinInfo& from)
{
	mMeshID = from.mMeshID;
	mBindShapeMatrix = from.mBindShapeMatrix;
	mJointNames = from.mJointNames;
	mJointKeys = from.mJointKeys;
	mInvBindMatrix = from.mInvBindMatrix;
	mAlternateBindMatrix = from.mAlternateBindMatrix;
	mHash = from.mHash;
	mPelvisOffset = from.mPelvisOffset;
	mLockScaleIfJointPosition = from.mLockScaleIfJointPosition;
	mInvalidJointsScrubbed = from.mLockScaleIfJointPosition;
}

void LLMeshSkinInfo::fromLLSD(const LLSD& skin)
{
	if (skin.has("joint_names"))
	{
		for (U32 i = 0, count = skin["joint_names"].size(); i < count; ++i)
		{
			std::string name = skin["joint_names"][i];
			mJointNames.emplace_back(name);
			mJointKeys.push_back(LLJoint::getKey(name));
		}
	}

	if (skin.has("inverse_bind_matrix"))
	{
		for (U32 i = 0, count = skin["inverse_bind_matrix"].size();
			 i < count; ++i)
		{
			LLMatrix4 mat;
			for (U32 j = 0; j < 4; ++j)
			{
				for (U32 k = 0; k < 4; ++k)
				{
					mat.mMatrix[j][k] =
						skin["inverse_bind_matrix"][i][j * 4 + k].asReal();
				}
			}
			mInvBindMatrix.push_back(mat);
		}
	}

	if (mJointNames.size() != mInvBindMatrix.size())
	{
		llwarns << "Joints vs bind matrix count mismatch. Dropping joint bindings for mesh "
				<< mMeshID << llendl;
		mJointNames.clear();
		mJointKeys.clear();
		mInvBindMatrix.clear();
	}

	if (skin.has("bind_shape_matrix"))
	{
		for (U32 j = 0; j < 4; ++j)
		{
			for (U32 k = 0; k < 4; ++k)
			{
				mBindShapeMatrix.mMatrix[j][k] =
					skin["bind_shape_matrix"][j * 4 + k].asReal();
			}
		}
	}

	size_t mat_size = llmin(mInvBindMatrix.size(),
							LL_CHARACTER_MAX_ANIMATED_JOINTS);
	mInvBindShapeMatrix.resize(mat_size);
	if (mat_size)
	{
		LLMatrix4a bind_shape, inv_bind, mat;
		bind_shape.loadu(mBindShapeMatrix);
		for (size_t i = 0; i < mat_size; ++i)
		{
			inv_bind.loadu(mInvBindMatrix[i]);
			mat.matMul(bind_shape, inv_bind);
			mInvBindShapeMatrix[i].set(mat.getF32ptr());
		}
	}

	if (skin.has("alt_inverse_bind_matrix"))
	{
		for (U32 i = 0, count = skin["alt_inverse_bind_matrix"].size();
			 i < count; ++i)
		{
			LLMatrix4 mat;
			for (U32 j = 0; j < 4; ++j)
			{
				for (U32 k = 0; k < 4; ++k)
				{
					mat.mMatrix[j][k] =
						skin["alt_inverse_bind_matrix"][i][j * 4 + k].asReal();
				}
			}
			mAlternateBindMatrix.push_back(mat);
		}
	}

	if (skin.has("pelvis_offset"))
	{
		mPelvisOffset = skin["pelvis_offset"].asReal();
	}

	if (skin.has("lock_scale_if_joint_position"))
	{
		mLockScaleIfJointPosition =
			skin["lock_scale_if_joint_position"].asBoolean();
	}
	else
	{
		mLockScaleIfJointPosition = false;
	}

	updateHash();
}

LLSD LLMeshSkinInfo::asLLSD(bool include_joints,
							bool lock_scale_if_joint_position) const
{
	LLSD ret;

	U32 joint_names_count = mJointNames.size();
	for (U32 i = 0; i < joint_names_count; ++i)
	{
		ret["joint_names"][i] = mJointNames[i];

		for (U32 j = 0; j < 4; ++j)
		{
			for (U32 k = 0; k < 4; ++k)
			{
				ret["inverse_bind_matrix"][i][j * 4 + k] =
					mInvBindMatrix[i].mMatrix[j][k];
			}
		}
	}

	for (U32 i = 0; i < 4; ++i)
	{
		for (U32 j = 0; j < 4; ++j)
		{
			ret["bind_shape_matrix"][i * 4 + j] =
				mBindShapeMatrix.mMatrix[i][j];
		}
	}

	if (include_joints && mAlternateBindMatrix.size() > 0)
	{
		for (U32 i = 0; i < joint_names_count; ++i)
		{
			for (U32 j = 0; j < 4; ++j)
			{
				for (U32 k = 0; k < 4; ++k)
				{
					ret["alt_inverse_bind_matrix"][i][j * 4 + k] =
						mAlternateBindMatrix[i].mMatrix[j][k];
				}
			}
		}

		if (lock_scale_if_joint_position)
		{
			ret["lock_scale_if_joint_position"] = lock_scale_if_joint_position;
		}

		ret["pelvis_offset"] = mPelvisOffset;
	}

	return ret;
}

void LLMeshSkinInfo::updateHash(bool force)
{
	// When the mesh UUID is known (which is always the case for LLMeshSkinInfo
	// instances created by the mesh repository), use its 64 bits digest; there
	// is no need to hash anything else, since a skin with the same UUID always
	// got the same joints, inverse bind matrix, etc. HB
	if (!force && mMeshID.notNull())
	{
		mHash = mMeshID.getDigest64();
		return;
	}

	// Let's use our super-fast vectorized 64 bits hashing. HB
	HBXXH64 hash;

	// Hash joint names (like in LL's performance viewer). HB
	for (U32 i = 0, count = mJointNames.size(); i < count; ++i)
	{
		hash.update(mJointNames[i]);
	}

	// Hash joint keys (LL's performance viewer uses joint numbers instead). HB
	hash.update((const void*)mJointKeys.data(),
				sizeof(U32) * mJointKeys.size());

	// Hash inverse bind matrix (like in LL's performance viewer).
	// Note: there should be no padding/aligment issue between elements in the
	// mInvBindMatrix LLMatrix4s vector, given that an LLMatrix4 is represented
	// by 16 32 bits values (64 bytes). So we can save a loop here and hash the
	// whole vector as one contiguous block of data. HB
	hash.update((const void*)mInvBindMatrix.data(),
				sizeof(LLMatrix4) * mInvBindMatrix.size());

	mHash = hash.digest();
}

///////////////////////////////////////////////////////////////////////////////
// LLModel class
///////////////////////////////////////////////////////////////////////////////

static std::string model_names[] =
{
	"lowest_lod",
	"low_lod",
	"medium_lod",
	"high_lod",
	"physics_mesh"
};

static const int MODEL_NAMES_LENGTH = sizeof(model_names) /
									  sizeof(std::string);

LLModel::LLModel(LLVolumeParams& params, F32 detail)
:	LLVolume(params, detail),
	mNormalizedScale(1.f, 1.f, 1.f),
	mPelvisOffset(0.f),
	mStatus(NO_ERRORS),
	mSubmodelID(0),
	mDecompID(-1),
	mLocalID(-1)
{
}

LLModel::~LLModel()
{
	if (mDecompID >= 0)
	{
		LLConvexDecomposition::getInstance()->deleteDecomposition(mDecompID);
	}
}

std::string LLModel::getName() const
{
	return mRequestedLabel.empty() ? mLabel : mRequestedLabel;
}

//static
std::string LLModel::getStatusString(U32 status)
{
	static const std::string status_strings[(size_t)INVALID_STATUS + 1] =
	{
		"status_no_error",
		"status_vertex_number_overflow",
		"bad_element"
		"invalid status"
	};
	return status < INVALID_STATUS ? status_strings[status]
								   : status_strings[INVALID_STATUS];
}

void LLModel::offsetMesh(const LLVector3& pivotPoint)
{
	LLVector4a pivot(pivotPoint[VX], pivotPoint[VY], pivotPoint[VZ]);

	for (std::vector<LLVolumeFace>::iterator faceIt = mVolumeFaces.begin();
		 faceIt != mVolumeFaces.end(); )
	{
		std::vector<LLVolumeFace>:: iterator currentFaceIt = faceIt++;
		LLVolumeFace& face = *currentFaceIt;
		LLVector4a* pos = (LLVector4a*)face.mPositions;
		for (S32 i = 0, count = face.mNumVertices; i < count ; ++i)
		{
			pos[i].add(pivot);
		}
	}
}

void LLModel::remapVolumeFaces()
{
	for (S32 i = 0; i < getNumVolumeFaces(); ++i)
	{
		mVolumeFaces[i].remap();
	}
}

void LLModel::optimizeVolumeFaces()
{
	for (S32 i = 0; i < getNumVolumeFaces(); ++i)
	{
		mVolumeFaces[i].optimize();
	}
}

struct MaterialBinding
{
	S32			index;
	std::string	matName;
};

struct MaterialSort
{
	bool operator()(const MaterialBinding& lhs, const MaterialBinding& rhs)
	{
		return LLStringUtil::compareInsensitive(lhs.matName, rhs.matName) < 0;
	}
};

void LLModel::sortVolumeFacesByMaterialName()
{
	S32 count = mVolumeFaces.size();
	S32 mat_count = mMaterialList.size();
	if (!count || !mat_count)
	{
		return;	// Nothing to do
	}

	std::vector<MaterialBinding> bindings;
	bindings.resize(count);

	for (S32 i = 0; i < count; ++i)
	{
		bindings[i].index = i;
		if (i < mat_count)
		{
			bindings[i].matName = mMaterialList[i];
		}
	}
	std::sort(bindings.begin(), bindings.end(), MaterialSort());

	// Re-map the faces to be in the same order the mats now are...
	std::vector<LLVolumeFace> new_faces;
	new_faces.resize(count);
	for (S32 i = 0; i < count; ++i)
	{
		new_faces[i] = mVolumeFaces[bindings[i].index];
		if (i < mat_count)
		{
			mMaterialList[i] = bindings[i].matName;
		}
	}

	mVolumeFaces = new_faces;
}

void LLModel::trimVolumeFacesToSize(U32 new_count,
									LLVolume::face_list_t* remainder)
{
	llassert(new_count <= (U32)LL_SCULPT_MESH_MAX_FACES);

	if (new_count && (U32)getNumVolumeFaces() > new_count)
	{
		// Copy out remaining volume faces for alternative handling, if
		// provided
		if (remainder)
		{
			(*remainder).assign(mVolumeFaces.begin() + new_count,
								mVolumeFaces.end());
		}

		// Trim down to the final set of volume faces (now stuffed to the
		// gills !)
		mVolumeFaces.resize(new_count);
	}
}

#if LL_NORMALIZE_ALL_MODELS
// Shrink group of models to fit on a 1x1x1 cube centered at the origin.
void LLModel::normalizeModels(const std::vector<LLPointer<LLModel> >& model_list)
{
	S32 models_count = model_list.size();
	S32 n = 0;
	while (n < models_count && model_list[n]->mVolumeFaces.empty())
	{
		++n;
	}
	// no model with faces
	if (n == models_count) return;

	LLVector4a min = model_list[n]->mVolumeFaces[0].mExtents[0];
	LLVector4a max = model_list[n]->mVolumeFaces[0].mExtents[1];

	// Treat models as a group: each model out of 1x1 cube needs scaling and
	// will affect whole group scale.
	while (n < models_count)
	{
		LLModel* model = model_list[n++].get();
		if (model && !model->mVolumeFaces.empty())
		{
			// For all of the volume faces in the model, loop over them and see
			// what the extents of the volume along each axis.
			for (S32 i = 1, count = model->mVolumeFaces.size(); i < count; ++i)
			{
				LLVolumeFace& face = model->mVolumeFaces[i];

				update_min_max(min, max, face.mExtents[0]);
				update_min_max(min, max, face.mExtents[1]);

				if (face.mTexCoords)
				{
					LLVector2& min_tc = face.mTexCoordExtents[0];
					LLVector2& max_tc = face.mTexCoordExtents[1];

					min_tc = face.mTexCoords[0];
					max_tc = face.mTexCoords[0];

					for (S32 j = 1; j < face.mNumVertices; ++j)
					{
						update_min_max(min_tc, max_tc, face.mTexCoords[j]);
					}
				}
				else
				{
					face.mTexCoordExtents[0].set(0, 0);
					face.mTexCoordExtents[1].set(1, 1);
				}
			}
		}
	}

	// Now that we have the extents of the model, we can compute the offset
	// needed to center the model at the origin.

	// Compute center of the model and make it negative to get translation
	// needed to center at origin.
	LLVector4a trans;
	trans.setAdd(min, max);
	trans.mul(-0.5f);

	// Compute the total size along all axes of the model.
	LLVector4a size;
	size.setSub(max, min);

	// Prevent division by zero.
	F32 x = size[0];
	F32 y = size[1];
	F32 z = size[2];
	F32 w = size[3];
	if (fabs(x) < F_APPROXIMATELY_ZERO)
	{
		x = 1.f;
	}
	if (fabs(y) < F_APPROXIMATELY_ZERO)
	{
		y = 1.f;
	}
	if (fabs(z) < F_APPROXIMATELY_ZERO)
	{
		z = 1.f;
	}
	size.set(x, y, z, w);

	// Compute scale as reciprocal of size
	LLVector4a scale;
	scale.splat(1.f);
	scale.div(size);
	LLVector4a inv_scale(1.f);
	inv_scale.div(scale);
	n = 0;
	while (n < models_count)
	{
		LLModel* model = model_list[n++].get();
		if (!model || model->mVolumeFaces.empty()) continue;

		for (S32 i = 0, count = model->mVolumeFaces.size(); i < count; ++i)
		{
			LLVolumeFace& face = model->mVolumeFaces[i];

			// We shrink the extents so that they fall within the unit cube.
			face.mExtents[0].add(trans);
			face.mExtents[0].mul(scale);

			face.mExtents[1].add(trans);
			face.mExtents[1].mul(scale);

			// For all the positions, we scale the positions to fit within the
			// unit cube.
			LLVector4a* pos = (LLVector4a*)face.mPositions;
			LLVector4a* norm = (LLVector4a*)face.mNormals;
			LLVector4a* t = (LLVector4a*)face.mTangents;

			for (S32 j = 0; j < face.mNumVertices; ++j)
			{
			 	pos[j].add(trans);
				pos[j].mul(scale);
				if (norm && !norm[j].equals3(LLVector4a::getZero()))
				{
					norm[j].mul(inv_scale);
					norm[j].normalize3();
				}

				if (t)
				{
					F32 w = t[j].getF32ptr()[3];
					t[j].mul(inv_scale);
					t[j].normalize3();
					t[j].getF32ptr()[3] = w;
				}
			}
		}

		// mNormalizedScale is the scale at which we would need to multiply the
		// model by to get the original size of the model instead of the
		// normalized size.
		LLVector4a normalized_scale;
		normalized_scale.splat(1.f);
		normalized_scale.div(scale);
		model->mNormalizedScale.set(normalized_scale.getF32ptr());
		model->mNormalizedTranslation.set(trans.getF32ptr());
		model->mNormalizedTranslation *= -1.f;
	}
}
#endif

// Shrink the model to fit on a 1x1x1 cube centered at the origin. The
// positions and extents multiplied by mNormalizedScale and offset by
// mNormalizedTranslation to be the "original" extents and position. Also, the
// positions will fit within the unit cube.
void LLModel::normalizeVolumeFaces()
{
	// Ensure we do not have too many faces
	if ((S32)mVolumeFaces.size() > LL_SCULPT_MESH_MAX_FACES)
	{
		mVolumeFaces.resize(LL_SCULPT_MESH_MAX_FACES);
	}

	if (!mVolumeFaces.empty())
	{
		LLVector4a min, max;

		// For all of the volume faces in the model, loop over them and see
		// what the extents of the volume along each axis.
		min = mVolumeFaces[0].mExtents[0];
		max = mVolumeFaces[0].mExtents[1];

		for (S32 i = 1, count = mVolumeFaces.size(); i < count; ++i)
		{
			LLVolumeFace& face = mVolumeFaces[i];

			update_min_max(min, max, face.mExtents[0]);
			update_min_max(min, max, face.mExtents[1]);

			if (face.mTexCoords)
			{
				LLVector2& min_tc = face.mTexCoordExtents[0];
				LLVector2& max_tc = face.mTexCoordExtents[1];

				min_tc = face.mTexCoords[0];
				max_tc = face.mTexCoords[0];

				for (S32 j = 1; j < face.mNumVertices; ++j)
				{
					update_min_max(min_tc, max_tc, face.mTexCoords[j]);
				}
			}
			else
			{
				face.mTexCoordExtents[0].set(0, 0);
				face.mTexCoordExtents[1].set(1, 1);
			}
		}

		// Now that we have the extents of the model, we can compute the offset
		// needed to center the model at the origin.

		// Compute center of the model and make it negative to get translation
		// needed to center at origin.
		LLVector4a trans;
		trans.setAdd(min, max);
		trans.mul(-0.5f);

		// Compute the total size along all axes of the model.
		LLVector4a size;
		size.setSub(max, min);

		// Prevent division by zero.
		F32 x = size[0];
		F32 y = size[1];
		F32 z = size[2];
		F32 w = size[3];
		if (fabs(x) < F_APPROXIMATELY_ZERO)
		{
			x = 1.f;
		}
		if (fabs(y) < F_APPROXIMATELY_ZERO)
		{
			y = 1.f;
		}
		if (fabs(z) < F_APPROXIMATELY_ZERO)
		{
			z = 1.f;
		}
		size.set(x, y, z, w);

		// Compute scale as reciprocal of size
		LLVector4a scale;
		scale.splat(1.f);
		scale.div(size);
		LLVector4a inv_scale(1.f);
		inv_scale.div(scale);
		for (S32 i = 0, count = mVolumeFaces.size(); i < count; ++i)
		{
			LLVolumeFace& face = mVolumeFaces[i];

			// We shrink the extents so that they fall within the unit cube.
			face.mExtents[0].add(trans);
			face.mExtents[0].mul(scale);

			face.mExtents[1].add(trans);
			face.mExtents[1].mul(scale);

			// For all the positions, we scale the positions to fit within the
			// unit cube.
			LLVector4a* pos = (LLVector4a*)face.mPositions;
			LLVector4a* norm = (LLVector4a*)face.mNormals;
			LLVector4a* t = (LLVector4a*)face.mTangents;

			for (S32 j = 0; j < face.mNumVertices; ++j)
			{
			 	pos[j].add(trans);
				pos[j].mul(scale);
				if (norm && !norm[j].equals3(LLVector4a::getZero()))
				{
					norm[j].mul(inv_scale);
					norm[j].normalize3();
				}

				if (t)
				{
					F32 w = t[j].getF32ptr()[3];
					t[j].mul(inv_scale);
					t[j].normalize3();
					t[j].getF32ptr()[3] = w;
				}
			}
		}

		// mNormalizedScale is the scale at which we would need to multiply the
		// model by to get the original size of the model instead of the
		// normalized size.
		LLVector4a normalized_scale;
		normalized_scale.splat(1.f);
		normalized_scale.div(scale);
		mNormalizedScale.set(normalized_scale.getF32ptr());
		mNormalizedTranslation.set(trans.getF32ptr());
		mNormalizedTranslation *= -1.f;

		// Remember normalized scale so original dimensions can be recovered
		// for mesh processing (i.e. tangent generation)
		for (S32 i = 0, count = mVolumeFaces.size(); i < count; ++i)
		{
			mVolumeFaces[i].mNormalizedScale = mNormalizedScale;
		}
	}
}

void LLModel::getNormalizedScaleTranslation(LLVector3& scale_out,
											LLVector3& translation_out)
{
	scale_out = mNormalizedScale;
	translation_out = mNormalizedTranslation;
}

void LLModel::setNumVolumeFaces(S32 count)
{
	mVolumeFaces.resize(count);
}

void LLModel::setVolumeFaceData(S32 f, LLStrider<LLVector3> pos,
								LLStrider<LLVector3> norm,
								LLStrider<LLVector2> tc,
								LLStrider<U16> ind, U32 num_verts,
								U32 num_indices)
{
	LLVolumeFace& face = mVolumeFaces[f];

	face.resizeVertices(num_verts);
	face.resizeIndices(num_indices);

	LLVector4a::memcpyNonAliased16((F32*)face.mPositions, (F32*)pos.get(),
								   num_verts * 4 * sizeof(F32));
	if (norm.get())
	{
		LLVector4a::memcpyNonAliased16((F32*)face.mNormals, (F32*)norm.get(),
									   num_verts * 4 * sizeof(F32));
	}
	else
	{
		// NOTE: normals are part of the same buffer as mPositions, do not free
		// them separately.
		face.mNormals = NULL;
	}

	if (tc.get())
	{
		U32 tex_size = (num_verts * 2 * sizeof(F32) + 0xF) & ~0xF;
		LLVector4a::memcpyNonAliased16((F32*)face.mTexCoords, (F32*)tc.get(),
									   tex_size);
	}
	else
	{
		// NOTE: texture coordinates are part of the same buffer as mPositions,
		// do not free them separately.
		face.mTexCoords = NULL;
	}

	U32 size = (num_indices * 2 + 0xF) & ~0xF;
	LLVector4a::memcpyNonAliased16((F32*)face.mIndices, (F32*)ind.get(), size);
}

void LLModel::addFace(const LLVolumeFace& face)
{
	if (face.mNumVertices == 0)
	{
		llerrs << "Cannot add empty face." << llendl;
	}

	mVolumeFaces.emplace_back(face);

	if (mVolumeFaces.size() > MAX_MODEL_FACES)
	{
		llerrs << "Model prims cannot have more than " << MAX_MODEL_FACES
			   << " faces !" << llendl;
	}
}

void LLModel::generateNormals(F32 angle_cutoff)
{
	// Generate normals for all faces by:
	// 1 - Create faceted copy of face with no texture coordinates
	// 2 - Weld vertices in faceted copy that are shared between triangles with
	//     less than "angle_cutoff" difference between normals
	// 3 - Generate smoothed set of normals based on welding results
	// 4 - Create faceted copy of face with texture coordinates
	// 5 - Copy smoothed normals to faceted copy, using closest normal to
	//     triangle normal where more than one normal exists for a given
	//     position
	// 6 - Remove redundant vertices from new faceted (now smooth) copy

	angle_cutoff = cosf(angle_cutoff);
	for (U32 j = 0; j < mVolumeFaces.size(); ++j)
	{
		LLVolumeFace& vol_face = mVolumeFaces[j];

		if (vol_face.mNumIndices > 65535)
		{
			llwarns << "Too many vertices for normal generation to work."
					<< llendl;
			continue;
		}

		// Create faceted copy of current face with no texture coordinates
		// (step 1)
		LLVolumeFace faceted;

		LLVector4a* src_pos = (LLVector4a*)vol_face.mPositions;
		//LLVector4a* src_norm = (LLVector4a*)vol_face.mNormals;

		faceted.resizeVertices(vol_face.mNumIndices);
		faceted.resizeIndices(vol_face.mNumIndices);
		// bake out triangles into temporary face, clearing texture coordinates
		for (S32 i = 0; i < vol_face.mNumIndices; ++i)
		{
			U32 idx = vol_face.mIndices[i];

			faceted.mPositions[i] = src_pos[idx];
			faceted.mTexCoords[i].clear();
			faceted.mIndices[i] = i;
		}

		LLVector4a lhs, rhs;
		// Generate normals for temporary face
		for (S32 i = 0; i < faceted.mNumIndices; i += 3)
		{
			// For each triangle
			U16 i0 = faceted.mIndices[i];
			U16 i1 = faceted.mIndices[i + 1];
			U16 i2 = faceted.mIndices[i + 2];

			LLVector4a& p0 = faceted.mPositions[i0];
			LLVector4a& p1 = faceted.mPositions[i1];
			LLVector4a& p2 = faceted.mPositions[i2];

			LLVector4a& n0 = faceted.mNormals[i0];
			LLVector4a& n1 = faceted.mNormals[i1];
			LLVector4a& n2 = faceted.mNormals[i2];

			lhs.setSub(p1, p0);
			rhs.setSub(p2, p0);

			n0.setCross3(lhs, rhs);
			n0.normalize3();
			n1 = n0;
			n2 = n0;
		}

		// Weld vertices in temporary face, respecting angle_cutoff (step 2)
		faceted.optimize(angle_cutoff);

		// Generate normals for welded face based on new topology (step 3)

		for (S32 i = 0; i < faceted.mNumVertices; ++i)
		{
			faceted.mNormals[i].clear();
		}

		LLVector4a n;
		for (S32 i = 0; i < faceted.mNumIndices; i += 3)
		{
			// For each triangle
			U16 i0 = faceted.mIndices[i];
			U16 i1 = faceted.mIndices[i + 1];
			U16 i2 = faceted.mIndices[i + 2];

			LLVector4a& p0 = faceted.mPositions[i0];
			LLVector4a& p1 = faceted.mPositions[i1];
			LLVector4a& p2 = faceted.mPositions[i2];

			LLVector4a& n0 = faceted.mNormals[i0];
			LLVector4a& n1 = faceted.mNormals[i1];
			LLVector4a& n2 = faceted.mNormals[i2];

			LLVector4a lhs, rhs;
			lhs.setSub(p1, p0);
			rhs.setSub(p2, p0);

			n.setCross3(lhs, rhs);

			n0.add(n);
			n1.add(n);
			n2.add(n);
		}

		// Normalize normals and build point map
		LLVolumeFace::VertexMapData::PointMap point_map;

		for (S32 i = 0; i < faceted.mNumVertices; ++i)
		{
			faceted.mNormals[i].normalize3();

			LLVolumeFace::VertexMapData v;
			v.setPosition(faceted.mPositions[i]);
			v.setNormal(faceted.mNormals[i]);

			point_map[LLVector3(v.getPosition().getF32ptr())].push_back(v);
		}

		// Create faceted copy of current face with texture coordinates
		// (step 4)
		LLVolumeFace new_face;

		// Bake out triangles into new face
		new_face.resizeIndices(vol_face.mNumIndices);
		new_face.resizeVertices(vol_face.mNumIndices);

		for (S32 i = 0; i < vol_face.mNumIndices; ++i)
		{
			U32 idx = vol_face.mIndices[i];
			LLVolumeFace::VertexData v;
			new_face.mPositions[i] = vol_face.mPositions[idx];
			new_face.mNormals[i].clear();
			new_face.mIndices[i] = i;
		}

		if (vol_face.mTexCoords)
		{
			for (S32 i = 0; i < vol_face.mNumIndices; ++i)
			{
				U32 idx = vol_face.mIndices[i];
				new_face.mTexCoords[i] = vol_face.mTexCoords[idx];
			}
		}
		else
		{
			// NOTE: texture coordinates are part of the same buffer as
			// mPositions, do not free them separately.
			new_face.mTexCoords = NULL;
		}

		// Generate normals for new face
		for (S32 i = 0; i < new_face.mNumIndices; i += 3)
		{
			// For each triangle
			U16 i0 = new_face.mIndices[i];
			U16 i1 = new_face.mIndices[i + 1];
			U16 i2 = new_face.mIndices[i + 2];

			LLVector4a& p0 = new_face.mPositions[i0];
			LLVector4a& p1 = new_face.mPositions[i1];
			LLVector4a& p2 = new_face.mPositions[i2];

			LLVector4a& n0 = new_face.mNormals[i0];
			LLVector4a& n1 = new_face.mNormals[i1];
			LLVector4a& n2 = new_face.mNormals[i2];

			LLVector4a lhs, rhs;
			lhs.setSub(p1, p0);
			rhs.setSub(p2, p0);

			n0.setCross3(lhs, rhs);
			n0.normalize3();
			n1 = n0;
			n2 = n0;
		}

		// Swap out normals in new_face with best match from point map (step 5)
		for (S32 i = 0; i < new_face.mNumVertices; ++i)
		{
			LLVolumeFace::VertexMapData::PointMap::iterator iter =
				point_map.find(LLVector3(new_face.mPositions[i].getF32ptr()));
			if (iter != point_map.end())
			{
				LLVector4a ref_norm = new_face.mNormals[i];
				F32 best = -2.f;
				for (S32 k = 0, count = iter->second.size(); k < count; ++k)
				{
					LLVector4a& n = iter->second[k].getNormal();

					F32 cur = n.dot3(ref_norm).getF32();
					if (cur > best)
					{
						best = cur;
						new_face.mNormals[i] = n;
					}
				}
			}
		}

		// Remove redundant vertices from new face (step 6)
		new_face.optimize();

		mVolumeFaces[j] = new_face;
	}
}

// Used to be a validate_model(const LLModel* mdl) global function. HB
bool LLModel::validate(bool check_nans) const
{
	S32 count = getNumVolumeFaces();
	if (count <= 0)
	{
		llwarns << "Model has no faces !" << llendl;
		return false;
	}

	for (S32 i = 0; i < count; ++i)
	{
		const LLVolumeFace& vol_face = getVolumeFace(i);

		if (vol_face.mNumVertices == 0)
		{
			llwarns << "Face has no vertices." << llendl;
			return false;
		}

		if (vol_face.mNumIndices == 0)
		{
			llwarns << "Face has no indices." << llendl;
			return false;
		}

        if (!vol_face.validate(check_nans))
		{
			return false;
		}
	}

	return true;
}

//static
LLSD LLModel::writeModel(std::ostream& ostr, LLModel* physics, LLModel* high,
						 LLModel* medium, LLModel* low, LLModel* impostor,
						 const LLModel::Decomposition& decomp,
						 bool upload_skin, bool upload_joints,
						 bool lock_scale_if_joint_position,
						 bool nowrite, bool as_slm, S32 submodel_id)
{
	LLSD mdl;

	LLModel* model[] =
	{
		impostor,
		low,
		medium,
		high,
		physics
	};

	bool skinning = upload_skin && high && !high->mSkinWeights.empty();
	if (skinning)
	{
		// Write skinning block
		mdl["skin"] = high->mSkinInfo.asLLSD(upload_joints,
											 lock_scale_if_joint_position);
	}

	if (!decomp.mBaseHull.empty() || !decomp.mHull.empty())
	{
		mdl["physics_convex"] = decomp.asLLSD();
		if (!decomp.mHull.empty() && !as_slm)
		{
			// Convex decomposition exists, physics mesh will not be used
			// (unless this is an slm file)
			model[LLModel::LOD_PHYSICS] = NULL;
		}
	}
	else if (submodel_id)
	{
		const LLModel::Decomposition fake_decomp;
		mdl["secondary"] = true;
        mdl["submodel_id"] = submodel_id;
		mdl["physics_convex"] = fake_decomp.asLLSD();
		model[LLModel::LOD_PHYSICS] = NULL;
	}

	if (as_slm)
	{
		// Save material list names
		for (U32 i = 0; i < high->mMaterialList.size(); ++i)
		{
			mdl["material_list"][i] = high->mMaterialList[i];
		}
	}

	for (S32 idx = 0; idx < MODEL_NAMES_LENGTH; ++idx)
	{
		LLModel* modelp = model[idx];
		if (!modelp || !modelp->getNumVolumeFaces() ||
			!modelp->getVolumeFace(0).mPositions)
		{
			llwarns << "Invalid model at index " << idx << ". Skipping."
					<< llendl;
			continue;
		}

		LLVector3 min_pos(modelp->getVolumeFace(0).mPositions[0].getF32ptr());
		LLVector3 max_pos = min_pos;

		// Find position domain
		for (S32 i = 0; i < modelp->getNumVolumeFaces(); ++i)
		{
			const LLVolumeFace& face = modelp->getVolumeFace(i);
			for (S32 j = 0; j < face.mNumVertices; ++j)
			{
				update_min_max(min_pos, max_pos,
							   face.mPositions[j].getF32ptr());
			}
		}

		LLVector3 pos_range = max_pos - min_pos;

		for (S32 i = 0; i < modelp->getNumVolumeFaces(); ++i)
		{
			const LLVolumeFace& face = modelp->getVolumeFace(i);
			if (face.mNumVertices < 3)
			{
				// Do not export an empty face
				mdl[model_names[idx]][i]["NoGeometry"] = true;
				continue;
			}

			S32 vertices = face.mNumVertices;
			LLSD::Binary verts(vertices * 6);
			LLSD::Binary tc(vertices * 4);
			LLSD::Binary normals(vertices * 6);
			LLSD::Binary indices(face.mNumIndices * 2);
#if LL_USE_TANGENTS
			LLSD::Binary tangents(face.mNumVertices * 8);
#endif

			LLVector2* ftc = (LLVector2*)face.mTexCoords;
			LLVector2 min_tc;
			LLVector2 max_tc;
			if (ftc)
			{
				min_tc = ftc[0];
				max_tc = min_tc;

				// Get texture coordinate domain
				for (S32 j = 0; j < vertices; ++j)
				{
					update_min_max(min_tc, max_tc, ftc[j]);
				}
			}

			U32 vert_idx = 0;
			U32 norm_idx = 0;
			U32 tc_idx = 0;
#if LL_USE_TANGENTS
			U32 tan_idx = 0;
#endif
			LLVector2 tc_range = max_tc - min_tc;
			for (S32 j = 0; j < vertices; ++j)
			{
				// For each vertex...
				F32* pos = face.mPositions[j].getF32ptr();

				// Position
				for (U32 k = 0; k < 3; ++k)
				{
					// For each component...
					// Convert to 16-bit normalized across domain
					U16 val = (U16)((pos[k] - min_pos.mV[k]) /
									pos_range.mV[k] * 65535);

					// Write to binary buffer
					U8* buff = (U8*)&val;
					verts[vert_idx++] = buff[0];
					verts[vert_idx++] = buff[1];
				}

				if (face.mNormals)
				{
					F32* norm = face.mNormals[j].getF32ptr();

					for (U32 k = 0; k < 3; ++k)
					{
						// For each component convert to 16 bits normalized
						constexpr F32 norm_factor = 0.5f * 65535.f;
						U16 val = (U16)((norm[k] + 1.f) * norm_factor);
						U8* buff = (U8*)&val;

						// Write to binary buffer
						normals[norm_idx++] = buff[0];
						normals[norm_idx++] = buff[1];
					}
				}

#if LL_USE_TANGENTS
				if (face.mTangents)
				{
					F32* tangent = face.mTangents[j].getF32ptr();
					for (U32 k = 0; k < 4; ++k)
					{
						// For each component...
						// Convert to 16-bit normalized
						U16 val = (U16)((tangent[k] +1.f) * 0.5f * 65535.f);

						// Write to binary buffer
						U8* buff = (U8*)&val;
						tangents[tan_idx++] = buff[0];
						tangents[tan_idx++] = buff[1];
					}
					
				}
#endif

				if (face.mTexCoords)
				{
					F32* src_tc = (F32*)face.mTexCoords[j].mV;
					for (U32 k = 0; k < 2; ++k)
					{
						// For each component...
						// Convert to 16-bit normalized
						U16 val = (U16)((src_tc[k] - min_tc.mV[k]) /
										tc_range.mV[k] * 65535.f);

						// Write to binary buffer
						U8* buff = (U8*)&val;
						tc[tc_idx++] = buff[0];
						tc[tc_idx++] = buff[1];
					}
				}
			}

			for (S32 j = 0, idx_idx = 0; j < face.mNumIndices; ++j)
			{
				U8* buff = (U8*)&(face.mIndices[j]);
				indices[idx_idx++] = buff[0];
				indices[idx_idx++] = buff[1];
			}

			// Write out face data
			mdl[model_names[idx]][i]["PositionDomain"]["Min"] =
				min_pos.getValue();
			mdl[model_names[idx]][i]["PositionDomain"]["Max"] =
				max_pos.getValue();
			mdl[model_names[idx]][i]["NormalizedScale"] =
				face.mNormalizedScale.getValue();
			mdl[model_names[idx]][i]["Position"] = verts;

			if (face.mNormals)
			{
				mdl[model_names[idx]][i]["Normal"] = normals;
			}

#if LL_USE_TANGENTS
			if (face.mTangents)
			{
				mdl[model_names[idx]][i]["Tangent"] = tangents;
			}
#endif

			if (face.mTexCoords)
			{
				mdl[model_names[idx]][i]["TexCoord0Domain"]["Min"] =
					min_tc.getValue();
				mdl[model_names[idx]][i]["TexCoord0Domain"]["Max"] =
					max_tc.getValue();
				mdl[model_names[idx]][i]["TexCoord0"] = tc;
			}

			mdl[model_names[idx]][i]["TriangleList"] = indices;

			if (skinning)
			{
				if (!modelp->mSkinWeights.empty())
				{
					// Write out skin weights

					// Each influence list entry is up to four 24 bits values:
					// first 8 bits is bone index, last 16 bits is bone
					// influence weight; a bone index of 0xFF signifies no more
					// influences for this vertex.

					std::stringstream ostr;

					for (S32 j = 0; j < vertices; ++j)
					{
						LLVector3 pos(face.mPositions[j].getF32ptr());

						weight_list& weights =
							model[idx]->getJointInfluences(pos);

						S32 count = 0;
						for (weight_list::iterator iter = weights.begin();
							 iter != weights.end(); ++iter)
						{
							if (iter->mJointIdx < 255 &&
								iter->mJointIdx >= 0)
							{
								U8 idx = (U8)iter->mJointIdx;
								ostr.write((const char*)&idx, 1);

								U16 influence = (U16)(iter->mWeight * 65535);
								ostr.write((const char*)&influence, 2);

								++count;
							}
						}
						U8 end_list = 0xFF;
						if (count < 4)
						{
							ostr.write((const char*)&end_list, 1);
						}
					}

					// Copy ostr to binary buffer
					std::string data = ostr.str();
					const U8* buff = (U8*)data.data();
					U32 bytes = data.size();

					LLSD::Binary w(bytes);
					for (U32 j = 0; j < bytes; ++j)
					{
						w[j] = buff[j];
					}

					mdl[model_names[idx]][i]["Weights"] = w;
				}
				else if (idx != LLModel::LOD_PHYSICS)
				{
					llwarns << "Attempting to use skinning without having skin weights"
							<< llendl;
				}
			}
		}
	}

	return writeModelToStream(ostr, mdl, nowrite, as_slm);
}

LLSD LLModel::writeModelToStream(std::ostream& ostr, LLSD& mdl, bool nowrite,
								 bool as_slm)
{
	std::string::size_type cur_offset = 0;

	LLSD header;

	if (as_slm && mdl.has("material_list"))
	{
		// Save material binding names to header
		header["material_list"] = mdl["material_list"];
	}

	std::string skin;

	if (mdl.has("skin"))
	{
		// write out skin block
		skin = zip_llsd(mdl["skin"]);

		U32 size = skin.size();
		if (size > 0)
		{
			header["skin"]["offset"] = (LLSD::Integer)cur_offset;
			header["skin"]["size"] = (LLSD::Integer)size;
			cur_offset += size;
		}
	}

	std::string decomposition;

	if (mdl.has("physics_convex"))
	{
		// Write out convex decomposition
		decomposition = zip_llsd(mdl["physics_convex"]);

		U32 size = decomposition.size();
		if (size > 0)
		{
			header["physics_convex"]["offset"] = (LLSD::Integer)cur_offset;
			header["physics_convex"]["size"] = (LLSD::Integer)size;
			cur_offset += size;
		}
	}

	if (mdl.has("submodel_id"))
	{
		// Xrite out submodel id
		header["submodel_id"] = (LLSD::Integer)mdl["submodel_id"];
	}

	std::string out[MODEL_NAMES_LENGTH];

	for (S32 i = 0; i < MODEL_NAMES_LENGTH; i++)
	{
		if (mdl.has(model_names[i]))
		{
			out[i] = zip_llsd(mdl[model_names[i]]);

			U32 size = out[i].size();

			header[model_names[i]]["offset"] = (LLSD::Integer)cur_offset;
			header[model_names[i]]["size"] = (LLSD::Integer)size;
			cur_offset += size;
		}
	}

	if (!nowrite)
	{
		LLSDSerialize::toBinary(header, ostr);

		if (!skin.empty())
		{
			// Write skin block
			ostr.write((const char*)skin.data(),
					   header["skin"]["size"].asInteger());
		}

		if (!decomposition.empty())
		{
			// Write decomposition block
			ostr.write((const char*)decomposition.data(),
					   header["physics_convex"]["size"].asInteger());
		}

		for (S32 i = 0; i < MODEL_NAMES_LENGTH; i++)
		{
			if (!out[i].empty())
			{
				ostr.write((const char*)out[i].data(),
						   header[model_names[i]]["size"].asInteger());
			}
		}
	}

	return header;
}

LLModel::weight_list& LLModel::getJointInfluences(const LLVector3& pos)
{
	// 1. If a vertex has been weighted then we will find it via pos and return
	// its weight list
	for (weight_map::iterator it = mSkinWeights.begin(),
							  end = mSkinWeights.end();
		 it != end; ++it)
	{
		if (jointPositionalLookup(it->first, pos))
		{
			return it->second;
		}
	}

	// 2. Otherwise we will use the older implementation
	weight_map::iterator iter = mSkinWeights.find(pos);

	if (iter != mSkinWeights.end())
	{
		if ((iter->first - pos).length() <= 0.1f)
		{
			return iter->second;
		}
		llwarns << "Could not find weight list for matching joint !  This is an error !"
				<< llendl;
		llassert(false);
		// For release viewers, fall back to something acceptable instead
		// of crashing...
	}

	// No exact match found, get closest point
	constexpr F32 epsilon = 1e-5f;
	weight_map::iterator iter_down;
	weight_map::iterator iter_up = mSkinWeights.lower_bound(pos);
	if (iter_up == mSkinWeights.end())
	{
		iter_down = iter_up--;
	}
	else
	{
		iter_down = ++iter_up;
	}

	weight_map::iterator best = iter_up;

	F32 min_dist = (best->first - pos).length();

	// Search up and down mSkinWeights from lower bound of pos until a match is
	// found within epsilon. If no match is found within epsilon, return
	// closest match.
	bool done = false;
	while (!done)
	{
		done = true;
		if (iter_up != mSkinWeights.end() && ++iter_up != mSkinWeights.end())
		{
			done = false;
			F32 dist = (iter_up->first - pos).length();
			if (dist < epsilon)
			{
				return iter_up->second;
			}
			if (dist < min_dist)
			{
				best = iter_up;
				min_dist = dist;
			}
		}

		if (iter_down != mSkinWeights.begin() &&
			--iter_down != mSkinWeights.begin())
		{
			done = false;

			F32 dist = (iter_down->first - pos).length();
			if (dist < epsilon)
			{
				return iter_down->second;
			}
			if (dist < min_dist)
			{
				best = iter_down;
				min_dist = dist;
			}
		}
	}

	return best->second;
}

void LLModel::setConvexHullDecomposition(const LLModel::hull_decomp& decomp)
{
	mPhysics.mHull = decomp;
	mPhysics.mMesh.clear();
	updateHullCenters();
}

void LLModel::updateHullCenters()
{
	mHullCenter.resize(mPhysics.mHull.size());
	mHullPoints = 0;
	mCenterOfHullCenters.clear();

	for (U32 i = 0, count = mPhysics.mHull.size(); i < count; ++i)
	{
		U32 count2 = mPhysics.mHull[i].size();

		LLVector3 cur_center;
		for (U32 j = 0; j < count2; ++j)
		{
			cur_center += mPhysics.mHull[i][j];
		}
		mCenterOfHullCenters += cur_center;
		cur_center *= 1.f / count2;
		mHullCenter[i] = cur_center;
		mHullPoints += count2;
	}

	if (mHullPoints > 0)
	{
		mCenterOfHullCenters *= 1.f / mHullPoints;
		llassert(mPhysics.hasHullList());
	}
}

bool LLModel::loadModel(std::istream& is)
{
	mSculptLevel = -1;  // default is an error occured

	LLSD header;
	{
		if (!LLSDSerialize::fromBinary(header, is, 1024 * 1024 * 1024))
		{
			llwarns << "Mesh header parse error. Not a valid mesh asset !"
					<< llendl;
			return false;
		}
	}

	if (header.has("material_list"))
	{
		// Load material list names
		mMaterialList.clear();
		for (S32 i = 0, count = header["material_list"].size(); i < count; ++i)
		{
			mMaterialList.emplace_back(header["material_list"][i].asString());
		}
	}

	mSubmodelID = header.has("submodel_id") ? header["submodel_id"].asInteger()
											: 0;

	// 4 mesh LODs (from 0 to 3) + 1 physical (4)
	constexpr S32 MODEL_MAX_LOD = 4;
	S32 lod = llclamp((S32)mDetail, 0, MODEL_MAX_LOD);

	if (header[model_names[lod]]["offset"].asInteger() == -1 ||
		header[model_names[lod]]["size"].asInteger() == 0)
	{
		// Cannot load requested LOD
		llwarns << "LoD data is invalid !" << llendl;
		return false;
	}

	bool has_skin = header["skin"]["offset"].asInteger() >=0 &&
					header["skin"]["size"].asInteger() > 0;

	if (lod == LLModel::LOD_HIGH && !mSubmodelID)
	{
		// Try to load skin info and decomp info
		std::ios::pos_type cur_pos = is.tellg();
		loadSkinInfo(header, is);
		is.seekg(cur_pos);
	}

	if ((lod == LLModel::LOD_HIGH || lod == LLModel::LOD_PHYSICS) &&
		!mSubmodelID)
	{
		std::ios::pos_type cur_pos = is.tellg();
		loadDecomposition(header, is);
		is.seekg(cur_pos);
	}

	is.seekg(header[model_names[lod]]["offset"].asInteger(), std::ios_base::cur);

	if (unpackVolumeFaces(is, header[model_names[lod]]["size"].asInteger()))
	{
		if (has_skin)
		{
			// Build out mSkinWeight from face info
			for (S32 i = 0; i < getNumVolumeFaces(); ++i)
			{
				const LLVolumeFace& face = getVolumeFace(i);

				if (face.mWeights)
				{
					for (S32 j = 0; j < face.mNumVertices; ++j)
					{
						LLVector4a& w = face.mWeights[j];

						std::vector<JointWeight> wght;
						for (S32 k = 0; k < 4; ++k)
						{
							S32 idx = (S32)w[k];
							F32 f = w[k] - idx;
							if (f > 0.f)
							{
								wght.emplace_back(idx, f);
							}
						}

						if (!wght.empty())
						{
							LLVector3 pos(face.mPositions[j].getF32ptr());
							mSkinWeights[pos] = wght;
						}
					}
				}
			}
		}
		return true;
	}
	else
	{
		llwarns << "Volume faces unpacking failed !" << llendl;
	}

	return false;
}

bool LLModel::isMaterialListSubset(LLModel* ref)
{
	if (!ref) return false;

	U32 model_count = mMaterialList.size();
	U32 ref_count = ref->mMaterialList.size();
	if (model_count > ref_count)
	{
		// This model cannot be a strict subset if it has more materials
		// than the reference.
		return false;
	}

	for (U32 src = 0; src < model_count; ++src)
	{
		bool found = false;

		for (U32 dst = 0; dst < ref_count; ++dst)
		{
			found = mMaterialList[src] == ref->mMaterialList[dst];
			if (found)
			{
				break;
			}
		}

		if (!found)
		{
			llwarns << "Could not find material " << mMaterialList[src]
					<< " in reference model " << ref->mLabel << llendl;
			return false;
		}
	}

	return true;
}

#if 0	// Not used
bool LLModel::needToAddFaces(LLModel* ref, S32& ref_face_cnt,
							 S32& mdl_face_cnt)
{
	bool changed = false;
	if (ref_face_cnt < mdl_face_cnt)
	{
		ref_face_cnt += mdl_face_cnt - ref_face_cnt;
		changed = true;
	}
	else if (mdl_face_cnt < ref_face_cnt)
	{
		mdl_face_cnt += ref_face_cnt - mdl_face_cnt;
		changed = true;
	}

	return changed;
}
#endif

#if 0	// Moved to llfloatermodelpreview.cpp
bool LLModel::matchMaterialOrder(LLModel* ref, S32& ref_face_cnt,
								 S32& mdl_face_cnt)
{
	// Is this a subset ?
	// LODs cannot currently add new materials, e.g.
	// 1. ref = a,b,c lod1 = d,e => This is not permitted
	// 2. ref = a,b,c lod1 = c => This would be permitted
	if (!isMaterialListSubset(ref))
	{
		llinfos << "Material of model is not a subset of reference." << llendl;
		return false;
	}

	if (mMaterialList.size() > ref->mMaterialList.size())
	{
		// We passed isMaterialListSubset, so materials are a subset, but a
		// subset is not supposed to be larger than original and if we keep
		// going, reordering will cause a crash.
		llinfos << "Material of model has more materials than a reference."
				<< llendl;
		return false;		
	}

	std::map<std::string, U32> index_map;

	// Build a map of material slot names to face indexes
	bool reorder = false;
	std::set<std::string> base_mat;
	std::set<std::string> cur_mat;

	for (U32 i = 0; i < mMaterialList.size(); ++i)
	{
		index_map[ref->mMaterialList[i]] = i;
		// If any material name does not match reference, we need to reorder
		reorder |= ref->mMaterialList[i] != mMaterialList[i];
		base_mat.insert(ref->mMaterialList[i]);
		cur_mat.insert(mMaterialList[i]);
	}

	if (reorder &&
		// Do not reorder if material name sets do not match
		base_mat == cur_mat)
	{
		std::vector<LLVolumeFace> new_face_list;
		new_face_list.resize(mVolumeFaces.size());

		std::vector<std::string> new_material_list;
		new_material_list.resize(mMaterialList.size());

		U32 faces_count = mVolumeFaces.size();

		// Rebuild face list so materials have the same order as the reference
		// model
		for (U32 i = 0, count = mMaterialList.size(); i < count; ++i)
		{
			U32 ref_idx = index_map[mMaterialList[i]];

			if (i < faces_count)
			{
				new_face_list[ref_idx] = mVolumeFaces[i];
			}
			new_material_list[ref_idx] = mMaterialList[i];
		}

		llassert(new_material_list == ref->mMaterialList);

		mVolumeFaces = new_face_list;
	}

	// Override material list with reference model ordering
	mMaterialList = ref->mMaterialList;

	return true;
}
#endif

bool LLModel::loadSkinInfo(const LLSD& header, std::istream& is)
{
	S32 offset = header["skin"]["offset"].asInteger();
	S32 size = header["skin"]["size"].asInteger();

	if (offset >= 0 && size > 0)
	{
		is.seekg(offset, std::ios_base::cur);

		LLSD skin_data;

		if (unzip_llsd(skin_data, is, size))
		{
			mSkinInfo.fromLLSD(skin_data);
			return true;
		}
	}

	return false;
}

bool LLModel::loadDecomposition(const LLSD& header, std::istream& is)
{
	S32 offset = header["physics_convex"]["offset"].asInteger();
	S32 size = header["physics_convex"]["size"].asInteger();

	if (offset >= 0 && size > 0 && !mSubmodelID)
	{
		is.seekg(offset, std::ios_base::cur);

		LLSD data;

		if (unzip_llsd(data, is, size))
		{
			mPhysics.fromLLSD(data);
			updateHullCenters();
		}
	}

	return true;
}

LLModel::Decomposition::Decomposition(const LLSD& data)
{
	fromLLSD(data);
}

LLModel::Decomposition::Decomposition(const LLSD& data, const LLUUID& mesh_id)
:	mMeshID(mesh_id)
{
	fromLLSD(data);
}

void LLModel::Decomposition::fromLLSD(const LLSD& decomp)
{
	if (decomp.has("HullList") && decomp.has("Positions"))
	{
		const LLSD::Binary& hulls = decomp["HullList"].asBinary();
		const LLSD::Binary& position = decomp["Positions"].asBinary();

		U16* p = (U16*)&position[0];

		mHull.resize(hulls.size());

		LLVector3 min;
		LLVector3 max;
		LLVector3 range;

		if (decomp.has("Min"))
		{
			min.setValue(decomp["Min"]);
			max.setValue(decomp["Max"]);
		}
		else
		{
			min.set(-0.5f, -0.5f, -0.5f);
			max.set(0.5f, 0.5f, 0.5f);
		}

		range = max-min;

		for (U32 i = 0; i < hulls.size(); ++i)
		{
			U16 count = hulls[i] == 0 ? 256 : hulls[i];

			std::set<U64> valid;

			// Each hull must contain at least 4 unique points

			for (U32 j = 0; j < count; ++j)
			{
				U64 test = (U64)p[0] | ((U64)p[1] << 16) | ((U64)p[2] << 32);
				// Point must be unique
				//llassert(valid.find(test) == valid.end());
				valid.insert(test);

				mHull[i].emplace_back((F32)p[0] / 65535.f * range.mV[0] + min.mV[0],
									  (F32)p[1] / 65535.f * range.mV[1] + min.mV[1],
									  (F32)p[2] / 65535.f * range.mV[2] + min.mV[2]);
				p += 3;

			}
		}
	}

	if (decomp.has("BoundingVerts"))
	{
		const LLSD::Binary& position = decomp["BoundingVerts"].asBinary();

		U16* p = (U16*)&position[0];

		LLVector3 min;
		LLVector3 max;
		LLVector3 range;

		if (decomp.has("Min"))
		{
			min.setValue(decomp["Min"]);
			max.setValue(decomp["Max"]);
		}
		else
		{
			min.set(-0.5f, -0.5f, -0.5f);
			max.set(0.5f, 0.5f, 0.5f);
		}

		range = max - min;

		U32 count = position.size() / 6;
		for (U32 j = 0; j < count; ++j)
		{
			mBaseHull.emplace_back((F32)p[0] / 65535.f * range.mV[0] + min.mV[0],
								   (F32)p[1] / 65535.f * range.mV[1] + min.mV[1],
								   (F32)p[2] / 65535.f * range.mV[2] + min.mV[2]);
			p += 3;
		}
	}
	else
	{
		// Empty base hull mesh to indicate decomposition has been loaded but
		// contains no base hull
		mBaseHullMesh.clear();
	}
}

bool LLModel::Decomposition::hasHullList() const
{
	return !mHull.empty();
}

LLSD LLModel::Decomposition::asLLSD() const
{
	LLSD ret;

	if (mBaseHull.empty() && mHull.empty())
	{
		// Nothing to write
		return ret;
	}

	// Write decomposition block
	// ["physics_convex"]["HullList"] -- list of 8 bit integers, each entry
	// represents a hull with specified number of points
	// ["physics_convex"]["Position"] -- list of 16-bit integers to be decoded
	// to given domain, encoded 3D points
	// ["physics_convex"]["BoundingVerts"] -- list of 16-bit integers to be
	// decoded to given domain, encoded 3D points representing a single hull
	// approximation of given shape

	// Get minimum and maximum
	LLVector3 min;

	if (mHull.empty())
	{
		min = mBaseHull[0];
	}
	else
	{
		min = mHull[0][0];
	}

	LLVector3 max = min;

	LLSD::Binary hulls(mHull.size());

	U32 total = 0;

	for (U32 i = 0; i < mHull.size(); ++i)
	{
		U32 size = mHull[i].size();
		total += size;
		hulls[i] = (U8)size;

		for (U32 j = 0; j < mHull[i].size(); ++j)
		{
			update_min_max(min, max, mHull[i][j]);
		}
	}

	for (U32 i = 0; i < mBaseHull.size(); ++i)
	{
		update_min_max(min, max, mBaseHull[i]);
	}

	ret["Min"] = min.getValue();
	ret["Max"] = max.getValue();

	LLVector3 range = max-min;

	if (!hulls.empty())
	{
		ret["HullList"] = hulls;
	}

	if (total > 0)
	{
		LLSD::Binary p(total * 6);

		U32 vert_idx = 0;

		for (U32 i = 0; i < mHull.size(); ++i)
		{
			std::set<U64> valid;

			llassert(!mHull[i].empty());

			for (U32 j = 0; j < mHull[i].size(); ++j)
			{
				U64 test = 0;
				const F32* src = mHull[i][j].mV;

				for (U32 k = 0; k < 3; k++)
				{
					// Convert to 16-bit normalized across domain
					U16 val =
						(U16)(((src[k] - min.mV[k]) / range.mV[k]) * 65535);

					if (valid.size() < 3)
					{
						switch (k)
						{
							case 0: test = test | (U64)val; break;
							case 1: test = test | ((U64)val << 16); break;
							case 2: test = test | ((U64)val << 32); break;
						};

						valid.insert(test);
					}

					// Write to binary buffer
					U8* buff = (U8*)&val;
					p[vert_idx++] = buff[0];
					p[vert_idx++] = buff[1];

					// Make sure we have not run off the end of the array
					llassert(vert_idx <= p.size());
				}
			}

			// Must have at least 3 unique points
			llassert(valid.size() > 2);
		}

		ret["Positions"] = p;
	}

	if (!mBaseHull.empty())
	{
		LLSD::Binary p(mBaseHull.size() * 6);

		U32 vert_idx = 0;
		for (U32 j = 0; j < mBaseHull.size(); ++j)
		{
			const F32* v = mBaseHull[j].mV;

			for (U32 k = 0; k < 3; k++)
			{
				// Convert to 16-bit normalized across domain
				U16 val = (U16)(((v[k] - min.mV[k]) / range.mV[k]) * 65535);

				U8* buff = (U8*)&val;
				//write to binary buffer
				p[vert_idx++] = buff[0];
				p[vert_idx++] = buff[1];

				if (vert_idx > p.size())
				{
					llerrs << "Index out of bounds" << llendl;
				}
			}
		}

		ret["BoundingVerts"] = p;
	}

	return ret;
}

void LLModel::Decomposition::merge(const LLModel::Decomposition* rhs)
{
	if (!rhs)
	{
		return;
	}

	if (mMeshID != rhs->mMeshID)
	{
		llerrs << "Attempted to merge with decomposition of some other mesh."
			   << llendl;
	}

	if (mBaseHull.empty())
	{
		// Take base hull and decomposition from rhs
		mHull = rhs->mHull;
		mBaseHull = rhs->mBaseHull;
		mMesh = rhs->mMesh;
		mBaseHullMesh = rhs->mBaseHullMesh;
	}

	if (mPhysicsShapeMesh.empty())
	{
		// Take physics shape mesh from rhs
		mPhysicsShapeMesh = rhs->mPhysicsShapeMesh;
	}
}

LLModelInstance::LLModelInstance(const LLSD& data)
:	LLModelInstanceBase()
{
	mLocalMeshID = data["mesh_id"].asInteger();
	mLabel = data["label"].asString();
	mTransform.setValue(data["transform"]);

	for (U32 i = 0, count = data["material"].size(); i < count; ++i)
	{
		LLImportMaterial mat(data["material"][i]);
		mMaterial[mat.mBinding] = mat;
	}
}

LLSD LLModelInstance::asLLSD()
{
	LLSD ret;
	ret["mesh_id"] = mModel->mLocalID;
	ret["label"] = mLabel;
	ret["transform"] = mTransform.getValue();

	U32 i = 0;
	for (std::map<std::string, LLImportMaterial>::iterator
			iter = mMaterial.begin(), end = mMaterial.end();
		 iter != end; ++iter)
	{
		ret["material"][i++] = iter->second.asLLSD();
	}

	return ret;
}

LLImportMaterial::LLImportMaterial(const LLSD& data)
{
	mDiffuseMapFilename = data["diffuse"]["filename"].asString();
	mDiffuseMapLabel = data["diffuse"]["label"].asString();
	mDiffuseColor.setValue(data["diffuse"]["color"]);
	mFullbright = data["fullbright"].asBoolean();
	mBinding = data["binding"].asString();
}

LLSD LLImportMaterial::asLLSD()
{
	LLSD ret;
	ret["diffuse"]["filename"] = mDiffuseMapFilename;
	ret["diffuse"]["label"] = mDiffuseMapLabel;
	ret["diffuse"]["color"] = mDiffuseColor.getValue();
	ret["fullbright"] = mFullbright;
	ret["binding"] = mBinding;

	return ret;
}

bool LLImportMaterial::operator<(const LLImportMaterial& rhs) const
{
	if (mDiffuseMapID != rhs.mDiffuseMapID)
	{
		return mDiffuseMapID < rhs.mDiffuseMapID;
	}

	if (mDiffuseMapFilename != rhs.mDiffuseMapFilename)
	{
		return mDiffuseMapFilename < rhs.mDiffuseMapFilename;
	}

	if (mDiffuseMapLabel != rhs.mDiffuseMapLabel)
	{
		return mDiffuseMapLabel < rhs.mDiffuseMapLabel;
	}

	if (mDiffuseColor != rhs.mDiffuseColor)
	{
		return mDiffuseColor < rhs.mDiffuseColor;
	}

	if (mBinding != rhs.mBinding)
	{
		return mBinding < rhs.mBinding;
	}

	return mFullbright < rhs.mFullbright;
}
