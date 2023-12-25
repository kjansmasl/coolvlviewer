/**
 * @file llpuppetmotion.cpp
 * @brief Implementation of the LLPuppetMotion class.
 *
 * $LicenseInfo:firstyear=2021&license=viewergpl$
 *
 * Copyright (c) 2021-2022, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include <array>
#include <set>

#include "llpuppetmotion.h"

#include "llcharacter.h"
#include "llcorehttputil.h"
#include "llmessage.h"
#include "llquantize.h"				// For F32_to_U16() and U16_to_F32()
#include "llsdutil_math.h"			// For ll_sd_from_vector3()

#include "llagent.h"
#include "llpuppetmodule.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"

///////////////////////////////////////////////////////////////////////////////
// LLPuppetEvent class: resides in llpuppetevent.cpp in LL's viewer sources,
// but is only used here, so... HB
///////////////////////////////////////////////////////////////////////////////

// *HACK: move this somewhere better.
constexpr U32 PUPPET_MAX_EVENT_BYTES = 200;
U8 PUPPET_WRITE_BUFFER[PUPPET_MAX_EVENT_BYTES];

// Helper functions

static size_t pack_vec3(U8* wptr, LLVector3 vec)
{
	// Pack F32 components into 16 bits
	vec.quantize16(-LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET,
				   -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
	U16 x = F32_to_U16(vec.mV[VX], -LL_MAX_PELVIS_OFFSET,
					   LL_MAX_PELVIS_OFFSET);
	U16 y = F32_to_U16(vec.mV[VY], -LL_MAX_PELVIS_OFFSET,
					   LL_MAX_PELVIS_OFFSET);
	U16 z = F32_to_U16(vec.mV[VZ], -LL_MAX_PELVIS_OFFSET,
					   LL_MAX_PELVIS_OFFSET);

	htonmemcpy(wptr, &x, MVT_U16, sizeof(U16));
	htonmemcpy(wptr + sizeof(U16), &y, MVT_U16, sizeof(U16));
	htonmemcpy(wptr + 2 * sizeof(U16), &z, MVT_U16, sizeof(U16));
	return 3 * sizeof(U16);
}

static size_t pack_quat(U8* wptr, LLQuaternion quat)
{
	// A Quaternion is a 4D object but the group isomorphic with rotations is
	// limited to the surface of the unit hypersphere (radius = 1).
	// Consequently the quaternions we care about have only three degrees of
	// freedom and we can store them in three floats. To do this we always make
	// sure the real component (W) is positive by negating the Quaternion as
	// necessary and then we store only the imaginary part (XYZ). The real
	// part can be obtained with the formula: W = sqrt(1.0 - X*X + Y*Y + Z*Z)
	if (quat.mQ[VW] < 0.f)
	{
		// Negate the quaternion to keep its real part positive
		quat = -1.f * quat;
	}

	// Pack F32 components into 16 bits
	quat.quantize16(-LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
	U16 x = F32_to_U16(quat.mQ[VX], -LL_MAX_PELVIS_OFFSET,
					   LL_MAX_PELVIS_OFFSET);
	U16 y = F32_to_U16(quat.mQ[VY], -LL_MAX_PELVIS_OFFSET,
					   LL_MAX_PELVIS_OFFSET);
	U16 z = F32_to_U16(quat.mQ[VZ], -LL_MAX_PELVIS_OFFSET,
					   LL_MAX_PELVIS_OFFSET);

	// Store the imaginary part
	htonmemcpy(wptr, &x, MVT_U16, sizeof(U16));
	htonmemcpy(wptr + sizeof(U16), &y, MVT_U16, sizeof(U16));
	htonmemcpy(wptr + 2 * sizeof(U16), &z, MVT_U16, sizeof(U16));
	return 3 * sizeof(U16);
}

static size_t unpack_vec3(U8* wptr, LLVector3& vec)
{
	U16 x, y, z;    // F32 data is packed in 16 bits
	htonmemcpy(&x, wptr, MVT_U16, sizeof(U16));
	htonmemcpy(&y, wptr + sizeof(U16), MVT_U16, sizeof(U16));
	htonmemcpy(&z, wptr + 2 * sizeof(U16), MVT_U16, sizeof(U16));
	vec.mV[VX] = U16_to_F32(x, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
	vec.mV[VY] = U16_to_F32(y, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
	vec.mV[VZ] = U16_to_F32(z, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
	return 3 * sizeof(U16);
}

static size_t unpack_quat(U8* wptr, LLQuaternion& quat)
{
	U16 x, y, z;    // F32 data is packed in 16 bits
	htonmemcpy(&x, wptr, MVT_U16, sizeof(U16));
	htonmemcpy(&y, wptr + sizeof(U16), MVT_U16, sizeof(U16));
	htonmemcpy(&z, wptr + 2 * sizeof(U16), MVT_U16, sizeof(U16));
	quat.mQ[VX] = U16_to_F32(x, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
	quat.mQ[VY] = U16_to_F32(y, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);
	quat.mQ[VZ] = U16_to_F32(z, -LL_MAX_PELVIS_OFFSET, LL_MAX_PELVIS_OFFSET);

	// A packed Quaternion only includes the imaginary part (XYZ) and the
	// real part (W) is obtained with the formula:
	// W = sqrt(1.0 - X*X + Y*Y + Z*Z)
	F32 imaginary_length_squared = quat.mQ[VX] * quat.mQ[VX] +
								   quat.mQ[VY] * quat.mQ[VY] +
								   quat.mQ[VZ] * quat.mQ[VZ];
	// DANGER: make sure we do not try to take the sqrt of a negative number.
	if (imaginary_length_squared > 1.f)
	{
		quat.mQ[VW] = 0.f;
		F32 scale = 1.f / sqrtf(imaginary_length_squared);
		quat.mQ[VX] *= scale;
		quat.mQ[VY] *= scale;
		quat.mQ[VZ] *= scale;
	}
	else
	{
		quat.mQ[VW] = sqrtf(1.f - imaginary_length_squared);
	}
	return 3 * sizeof(U16);
}

// LLPuppetJointEvent class

void LLPuppetJointEvent::interpolate(F32 del, const LLPuppetJointEvent& a,
									 const LLPuppetJointEvent& b)
{
	// Copy all of A just in case B is incompatible
	mRotation = a.mRotation;
	mPosition = a.mPosition;
	mScale = a.mScale;
	mJointID = a.mJointID;
	mMask = a.mMask;

	// Interpolate
	del = llclamp(del, 0.f, 1.f); // Keep del in range [0,1]
	U8 mask = mMask & LLIK::MASK_ROT;
	if (mask && (mMask & LLIK::MASK_ROT) == (b.mMask & LLIK::MASK_ROT))
	{
		mRotation = slerp(del, a.mRotation, b.mRotation);
	}
	mask = mMask & LLIK::MASK_POS;
	if (mask && (mMask & LLIK::MASK_POS) == (b.mMask &  LLIK::MASK_POS))
	{
		mPosition = (1.f - del) * a.mPosition + del * b.mPosition;
	}
	if ((mMask & LLIK::CONFIG_FLAG_LOCAL_SCALE) &&
		(b.mMask & LLIK::CONFIG_FLAG_LOCAL_SCALE))
	{
		mScale = (1.f - del) * a.mScale + del * b.mScale;
	}
}

size_t LLPuppetJointEvent::getSize() const
{
	constexpr U32 BYTES_PER_VEC_3 = 3 * sizeof(F32);
	size_t num_bytes = sizeof(S16) + sizeof(S8);  // mJointID, mMask
	if (mMask & LLIK::MASK_ROT)
	{
		num_bytes += BYTES_PER_VEC_3;
	}
	if (mMask & LLIK::MASK_POS)
	{
		num_bytes += BYTES_PER_VEC_3;
	}
	if (mMask & LLIK::CONFIG_FLAG_LOCAL_SCALE)
	{
		num_bytes += BYTES_PER_VEC_3;
	}
	return num_bytes;
}

size_t LLPuppetJointEvent::pack(U8* wptr) const
{
	// Stuff everything into a binary blob to save overhead.
	size_t offset = 0;

	htonmemcpy(wptr, &mJointID, MVT_S16, sizeof(S16));
	offset += sizeof(S16);

	htonmemcpy(wptr + offset, &mMask, MVT_U8, sizeof(U8));
	offset += sizeof(U8);

	// Pack these into the buffer in the same order as the flags.
	if (mMask & LLIK::MASK_ROT)
	{
		offset += pack_quat(wptr + offset, mRotation);
	}
	if (mMask & LLIK::MASK_POS)
	{
		offset += pack_vec3(wptr + offset, mPosition);
	}
	if (mMask & LLIK::CONFIG_FLAG_LOCAL_SCALE)
	{
		offset += pack_vec3(wptr + offset, mScale);
	}

	return offset;
}

size_t LLPuppetJointEvent::unpack(U8* wptr)
{
	htonmemcpy(&mJointID, wptr, MVT_S16, sizeof(S16));
	size_t offset = sizeof(S16);

	htonmemcpy(&mMask, wptr + offset, MVT_U8, sizeof(U8));
	offset += sizeof(U8);

	// Unpack in the same order as the flags.
	if (mMask & LLIK::MASK_ROT)
	{
		offset += unpack_quat(wptr + offset, mRotation);
	}
	if (mMask & LLIK::MASK_POS)
	{
		offset += unpack_vec3(wptr + offset, mPosition);
	}
	if (mMask & LLIK::CONFIG_FLAG_LOCAL_SCALE)
	{
		offset += unpack_vec3(wptr + offset, mScale);
	}

	return offset;
}

S32 LLPuppetEvent::getMinEventSize() const
{
	// Time, num and the size of the event buffer.
	S32 min_sz = sizeof(S32) + sizeof(S16) + sizeof(U32);
	if (mJointEvents.size() > 0)
	{
		min_sz = (S32)(mJointEvents.begin()->getSize());
	}
	return min_sz;
}

// A PuppetEvent contains a timestamp and one or more joints with one or more
// actions applied to it. Return value is true if we packed all joints into
// this event.
bool LLPuppetEvent::pack(LLDataPackerBinaryBuffer& buffer, S32& out_num_joints)
{
	S16 num_joints = 0;
	size_t buffer_size = buffer.getBufferSize() - buffer.getCurrentSize();
	bool result = true;

	static std::array<U8, PUPPET_MAX_EVENT_BYTES> scratch_buffer;

	// Accounting for time and num first plus an extra S32 for binary data size
	size_t len = sizeof(S32) + sizeof(S16) + sizeof(S32);

	size_t buf_sz = 0;
	U8* wptr = scratch_buffer.data();
	joint_deq_t::iterator iter = mJointEvents.begin();
	while (iter != mJointEvents.end())
	{
		if (len + buf_sz + iter->getSize() > buffer_size)
		{
			result = false;
			break;
		}

		size_t offset = iter->pack(wptr);
		++num_joints;
		wptr += offset;
		buf_sz += offset;
		mJointEvents.pop_front();
		iter = mJointEvents.begin();
	}
	len += buf_sz;

	buffer.packS32(mTimestamp, "time");
	buffer.packU16(num_joints, "num");
	buffer.packBinaryData(scratch_buffer.data(), buf_sz, "data");

	out_num_joints = num_joints;

	return result;
}

bool LLPuppetEvent::unpack(LLDataPackerBinaryBuffer& buffer)
{
	if (!buffer.unpackS32(mTimestamp, "time"))
	{
		LL_DEBUGS("Puppetry") << "Unable to unpack timestamp from puppetry packet."
							  << LL_ENDL;
		return false;
	}

	U16 num_joints = 0;
	if (!buffer.unpackU16(num_joints, "num"))
	{
		LL_DEBUGS("Puppetry") << "Unable to unpack expected joint count from puppetry packet."
							  << LL_ENDL;
		return false;
	}

	static std::array<U8, PUPPET_MAX_EVENT_BYTES> scratch_buffer;
	S32 buff_sz = scratch_buffer.size();
	if (!buffer.unpackBinaryData(scratch_buffer.data(), buff_sz, "data"))
	{
		LL_DEBUGS("Puppetry") << "Unable to unpack puppetry payload data from puppetry packet."
							  << LL_ENDL;
		return false;
	}

	U8* wptr = scratch_buffer.data();
	S32 offset = 0;
	S32 index = 0;
	for ( ; index < num_joints && offset < buff_sz; ++index)
	{
		LLPuppetJointEvent jev;
		offset += jev.unpack(wptr + offset);
		mJointEvents.push_back(jev);
	}

	if (index != num_joints)
	{
		LL_DEBUGS("Puppetry") << "Unexpected joint count unpacking puppetry, expecting "
							  << num_joints << ", only read " << index
							  << LL_ENDL;
		return false;
	}

	if (offset != buff_sz)
	{
		LL_DEBUGS("Puppetry") << "Unread data in buffer. " << buff_sz
							  << " bytes received, but only " << offset
							  << " bytes used." << LL_ENDL;
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// LLPuppetMotion utilities
///////////////////////////////////////////////////////////////////////////////

// *HACK: hard-coded joint_ids
constexpr U16 CHEST_ID = 6;
constexpr S16 WRIST_LEFT_ID = 61;
constexpr S16 WRIST_RIGHT_ID = 80;

// Other constants
// This is the largest possible size event:
constexpr U32 PUPPET_MAX_MSG_BYTES = 255;
constexpr F32 PUPPET_BROADCAST_INTERVAL = 0.05f;	// Time in seconds
constexpr S32 POSED_JOINT_EXPIRY_PERIOD = 3000;		// In milliseconds

// Static members
bool LLPuppetMotion::sIsPuppetryEnabled = false;
size_t LLPuppetMotion::sPuppeteerEventMaxSize = 0;

///////////////////////////////////////////////////////////////////////////////
// LLPuppetMotion class proper
///////////////////////////////////////////////////////////////////////////////

LLPuppetMotion::LLPuppetMotion(const LLUUID& id)
:	LLMotion(id),
	mMotionPriority(LLJoint::PUPPET_PRIORITY),
	mNextJointStateExpiry(S32_MAX),
	mRemoteToLocalClockOffset(F32_MIN),
	mArmSpan(2.f),
	mIsSelf(false)
{
	mName = "puppet_motion";
	mBroadcastTimer.resetWithExpiry(PUPPET_BROADCAST_INTERVAL);
}

//virtual
bool LLPuppetMotion::needsUpdate() const
{
	return !mExpressionEvents.empty() || !mEventQueues.empty() ||
		   LLMotion::needsUpdate();
}

//virtual
LLMotion::LLMotionInitStatus LLPuppetMotion::onInitialize(LLCharacter* charp)
{
	if (!charp)	// Paranoia
	{
		return STATUS_FAILURE;
	}

	mIsSelf = ((LLVOAvatar*)charp)->isSelf();

	LLJoint* jointp = charp->getJoint(LL_JOINT_KEY_PELVIS);
	if (!jointp)
	{
		return STATUS_FAILURE;
	}

	S16 joint_id = jointp->getJointNum();
	mIKSolver.setRootID(joint_id);

	collectJoints(jointp);
	mIKSolver.addWristID(WRIST_LEFT_ID);
	mIKSolver.addWristID(WRIST_RIGHT_ID);

	// Compute arms reach
	measureArmSpan();

	// Generate reference rotation
	mIKSolver.resetSkeleton();

	// *HACK: whitelist of sub-bases: joints that have only child Chains and
	// should always be Chain endpoints, never in the middle of a Chain.
	std::set<S16> ids;
	ids.insert(CHEST_ID);
	mIKSolver.setSubBaseIds(ids);
#if 0
	// *HACK: whitelist of sub-roots. This prevents the spine from being
	// included in the IK solution, effectively preventing the spine from
	// moving.
	ids.clear();
	ids.insert(CHEST_ID);
	mIKSolver.setSubRootIds(ids);
#endif

	return STATUS_SUCCESS;
}

void LLPuppetMotion::clearAll()
{
	mEventQueues.clear();
	mOutgoingEvents.clear();
	mJointStateExpiries.clear();
	mJointsToRemoveFromPose.clear();
	mIKSolver.resetSkeleton();
}

void LLPuppetMotion::addExpressionEvent(const LLPuppetJointEvent& event)
{
	// We used to collect these events in a map, keyed by joint_id, but now we
	// just collect them onto a vector and process them FIFO later.
	mExpressionEvents.emplace_back(event);
}

void LLPuppetMotion::addJointToSkeletonData(LLSD& skeleton_sd, LLJoint* joint,
											const LLVector3& parent_rel_pos,
											const LLVector3& tip_rel_end_pos)
{
	LLSD bone_sd;
	S16 joint_id = joint->getJointNum();
	bone_sd["id"] = joint_id;
	if (joint->getParent())
	{
		bone_sd["parent_id"] = joint->getParent()->getJointNum();
		bone_sd["parent_relative_position"] = ll_sd_from_vector3(parent_rel_pos);
	}
	bone_sd["tip_relative_end_position"] = ll_sd_from_vector3(tip_rel_end_pos);
	if (joint->getNumChildren() > 0)
	{
		bone_sd["children"] = LLSD::emptyArray();
		for (LLJoint::child_list_t::iterator it = joint->mChildren.begin(),
											 end = joint->mChildren.end();
			 it != end; ++it)
		{
			if ((*it)->isBone())
			{
				LLJoint* child = *it;
				bone_sd["children"].append(child->getJointNum());
				break;
			}
		}
	}
	skeleton_sd[joint->getName()] = bone_sd;
}

LLSD LLPuppetMotion::getSkeletonData()
{
	LLSD skeleton_sd;
	for (state_map_t::iterator it = mJointStates.begin(),
							   end = mJointStates.end();
		 it != end; ++it)
	{
		LLPointer<LLJointState>& jstatep = it->second;
		LLJoint* jointp = jstatep->getJoint();
		if (!jointp)	// Paranoia
		{
			continue;
		}
		LLVector3 local_pos_in_parent_frame =
			jointp->getPosition().scaledVec(jointp->getScale());
		LLVector3 bone_in_local_frame =
			jointp->getEnd().scaledVec(jointp->getScale());
		addJointToSkeletonData(skeleton_sd, jointp, local_pos_in_parent_frame,
							   bone_in_local_frame);
	}
	skeleton_sd["scale"] = mArmSpan;
	return skeleton_sd;
}

void LLPuppetMotion::updateSkeletonGeometry()
{
	LLIKConstraintFactory* factoryp = LLIKConstraintFactory::getInstance();

	for (state_map_t::iterator it = mJointStates.begin(),
							   end = mJointStates.end();
		 it != end; ++it)
	{
		LLPointer<LLJointState>& jstatep = it->second;
		if (!jstatep)	// Paranoia
		{
			continue;
		}
		LLJoint* jointp = jstatep->getJoint();
		if (jointp)	// Paranoia
		{
			LLIK::Constraint::ptr_t constraint =
				factoryp->getConstrForJoint(jointp->getName());
			mIKSolver.resetJointGeometry(it->first, constraint);
		}
	}

	measureArmSpan();
}

void LLPuppetMotion::rememberPosedJoint(S16 joint_id,
										LLPointer<LLJointState> jstatep,
										S32 now)
{
	S32 expiry = now + POSED_JOINT_EXPIRY_PERIOD;
	expiries_map_t::iterator itr = mJointStateExpiries.find(joint_id);
	if (itr == mJointStateExpiries.end())
	{
		// Always bump remembered joints to HIGHEST_PRIORITY
		jstatep->setPriority(LLJoint::USE_MOTION_PRIORITY);
		mJointStateExpiries.emplace(joint_id,
									JointStateExpiry(jstatep, expiry));
		addJointState(jstatep);

		// Check for and remove mentions of jstatep in mJointsToRemoveFromPose
		size_t i = 0;
		while (i < mJointsToRemoveFromPose.size())
		{
			if (mJointsToRemoveFromPose[i] == jstatep)
			{
				size_t last_index = mJointsToRemoveFromPose.size() - 1;
				if (i < last_index)
				{
					mJointsToRemoveFromPose[i] =
						mJointsToRemoveFromPose[last_index];
				}
				mJointsToRemoveFromPose.pop_back();
			}
			else
			{
				++i;
			}
		}
	}
	else
	{
		itr->second.mExpiry = expiry;
	}
	if (expiry < mNextJointStateExpiry)
	{
		mNextJointStateExpiry = expiry;
	}
}

void LLPuppetMotion::reportRootRelativePosition(S16 joint_id, S32 request_id)
{
	// Sanity checks.
	if (!mJointStates.count(joint_id))
	{
		return;
	}
	LLPointer<LLJointState>& rstatep = mJointStates[0];
	LLPointer<LLJointState>& jstatep = mJointStates[joint_id];
	if (rstatep.isNull() || jstatep.isNull())
	{
		return;
	}
	LLJoint* rootp = rstatep->getJoint();
	LLJoint* jointp = jstatep->getJoint();
	if (!rootp || !jointp || rootp == jointp)	// No reporting for root
	{
		return;
	}

	const std::string name = jointp->getName();
	const LLVector3& rpos = rootp->getWorldPosition();
	const LLQuaternion& rrot = rootp->getWorldRotation();
	LLVector3 jpos = jointp->getWorldPosition();
	LLVector3 jend = jointp->getEnd();

	jend.rotVec(jointp->getWorldRotation());
	jpos += jend;

	LLVector3 opos = jpos - rpos;	// Vector from root to joint end.
	opos.rotVec(~rrot);				// Remove root rotation.
	LLVector3 opos2 = opos * (2.f / mArmSpan);
	const LLQuaternion& rlrot = rootp->getRotation();

	LL_DEBUGS("PuppetrySpam") << "PostIK report for: " << name << " ("
							  << joint_id << ") 1m scale position: ("
							  << opos.mV[0] << ", " << opos.mV[1] << ", "
							  << opos.mV[2] << ") - Arm span scale: "
							  << opos2.mV[0] << ", " << opos2.mV[1] << ", "
							  << opos2.mV[2];
	LLVector3 reuler;
	rlrot.getEulerAngles(&reuler.mV[0], &reuler.mV[1], &reuler.mV[2]);
	reuler *= RAD_TO_DEG;
	LL_CONT << ") - Parent relative Euler rotation (" << reuler.mV[0] << ", "
			<< reuler.mV[1] << ", " << reuler.mV[2] << ")" << LL_ENDL;

	LLSD data;
	data["joint_id"] = joint_id;
	data["name"] = name;
	data["position"] = LLSDMap();
	data["position"]["one_meter"] = ll_sd_from_vector3(opos);
	data["position"]["armspan"] = ll_sd_from_vector3(opos2);
	data["rotation"] = ll_sd_from_quaternion(rlrot);
	if (request_id > -1)
	{
		data["reqid"] = request_id;
	}
	gEventPumps.obtain("JointReport").post(data);
}

// Note: this only ever called when mIsSelf is true and configs not empty
void LLPuppetMotion::solveIKAndHarvestResults(ik_map_t& configs, S32 now)
{
	LLPuppetModule* modulep = LLPuppetModule::getInstance();

	bool local_puppetry = !modulep->getEcho();
	if (local_puppetry)
	{
		// Do not actually apply puppetry when local agent is in mouselook
		ECameraMode camera_mode = gAgent.getCameraMode();
		local_puppetry = camera_mode != CAMERA_MODE_MOUSELOOK &&
						 camera_mode != CAMERA_MODE_CUSTOMIZE_AVATAR;
	}

	bool is_sending = modulep->isSending();
	if (!local_puppetry && !is_sending)
	{
		return;
	}

	bool config_changed = mIKSolver.updateJointConfigs(configs);
	if (config_changed)
	{
		mIKSolver.solve();
	}
#if 0	// ATM we still need to constantly re-send unchanged Puppetry data so
		// we DO NOT bail early here... yet.
	else
	{
		// *TODO: figure out how to send partial updates, and how to explicitly
		// clear joint settings in the Puppetry stream.
		return;
	}
#endif

	LLPuppetEvent broadcast_event;
	const LLIK::Solver::joint_list_t& active_joints =
		mIKSolver.getActiveJoints();
	for (auto joint : active_joints)
	{
		S16 id = joint->getID();
		U8 flags = joint->getHarvestFlags();
		if (local_puppetry)
		{
			LLPointer<LLJointState>& jstatep = mJointStates[id];
			jstatep->setUsage(U32(flags & LLIK::MASK_JOINT_STATE_USAGE));
			if (flags & LLIK::CONFIG_FLAG_LOCAL_POS)
			{
				jstatep->setPosition(joint->getPreScaledLocalPos());
			}
			if (flags & LLIK::CONFIG_FLAG_LOCAL_ROT)
			{
				jstatep->setRotation(joint->getLocalRot());
			}
			if (flags & LLIK::CONFIG_FLAG_LOCAL_SCALE)
			{
				jstatep->setScale(joint->getLocalScale());
			}
			rememberPosedJoint(id, jstatep, now);
		}
		if (is_sending)
		{
			LLPuppetJointEvent joint_event;
			joint_event.setJointID(id);
			joint_event.setReferenceFrame(LLPuppetJointEvent::PARENT_FRAME);
			if (flags & LLIK::CONFIG_FLAG_LOCAL_POS)
			{
				// We send positions with correct scale, so they can be applied
				// on the receiving end without modification.
				joint_event.setPosition(joint->getPreScaledLocalPos());
			}
			if (flags & LLIK::CONFIG_FLAG_LOCAL_ROT)
			{
				joint_event.setRotation(joint->getLocalRot());
			}
			if (flags & LLIK::CONFIG_FLAG_LOCAL_SCALE)
			{
				joint_event.setScale(joint->getLocalScale());
			}
			if (flags & LLIK::CONFIG_FLAG_DISABLE_CONSTRAINT)
			{
				joint_event.disableConstraint();
			}
			broadcast_event.addJointEvent(joint_event);
		}
	}
	if (is_sending)
	{
		broadcast_event.updateTimestamp();
		queueOutgoingEvent(broadcast_event);
	}
}

void LLPuppetMotion::applyEvent(const LLPuppetJointEvent& event, U64 now,
								ik_map_t& configs)
{
	S16 joint_id = event.getJointID();
	if (mJointStates.count(joint_id))
	{
		LLIK::Joint::Config config;
		bool something_changed = false;
		U8 mask = event.getMask();
		bool local = bool(mask & LLIK::CONFIG_FLAG_LOCAL_ROT);
		if (mask & LLIK::MASK_ROT)
		{
			if (local)
			{
				config.setLocalRot(event.getRotation());
			}
			else
			{
				config.setTargetRot(event.getRotation());
			}
			something_changed = true;
		}
		if (mask & LLIK::MASK_POS)
		{
			if (local)
			{
				config.setLocalPos(event.getPosition());
			}
			else
			{
				// Do not forget to scale by half mArmSpan
				config.setTargetPos(event.getPosition() * (0.5f * mArmSpan));
			}
			something_changed = true;
		}
		if (mask & LLIK::CONFIG_FLAG_DISABLE_CONSTRAINT)
		{
			config.disableConstraint();
			something_changed = true;
		}
		if (mask & LLIK::CONFIG_FLAG_ENABLE_REPORTING)
		{
			config.enableReporting(event.getRequestID());
		}
		if (something_changed)
		{
			configs[joint_id] = config;
		}
	}
}

// Note: if we get here mIsSelf must be true
void LLPuppetMotion::updateFromExpression(S32 now)
{
	if (mExpressionEvents.empty())
	{
		return;
	}
	bool reporting = false;
	ik_map_t configs;
	for (const auto& event : mExpressionEvents)
	{
		S16 joint_id = event.getJointID();
		if (!mJointStates.count(joint_id))
		{
			continue;
		}

		LLIK::Joint::Config config;
		bool something_changed = false;
		U8 mask = event.getMask();
		if (mask & LLIK::MASK_ROT)
		{
			if (mask & LLIK::CONFIG_FLAG_LOCAL_ROT)
			{
				config.setLocalRot(event.getRotation());
			}
			else
			{
				config.setTargetRot(event.getRotation());
			}
			something_changed = true;
		}
		if (mask & LLIK::MASK_POS)
		{
			if (mask & LLIK::CONFIG_FLAG_LOCAL_POS)
			{
				config.setLocalPos(event.getPosition());
			}
			else
			{
				// Do not forget to scale by half mArmSpan
				config.setTargetPos(event.getPosition() * (0.5f * mArmSpan));
			}
			something_changed = true;
		}
		if (mask & LLIK::CONFIG_FLAG_DISABLE_CONSTRAINT)
		{
			config.disableConstraint();
			something_changed = true;
		}
		if (mask & LLIK::CONFIG_FLAG_ENABLE_REPORTING)
		{
			reporting = true;
		}
		if (something_changed)
		{
			LLIK::Solver::joint_config_map_t::iterator itr =
				configs.find(joint_id);
			if (itr == configs.end())
			{
				configs.emplace(joint_id, config);
			}
			else
			{
				itr->second.updateFrom(config);
			}
		}
	}

	if (!configs.empty())
	{
		solveIKAndHarvestResults(configs, now);
	}

	if (reporting && LLPuppetModule::getInstance()->getEcho())
	{
		for (const auto& event : mExpressionEvents)
		{
			S16 joint_id = event.getJointID();
			// No reporting for the root joint or unknown joints
			if (joint_id && mJointStates.count(joint_id) &&
				(event.getMask() & LLIK::CONFIG_FLAG_ENABLE_REPORTING))
			{
				reportRootRelativePosition(joint_id, event.getRequestID());
			}
		}
	}

	mExpressionEvents.clear();
}

void LLPuppetMotion::applyBroadcastEvent(const LLPuppetJointEvent& event,
										 S32 now)
{
	S16 joint_id = event.getJointID();
	state_map_t::iterator it = mJointStates.find(joint_id);
	if (it == mJointStates.end())
	{
		return;
	}

	LLPointer<LLJointState>& jstatep = it->second;
	if (!jstatep)	// Paranoia
	{
		return;
	}

	U8 flags = event.getMask();
	jstatep->setUsage(U32(flags & LLIK::MASK_JOINT_STATE_USAGE));
	// Note: we assume broadcast event always in parent-frame, e.g.
	// (flags & LLIK::MASK_TARGET) == 0
	if (flags & LLIK::CONFIG_FLAG_LOCAL_POS)
	{
		// We expect received position to be scaled correctly so it can be
		// applied without modification.
		jstatep->setPosition(event.getPosition());
	}
	if (flags & LLIK::CONFIG_FLAG_LOCAL_ROT)
	{
		jstatep->setRotation(event.getRotation());
	}
	if (flags & LLIK::CONFIG_FLAG_LOCAL_SCALE)
	{
		jstatep->setScale(event.getScale());
	}
	rememberPosedJoint(joint_id, jstatep, now);
}

void LLPuppetMotion::updateFromBroadcast(S32 now)
{
	LLPuppetModule* modulep = LLPuppetModule::getInstance();

	bool accept_broadcast = modulep->isReceiving();
	if (accept_broadcast && mIsSelf)
	{
		ECameraMode camera_mode = gAgent.getCameraMode();
		accept_broadcast = camera_mode != CAMERA_MODE_MOUSELOOK &&
						   camera_mode != CAMERA_MODE_CUSTOMIZE_AVATAR;
	}
	if (!accept_broadcast)
	{
		// Drop unapplied data.
		mEventQueues.clear();
		return;
	}

	// We walk the queue looking for the two bounding events: the last previous
	// and the next pending: we will interpolate between them. If we do not
	// find bounding events we will use whatever we have got.
	evqueues_map_t::iterator queue_itr = mEventQueues.begin();
	while (queue_itr != mEventQueues.end())
	{
		event_queue_t& queue = queue_itr->second.getEventQueue();
		event_queue_t::iterator event_itr = queue.begin();
		while (event_itr != queue.end())
		{
			S32 timestamp = event_itr->first;
			const LLPuppetJointEvent& event = event_itr->second;
			if (timestamp > now)
			{
				// First available event is in the future; we have no choice
				// but to apply what we have
				applyBroadcastEvent(event, now);
				break;
			}

			// Event is in the past --> check next event
			++event_itr;
			if (event_itr == queue.end())
			{
				// We are at the end of the queue
				constexpr S32 STALE_QUEUE_DURATION = 3000;
				if (timestamp < now - STALE_QUEUE_DURATION)
				{
					// This queue is stale; the "remembered pose" will be
					// purged elewhere
					queue.clear();
				}
				else
				{
					// Presumeably we already interpolated close to this event
					// but just in case we didn't quite reach it yet: apply
					applyBroadcastEvent(event, now);
				}
				break;
			}

			S32 next_time = event_itr->first;
			if (next_time < now)
			{
				// Event is stale --> drop it
				queue.pop_front();
				event_itr = queue.begin();
				continue;
			}

			// Next event is in the future, which means we have found the two
			// events that straddle 'now' --> create an interpolated event and
			// apply that.
			F32 del = (F32)(now - timestamp) / (F32)(next_time - timestamp);
			LLPuppetJointEvent interpolated_event;
			const LLPuppetJointEvent& next_event = event_itr->second;
			interpolated_event.interpolate(del, event, next_event);
			applyBroadcastEvent(interpolated_event, now);
			break;
		}
		if (queue.empty())
		{
			queue_itr = mEventQueues.erase(queue_itr);
		}
		else
		{
			++queue_itr;
		}
	}
}

// Note: we expect Puppetry data to be in the "normalized-frame" where the
// arm-span is 2.0 units. We will scale the inbound data by half mArmSpan.
void LLPuppetMotion::measureArmSpan()
{
	// "arm span" is twice the y-component of the longest arm
	F32 reach_left = mIKSolver.computeReach(CHEST_ID, WRIST_LEFT_ID).mV[VY];
	F32 reach_right = mIKSolver.computeReach(CHEST_ID, WRIST_RIGHT_ID).mV[VY];
	mArmSpan = 2.f * llmax(fabsf(reach_left), fabsf(reach_right));
}

void LLPuppetMotion::queueEvent(const LLPuppetEvent& puppet_event)
{
	// Adjust the timestamp for local clock and push into the future to
	// allow interpolation
	S32 remote_timestamp = puppet_event.getTimestamp();
	S32 now = (S32)(LLFrameTimer::getElapsedSeconds() * 1000.0);
	S32 clock_skew = now - remote_timestamp;
	if (mRemoteToLocalClockOffset == std::numeric_limits<F32>::min())
	{
		mRemoteToLocalClockOffset = (F32)(clock_skew);
	}
	else
	{
		// Compute a running average
		constexpr F32 DEL = 0.05f;
		mRemoteToLocalClockOffset = (1.f - DEL) * mRemoteToLocalClockOffset +
									DEL * clock_skew;
	}
	S32 local_timestamp = remote_timestamp + S32(mRemoteToLocalClockOffset);

	// Split puppet_event into joint-specific streams
	for (const auto& joint_event : puppet_event.mJointEvents)
	{
		S16 joint_id = joint_event.getJointID();
		if (!mJointStates.count(joint_id))
		{
			// Ignore this unknown joint_id
			continue;
		}
		DelayedEventQueue& queue = mEventQueues[joint_id];
		queue.addEvent(remote_timestamp, local_timestamp, joint_event);
	}
}

//virtual
bool LLPuppetMotion::onUpdate(F32 time, U8* joint_mask)
{
	if (!sIsPuppetryEnabled || mJointStates.empty())
	{
		return false;
	}

	// On each update we push mStopTimestamp into the future. If the updates
	// stop happening then this Motion will be stopped.
	if (!mStopped)
	{
		constexpr F32 INACTIVITY_TIMEOUT = 2.f; // In seconds
		mStopTimestamp = mActivationTimestamp + time + INACTIVITY_TIMEOUT;
	}

	S32 now = S32(LLFrameTimer::getElapsedSeconds() * 1000.0);
	if (mIsSelf)
	{
		// *TODO: combine the two event maps into one vector of targets
		// LLIK::Solver::joint_config_map_t targets;
		updateFromExpression(now);
		pumpOutgoingEvents();
		if (LLPuppetModule::getInstance()->getEcho())
		{
			// Check for updates from server if we are echoing from there
			updateFromBroadcast(now);
		}
	}
	else
	{
		// Some other agent: just update from any incoming data
		updateFromBroadcast(now);
	}

	U32 jcount = mJointsToRemoveFromPose.size();
	if (jcount)
	{
		for (U32 i = 0; i < jcount; ++i)
		{
			LLPointer<LLJointState>& jstatep = mJointsToRemoveFromPose[i];
			if (jstatep)
			{
				jstatep->setUsage(0);
				mPose.removeJointState(jstatep);
			}
		}
		mJointsToRemoveFromPose.clear();
	}

	// Expire joints that have not been updated in a while
	if (now > mNextJointStateExpiry)
	{
		mNextJointStateExpiry = S32_MAX;
		expiries_map_t::iterator it = mJointStateExpiries.begin();
		while (it != mJointStateExpiries.end())
		{
			JointStateExpiry& jstate_expiryp = it->second;
			if (now > jstate_expiryp.mExpiry)
			{
				// Instead of removing the joint from mPose during this
				// onUpdate(), we set its priority LOW and clear its local
				// rotation which will reset the avatar's joint...  If no
				// other animations contribute to it. We will remove it from
				// mPose next onUpdate().
				jstate_expiryp.mState->setPriority(LLJoint::LOW_PRIORITY);
				jstate_expiryp.mState->setRotation(LLQuaternion::DEFAULT);
				mJointsToRemoveFromPose.push_back(jstate_expiryp.mState);
				it = mJointStateExpiries.erase(it);
			}
			else
			{
				if (jstate_expiryp.mExpiry < mNextJointStateExpiry)
				{
					mNextJointStateExpiry = jstate_expiryp.mExpiry;
				}
				++it;
			}
		}
	}

	// We must return true else LLMotionController will stop and purge this
	// motion.
	//
	// *TODO ?  Figure out when to return false so that LLMotionController can
	// reduce its idle load. Also will need to plumb LLPuppetModule to be able
	// to reintroduce this motion to the controller when puppetry restarts.
	return true;
}

//virtual
bool LLPuppetMotion::onActivate()
{
	mStopTimestamp = 0.f;
	return true;
}

//virtual
void LLPuppetMotion::onDeactivate()
{
	mPose.removeAllJointStates();
	mJointsToRemoveFromPose.clear();
	for (state_map_t::iterator it = mJointStates.begin(),
							   end = mJointStates.end();
		 it != end; ++it)
	{
		LLPointer<LLJointState>& jstatep = it->second;
		if (jstatep)
		{
			jstatep->setUsage(0);
		}
	}
	// Clear solver memory.
	LLIK::Solver::joint_config_map_t empty_configs;
	mIKSolver.updateJointConfigs(empty_configs);
}

void LLPuppetMotion::collectJoints(LLJoint* joint)
{
	// The PuppetMotion controller starts with the passed joint and recurses
	// into its children, collecting all the joints and putting them under
	// control of this motion controller.

	if (!joint->isBone())
	{
		return;
	}

	S16 parent_id = joint->getParent()->getJointNum();

	// BEGIN HACK: bypass mSpine joints
	//
	// mTorso	6
	//	|
	// mSpine4   5
	//	|
	// mSpine3   4
	//	|
	// mChest	3
	//	|
	// mSpine2   2
	//	|
	// mSpine1   1
	//	|
	// mPelvis   0

	while (joint->getName().rfind("mSpine", 0) == 0)
	{
		for (LLJoint::child_list_t::iterator itr = joint->mChildren.begin();
			 itr != joint->mChildren.end(); ++itr)
		{
			if ((*itr)->isBone())
			{
				joint = *itr;
				break;
			}
		}
	}
	// END HACK

	LLPointer<LLJointState> jstatep = new LLJointState(joint);
	S16 joint_id = joint->getJointNum();
	mJointStates[joint_id] = jstatep;
	LLIK::Constraint::ptr_t constraint =
		LLIKConstraintFactory::getInstance()->getConstrForJoint(joint->getName());
	mIKSolver.addJoint(joint_id, parent_id, joint, constraint);

	// Recurse through the children of this joint and add them to our joint
	// control list
	for (LLJoint::child_list_t::iterator it = joint->mChildren.begin(),
										 end = joint->mChildren.end();
		 it != end; ++it)
	{
		collectJoints(*it);
	}
}

void LLPuppetMotion::pumpOutgoingEvents()
{
	if (mBroadcastTimer.hasExpired())
	{
		packEvents();
		mBroadcastTimer.resetWithExpiry(PUPPET_BROADCAST_INTERVAL);
	}
}

void LLPuppetMotion::queueOutgoingEvent(const LLPuppetEvent& event)
{
	mOutgoingEvents.emplace_back(event);
}

void LLPuppetMotion::packEvents()
{
	if (mOutgoingEvents.empty())
	{
		return;
	}

	if (!sIsPuppetryEnabled || sPuppeteerEventMaxSize < 30)
	{
		llwarns_once << "Puppetry enabled=" << sIsPuppetryEnabled
					 << " - event_window=" << sPuppeteerEventMaxSize
					 << llendl;
		mOutgoingEvents.clear();
		return;
	}

	std::array<U8, PUPPET_MAX_MSG_BYTES> puppet_pack_buffer;

	LLDataPackerBinaryBuffer data_packer(puppet_pack_buffer.data(),
										 sPuppeteerEventMaxSize);

	// Send the agent and session information.
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_AgentAnimation);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	S32 msgblock_count = 0;

	S32 joint_count = 0;
	update_deq_t::iterator event = mOutgoingEvents.begin();
	while (event != mOutgoingEvents.end())
	{
		data_packer.reset();

		// While the datapacker can fit at least some of the current event in
		// the buffer...
		while (event != mOutgoingEvents.end() &&
			   data_packer.getCurrentSize() +
			   event->getMinEventSize() < data_packer.getBufferSize())
		{
			S32 packed_joints = 0;
			bool all_done = event->pack(data_packer, packed_joints);
			joint_count += packed_joints;
			++msgblock_count;
			if (!all_done)
			{
				// Pack was not able to fit everything into this buffer it is
				// full so time to send it.
				break;
			}
			++event;
		}

		// If datapacker has some data, we should put it into the message and
		// perhaps send it.
		if (data_packer.getCurrentSize() > 0)
		{
			if (msg->getCurrentSendTotal() +
				data_packer.getCurrentSize() + 16 >= MTUBYTES)
			{
				LL_DEBUGS("PuppetrySpam") << "Message would overflow MTU, sending message with "
										  << msgblock_count << " blocks and "
										  << joint_count << " joints."
										  << LL_ENDL;
				// Send the old message and get a new one ready.
				gAgent.sendMessage();
				joint_count = 0;
				msgblock_count = 0;
				// Create the next message header
				msg->newMessageFast(_PREHASH_AgentAnimation);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			}

			msg->nextBlockFast(_PREHASH_PhysicalAvatarEventList);
			msg->addBinaryDataFast(_PREHASH_TypeData,
								   puppet_pack_buffer.data(),
								   data_packer.getCurrentSize());
		}
	}

	mOutgoingEvents.clear();

	if (msgblock_count)
	{
		LL_DEBUGS("PuppetrySpam") << "Sending message with "
								  << msgblock_count << " blocks and "
								  << joint_count << " joints." << LL_ENDL;
		 // There are some events that were not sent above. Send them along.
		gAgent.sendMessage();
	}
	else
	{
		// Clean up the message we started
		msg->clearMessage();
	}
}

void LLPuppetMotion::unpackEvents(LLMessageSystem* mesgsys, int blocknum)
{
	std::array<U8, PUPPET_MAX_MSG_BYTES> puppet_pack_buffer;

	LLDataPackerBinaryBuffer data_packer(puppet_pack_buffer.data(),
										 PUPPET_MAX_MSG_BYTES);
	data_packer.reset();

	S32 data_size = mesgsys->getSizeFast(_PREHASH_PhysicalAvatarEventList,
										  blocknum, _PREHASH_TypeData);
	mesgsys->getBinaryDataFast(_PREHASH_PhysicalAvatarEventList,
							   _PREHASH_TypeData, puppet_pack_buffer.data(),
							   data_size , blocknum,PUPPET_MAX_MSG_BYTES);

	LLPuppetEvent event;
	if (event.unpack(data_packer))
	{
		queueEvent(event);
	}
	else
	{
		llwarns_sparse << "Invalid puppetry packet received. Rejecting !"
					   << llendl;
	}

	// HACK: set mPose weight < 1.f to trigger non-idle updates in
	// MotionController
	if (mPose.getWeight() == 1.f && mPose.getNumJointStates() == 0)
	{
		mPose.setWeight(0.999f);
	}
}

//static
void LLPuppetMotion::setPuppetryEnabled(bool enabled, size_t event_size)
{
	bool was_enabled = sIsPuppetryEnabled;
	sPuppeteerEventMaxSize = llmin(event_size, size_t(255));
	sIsPuppetryEnabled = enabled && sPuppeteerEventMaxSize > 0 &&
						 gSavedSettings.getBool("PuppetryAllowed");
	if (sIsPuppetryEnabled)
	{
		llinfos << "Puppetry is enabled with event window of " << event_size
				<< " bytes." << llendl;
		return;
	}

	// Unload any running puppetry plugin... HB
	if (was_enabled)
	{
		LLPuppetModule* modulep = LLPuppetModule::getInstance();
		if (modulep->havePuppetModule())
		{
			modulep->setSending(false);
			modulep->setEcho(false);
			modulep->clearLeapModule();
		}
	}
	llinfos << "Puppetry is disabled." << llendl;
}

//static
void LLPuppetMotion::updatePuppetryEnabling()
{
	if (sIsPuppetryEnabled != gSavedSettings.getBool("PuppetryAllowed"))
	{
		// If enablement changed, refresh the status.
		requestPuppetryStatus(gAgent.getRegion());
	}
}

//static
void LLPuppetMotion::requestPuppetryStatus(LLViewerRegion* regionp)
{
	if (!regionp) return;

	// Turn off puppetry while we ask the simulator
	setPuppetryEnabled(false, 0);

	if (!gSavedSettings.getBool("PuppetryAllowed"))
	{
		return;	// Forget it: the user does not want puppetry at all !  HB
	}

	std::string cap = regionp->getCapability("Puppetry");
	if (cap.empty())
	{
		return;
	}

	gCoros.launch("RequestPuppetryStatusCoro",
				  [cap]()
				  {
						requestPuppetryStatusCoro(cap);
				  });
}

void LLPuppetMotion::requestPuppetryStatusCoro(const std::string& capurl)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("requestPuppetryStatusCoro");
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setFollowRedirects(true);

	S32 retry_count = 0;
	LLSD result;
	LLCore::HttpStatus status;
	while (true)
	{
		result = adapter.getAndSuspend(capurl, options);
		status =
			LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
		if (status.getType() == HTTP_NOT_FOUND)
		{
			// There seems to be a case at first login where the simulator is
			// slow getting all of the caps connected for the agent. It has
			// given us back the cap URL but returns a 404 when we try and hit
			// it. Pause, take a breath and give it another shot.
			if (++retry_count >= 3)
			{
				llwarns << "Failed to get puppetry information." << llendl;
				return;
			}
			llcoro::suspendUntilTimeout(0.25f);
		}
		else if (!status)
		{
			llwarns << "Failed to get puppetry information." << llendl;
			return;
		}
		else
		{
			break;	// Success
		}
	}

	size_t event_size = result["event_size"].asInteger();
	// Maybe turn on puppetry (depending on user choice) and set the event size
	LLPuppetMotion::setPuppetryEnabled(true, event_size);
	if (!sIsPuppetryEnabled)
	{
		return;
	}

	LLPuppetModule::getInstance()->parsePuppetryResponse(result);

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!isAgentAvatarValid() || !regionp ||
		!regionp->getRegionFlag(REGION_FLAGS_ENABLE_ANIMATION_TRACKING))
	{
		return;
	}
	F32 period = DEFAULT_ATTACHMENT_UPDATE_PERIOD;
	if (result.has("update_period"))
	{
		period = result["update_period"].asReal();
	}
	gAgentAvatarp->setAttachmentUpdatePeriod(period);
}

// LLPuppetMotion::DelayedEventQueue sub-class

void LLPuppetMotion::DelayedEventQueue::addEvent(S32 remote_timestamp,
												 S32 local_timestamp,
												 const LLPuppetJointEvent& event)
{
	if (mLastRemoteTimestamp != -1)
	{
		// Dynamically measure mEventPeriod and mEventJitter
		constexpr F32 DEL = 0.1f;
		S32 this_period = remote_timestamp - mLastRemoteTimestamp;
		mEventJitter = (1.f - DEL) * mEventJitter +
					   DEL * fabsf(mEventPeriod - (F32)this_period);

		// mEventPeriod is a running average of the period between events
		mEventPeriod = (1.f - DEL) * mEventPeriod + DEL * this_period;
	}
	mLastRemoteTimestamp = remote_timestamp;

	// We push event into the future so we have something to interpolate toward
	// while we wait for the next
	S32 delayed_timestamp = local_timestamp + S32(mEventPeriod + mEventJitter);
	mQueue.emplace_back(delayed_timestamp, event);
}
