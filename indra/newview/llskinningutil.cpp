/**
* @file llskinningutil.cpp
* @brief  Functions for mesh object skinning
* @author vir@lindenlab.com
*
* $LicenseInfo:firstyear=2015&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2015, Linden Research, Inc.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2.1 of the License only.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
*/

#include "llviewerprecompiledheaders.h"

#include "llskinningutil.h"

#include "llthread.h"

#include "llmeshrepository.h"
#include "llvoavatar.h"

#define OPTIMIZED 1

//static
void LLSkinningUtil::scrubInvalidJoints(LLVOAvatar* avatar,
										LLMeshSkinInfo* skin)
{
	// Skip if already done.
	if (!skin || !avatar || skin->mInvalidJointsScrubbed)
	{
		return;
	}

	// NOTE: do NOT use OpenMP here. Even with the is_main_thread() check,
	// you would get a crash in avatar->getJoint(skin->mJointKeys[j]). HB
	for (U32 j = 0, count = skin->mJointKeys.size(); j < count; ++j)
	{
		// Fix invalid to pelvis joint. Currently meshes with invalid names
		// will be blocked on upload, so this is just needed for handling of
		// any legacy bad data.
		LLJoint* joint = avatar->getJoint(skin->mJointKeys[j]);
		// Check against joint num is needed to catch some special joints
		// like mRoot.
		if (!joint || joint->getJointNum() < 0)
		{
			LL_DEBUGS("Avatar") << "Mesh rigged to invalid joint"
								<< skin->mJointNames[j] << LL_ENDL;
			skin->mJointKeys[j] = LL_JOINT_KEY_PELVIS;
			skin->mJointNames[j] = "mPelvis";
		}
	}

	skin->mInvalidJointsScrubbed = true;
}

//static
U32 LLSkinningUtil::initSkinningMatrixPalette(LLMatrix4a* mat,
											  const LLMeshSkinInfo* skin,
											  LLVOAvatar* avatar)
{
	if (!mat || !skin || !avatar) return 0;

	U32 count = llmin(LL_MAX_JOINTS_PER_MESH_OBJECT,
					  (U32)skin->mJointKeys.size());

	LLMatrix4a bind, world;
#if LL_OPENMP
	// NOTE: we cannot use OpenMP when called from the mesh repository which is
	// itself a (p)thread (pthread and OpenMP threads are incompatible)... HB
	if (is_main_thread())
	{
#		pragma omp parallel for private(bind, world)
		// NOTE: VS2017 OpenMP requires a signed integer loop index... HB
		for (S32 j = 0; j < (S32)count; ++j)
		{
			LLJoint* joint = avatar->getJoint(skin->mJointKeys[j]);
			if (joint)
			{
				bind.loadu(skin->mInvBindMatrix[j]);
				world.loadu(joint->getWorldMatrix());
				mat[j].matMul(bind, world);
			}
			else
			{
				// This should not happen; in mesh upload, skinned rendering
				// should be disabled unless all joints are valid. In other
				// cases of skinned rendering, invalid joints should already
				// have been removed during remap.
				llwarns_once << "Rigged to invalid joint name: "
							 << skin->mJointNames[j] << llendl;
				mat[j].loadu(skin->mInvBindMatrix[j]);
			}
		}
	}
	else
#endif
	{
		for (U32 j = 0; j < count; ++j)
		{
			LLJoint* joint = avatar->getJoint(skin->mJointKeys[j]);
			if (joint)
			{
				bind.loadu(skin->mInvBindMatrix[j]);
				world.loadu(joint->getWorldMatrix());
				mat[j].matMul(bind, world);
			}
			else
			{
				// This should not happen; in mesh upload, skinned rendering
				// should be disabled unless all joints are valid. In other
				// cases of skinned rendering, invalid joints should already
				// have been removed during remap.
				llwarns_once << "Rigged to invalid joint name: "
							 << skin->mJointNames[j] << llendl;
				mat[j].loadu(skin->mInvBindMatrix[j]);
			}
		}
	}

	return count;
}

//static
void LLSkinningUtil::checkSkinWeights(const LLVector4a* weights,
									  U32 num_vertices,
									  const LLMeshSkinInfo* skin)
{
#ifdef LL_DEBUG
	const S32 max_joints = skin->mJointKeys.size();
	for (U32 j = 0; j < num_vertices; ++j)
	{
		const F32* w = weights[j].getF32ptr();
		F32 wsum = 0.f;
		for (U32 k = 0; k < 4; ++k)
		{
			S32 i = llfloor(w[k]);
			llassert(i >= 0 && i < max_joints);
			wsum += w[k] - i;
		}
		llassert(wsum > 0.f);
	}
#endif
}

//static
void LLSkinningUtil::scrubSkinWeights(LLVector4a* weights, U32 num_vertices,
									  const LLMeshSkinInfo* skin)
{
	const S32 max_joints = skin->mJointNames.size() - 1;
#if LL_OPENMP
	// NOTE: we cannot use OpenMP when called from the mesh repository which is
	// itself a (p)thread (pthread and OpenMP threads are incompatible)... HB
	if (is_main_thread())
	{
#		pragma omp parallel for
		// NOTE: VS2017 OpenMP requires a signed integer loop index... HB
		for (S32 j = 0; j < (S32)num_vertices; ++j)
		{
			F32* w = weights[j].getF32ptr();

			// Unrolled loop on w[k]
			S32 i = llfloor(w[0]);
			F32 f = w[0] - i;
			i = llclamp(i, 0, max_joints);
			w[0] = i + f;

			i = llfloor(w[1]);
			f = w[1] - i;
			i = llclamp(i, 0, max_joints);
			w[1] = i + f;

			i = llfloor(w[2]);
			f = w[2] - i;
			i = llclamp(i, 0, max_joints);
			w[2] = i + f;

			i = llfloor(w[3]);
			f = w[3] - i;
			i = llclamp(i, 0, max_joints);
			w[3] = i + f;
		}
	}
	else
#endif
	{
		for (U32 j = 0; j < num_vertices; ++j)
		{
			F32* w = weights[j].getF32ptr();

			// Unrolled loop on w[k]
			S32 i = llfloor(w[0]);
			F32 f = w[0] - i;
			i = llclamp(i, 0, max_joints);
			w[0] = i + f;

			i = llfloor(w[1]);
			f = w[1] - i;
			i = llclamp(i, 0, max_joints);
			w[1] = i + f;

			i = llfloor(w[2]);
			f = w[2] - i;
			i = llclamp(i, 0, max_joints);
			w[2] = i + f;

			i = llfloor(w[3]);
			f = w[3] - i;
			i = llclamp(i, 0, max_joints);
			w[3] = i + f;
		}
	}

	checkSkinWeights(weights, num_vertices, skin);
}

//static
void LLSkinningUtil::getPerVertexSkinMatrix(const LLVector4a& weights,
											const LLMatrix4a* mat,
											LLMatrix4a& final_mat,
											bool handle_bad_scale)
{
	bool valid_weights = true;

#if OPTIMIZED
	static const LLQuad m_zero = _mm_set_ps1(0.f);
	constexpr S16 LAST_JOINT = (S16)LL_MAX_JOINTS_PER_MESH_OBJECT - 1;
	static const __m128i max_idx = _mm_set_epi16(LAST_JOINT, LAST_JOINT,
												 LAST_JOINT, LAST_JOINT,
												 LAST_JOINT, LAST_JOINT,
												 LAST_JOINT, LAST_JOINT);

	__m128i m_idx = _mm_cvttps_epi32((LLQuad)weights);

	LLVector4a wght = _mm_sub_ps((LLQuad)weights, _mm_cvtepi32_ps(m_idx));

	alignas(16) S32 idx[4];
	_mm_store_si128((__m128i*)idx, _mm_min_epi16(m_idx, max_idx));

	LLQuad m_scale = _mm_add_ps(wght, _mm_movehl_ps(wght, wght));
	m_scale = _mm_add_ss(m_scale, _mm_shuffle_ps(m_scale, m_scale, 1));
	m_scale = _mm_shuffle_ps(m_scale, m_scale, 0);

	if (handle_bad_scale && _mm_comigt_ss(m_scale, m_zero) != 1)
	{
		wght = LLVector4a(1.f, 0.f, 0.f, 0.f);
		valid_weights = false;
	}
	else
	{
		wght = _mm_div_ps(wght, m_scale);
	}
#else
	const F32* fwghts = weights.getF32ptr();
	constexpr S32 LAST_JOINT = (S32)LL_MAX_JOINTS_PER_MESH_OBJECT - 1;
	LLVector4 wght;
	S32 idx[4];
	F32 scale = 0.f;
	for (U32 k = 0; k < 4; ++k)
	{
		F32 w = fwghts[k];
		F32 temp = floorf(w);
		idx[k] = llclamp((S32)temp, 0, LAST_JOINT);
		temp = w - temp;
		wght[k] = temp;
		scale += temp;
	}
	if (handle_bad_scale && scale <= 0.f)
	{
		wght = LLVector4(1.f, 0.f, 0.f, 0.f);
		valid_weights = false;
	}
	else
	{
		wght /= scale;
	}
#endif

	final_mat.clear();
	LLMatrix4a src;
	for (U32 k = 0; k < 4; ++k)
	{
		src.setMul(mat[idx[k]], wght[k]);
		final_mat.add(src);
	}

	// SL-366 - with weight validation/cleanup code, it should no longer be
	// possible to hit the bad scale case.
	if (!valid_weights)
	{
		llwarns << "Invalid weights !" << llendl;
		llassert(false);
	}
}

void LLSkinningUtil::updateRiggingInfo(const LLMeshSkinInfo* skin,
									   LLVOAvatar* avatar,
									   LLVolumeFace& vol_face)
{
	S32 num_verts = vol_face.mNumVertices;
	U32 max_count = (U32)skin->mJointKeys.size();
	if (num_verts <= 0 || !vol_face.mWeights || !max_count)
	{
		return;
	}

	LLJointRiggingInfoTab& riginfotab = vol_face.mJointRiggingInfoTab;
	if (riginfotab.size() || !riginfotab.needsUpdate())
	{
		return;
	}

	riginfotab.resize(LL_CHARACTER_MAX_ANIMATED_JOINTS);

#if !LL_OPENMP
	static LLMatrix4a inv_bind;
	static LLVector4a pos_joint_space;
	static LLVector4 wght;
	static S32 idx[4];
#else
	LLMatrix4a inv_bind;
	LLVector4a pos_joint_space;
	LLVector4 wght;
	S32 idx[4];
	assert_main_thread();
#	pragma omp parallel for private(inv_bind, pos_joint_space, wght, idx)
#endif
	for (S32 i = 0; i < num_verts; ++i)
	{
		LLVector4a& pos = vol_face.mPositions[i];
		F32* weights = vol_face.mWeights[i].getF32ptr();
		F32 scale = 0.f;
		// *TODO: unpacking of weights should be optimized if possible.
		for (U32 k = 0; k < 4; ++k)
		{
			F32 w = weights[k];
			idx[k] = llclamp((S32)floorf(w), 0,
							 (S32)LL_CHARACTER_MAX_ANIMATED_JOINTS - 1);
			wght[k] = w - idx[k];
			scale += wght[k];
		}
		if (scale > 0.f)
		{
			F32 scale_inv = 1.f / scale;
			wght[0] *= scale_inv;
			wght[1] *= scale_inv;
			wght[2] *= scale_inv;
			wght[3] *= scale_inv;
		}
		for (U32 k = 0; k < 4; ++k)
		{
			U32 joint_index = idx[k];
			if (joint_index >= max_count || wght[k] <= 0.f)
			{
				continue;
			}

			// Note: joint key 0 = "unnamed", 1 = "mScreen" (so we skip them)
			S32 i = (S32)skin->mJointKeys[joint_index] - 2;
			if (i >= 0 && i < (S32)LL_CHARACTER_MAX_ANIMATED_JOINTS)
			{
				riginfotab[i].setIsRiggedTo();
				inv_bind.loadu(skin->mInvBindShapeMatrix[joint_index]);
				inv_bind.affineTransform(pos, pos_joint_space);
				pos_joint_space.mul(wght[k]);
				LLVector4a* extents = riginfotab[i].getRiggedExtents();
				update_min_max(extents[0], extents[1], pos_joint_space);
			}
		}
	}

	riginfotab.setNeedsUpdate(false);
}

// This is used for extracting rotation from a bind shape matrix that already
// has scales baked in
LLQuaternion LLSkinningUtil::getUnscaledQuaternion(const LLMatrix4& mat4)
{
	LLMatrix3 bind_mat = mat4.getMat3();
#if LL_GNUC && GCC_VERSION >= 80000
# pragma GCC unroll 3
#elif LL_CLANG
# pragma clang loop unroll(full)
#endif
	for (U32 i = 0; i < 3; ++i)
	{
		auto& coords = bind_mat.mMatrix[i];
		F32 len = coords[0] * coords[0];
		len += coords[1] * coords[1];
		len += coords[2] * coords[2];

		if (len > 0.f)
		{
			F32 inv_len = 1.f / sqrtf(len);
			coords[0] *= inv_len;
			coords[1] *= inv_len;
			coords[2] *= inv_len;
		}
	}
	bind_mat.invert();
	LLQuaternion bind_rot = bind_mat.quaternion();
	bind_rot.normalize();
	return bind_rot;
}
