/**
 * @file lljoint.cpp
 * @brief Implementation of LLJoint class.
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

#include "linden_common.h"

#include "lljoint.h"

#include "llmath.h"

S32 LLJoint::sNumUpdates = 0;
S32 LLJoint::sNumTouches = 0;
std::vector<std::string> LLJoint::sJointNamesList;
joint_alias_map_t LLJoint::sAvatarJointAliasMap;

template <class T>
LL_INLINE bool attachment_map_iter_compare_key(const T& a, const T& b)
{
	return a.first < b.first;
}

bool LLVector3OverrideMap::findActiveOverride(LLUUID& mesh_id,
											  LLVector3& pos) const
{
	pos.setZero();
	mesh_id.setNull();
	bool found = false;

	map_t::const_iterator it =
		std::max_element(mMap.begin(), mMap.end(),
						 attachment_map_iter_compare_key<map_t::value_type>);
	if (it != mMap.end())
	{
		found = true;
		pos = it->second;
		mesh_id = it->first;
	}

	return found;
}

void LLVector3OverrideMap::showJointVector3Overrides(std::ostringstream& os) const
{
	map_t::const_iterator max_it =
		std::max_element(mMap.begin(), mMap.end(),
						 attachment_map_iter_compare_key<map_t::value_type>);
	for (map_t::const_iterator it = mMap.begin(), end = mMap.end();
		 it != end; ++it)
	{
		const LLVector3& pos = it->second;
		os << " " << "[" << it->first <<": " << pos << "]"
		   << (it == max_it ? "*" : "");
	}
}

//static
U32 LLJoint::getKey(const std::string& name, bool add_if_unknown)
{
	if (sJointNamesList.empty())
	{
		// These are the "well known" joints (used in the code). If you change
		// this list, be sure to change the corresponding constants in
		// lljoint.h !
		sJointNamesList.reserve(LL_CHARACTER_MAX_ANIMATED_JOINTS + 1);
		sJointNamesList.emplace_back("unnamed");
		sJointNamesList.emplace_back("mScreen");
		sJointNamesList.emplace_back("mRoot");
		sJointNamesList.emplace_back("mPelvis");
		sJointNamesList.emplace_back("mTorso");
		sJointNamesList.emplace_back("mChest");
		sJointNamesList.emplace_back("mNeck");
		sJointNamesList.emplace_back("mHead");
		sJointNamesList.emplace_back("mSkull");
		sJointNamesList.emplace_back("mHipLeft");
		sJointNamesList.emplace_back("mHipRight");
		sJointNamesList.emplace_back("mKneeLeft");
		sJointNamesList.emplace_back("mKneeRight");
		sJointNamesList.emplace_back("mAnkleLeft");
		sJointNamesList.emplace_back("mAnkleRight");
		sJointNamesList.emplace_back("mFootLeft");
		sJointNamesList.emplace_back("mFootRight");
		sJointNamesList.emplace_back("mWristLeft");
		sJointNamesList.emplace_back("mWristRight");
		sJointNamesList.emplace_back("mEyeLeft");
		sJointNamesList.emplace_back("mEyeRight");
		sJointNamesList.emplace_back("mElbowLeft");
		sJointNamesList.emplace_back("mShoulderLeft");
		sJointNamesList.emplace_back("mFaceEyeAltLeft");
		sJointNamesList.emplace_back("mFaceEyeAltRight");
	}

	for (U32 i = 0, count = sJointNamesList.size(); i < count; ++i)
	{
		if (sJointNamesList[i] == name)
		{
			return i;
		}
	}

	U32 ret = 0;
	if (add_if_unknown)
	{
		ret = sJointNamesList.size();
		sJointNamesList.emplace_back(name);
	}
	return ret;
}

//static
U32 LLJoint::getAliasedJointKey(const std::string& name)
{
	U32 key = getKey(name, false);
	if (key != 0)
	{
		return key;
	}
	joint_alias_map_t::const_iterator it = sAvatarJointAliasMap.find(name);
	if (it == sAvatarJointAliasMap.end())
	{
		return 0;
	}
	return getKey(it->second, false);
}

//static
const std::string& LLJoint::getName(U32 key)
{
	return (size_t)key < sJointNamesList.size() ? sJointNamesList[key]
												: sJointNamesList[0];
}

void LLJoint::init()
{
	mKey = 0;
	mParent = NULL;
	mXform.setScaleChildOffset(true);
	mXform.setScale(LLVector3(1.0f, 1.0f, 1.0f));
	mDirtyFlags = MATRIX_DIRTY | ROTATION_DIRTY | POSITION_DIRTY;
	mUpdateXform = true;
	mSupport = SUPPORT_BASE;
	mEnd = LLVector3(0.f, 0.f, 0.f);
}

LLJoint::LLJoint()
:	mJointNum(-1),
	mIsBone(false)
{
	init();
	touch();
}

LLJoint::LLJoint(const std::string& name, LLJoint* parent)
:	mJointNum(-2),
	mIsBone(false)
{
	init();
	mUpdateXform = false;

	setName(name);
	if (parent)
	{
		parent->addChild(this);
	}
	touch();
}

LLJoint::~LLJoint()
{
	if (mParent)
	{
		mParent->removeChild(this);
	}
	removeAllChildren();
}

void LLJoint::setup(const std::string& name, LLJoint* parent)
{
	setName(name);
	if (parent)
	{
		parent->addChild(this);
	}
}

void LLJoint::setSupport(const std::string& support_name)
{
	if (support_name == "extended")
	{
		setSupport(SUPPORT_EXTENDED);
	}
	else if (support_name == "base")
	{
		setSupport(SUPPORT_BASE);
	}
	else
	{
		llwarns_once << "Unknown support base name: " << support_name
					 << ". Using default support base." << llendl;
		setSupport(SUPPORT_BASE);
	}
}

void LLJoint::touch(U32 flags)
{
	if ((flags | mDirtyFlags) != mDirtyFlags)
	{
		++sNumTouches;
		mDirtyFlags |= flags;
		U32 child_flags = flags;
		if (flags & ROTATION_DIRTY)
		{
			child_flags |= POSITION_DIRTY;
		}

		for (S32 i = 0, count = mChildren.size(); i < count; ++i)
		{
			LLJoint* joint = mChildren[i];
			if (joint)	// Paranoia
			{
				joint->touch(child_flags);
			}
		}
	}
}

void LLJoint::setJointNum(S32 joint_num)
{
	mJointNum = joint_num;
	if (mJointNum + 2 >= (S32)LL_CHARACTER_MAX_ANIMATED_JOINTS)
	{
		llwarns << "Joint number " << joint_num << " + 2 is too large for "
				<< LL_CHARACTER_MAX_ANIMATED_JOINTS << llendl;
		llassert(false);
	}
}

LLJoint* LLJoint::getRoot()
{
	LLJoint* parent = getParent();
	return parent ? parent->getRoot() : this;
}

LLJoint* LLJoint::findJoint(U32 key)
{
	if (key == mKey)
	{
		return this;
	}

	for (S32 i = 0, count = mChildren.size(); i < count; ++i)
	{
		LLJoint* joint = mChildren[i];
		if (joint)	// Paranoia
		{
			LLJoint* found = joint->findJoint(key);
			if (found)
			{
				return found;
			}
		}
	}

	return NULL;
}

void LLJoint::addChild(LLJoint* joint)
{
	if (!joint)
	{
		llwarns << "Tried to add a NULL joint (ignored) !" << llendl;
		return;
	}

	if (joint->mParent)
	{
		joint->mParent->removeChild(joint);
	}

	mChildren.push_back(joint);
	joint->mXform.setParent(&mXform);
	joint->mParent = this;
	joint->touch();
}

void LLJoint::removeChild(LLJoint* joint)
{
	if (!joint)
	{
		llwarns << "Tried to remove a NULL joint (ignored) !" << llendl;
		return;
	}

	for (S32 i = 0, count = mChildren.size(); i < count; ++i)
	{
		if (joint == mChildren[i])
		{
			if (i < count - 1)
			{
				mChildren[i] = mChildren[count - 1];
			}
			mChildren.pop_back();

			joint->mXform.setParent(NULL);
			joint->mParent = NULL;
			joint->touch();
			return;
		}
	}
}

void LLJoint::removeAllChildren()
{
	for (S32 i = 0, count = mChildren.size(); i < count; ++i)
	{
		LLJoint* joint = mChildren[i];
		if (joint)	// Paranoia
		{
			joint->mXform.setParent(NULL);
			joint->mParent = NULL;
			joint->touch();
		}
	}
	mChildren.clear();
}

void LLJoint::setPosition(const LLVector3& requested_pos, bool do_override)
{
	LLVector3 pos(requested_pos);

	if (do_override)
	{
		LLVector3 active_override;
		LLUUID mesh_id;
		if (mAttachmentPosOverrides.findActiveOverride(mesh_id,
													   active_override))
		{
			pos = active_override;
			LL_DEBUGS("Avatar") << "Joint: " << getName()
								<< " - Requested pos: " << requested_pos
								<< " overridden by attachment to: " << pos
								<< LL_ENDL;
		}
	}

	if (pos != mXform.getPosition())
	{
		mXform.setPosition(pos);
		touch(MATRIX_DIRTY | POSITION_DIRTY);
	}
}

void showJointPosOverrides(const LLJoint& joint, const std::string& note,
						   const std::string& av_info)
{
	std::ostringstream os;
	os << joint.mPosBeforeOverrides;
	joint.mAttachmentPosOverrides.showJointVector3Overrides(os);
	LL_DEBUGS("Avatar") << "Avatar: " << av_info
						<< " - Joint: " << joint.getName()
						<< " " << note << " " << os.str() << LL_ENDL;
}

void showJointScaleOverrides(const LLJoint& joint, const std::string& note,
							 const std::string& av_info)
{
	std::ostringstream os;
	os << joint.mScaleBeforeOverrides;
	joint.mAttachmentScaleOverrides.showJointVector3Overrides(os);
	LL_DEBUGS("Avatar") << "Avatar: " << av_info
						<< " - Joint: " << joint.getName()
						<< " " << note << " " << os.str() << LL_ENDL;
}

constexpr F32 MAX_SQUARED_OFFSET = LL_JOINT_TRESHOLD_POS_OFFSET *
								   LL_JOINT_TRESHOLD_POS_OFFSET;

bool LLJoint::aboveJointPosThreshold(const LLVector3& pos) const
{
	LLVector3 diff = pos - mDefaultPosition;
	return diff.lengthSquared() > MAX_SQUARED_OFFSET;
}

bool LLJoint::aboveJointScaleThreshold(const LLVector3& scale) const
{
	LLVector3 diff = scale - mDefaultScale;
	return diff.lengthSquared() > MAX_SQUARED_OFFSET;
}

void LLJoint::addAttachmentPosOverride(const LLVector3& pos,
									   const LLUUID& mesh_id,
									   const std::string& av_info,
									   bool* active_override_changed)
{
	if (active_override_changed)
	{
		*active_override_changed = false;
	}
	if (mesh_id.isNull())
	{
		return;
	}

	LLUUID id;
	LLVector3 before_pos;
	bool active_override_before = hasAttachmentPosOverride(before_pos, id);

	if (!mAttachmentPosOverrides.count())
	{
		LL_DEBUGS("Avatar") << "Avatar: " << av_info << " - Joint: "
							<< getName() << " - Saving mPosBeforeOverrides: "
							<< getPosition() << LL_ENDL;
		mPosBeforeOverrides = getPosition();
	}
	mAttachmentPosOverrides.add(mesh_id, pos);

	LLVector3 after_pos;
	hasAttachmentPosOverride(after_pos, id);
	if (!active_override_before || after_pos != before_pos)
	{
		if (active_override_changed)
		{
			*active_override_changed = true;
		}
		LL_DEBUGS("Avatar") << "Avatar: " << av_info << " - Joint: "
							<< getName() << " - Position for mesh '"
							<< mesh_id << "': " << pos << LL_ENDL;
		updatePos(av_info);
	}
}

void LLJoint::removeAttachmentPosOverride(const LLUUID& mesh_id,
										  const std::string& av_info,
										  bool* active_override_changed)
{
	if (active_override_changed)
	{
		*active_override_changed = false;
	}
	if (mesh_id.isNull())
	{
		return;
	}

	LLUUID id;
	LLVector3 before_pos;
	bool active_override_before = hasAttachmentPosOverride(before_pos, id);

	if (mAttachmentPosOverrides.remove(mesh_id))
	{
		LLVector3 after_pos;
		hasAttachmentPosOverride(after_pos, id);
		if (!active_override_before || after_pos != before_pos)
		{
			if (active_override_changed)
			{
				*active_override_changed = true;
			}
			bool show = false;
			LL_DEBUGS("Avatar") << "Avatar: " << av_info;
			show = true;
			LL_CONT	<< " - Joint: " << getName()
					<< " - Removing pos override for mesh: " << mesh_id
					<< LL_ENDL;
			if (show)
			{
				showJointPosOverrides(*this, "remove", av_info);
			}
			updatePos(av_info);
		}
	}
}

void LLJoint::clearAttachmentPosOverrides()
{
	if (mAttachmentPosOverrides.count())
	{
		mAttachmentPosOverrides.clear();
		setPosition(mPosBeforeOverrides);
	}
}

S32 LLJoint::getAllAttachmentPosOverrides(std::set<LLVector3>& overrides) const
{
	for (LLVector3OverrideMap::map_t::const_iterator
			it = mAttachmentPosOverrides.getMap().begin(),
			end = mAttachmentPosOverrides.getMap().end();
		 it != end; ++it)
	{
		overrides.emplace(it->second);
	}
	return mAttachmentPosOverrides.count();
}

S32 LLJoint::getAllAttachmentScaleOverrides(std::set<LLVector3>& overrides) const
{
	for (LLVector3OverrideMap::map_t::const_iterator
			it = mAttachmentScaleOverrides.getMap().begin(),
			end = mAttachmentScaleOverrides.getMap().end();
		 it != end; ++it)
	{
		overrides.emplace(it->second);
	}
	return mAttachmentScaleOverrides.count();
}

void LLJoint::updatePos(const std::string& av_info)
{
	LLVector3 pos, found_pos;
	LLUUID mesh_id;
	if (mAttachmentPosOverrides.findActiveOverride(mesh_id, found_pos))
	{
		LL_DEBUGS("Avatar") << "Avatar: " << av_info
							<< " - Joint: " << getName()
							<< " - Winner of "
							<< mAttachmentPosOverrides.count()
							<< " is mesh " << mesh_id
							<< " - Position = " << found_pos << LL_ENDL;
		setPosition(found_pos);
	}
	else
	{
		LL_DEBUGS("Avatar") << "Avatar: " << av_info
							 << " - Joint: " << getName()
							<< " - Winner is mPosBeforeOverrides = "
							<< mPosBeforeOverrides << LL_ENDL;
		setPosition(mPosBeforeOverrides);
	}
}

void LLJoint::updateScale(const std::string& av_info)
{
	LLVector3 scale, found_scale;
	LLUUID mesh_id;
	if (mAttachmentScaleOverrides.findActiveOverride(mesh_id, found_scale))
	{
		LL_DEBUGS("Avatar") << "Avatar: " << av_info
							<< " - Joint: " << getName()
							<< " - Winner of "
							<< mAttachmentScaleOverrides.count()
							<< " is mesh " << mesh_id
							<< " - Position = " << found_scale << LL_ENDL;
		setScale(found_scale);
	}
	else
	{
		LL_DEBUGS("Avatar") << "Avatar: " << av_info
							 << " - Joint: " << getName()
							<< " - Winner is mScaleBeforeOverrides = "
							<< mPosBeforeOverrides << LL_ENDL;
		setScale(mScaleBeforeOverrides);
	}
}

void LLJoint::addAttachmentScaleOverride(const LLVector3& scale,
										 const LLUUID& mesh_id,
										 const std::string& av_info)
{
	if (mesh_id.isNull())
	{
		return;
	}

	if (!mAttachmentScaleOverrides.count())
	{
		LL_DEBUGS("Avatar") << "Avatar: " << av_info << " - Joint: "
							<< getName() << " - Saving mScaleBeforeOverrides: "
							<< getScale() << LL_ENDL;
		mScaleBeforeOverrides = getScale();
	}
	mAttachmentScaleOverrides.add(mesh_id, scale);

	LL_DEBUGS("Avatar") << "Avatar: " << av_info << " - Joint: "
						<< getName() << " - Scale for mesh '"
							<< mesh_id << "': " << scale << LL_ENDL;
	updateScale(av_info);
}

void LLJoint::removeAttachmentScaleOverride(const LLUUID& mesh_id,
											const std::string& av_info)
{
	if (mesh_id.isNull())
	{
		return;
	}

	if (mAttachmentScaleOverrides.remove(mesh_id))
	{
		bool show = false;
		LL_DEBUGS("Avatar") << "Avatar: " << av_info;
		show = true;
		LL_CONT << " - Joint: " << getName()
				<< " - Removing scale override for mesh: " << mesh_id
				<< LL_ENDL;
		if (show)
		{
			showJointScaleOverrides(*this, "remove", av_info);
		}
		updateScale(av_info);
	}
}

void LLJoint::clearAttachmentScaleOverrides()
{
	if (mAttachmentScaleOverrides.count())
	{
		mAttachmentScaleOverrides.clear();
		setScale(mScaleBeforeOverrides);
	}
}

const LLVector3& LLJoint::getWorldPosition()
{
	updateWorldPRSParent();
	return mXform.getWorldPosition();
}

const LLVector3& LLJoint::getLastWorldPosition() const
{
	return mXform.getWorldPosition();
}

void LLJoint::setWorldPosition(const LLVector3& pos)
{
	if (mParent == NULL)
	{
		setPosition(pos);
		return;
	}

	LLMatrix4 temp_matrix = getWorldMatrix();
	temp_matrix.mMatrix[VW][VX] = pos.mV[VX];
	temp_matrix.mMatrix[VW][VY] = pos.mV[VY];
	temp_matrix.mMatrix[VW][VZ] = pos.mV[VZ];

	LLMatrix4 parent_matrix = mParent->getWorldMatrix();

	temp_matrix *= parent_matrix.invert();

	LLVector3 local_pos(temp_matrix.mMatrix[VW][VX],
						temp_matrix.mMatrix[VW][VY],
						temp_matrix.mMatrix[VW][VZ]);

	setPosition(local_pos);
}

const LLQuaternion& LLJoint::getRotation() const
{
	return mXform.getRotation();
}

void LLJoint::setRotation(const LLQuaternion& rot)
{
	if (rot.isFinite())
	{
#if 0
		if (mXform.getRotation() != rot)
#endif
		{
			mXform.setRotation(rot);
			touch(MATRIX_DIRTY | ROTATION_DIRTY);
		}
	}
}

const LLQuaternion& LLJoint::getWorldRotation()
{
	updateWorldPRSParent();
	return mXform.getWorldRotation();
}

const LLQuaternion& LLJoint::getLastWorldRotation()
{
	return mXform.getWorldRotation();
}

void LLJoint::setWorldRotation(const LLQuaternion& rot)
{
	if (mParent == NULL)
	{
		setRotation(rot);
		return;
	}

	LLMatrix4 temp_mat(rot);

	LLMatrix4 parent_matrix = mParent->getWorldMatrix();
	parent_matrix.mMatrix[VW][VX] = 0;
	parent_matrix.mMatrix[VW][VY] = 0;
	parent_matrix.mMatrix[VW][VZ] = 0;

	temp_mat *= parent_matrix.invert();

	setRotation(LLQuaternion(temp_mat));
}

const LLVector3& LLJoint::getScale() const
{
	return mXform.getScale();
}

void LLJoint::setScale(const LLVector3& scale, bool apply_attachment_overrides)
{
	if (apply_attachment_overrides)
	{
		LLUUID mesh_id;
		LLVector3 active_override;
		if (mAttachmentScaleOverrides.findActiveOverride(mesh_id,
														 active_override))
		{
			if (scale != active_override)
			{
				LL_DEBUGS("Avatar") << "Joint: " << getName() << " - Mesh Id: "
									<< mesh_id << " - Requested scale: "
									<< scale << " overriden by attachment to: "
									<< active_override << LL_ENDL;
			}
			mXform.setScale(active_override);
			touch();
			return;
		}
	}

	mXform.setScale(scale);
	touch();
}

const LLMatrix4& LLJoint::getWorldMatrix()
{
	updateWorldMatrixParent();
	return mXform.getWorldMatrix();
}

void LLJoint::updateWorldMatrixParent()
{
	if (mDirtyFlags & MATRIX_DIRTY)
	{
		LLJoint* parent = getParent();
		if (parent)
		{
			parent->updateWorldMatrixParent();
		}
		updateWorldMatrix();
	}
}

void LLJoint::updateWorldPRSParent()
{
	if (mDirtyFlags & (ROTATION_DIRTY | POSITION_DIRTY))
	{
		LLJoint* parent = getParent();
		if (parent)
		{
			parent->updateWorldPRSParent();
		}

		mXform.update();
		mDirtyFlags &= ~(ROTATION_DIRTY | POSITION_DIRTY);
	}
}

void LLJoint::updateWorldMatrixChildren()
{
	if (!mUpdateXform) return;

	if (mDirtyFlags & MATRIX_DIRTY)
	{
		updateWorldMatrix();
	}
	for (S32 i = 0, count = mChildren.size(); i < count; ++i)
	{
		LLJoint* joint = mChildren[i];
		if (joint)	// Paranoia
		{
			joint->updateWorldMatrixChildren();
		}
	}
}

void LLJoint::updateWorldMatrix()
{
	if (mDirtyFlags & MATRIX_DIRTY)
	{
		++sNumUpdates;
		mXform.updateMatrix(false);
		mDirtyFlags = 0x0;
	}
}
