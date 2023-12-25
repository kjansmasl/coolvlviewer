/**
 * @file llik.h
 * @brief Implementation of LLIK::Solver class and related helpers.
 *
 * $LicenseInfo:firstyear=2021&license=viewergpl$
 *
 * Copyright (c) 2021, Linden Research, Inc.
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

#ifndef LL_LLIK_H
#define LL_LLIK_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "hbfastmap.h"
#include "llmath.h"
#include "llsingleton.h"
#include "llvector3.h"

// EXPERIMENTAL: disabled since we hit the constraint during the swing; perhaps
// some twist can get us closer
#define LLIK_EXPERIMENTAL 0

// Inverse Kinematics (IK) for humanoid character.
//
// The Solver uses Forward and Backward Reaching Inverse Kinematics (FABRIK)
// algorightm to iterate toward a solution:
//
//	  http://andreasaristidou.com/FABRIK.html
//
// The Joints can have Constraints which limite their parent-local orientations.

class LLJoint;

namespace LLIK
{
class Solver;
class Joint;

constexpr F32 IK_DEFAULT_ACCEPTABLE_ERROR = 5.0e-4f; // Half a millimeter

// Local flags:
constexpr U8 CONFIG_FLAG_LOCAL_POS			= 1 << 0;
constexpr U8 CONFIG_FLAG_LOCAL_ROT			= 1 << 1;
constexpr U8 CONFIG_FLAG_LOCAL_SCALE		= 1 << 2;
constexpr U8 CONFIG_FLAG_DISABLE_CONSTRAINT	= 1 << 3;

// Config flags:
constexpr U8 CONFIG_FLAG_TARGET_POS			= 1 << 4;
constexpr U8 CONFIG_FLAG_TARGET_ROT			= 1 << 5;
#if LLIK_EXPERIMENTAL
constexpr U8 CONFIG_FLAG_HAS_DELEGATED		= 1 << 6;
#endif
constexpr U8 CONFIG_FLAG_ENABLE_REPORTING	= 1 << 7;

constexpr U8 MASK_POS = CONFIG_FLAG_TARGET_POS | CONFIG_FLAG_LOCAL_POS;
constexpr U8 MASK_ROT = CONFIG_FLAG_TARGET_ROT | CONFIG_FLAG_LOCAL_ROT;
constexpr U8 MASK_TRANSFORM = MASK_POS | MASK_ROT;
constexpr U8 MASK_LOCAL = CONFIG_FLAG_LOCAL_POS | CONFIG_FLAG_LOCAL_ROT |
						  CONFIG_FLAG_DISABLE_CONSTRAINT;
constexpr U8 MASK_TARGET = CONFIG_FLAG_TARGET_POS | CONFIG_FLAG_TARGET_ROT;
// This mask relates to LLJointState::Usage enum
constexpr U8 MASK_JOINT_STATE_USAGE = CONFIG_FLAG_LOCAL_POS |
									  CONFIG_FLAG_LOCAL_ROT |
									  CONFIG_FLAG_LOCAL_SCALE;

// IK has adjusted local_rot
constexpr U8 IK_FLAG_LOCAL_ROT = 1 << 1;
constexpr U8 IK_FLAG_ACTIVE = 1 << 5;
// local_rot is locked during IK
constexpr U8 IK_FLAG_LOCAL_ROT_LOCKED = 1 << 7;

// A Constraint exists at the tip of Joint and limits the range of
// Joint.mLocalRot.
class Constraint
{
public:
	typedef std::shared_ptr<Constraint> ptr_t;

	enum ConstraintType
	{
		NULL_CONSTRAINT,
		UNKNOWN_CONSTRAINT,
		SIMPLE_CONE_CONSTRAINT,
		TWIST_LIMITED_CONE_CONSTRAINT,
		ELBOW_CONSTRAINT,
		KNEE_CONSTRAINT,
		ACUTE_ELLIPSOIDAL_CONE_CONSTRAINT,
		DOUBLE_LIMITED_HINGE_CONSTRAINT
	};

	LL_INLINE Constraint()
	:	mType(NULL_CONSTRAINT)
	{
	}

	Constraint(ConstraintType type, const LLSD& parameters);

	virtual ~Constraint() = default;

	virtual LLSD asLLSD() const;
	virtual U64 getHash() const;

	LL_INLINE ConstraintType getType() const		{ return mType; }
	const std::string& typeToName() const;

	bool enforce(Joint& joint) const;
	virtual LLQuaternion computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const = 0;
	virtual LLQuaternion minimizeTwist(const LLQuaternion& j_loc_rot) const;

	// All Constraints have a forward axis
	LL_INLINE const LLVector3& getForwardAxis() const
	{
		return mForward;
	}

	LL_INLINE virtual bool allowsTwist() const		{ return true; }

protected:
	LLVector3		mForward;
	ConstraintType	mType;
};

// SimpleCone Constrainte can twist arbitrarily about its 'forward' axis
// but has a uniform bend limit for orientations perpendicular to 'forward'.
class SimpleCone : public Constraint
{
	// Constrains forward axis inside cone.
	//
	//		/ max_angle
	//	   /
	//   ---@--------> forward
	//	   `
	//		` max_angle
	//
public:
	SimpleCone(const LLVector3& forward_axis, F32 max_angle);
	SimpleCone(const LLSD& parameters);

	LLSD asLLSD() const override;
	U64 getHash() const override;

	LLQuaternion computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const override;

private:
	F32 mMaxAngle;
	F32 mCosConeAngle;
	F32 mSinConeAngle;
};

// TwistLimitedCone Constrainte can has limited twist about its 'forward' axis
// but has a uniform bend limit for orientations perpendicular to 'forward'.
class TwistLimitedCone : public Constraint
{
	// A constraint for the shoulder. Like SimpleCone but with limited twist
	//
	// View from side:				 View with foward out of page:
	//										 max_twist
	//		/ cone_angle				  | /
	//	   /							  |/
	//   ---@--------> forward_axis	----(o)----> perp_axis
	//	   `							 /|
	//		` cone_angle				/ |
	//							 min_twist
	//
	//
public:
	TwistLimitedCone(const LLVector3& forward_axis, F32 cone_angle,
					 F32 min_twist, F32 max_twist);
	TwistLimitedCone(const LLSD& parameters);

	LLSD asLLSD() const override;
	U64 getHash() const override;

	LLQuaternion computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const override;
	LLQuaternion minimizeTwist(const LLQuaternion& j_loc_rot) const override;

private:
	F32 mConeAngle;
	F32 mCosConeAngle;
	F32 mSinConeAngle;
	F32 mMinTwist;
	F32 mMaxTwist;
};

// ElbowConstraint can only bend (with limits) about its 'pivot' axis
// and allows limited twist about its 'forward' axis.
class ElbowConstraint : public Constraint
{
	// A Constraint for Elbow: limited hinge with limited twist about forward
	// (forearm) axis.
	//
	// View from the side,			 View with foreward axis out of page:
	// with pivot axis out of page:
	//									  up  max_twist
	//		/ max_bend					 | /
	//	   /							   |/
	//  ---(o)--------+  forward		----(o)----> left
	//	   `							  /|
	//		` min_bend				   / |
	//							  min_twist
	//
public:
	ElbowConstraint(const LLVector3& forward_axis, const LLVector3& pivot_axis,
					F32 min_bend, F32 max_bend, F32 min_twist, F32 max_twist);
	ElbowConstraint(const LLSD& parameters);

	LLSD asLLSD() const override;
	U64 getHash() const override;

	LLQuaternion computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const override;
	LLQuaternion minimizeTwist(const LLQuaternion& j_loc_rot) const override;

private:
	LLVector3	mPivotAxis;
	LLVector3	mLeft;
	F32			mMinBend;
	F32			mMaxBend;
	F32			mMinTwist;
	F32			mMaxTwist;
};

// KneeConstraint only allows bend (limited) about its 'pivot' axis but does
// not allow any twist about its 'forward' axis.
class KneeConstraint : public Constraint
{
	// A Constraint for knee, or finger. Like ElbowConstraint but
	// no twist allowed, min/max limits on angle about pivot.
	//
	// View from the side, with pivot axis out of page:
	//
	//		/ max_bend
	//	   /
	//  ---(o)--------+
	//	   `
	//		` min_bend
	//
public:
	KneeConstraint(const LLVector3& forward_axis, const LLVector3& pivot_axis,
				   F32 min_bend, F32 max_bend);
	KneeConstraint(const LLSD& parameters);

	LLSD asLLSD() const override;
	U64 getHash() const override;

	LLQuaternion computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const override;
	LL_INLINE bool allowsTwist() const override		{ return false; }
	LLQuaternion minimizeTwist(const LLQuaternion& j_loc_rot) const override;

private:
	LLVector3	mPivotAxis;
	LLVector3	mLeft;
	F32			mMinBend;
	F32			mMaxBend;
};

// AcuteEllipsoidalCone is like SimpleCone but with asymmetric radiuses in the
// up, left, down, right directions.  In other words: it has non-symmetric bend
// limits for axes perpendicular to its 'forward' axix.  It was implemented
// mostly as an exercise, since it is similar to the Constraint described in
// the original FABRIK papter. The geometry of the ellipsoidal boundary are
// described by defining the forward offset of the "cross" of radiuses. Each
// quadrant of the cross in the left-up plane is bound by an elliptical curve
// that depends on its bounding radiuses.
//
//	 up  left			|
//	  | /				| /
//	  |/				 |/
//   ---@------------------+
//		   forward	  /|
//
class AcuteEllipsoidalCone : public Constraint
{
public:
	AcuteEllipsoidalCone(const LLVector3& forw_axis, const LLVector3& up_axis,
						 F32 forward, F32 up, F32 left, F32 down, F32 right);
	AcuteEllipsoidalCone(const LLSD& parameters);

	LLSD asLLSD() const override;
	U64 getHash() const override;

	LLQuaternion computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const override;

private:
	LLVector3	mUp;
	LLVector3	mLeft;

	F32			mXForward;
	F32			mXUp;
	F32			mXDown;
	F32			mXLeft;
	F32			mXRight;

	// for each quadrant we cache these parameters to help
	// us project onto each partial ellipse.
	F32			mQuadrantScales[4];
	F32			mQuadrantCosAngles[4];
	F32			mQuadrantCotAngles[4];
};

// The DoubleLimitedHinge constraint is intended for uses on Joints like
// the wrist, or first finger Joints.  It allows for yaw and pitch bends
// but zero twist.
class DoubleLimitedHinge : public Constraint
{
	// A Constraint for first finger bones.
	// No twist allowed, min/max limits on yaw, then pitch.
	//
	// View from above					 View from right
	// with UP out of page				 (remember to use right-hand-rule)
	//
	//   left_axis							up_axis
	//	  |								   |
	//	  | / max_yaw_angle				   | / min_pitch_angle
	//	  |/								  |/
	//  ---(o)--------> forward_axis		---(x)--------> forward_axis
	//	up `							  left `
	//		` min_yaw_angle					 ` max_pitch_angle
	//
public:
	DoubleLimitedHinge(const LLVector3& forward_axis, const LLVector3& up_axis,
					   F32 min_yaw, F32 max_yaw, F32 min_pitch, F32 max_pitch);
	DoubleLimitedHinge(const LLSD& parameters);

	LLSD asLLSD() const override;
	U64 getHash() const override;

	LLQuaternion computeAdjustedLocalRot(const LLQuaternion& joint_loc_rot) const override;
	LLQuaternion minimizeTwist(const LLQuaternion& joint_loc_rot) const override;

private:
	LLVector3	mUp;
	LLVector3	mLeft; // mUp X mForward
	F32			mMinYaw;
	F32			mMaxYaw;
	F32			mMinPitch;
	F32			mMaxPitch;
};

// Joint represents a constrained bone in the skeleton heirarchy. It typically
// has a parent Joint, a fixed mLocalPos position in its parent's local-frame,
// and a fixed mBone to its 'end' position in its own local-frame. A summary of
// its important data members is as follows:
//
//	 mLocalPos	= tip position in parent's local-frame
//	 mLocalRot	= orientation of Joint's tip relative to parent's local-frame
//	 mBone		= invarient end position in local-frame
//	 mPos		= tip position in world-frame (we call it 'world-frame'
//				  but really it is the 'root-frame' of the Skeleton hierarchy).
//	 mRot		= orientation of Joint in world-frame.
//
// Some important formula to keep in mind:
//
//	 mPos = mParent->mPos + mLocalPos * mParent->mRot
//	 mRot = mLocalRot * mParent->mRot
//
// The world-frame 'end' position of the Joint can be calculated:
//
//	 world_end_pos = mPos + mBone * mRot
class Joint
{
protected:
	LOG_CLASS(LLIK::Joint);

public:
	class Config
	{
	public:
		LL_INLINE Config()
		:	mFlags(0)
		{
		}

		LL_INLINE bool hasLocalPos() const
		{
			return mFlags & CONFIG_FLAG_LOCAL_POS;
		}

		LL_INLINE void setLocalPos(const LLVector3& pos)
		{
			mLocalPos = pos;
			mFlags |= CONFIG_FLAG_LOCAL_POS;
		}

		LL_INLINE bool hasLocalRot() const
		{
			return mFlags & CONFIG_FLAG_LOCAL_ROT;
		}

		LL_INLINE bool hasLocalScale() const
		{
			return mFlags & CONFIG_FLAG_LOCAL_SCALE;
		}

		LL_INLINE void setLocalRot(const LLQuaternion& rot)
		{
			mLocalRot = rot;
			mLocalRot.normalize();
			mFlags |= CONFIG_FLAG_LOCAL_ROT;
		}

		LL_INLINE bool constraintIsDisabled() const
		{
			return mFlags & CONFIG_FLAG_DISABLE_CONSTRAINT;
		}

		LL_INLINE void disableConstraint()
		{
			mFlags |= CONFIG_FLAG_DISABLE_CONSTRAINT;
		}

		LL_INLINE const LLVector3& getLocalPos() const
		{
			return mLocalPos;
		}

		LL_INLINE const LLQuaternion& getLocalRot() const
		{
			return mLocalRot;
		}

		LL_INLINE bool hasTargetPos() const
		{
			return mFlags & CONFIG_FLAG_TARGET_POS;
		}

		LL_INLINE void setTargetPos(const LLVector3& pos)
		{
			mTargetPos = pos;
			mFlags |= CONFIG_FLAG_TARGET_POS;
		}

		LL_INLINE const LLVector3& getTargetPos() const
		{
			return mTargetPos;
		}

		LL_INLINE bool hasTargetRot() const
		{
			return mFlags & CONFIG_FLAG_TARGET_ROT;
		}

		LL_INLINE void setTargetRot(const LLQuaternion& rot)
		{
			mTargetRot = rot;
			mTargetRot.normalize();
			mFlags |= CONFIG_FLAG_TARGET_ROT;
		}

		LL_INLINE const LLQuaternion& getTargetRot() const
		{
			return mTargetRot;
		}

		LL_INLINE void setLocalScale(const LLVector3& scale)
		{
			mLocalScale = scale;
			mFlags |= CONFIG_FLAG_LOCAL_SCALE;
		}

		LL_INLINE const LLVector3& getLocalScale() const
		{
			return mLocalScale;
		}

		LL_INLINE void enableReporting(S32 reqid)
		{
			mFlags |= CONFIG_FLAG_ENABLE_REPORTING;
		}

#if LLIK_EXPERIMENTAL
		LL_INLINE void delegate()
		{
			mFlags |= CONFIG_FLAG_HAS_DELEGATED;
		}

		LL_INLINE bool hasDelegated() const
		{
			return mFlags & CONFIG_FLAG_HAS_DELEGATED;
		}
#endif

		LL_INLINE U8 getFlags() const				{ return mFlags; }

		void updateFrom(const Config& other_config);

	private:
		LLVector3		mLocalScale;
		LLVector3		mLocalPos;
		LLVector3		mTargetPos;
		LLQuaternion	mLocalRot;
		LLQuaternion	mTargetRot;
		U8				mFlags;		// Per-feature bits
	};

	typedef std::shared_ptr<Joint> ptr_t;
	typedef std::vector<ptr_t> child_vec_t;

	Joint(LLJoint* info);

	void resetFromInfo();

	void addChild(const ptr_t& child);
	void setParent(const ptr_t& parent);
	void resetRecursively();
	void relaxRotationsRecursively(F32 blend_factor);
	F32 recursiveComputeLongestChainLength(F32 length) const;

	void updateGeometry(const LLVector3& local_pos, const LLVector3& bone);

	LLVector3 computeEndTargetPos() const;
	LLVector3 computeWorldTipOffset() const;
	void updateEndInward();
	void updateEndOutward();
	void updateBranchRoot();
	void updateInward(const ptr_t& child);
	void updatePosAndRotFromParent();
	void updateOutward();
	void applyLocalRot();
	void updateLocalRot();
	LLQuaternion computeParentRot() const;
	void updateChildLocalRots() const;

	LL_INLINE LLVector3 computePosFromParent() const
	{
		return mParent->mPos + mLocalPos * mParent->mRot;
	}

	LL_INLINE const LLVector3& getWorldTipPos() const
	{
		return mPos;
	}

	LL_INLINE const LLQuaternion& getWorldRot() const
	{
		return mRot;
	}

	LL_INLINE LLVector3 computeWorldEndPos() const
	{
		return mPos + mBone * mRot;
	}

	// Only call this if you know what you are doing: this should only be
	// called once before starting IK algorithm iterations.
	LL_INLINE void setLocalPos(const LLVector3& pos)
	{
		mLocalPos = pos.scaledVec(mLocalScale);
		mLocalPosLength = mLocalPos.length();
		if (!mParent)
		{
			mPos = mLocalPos;
		}
	}

	void setLocalScale(const LLVector3& scale);

	// Returns local_pos with any non-uniform scale from the "info" removed.
	LLVector3 getPreScaledLocalPos() const;

	void setLocalRot(const LLQuaternion& new_local_rot);

	LL_INLINE void setWorldPos(const LLVector3& p)	{ mPos = p; }

	LL_INLINE void setWorldRot(const LLQuaternion& rot)
	{
		mRot = rot;
	}

	void adjustWorldRot(const LLQuaternion& adjustment);

	LL_INLINE void shiftPos(const LLVector3& shift)	{ mPos += shift; }

	LL_INLINE void setConfig(const Config& config)
	{
		// We only remember the config here; it gets applied later when we
		// build the chains.
		mConfig = &config;
		mConfigFlags = config.getFlags();
	}

	void setTargetPos(const LLVector3& pos);

	LL_INLINE LLVector3 getTargetPos()
	{
		return mConfig->getTargetPos();
	}

	LL_INLINE const Config* getConfig() const		{ return mConfig; }

	LL_INLINE bool hasPosTarget() const
	{
		return mConfigFlags & CONFIG_FLAG_TARGET_POS;
	}

	LL_INLINE bool hasRotTarget() const
	{
		return mConfigFlags & CONFIG_FLAG_TARGET_ROT;
	}

	LL_INLINE U8 getConfigFlags() const				{ return mConfigFlags; }

	LL_INLINE U8 getHarvestFlags() const
	{
		return (mConfigFlags | mIkFlags) & MASK_LOCAL;
	}

	LL_INLINE void resetFlags()
	{
		mConfig = NULL;
		mConfigFlags = 0;
		// Root Joint always has IK_FLAG_LOCAL_ROT_LOCKED set
		mIkFlags = mParent ? 0 : IK_FLAG_LOCAL_ROT_LOCKED;
	}

	void lockLocalRot(const LLQuaternion& local_rot);

	LL_INLINE void setConstraint(std::shared_ptr<Constraint> constraint)
	{
		mConstraint = constraint;
	}

	bool enforceConstraint();
	void updateWorldTransformsRecursively();

	LL_INLINE const LLQuaternion& getLocalRot() const
	{
		return mLocalRot;
	}

	LL_INLINE S16 getID() const						{ return mID; }
	LL_INLINE const LLVector3& getBone() const		{ return mBone; }

	LL_INLINE const LLVector3& getLocalPos() const	{ return mLocalPos; }

	LL_INLINE const LLVector3& getLocalScale() const
	{
		return mLocalScale;
	}

	LL_INLINE F32 getBoneLength() const				{ return mBone.length(); }
	LL_INLINE F32 getLocalPosLength() const			{ return mLocalPosLength; }

	LL_INLINE ptr_t getParent()						{ return mParent; }

	LL_INLINE void activate()
	{
		mIkFlags |= IK_FLAG_ACTIVE;
	}

	LL_INLINE bool isActive() const
	{
		return mIkFlags & IK_FLAG_ACTIVE;
	}

	LL_INLINE bool hasDisabledConstraint() const
	{
		return mConfigFlags & CONFIG_FLAG_DISABLE_CONSTRAINT;
	}

	// Joint::mLocalRot is considered "locked" when its mConfigFlag's
	// CONFIG_FLAG_LOCAL_ROT bit is set
	LL_INLINE bool localRotLocked() const
	{
		return mIkFlags & IK_FLAG_LOCAL_ROT_LOCKED;
	}

	LL_INLINE size_t getNumChildren() const
	{
		return mChildren.size();
	}

	ptr_t getSingleActiveChild();

	void transformTargetsToParentLocal(std::vector<LLVector3>& ltargets) const;
	bool swingTowardTargets(const std::vector<LLVector3>& local_targets,
							const std::vector<LLVector3>& world_targets);
#if LLIK_EXPERIMENTAL
	void twistTowardTargets(const std::vector<LLVector3>& local_targets,
							const std::vector<LLVector3>& world_targets);
#endif
	void untwist();

	// We call flagForHarvest() when we expect the joint to be updated by IK so
	// we know to harvest its mLocalRot later.
	LL_INLINE void flagForHarvest()
	{
		mIkFlags |= IK_FLAG_LOCAL_ROT;
	}

	void collectTargetPositions(std::vector<LLVector3>& local_targets,
								std::vector<LLVector3>& world_targets) const;

protected:
	void reset();
	void relaxRot(F32 blend_factor);

protected:
	ptr_t				mParent;
	Constraint::ptr_t	mConstraint;
	// Pointer into Solver::mJointConfigs
	const Config*		mConfig;

	const LLJoint*		mInfoPtr;

	// List of joint_ids attached to this one.
	typedef std::vector<ptr_t> child_list_t;
	child_list_t	mChildren;

	LLVector3		mLocalScale;

	LLVector3		mLocalPos;			// Current pos in parent-frame
	LLVector3		mPos;				// Pos in world-frame
	// The fundamental position formula is:
	//	 mPos = mParent->mPos + mLocalPos * mParent->mRot;

	// Note: there is no mDefaultLocalRot because it is identity
	LLQuaternion	mLocalRot;			// Orientations in parent-frame
	LLQuaternion	mRot;
	// The fundamental orientations formula is:
	//	 mRot = mLocalRot * mParent->mRot

	LLVector3		mBone;
	// There is another fundamental formula:
	//	world_end_pos = mPos + mBone * mRot

	F32				mLocalPosLength;	// Cached copy of mLocalPos.length()
	S16				mID;

	U8				mConfigFlags;		// Cache of mConfig->mFlags
	U8				mIkFlags;			// Flags for IK calculations
};

// The Solver maintains a skeleton of connected Joints and computes the
// parent-relative orientations to allow end-effectors to reach their targets.
class Solver
{
protected:
	LOG_CLASS(LLIK::Solver);

public:
	typedef std::map<S16, Joint::ptr_t> joint_map_t;
	typedef std::vector<S16> S16_vec_t;
	typedef std::map<S16, Joint::Config> joint_config_map_t;
	typedef std::vector<Joint::ptr_t> joint_list_t;
	typedef std::map<S16, joint_list_t> chain_map_t;

	LL_INLINE Solver()
	:	mRootID(-1),
		mAcceptableError(IK_DEFAULT_ACCEPTABLE_ERROR),
		mLastError(0.f)
	{
	}

	// Puts skeleton back into default orientation (e.g. T-Pose for a humanoid
	// character)
	void resetSkeleton();

	// Computes the offset from the "tip" of from_id to the "end" of to_id or
	// the negative when from_id > to_id
	LLVector3 computeReach(S16 to_id, S16 from_id) const;

	// Adds a Joint to the skeleton.
	void addJoint(S16 joint_id, S16 parent_id, LLJoint* info_ptr,
				  const Constraint::ptr_t& constraint);

	// Apply configs and return 'true' if something changed
	bool updateJointConfigs(const joint_config_map_t& configs);

	// Solves the IK problem for the given list of joint configurations
	F32 solve();

	// Specifies a joint as a 'wrist'. Will be used to help 'drop the elbow' of
	// the arm to achieve a more realistic solution.
	void addWristID(S16 wrist_id);

	LL_INLINE void setRootID(S16 root_id)			{ mRootID = root_id; }
	LL_INLINE S16 getRootID() const					{ return mRootID; }

	LL_INLINE const joint_list_t getActiveJoints() const
	{
		return mActiveJoints;
	}

	// Specifies the list of joint Ids that should be considered as sub-bases
	// e.g. joints that are known to have multipe child chains, like the chest
	// (chains on left and right collar children) or wrists (chain for each
	// fingers).
	LL_INLINE void setSubBaseIds(const std::set<S16>& ids)
	{
		mSubBaseIds = ids;
	}

	// Set list of joint ids that should be considered sub-roots where the IK
	// chains stop. This HACK was used to remove the spine from the solver
	// before spine constraints were working.
	LL_INLINE void setSubRootIds(const std::set<S16>& ids)
	{
		mSubRootIds = ids;
	}

	// Per-Joint property accessors from outside Solver
	LLQuaternion getJointLocalRot(S16 joint_id) const;
	LLVector3 getJointLocalPos(S16 joint_id) const;
	bool getJointLocalTransform(S16 joint_id, LLVector3& pos,
								LLQuaternion& rot) const;
	LLVector3 getJointWorldEndPos(S16 joint_id) const;
	LLQuaternion getJointWorldRot(S16 joint_id) const;

	void resetJointGeometry(S16 joint_id, const Constraint::ptr_t& constraint);

	LL_INLINE void setAcceptableError(F32 slop)		{ mAcceptableError = slop; }

private:
	// Sometimes we cannot rely on the skeleton topology to determine whether a
	// Joint is a sub-base or not. So so we offer this workaround: outside
	// logic can supply a whitelist of sub-base ids.
	LL_INLINE bool isSubBase(S16 joint_id) const
	{
		return mSubBaseIds.count(joint_id);
	}

	LL_INLINE bool isSubRoot(S16 joint_id) const
	{
		return mSubRootIds.count(joint_id);
	}

	void dropElbow(const Joint::ptr_t& wrist_joint);
	void rebuildAllChains();
	void buildChain(Joint::ptr_t joint, joint_list_t& chain,
					std::set<S16>& sub_bases);
	void executeFabrikInward(const joint_list_t& chain);
	void executeFabrikOutward(const joint_list_t& chain);
	void shiftChainToBase(const joint_list_t& chain);
	void executeFabrikPass();
	void enforceConstraintsOutward();
	void untwistChain(const joint_list_t& chain);
	F32 measureMaxError();
	F32 solveOnce();
	void executeFabrik(bool constrain = false, bool drop_elbow = false,
					  bool untwist = false);
#if LLIK_EXPERIMENTAL
	void executeCcdPass(bool constrain);
	void executeCcd(bool constrain = false, bool drop_elbow = false,
					bool untwist = false);
	void executeCcdInward(const joint_list_t& chain, bool constrain);
	void adjustTargets(joint_config_map_t& targets);
#endif

private:
	joint_map_t					mSkeleton;
	joint_config_map_t			mJointConfigs;

	chain_map_t					mChainMap;
	std::set<S16>				mSubBaseIds;	// HACK: whitelist of sub-bases
	std::set<S16>				mSubRootIds;	// HACK: whitelist of sub-roots
	std::set<Joint::ptr_t>		mActiveRoots;
	// Joints with non-default local-pos
	std::vector<Joint::ptr_t>	mActiveJoints;
	joint_list_t				mWristJoints;
	F32							mAcceptableError;
	F32							mLastError;
	// ID number of the root joint for this skeleton
	S16							mRootID;
};

} // namespace LLIK

// Constraints are 'stateless' configurations so we use a Factory pattern to
// allocate them, which allows multiple Joints with identical constraint
// configs to use a single Constraint instance.
class LLIKConstraintFactory : public LLSingleton<LLIKConstraintFactory>
{
	friend class LLSingleton<LLIKConstraintFactory>;

protected:
	LOG_CLASS(LLIKConstraintFactory);
	
public:
	LLIK::Constraint::ptr_t getConstrForJoint(const std::string& jname) const;

private:
	void initSingleton() override;

	LLIK::Constraint::ptr_t getConstraint(const LLSD& data);
	static LLIK::Constraint::ptr_t create(const LLSD& info);

private:
	typedef flat_hmap<U64, LLIK::Constraint::ptr_t> cache_map_t;
	cache_map_t	mConstraints;

	typedef flat_hmap<std::string, LLIK::Constraint::ptr_t> joint_map_t;
	joint_map_t	mJointMapping;
};

// std::hash implementation for Constraint
namespace std
{
	template<> struct hash<LLIK::Constraint>
	{
		LL_INLINE size_t operator()(const LLIK::Constraint& s) const noexcept
		{
			return s.getHash();
		}
	};
}

// For use with boost::unordered_map and boost::unordered_set
LL_INLINE size_t hash_value(const LLIK::Constraint& s) noexcept
{
	return s.getHash();
}

#endif // LL_LLIK_H
