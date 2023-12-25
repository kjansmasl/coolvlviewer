/**
 * @file lljoint.h
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

#ifndef LL_LLJOINT_H
#define LL_LLJOINT_H

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "hbfastmap.h"
#include "llxform.h"

class LLAvatarJoint;
class LLViewerJoint;

constexpr S32 LL_CHARACTER_MAX_JOINTS_PER_MESH = 15;
// Need to set this to count of animate-able joints, currently = #bones +
// #collision_volumes + #attachments + 2, rounded to next multiple of 4.
constexpr U32 LL_CHARACTER_MAX_ANIMATED_JOINTS = 216;
constexpr U32 LL_MAX_JOINTS_PER_MESH_OBJECT = 110;

constexpr F32 LL_JOINT_TRESHOLD_POS_OFFSET = 0.0001f;	// 0.1 mm

// These should be higher than the joint_num of any other joint, to avoid
// conflicts in updateMotionsByType()
constexpr U32 LL_HAND_JOINT_NUM = LL_CHARACTER_MAX_ANIMATED_JOINTS - 1;
constexpr U32 LL_FACE_JOINT_NUM = LL_CHARACTER_MAX_ANIMATED_JOINTS - 2;

constexpr S32 LL_CHARACTER_MAX_PRIORITY = 7;
constexpr F32 LL_MAX_PELVIS_OFFSET = 5.f;

// These are the indexes of "well known" joints (used in the code) in
// sJointNamesList. If you change this list, be sure to change the static
// LLJoint::getKey() method accordingly !
constexpr U32 LL_JOINT_KEY_SCREEN		= 1;
constexpr U32 LL_JOINT_KEY_ROOT			= 2;
constexpr U32 LL_JOINT_KEY_PELVIS		= 3;
constexpr U32 LL_JOINT_KEY_TORSO		= 4;
constexpr U32 LL_JOINT_KEY_CHEST		= 5;
constexpr U32 LL_JOINT_KEY_NECK			= 6;
constexpr U32 LL_JOINT_KEY_HEAD			= 7;
constexpr U32 LL_JOINT_KEY_SKULL		= 8;
constexpr U32 LL_JOINT_KEY_HIPLEFT		= 9;
constexpr U32 LL_JOINT_KEY_HIPRIGHT		= 10;
constexpr U32 LL_JOINT_KEY_KNEELEFT		= 11;
constexpr U32 LL_JOINT_KEY_KNEERIGHT	= 12;
constexpr U32 LL_JOINT_KEY_ANKLELEFT	= 13;
constexpr U32 LL_JOINT_KEY_ANKLERIGHT	= 14;
constexpr U32 LL_JOINT_KEY_FOOTLEFT		= 15;
constexpr U32 LL_JOINT_KEY_FOOTRIGHT	= 16;
constexpr U32 LL_JOINT_KEY_WRISTLEFT	= 17;
constexpr U32 LL_JOINT_KEY_WRISTRIGHT	= 18;
constexpr U32 LL_JOINT_KEY_EYELEFT		= 19;
constexpr U32 LL_JOINT_KEY_EYERIGHT		= 20;
constexpr U32 LL_JOINT_KEY_ELBOWLEFT	= 21;
constexpr U32 LL_JOINT_KEY_SHOULDERLEFT	= 22;
constexpr U32 LL_JOINT_KEY_EYEALTLEFT	= 23;
constexpr U32 LL_JOINT_KEY_EYEALTRIGHT	= 24;

typedef std::map<std::string, std::string> joint_alias_map_t;

class LLVector3OverrideMap
{
public:
	LLVector3OverrideMap()								{}

	LL_INLINE void add(const LLUUID& mesh_id,
					   const LLVector3& pos)			{ mMap[mesh_id] = pos; }

	LL_INLINE bool remove(const LLUUID& mesh_id)		{ return mMap.erase(mesh_id) > 0; }

	LL_INLINE U32 count() const							{ return mMap.size(); }

	typedef fast_hmap<LLUUID, LLVector3> map_t;
	LL_INLINE const map_t& getMap() const				{ return mMap; }

	LL_INLINE void clear()								{ mMap.clear(); }

	bool findActiveOverride(LLUUID& mesh_id, LLVector3& pos) const;
	void showJointVector3Overrides(std::ostringstream& os) const;

private:
	map_t mMap;
};

LL_INLINE bool operator==(const LLVector3OverrideMap& a,
						  const LLVector3OverrideMap& b)
{
	return a.getMap() == b.getMap();
}

LL_INLINE bool operator!=(const LLVector3OverrideMap& a,
						  const LLVector3OverrideMap& b)
{
	return a.getMap() != b.getMap();
}

//-----------------------------------------------------------------------------
// class LLJoint
//-----------------------------------------------------------------------------

class LLJoint
{
protected:
	LOG_CLASS(LLJoint);

public:
	// Priority levels, from highest to lowest
	enum JointPriority
	{
		USE_MOTION_PRIORITY = -1,
		LOW_PRIORITY = 0,
		MEDIUM_PRIORITY,
		HIGH_PRIORITY,
		HIGHER_PRIORITY,
		HIGHEST_PRIORITY,
		ADDITIVE_PRIORITY = LL_CHARACTER_MAX_PRIORITY,
		// Extra constant just for clarity. 
		PUPPET_PRIORITY = LL_CHARACTER_MAX_PRIORITY
	};

	enum DirtyFlags
	{
		MATRIX_DIRTY = 0x1 << 0,
		ROTATION_DIRTY = 0x1 << 1,
		POSITION_DIRTY = 0x1 << 2,
		ALL_DIRTY = 0x7
	};

	enum SupportCategory
	{
		SUPPORT_BASE,
 		SUPPORT_EXTENDED
	};

	LLJoint();
	LLJoint(const std::string& name, LLJoint* parent = NULL);

	virtual ~LLJoint();

	LL_INLINE virtual LLViewerJoint* asViewerJoint()	{ return NULL; }
	LL_INLINE virtual LLAvatarJoint* asAvatarJoint()	{ return NULL; }

	// Sets name and parent
	void setup(const std::string& name, LLJoint* parent = NULL);

	void touch(U32 flags = ALL_DIRTY);

	static U32 getKey(const std::string& name, bool add_if_unknown = true);
	static U32 getAliasedJointKey(const std::string& name);
	LL_INLINE U32 getKey() const						{ return mKey; }

	static const std::string& getName(U32 key);
	LL_INLINE const std::string& getName() const		{ return sJointNamesList[mKey]; }
	LL_INLINE void setName(const std::string& name)		{ mKey = getKey(name); }

	LL_INLINE S32 getJointNum() const					{ return mJointNum; }
	void setJointNum(S32 joint_num);

	LL_INLINE bool isBone() const						{ return mIsBone; }
	LL_INLINE void setIsBone(bool is_bone)				{ mIsBone = is_bone; }

	LL_INLINE SupportCategory getSupport() const		{ return mSupport; }
	LL_INLINE void setSupport(const SupportCategory& s)	{ mSupport = s; }
	void setSupport(const std::string& support_string);

	LL_INLINE const LLVector3& getEnd() const			{ return mEnd; }
	LL_INLINE void setEnd(const LLVector3& end)			{ mEnd = end; }

	LL_INLINE LLJoint* getParent()						{ return mParent; }

	LLJoint* getRoot();

	// Searches for child joint by key
	LLJoint* findJoint(U32 key);

	// Searches for child joint by name
	LLJoint* findJoint(const std::string& name)
	{
		return findJoint(getKey(name, false));
	}

	// Searches for child joint by name, with aliases
	LL_INLINE LLJoint* findAliasedJoint(const std::string& name)
	{
		return findJoint(getAliasedJointKey(name));
	}

	void addChild(LLJoint* joint);
	void removeChild(LLJoint* joint);
	void removeAllChildren();

	// A returned number of 0 indicates an end effector, 1 a normal joint and
	// over 1 a leaf.
	LL_INLINE U32 getNumChildren() const				{ return mChildren.size(); }

	LL_INLINE const LLVector3& getPosition() const		{ return mXform.getPosition(); }
	void setPosition(const LLVector3& pos, bool do_override = false);

	LL_INLINE void setDefaultPosition(const LLVector3& pos)
	{
		mDefaultPosition = pos;
	}

	LL_INLINE const LLVector3& getDefaultPosition() const
	{
		return mDefaultPosition;
	}

	LL_INLINE void setDefaultScale(const LLVector3& scale)
	{
		mDefaultScale = scale;
	}

	LL_INLINE const LLVector3& getDefaultScale() const
	{
		return mDefaultScale;
	}

	const LLVector3& getWorldPosition();
	const LLVector3& getLastWorldPosition() const;
	void setWorldPosition(const LLVector3& pos);

	const LLQuaternion& getRotation() const;
	void setRotation(const LLQuaternion& rot);

	const LLQuaternion& getWorldRotation();
	const LLQuaternion& getLastWorldRotation();
	void setWorldRotation(const LLQuaternion& rot);

	const LLVector3& getScale() const;
	void setScale(const LLVector3& scale,
				  bool apply_attachment_overrides = false);

	const LLMatrix4& getWorldMatrix();

	void updateWorldMatrixChildren();
	void updateWorldMatrixParent();

	void updateWorldPRSParent();

	void updateWorldMatrix();

	LL_INLINE const LLVector3& getSkinOffset() const	{ return mSkinOffset; }
	LL_INLINE void setSkinOffset(const LLVector3& o)	{ mSkinOffset = o; }

	LL_INLINE LLXformMatrix* getXform()					{ return &mXform; }

	LL_INLINE virtual bool isAnimatable() const			{ return true; }

	void addAttachmentPosOverride(const LLVector3& pos,
								  const LLUUID& mesh_id,
								  const std::string& av_info,
								  bool* active_override_changed = NULL);

	void addAttachmentScaleOverride(const LLVector3& scale,
									const LLUUID& mesh_id,
									const std::string& av_info);

	void removeAttachmentPosOverride(const LLUUID& mesh_id,
									 const std::string& av_info,
									 bool* active_override_changed = NULL);

	void removeAttachmentScaleOverride(const LLUUID& mesh_id,
									   const std::string& av_info);

	LL_INLINE bool hasAttachmentPosOverride(LLVector3& pos,
											LLUUID& mesh_id) const
	{
		return mAttachmentPosOverrides.findActiveOverride(mesh_id, pos);
	}

	LL_INLINE bool hasAttachmentScaleOverride(LLVector3& scale,
											  LLUUID& mesh_id) const
	{
		return mAttachmentScaleOverrides.findActiveOverride(mesh_id, scale);
	}

	void clearAttachmentPosOverrides();
	void clearAttachmentScaleOverrides();

	S32 getAllAttachmentPosOverrides(std::set<LLVector3>& overrides) const;
	S32 getAllAttachmentScaleOverrides(std::set<LLVector3>& overrides) const;

	void updatePos(const std::string& av_info);
	void updateScale(const std::string& av_info);

	// These are used in checks of whether a pos/scale override is considered
	// significant.
	bool aboveJointPosThreshold(const LLVector3& pos) const;
	bool aboveJointScaleThreshold(const LLVector3& scale) const;

private:
	void init();

protected:
	// Explicit transformation members
	LLXformMatrix			mXform;

	// Parent joint
	LLJoint*				mParent;

	LLVector3				mDefaultPosition;
	LLVector3				mDefaultScale;

	U32						mKey;

	SupportCategory			mSupport;

public:
	// Describes the skin binding pose
	LLVector3				mSkinOffset;

	// End point of the bone, if applicable. This is only relevant for external
	// programs like Blender, and for diagnostic display.
	LLVector3				mEnd;

	S32						mJointNum;

	U32						mDirtyFlags;

	// Child joints
	typedef std::vector<LLJoint*> child_list_t;
	child_list_t			mChildren;

	// Position overrides
	LLVector3OverrideMap	mAttachmentPosOverrides;
	LLVector3				mPosBeforeOverrides;

	// Scale overrides
	LLVector3OverrideMap	mAttachmentScaleOverrides;
	LLVector3				mScaleBeforeOverrides;

	bool					mUpdateXform;
	bool					mIsBone;

	// Debug statics
	static S32				sNumTouches;
	static S32				sNumUpdates;

	static std::vector<std::string> sJointNamesList;
	static joint_alias_map_t		sAvatarJointAliasMap;
};

#endif // LL_LLJOINT_H
