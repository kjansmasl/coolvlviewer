/**
 * @file llik.cpp
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

#include <algorithm>
#include <sstream>

#include "linden_common.h"

#include "llik.h"

#include "lldir.h"
#include "lljoint.h"
#include "llsdserialize.h"
#include "hbxxh.h"

static const std::string NULL_CONSTRAINT_NAME("NULL_CONSTRAINT");
static const std::string SIMPLE_CONE_NAME("SIMPLE_CONE");
static const std::string TWIST_LIMITED_CONE_NAME("TWIST_LIMITED_CONE");
static const std::string ELBOW_NAME("ELBOW");
static const std::string KNEE_NAME("KNEE");
static const std::string ACUTE_ELLIPSOIDAL_NAME("ACUTE_ELLIPSOIDAL_CONE");
static const std::string DOUBLE_LIMITED_HINGE_NAME("DOUBLE_LIMITED_HINGE");
static const std::string UNKNOWN_CONSTRAINT_NAME("UNKNOWN_CONSTRAINT");

namespace LLIK
{

// Utility function for truncating angle to range: [0, F_TWO_PI[
static F32 remove_multiples_of_two_pi(F32 angle)
{
	return angle - F_TWO_PI * (S32)(angle / F_TWO_PI);
}

// Utility function for clamping angle limits in range [-PI, PI]. Note:
// arguments are passed by reference and modified as side-effect
static void compute_angle_limits(F32& min_angle, F32& max_angle)
{
	max_angle = remove_multiples_of_two_pi(max_angle);
	if (max_angle > F_PI)
	{
		max_angle -= F_TWO_PI;
	}
	min_angle = remove_multiples_of_two_pi(min_angle);
	if (min_angle > F_PI)
	{
		min_angle -= F_TWO_PI;
	}
	if (min_angle > max_angle)
	{
		F32 temp = min_angle;
		min_angle = max_angle;
		max_angle = temp;
	}
}

// Utility function for clamping angle between two limits. Consider angle
// limits: min_angle and max_angle with axis out of the page. There exists an
// "invalid bisector" angle which splits the invalid zone between the that
// which is closest to mMinBend or mMaxBend.
//
//				max_angle
//				  `
//				   `
//					`
//					(o)--------> 0
//				 .-'  `
//			  .-'	  `
//		   .-'		  `
// invalid_bisector	   min_angle
//
static F32 compute_clamped_angle(F32 angle, F32 min_angle, F32 max_angle)
{
	F32 invalid_bisector = max_angle +
						   0.5f * (F_TWO_PI - (max_angle - min_angle));
	if ((angle > max_angle && angle < invalid_bisector) ||
		angle < invalid_bisector - F_TWO_PI)
	{
		return max_angle;
	}
	return min_angle;
}

///////////////////////////////////////////////////////////////////////////////
// LLIK::Constraint class
///////////////////////////////////////////////////////////////////////////////

Constraint::Constraint(ConstraintType type, const LLSD& parameters)
:	mType(type)
{
	mForward = LLVector3(parameters["forward_axis"]);
	mForward.normalize();
}

//virtual
LLSD Constraint::asLLSD() const
{
	LLSD data = LLSD::emptyMap();
	data["forward_axis"] = mForward.getValue();
	data["type"] = typeToName();
	return data;
}

//virtual
U64 Constraint::getHash() const
{
	return HBXXH64::digest((const void*)this, sizeof(Constraint));
}

bool Constraint::enforce(Joint& joint) const
{
	const LLQuaternion& local_rot = joint.getLocalRot();
	LLQuaternion adjusted_loc_rot = computeAdjustedLocalRot(local_rot);
	if (!LLQuaternion::almost_equal(adjusted_loc_rot, local_rot))
	{
		joint.setLocalRot(adjusted_loc_rot);
		return true;
	}
	return false;
}

LLQuaternion Constraint::minimizeTwist(const LLQuaternion& j_loc_rot) const
{
	// Default behavior of minimizeTwist() is to compute the shortest rotation
	// that produces the same swing.
	LLVector3 joint_forward = mForward * j_loc_rot;
	LLVector3 swing_axis = mForward % joint_forward;
	LLQuaternion new_local_rot = LLQuaternion::DEFAULT;
	constexpr F32 MIN_AXIS_LENGTH = 1.0e-5f;
	if (swing_axis.length() > MIN_AXIS_LENGTH)
	{
		F32 swing_angle = acosf(mForward * joint_forward);
		new_local_rot.setAngleAxis(swing_angle, swing_axis);
	}
	return new_local_rot;
}

const std::string& Constraint::typeToName() const
{
	switch (mType)
	{
		case Constraint::NULL_CONSTRAINT:
			return NULL_CONSTRAINT_NAME;

		case Constraint::SIMPLE_CONE_CONSTRAINT:
			return SIMPLE_CONE_NAME;

		case Constraint::TWIST_LIMITED_CONE_CONSTRAINT:
			return TWIST_LIMITED_CONE_NAME;

		case Constraint::ELBOW_CONSTRAINT:
			return ELBOW_NAME;

		case Constraint::KNEE_CONSTRAINT:
			return KNEE_NAME;
			
		case Constraint::ACUTE_ELLIPSOIDAL_CONE_CONSTRAINT:
			return ACUTE_ELLIPSOIDAL_NAME;
			
		case Constraint::DOUBLE_LIMITED_HINGE_CONSTRAINT:
			return DOUBLE_LIMITED_HINGE_NAME;

		default:
			return UNKNOWN_CONSTRAINT_NAME;
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLIK::SimpleCone class
///////////////////////////////////////////////////////////////////////////////

SimpleCone::SimpleCone(const LLVector3& forward, F32 max_angle)
{
	mType = SIMPLE_CONE_CONSTRAINT;
	mForward = forward;
	mForward.normalize();

	mMaxAngle = fabsf(max_angle);
	mCosConeAngle = cosf(mMaxAngle);
	mSinConeAngle = sinf(mMaxAngle);
}

SimpleCone::SimpleCone(const LLSD& parameters)
:	Constraint(Constraint::SIMPLE_CONE_CONSTRAINT, parameters)
{
	mMaxAngle = fabsf(F32(parameters["max_angle"].asReal()) * DEG_TO_RAD);
	mCosConeAngle = cosf(mMaxAngle);
	mSinConeAngle = sinf(mMaxAngle);
}

//virtual
LLSD SimpleCone::asLLSD() const
{
	LLSD data = Constraint::asLLSD();
	data["max_angle"] = mMaxAngle * RAD_TO_DEG;
	return data;
}

//virtual
U64 SimpleCone::getHash() const
{
	return HBXXH64::digest((const void*)this, sizeof(SimpleCone));
}

LLQuaternion SimpleCone::computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const
{
	LLVector3 joint_forward = mForward * j_loc_rot;
	F32 forward_component = joint_forward * mForward;
	if (forward_component < mCosConeAngle)
	{
		// The joint's version of mForward lies outside the cone, so we project
		// it onto the surface of the cone...
		// projection = (forward_part) + (orthogonal_part)
		LLVector3 perp = joint_forward - forward_component * mForward;
		perp.normalize();
		LLVector3 new_j_forw = mCosConeAngle * mForward + mSinConeAngle * perp;

		// ... then compute the adjusted rotation
		LLQuaternion adjustment;
		adjustment.shortestArc(joint_forward, new_j_forw);
		LLQuaternion adjusted_loc_rot = j_loc_rot * adjustment;
		adjusted_loc_rot.normalize();
		return adjusted_loc_rot;
	}
	return j_loc_rot;
}

///////////////////////////////////////////////////////////////////////////////
// LLIK::TwistLimitedCone class
///////////////////////////////////////////////////////////////////////////////

TwistLimitedCone::TwistLimitedCone(const LLVector3& forward,
								   F32 cone_angle, F32 min_twist,
								   F32 max_twist)
:	mConeAngle(cone_angle)
{
	mType = TWIST_LIMITED_CONE_CONSTRAINT;

	mForward = forward;
	mForward.normalize();

	mCosConeAngle = cosf(cone_angle);
	mSinConeAngle = sinf(cone_angle);

	mMinTwist = min_twist;
	mMaxTwist = max_twist;
	compute_angle_limits(mMinTwist, mMaxTwist);
}

TwistLimitedCone::TwistLimitedCone(const LLSD& parameters)
:	Constraint(Constraint::TWIST_LIMITED_CONE_CONSTRAINT, parameters)
{
	mConeAngle = parameters["cone_angle"].asReal() * DEG_TO_RAD;
	mCosConeAngle = cosf(mConeAngle);
	mSinConeAngle = sinf(mConeAngle);

	mMinTwist = parameters["min_twist"].asReal() * DEG_TO_RAD;
	mMaxTwist = parameters["max_twist"].asReal() * DEG_TO_RAD;
	compute_angle_limits(mMinTwist, mMaxTwist);
}

//virtual
LLSD TwistLimitedCone::asLLSD() const
{
	LLSD data = Constraint::asLLSD();
	data["cone_angle"] = mConeAngle * RAD_TO_DEG;
	data["min_twist"] = mMinTwist * RAD_TO_DEG;
	data["max_twist"] = mMaxTwist * RAD_TO_DEG;
	return data;
}

//virtual
U64 TwistLimitedCone::getHash() const
{
	return HBXXH64::digest((const void*)this, sizeof(TwistLimitedCone));
}

LLQuaternion TwistLimitedCone::computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const
{
	LLVector3 joint_forward = mForward * j_loc_rot;
	LLQuaternion adjusted_loc_rot = j_loc_rot;
	F32 forward_component = joint_forward * mForward;
	if (forward_component < mCosConeAngle)
	{
		// The joint's version of mForward lies outside the cone, so we project
		// it onto the surface of the cone...
		// projection  = forward_part + orthogonal_part
		LLVector3 perp = joint_forward - forward_component * mForward;
		perp.normalize();
		LLVector3 new_j_forw = mCosConeAngle * mForward + mSinConeAngle * perp;

		// ... then compute the adjusted rotation
		LLQuaternion adjustment;
		adjustment.shortestArc(joint_forward, new_j_forw);
		adjusted_loc_rot = j_loc_rot * adjustment;
	}

	// Rotate mForward by adjusted_loc_rot
	joint_forward = mForward * adjusted_loc_rot;
	forward_component = joint_forward * mForward;

	// Compute two axes perpendicular to joint_forward: perp_x and perp_y
	LLVector3 perp_x = mForward % joint_forward;
	F32 perp_length = perp_x.length();
	constexpr F32 MIN_PERP_LENGTH = 1.0e-3f;
	if (perp_length < MIN_PERP_LENGTH)
	{
		perp_x = LLVector3::y_axis % mForward;
		perp_length = perp_x.length();
		if (perp_length < MIN_PERP_LENGTH)
		{
			perp_x = mForward % LLVector3::x_axis;
		}
	}
	perp_x.normalize();
	LLVector3 perp_y = joint_forward % perp_x;

	// The components of joint_perp on each direction allow us to compute twist
	// angle
	LLVector3 joint_perp = perp_x * adjusted_loc_rot;
	F32 twist = atan2f(joint_perp * perp_y, joint_perp * perp_x);

	// Clamp twist within bounds
	if (twist > mMaxTwist || twist < mMinTwist)
	{
		twist = compute_clamped_angle(twist, mMinTwist, mMaxTwist);
		joint_perp -= (joint_perp * joint_forward) * joint_forward;
		LLVector3 new_joint_perp = cosf(twist) * perp_x + sinf(twist) * perp_y;
		LLQuaternion adjustment;
		adjustment.shortestArc(joint_perp, new_joint_perp);
		adjusted_loc_rot = adjusted_loc_rot * adjustment;
	}
	adjusted_loc_rot.normalize();
	return adjusted_loc_rot;
}

LLQuaternion TwistLimitedCone::minimizeTwist(const LLQuaternion& j_loc_rot) const
{
	// Compute the swing and combine with default twist
	// which is the midpoint of the twist range.
	LLQuaternion mid_twist;
	mid_twist.setAngleAxis(0.5f * (mMinTwist + mMaxTwist), mForward);

	// j_loc_rot = mid_twist * swing
	LLQuaternion new_local_rot = mid_twist;

	LLVector3 joint_forward = mForward * j_loc_rot;
	LLVector3 swing_axis = mForward % joint_forward;
	constexpr F32 MIN_SWING_AXIS_LENGTH = 1.0e-3f;
	if (swing_axis.length() > MIN_SWING_AXIS_LENGTH)
	{
		LLQuaternion swing;
		F32 swing_angle = acosf(mForward * joint_forward);
		swing.setAngleAxis(swing_angle, swing_axis);
		new_local_rot = mid_twist * swing;
	}

	return new_local_rot;
}

///////////////////////////////////////////////////////////////////////////////
// LLIK::ElbowConstraint class
///////////////////////////////////////////////////////////////////////////////

ElbowConstraint::ElbowConstraint(const LLVector3& forward_axis,
								 const LLVector3& pivot_axis,
								 F32 min_bend, F32 max_bend,
								 F32 min_twist, F32 max_twist)
{
	mType = ELBOW_CONSTRAINT;

	mForward = forward_axis;
	mForward.normalize();
	mPivotAxis = mForward % (pivot_axis % mForward);
	mPivotAxis.normalize();
	mLeft = mPivotAxis % mForward;

	mMinBend = min_bend;
	mMaxBend = max_bend;
	compute_angle_limits(mMinBend, mMaxBend);

	mMinTwist = min_twist;
	mMaxTwist = max_twist;
	compute_angle_limits(mMinTwist, mMaxTwist);
}

ElbowConstraint::ElbowConstraint(const LLSD& parameters)
:	Constraint(Constraint::ELBOW_CONSTRAINT, parameters)
{
	mPivotAxis = mForward % (LLVector3(parameters["pivot_axis"]) % mForward);
	mPivotAxis.normalize();
	mLeft = mPivotAxis % mForward;

	mMinBend = parameters["min_bend"].asReal() * DEG_TO_RAD;
	mMaxBend = parameters["max_bend"].asReal() * DEG_TO_RAD;
	compute_angle_limits(mMinBend, mMaxBend);

	mMinTwist = parameters["min_twist"].asReal() * DEG_TO_RAD;
	mMaxTwist = parameters["max_twist"].asReal() * DEG_TO_RAD;
	compute_angle_limits(mMinTwist, mMaxTwist);
}

//virtual
LLSD ElbowConstraint::asLLSD() const
{
	LLSD data = Constraint::asLLSD();
	data["pivot_axis"] = mPivotAxis.getValue();
	data["min_bend"] = mMinBend * RAD_TO_DEG;
	data["max_bend"] = mMaxBend * RAD_TO_DEG;
	data["min_twist"] = mMinTwist * RAD_TO_DEG;
	data["max_twist"] = mMaxTwist * RAD_TO_DEG;
	return data;
}

//virtual
U64 ElbowConstraint::getHash() const
{
	return HBXXH64::digest((const void*)this, sizeof(ElbowConstraint));
}

LLQuaternion ElbowConstraint::computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const
{
	// Rotate mForward into joint-frame
	LLVector3 joint_forward = mForward * j_loc_rot;

	// Compute adjustment required to move joint_forward back into hinge plane
	LLVector3 proj_j_forw = joint_forward -
							(joint_forward * mPivotAxis) * mPivotAxis;
	LLQuaternion adjustment;
	adjustment.shortestArc(joint_forward, proj_j_forw);
	LLQuaternion adjusted_loc_rot = j_loc_rot * adjustment;

	// Measure twist
	LLVector3 twisted_pivot = mPivotAxis * adjusted_loc_rot;
	F32 cos_part = twisted_pivot * mPivotAxis;
	F32 sin_part = (mLeft * adjusted_loc_rot) * mPivotAxis;
	F32 twist = atan2f(sin_part, cos_part);

	LLVector3 new_j_forw = mForward * adjusted_loc_rot;
	if (twist < mMinTwist || twist > mMaxTwist)
	{
		// adjust twist
		twist = compute_clamped_angle(twist, mMinTwist, mMaxTwist);
		LLVector3 swung_left_axis = mPivotAxis % new_j_forw;
		LLVector3 new_twisted_pivot = cosf(twist) * mPivotAxis -
									  sinf(twist) * swung_left_axis;
		adjustment.shortestArc(twisted_pivot, new_twisted_pivot);
		adjusted_loc_rot = adjusted_loc_rot * adjustment;
		new_j_forw = mForward * adjusted_loc_rot;
	}

	// Measure bend
	F32 bend = atan2f(new_j_forw * mLeft, new_j_forw * mForward);

	if (bend > mMaxBend || bend < mMinBend)
	{
		// Adjust bend
		bend = compute_clamped_angle(bend, mMinBend, mMaxBend);
		new_j_forw = cosf(bend) * mForward + sinf(bend) * mLeft;
		adjustment.shortestArc(joint_forward, new_j_forw);
		adjusted_loc_rot = adjusted_loc_rot * adjustment;
	}
	adjusted_loc_rot.normalize();
	return adjusted_loc_rot;
}

LLQuaternion ElbowConstraint::minimizeTwist(const LLQuaternion& j_loc_rot) const
{
	// Assume all swing is really just bend about mPivotAxis and twist is
	// centered in the valid twist range. If bend_angle is outside the limits
	// then we check both +/- bend_angle and pick the one closest to the
	// allowed range. This comes down to a simple question: which is closer to
	// the midpoint of the bend range ?

	LLVector3 joint_forward = mForward * j_loc_rot;
	F32 fdot = joint_forward * mForward;
	LLVector3 perp_part = joint_forward - fdot * mForward;
	F32 bend_angle = atan2f(perp_part.length(), fdot);

	if (bend_angle < mMinBend  || bend_angle > mMaxBend)
	{
		F32 alt_bend_angle = - bend_angle;
		F32 mid_bend = 0.5f * (mMinBend + mMaxBend);
		if (fabsf(alt_bend_angle - mid_bend) < fabsf(bend_angle - mid_bend))
		{
			bend_angle = alt_bend_angle;
		}
	}
	LLQuaternion bend;
	bend.setAngleAxis(bend_angle, mPivotAxis);

	LLQuaternion mid_twist;
	mid_twist.setAngleAxis(0.5f * (mMinTwist + mMaxTwist), mForward);
	return mid_twist * bend;
}

///////////////////////////////////////////////////////////////////////////////
// LLIK::KneeConstraint class
///////////////////////////////////////////////////////////////////////////////

KneeConstraint::KneeConstraint(const LLVector3& forward_axis,
							   const LLVector3& pivot_axis,
							   F32 min_bend, F32 max_bend)
{
	mType = KNEE_CONSTRAINT;

	mForward = forward_axis;
	mForward.normalize();
	mPivotAxis = mForward % (pivot_axis % mForward);
	mPivotAxis.normalize();
	mLeft = mPivotAxis % mForward;

	mMinBend = min_bend;
	mMaxBend = max_bend;
	compute_angle_limits(mMinBend, mMaxBend);
}

KneeConstraint::KneeConstraint(const LLSD& parameters)
:	Constraint(Constraint::KNEE_CONSTRAINT, parameters)
{
	mPivotAxis = mForward % (LLVector3(parameters["pivot_axis"]) % mForward);
	mPivotAxis.normalize();
	mLeft = mPivotAxis % mForward;

	mMinBend = parameters["min_bend"].asReal() * DEG_TO_RAD;
	mMaxBend = parameters["max_bend"].asReal() * DEG_TO_RAD;
	compute_angle_limits(mMinBend, mMaxBend);
}

//virtual
LLSD KneeConstraint::asLLSD() const
{
	LLSD data = Constraint::asLLSD();
	data["pivot_axis"] = mPivotAxis.getValue();
	data["min_bend"] = mMinBend * RAD_TO_DEG;
	data["max_bend"] = mMaxBend * RAD_TO_DEG;
	return data;
}

//virtual
U64 KneeConstraint::getHash() const
{
	return HBXXH64::digest((const void*)this, sizeof(KneeConstraint));
}

LLQuaternion KneeConstraint::computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const
{
	// Rotate mPivotAxis into joint-frame
	LLVector3 joint_axis = mPivotAxis * j_loc_rot;
	LLQuaternion adjustment;
	adjustment.shortestArc(joint_axis, mPivotAxis);
	LLQuaternion adjusted_loc_rot = j_loc_rot * adjustment;

	// rotate mForward into joint-frame
	LLVector3 joint_forward = mForward * adjusted_loc_rot;

	LLVector3 new_j_forw = joint_forward;

	// compute angle between mForward and new_j_forw
	F32 bend = atan2f(new_j_forw * mLeft, new_j_forw * mForward);
	if (bend > mMaxBend || bend < mMinBend)
	{
		bend = compute_clamped_angle(bend, mMinBend, mMaxBend);
		new_j_forw = cosf(bend) * mForward + sinf(bend) * mLeft;
		adjustment.shortestArc(joint_forward, new_j_forw);
		adjusted_loc_rot = adjusted_loc_rot * adjustment;
	}

	adjusted_loc_rot.normalize();
	return adjusted_loc_rot;
}

LLQuaternion KneeConstraint::minimizeTwist(const LLQuaternion& j_loc_rot) const
{
	// Assume all swing is really just bend about mPivotAxis.
	// If bend_angle is outside the limits then we check both +/- bend_angle and pick
	// the one closest to the allowed range.  This comes down to a simple question:
	// which is closer to the midpoint of the bend range?
	LLVector3 joint_forward = mForward * j_loc_rot;
	F32 fdot = joint_forward * mForward;
	LLVector3 perp_part = joint_forward - fdot * mForward;
	F32 bend_angle = atan2f(perp_part.length(), fdot);
	if (bend_angle < mMinBend  || bend_angle > mMaxBend)
	{
		F32 alt_bend_angle = - bend_angle;
		F32 mid_bend = 0.5f * (mMinBend + mMaxBend);
		if (fabsf(alt_bend_angle - mid_bend) < fabsf(bend_angle - mid_bend))
		{
			bend_angle = alt_bend_angle;
		}
	}
	LLQuaternion bend;
	bend.setAngleAxis(bend_angle, mPivotAxis);
	return bend;
}

///////////////////////////////////////////////////////////////////////////////
// AcuteEllipsoidalCone class
///////////////////////////////////////////////////////////////////////////////

AcuteEllipsoidalCone::AcuteEllipsoidalCone(const LLVector3& forward_axis,
										   const LLVector3& up_axis,
										   F32 forward, F32 up, F32 left,
										   F32 down, F32 right)
:	mXForward(forward),
	mXUp(up),
	mXDown(down),
	mXLeft(left),
	mXRight(right)
{
	mType = ACUTE_ELLIPSOIDAL_CONE_CONSTRAINT;

	mUp = up_axis;
	mUp.normalize();
	mForward = (mUp % forward_axis) % mUp;
	mForward.normalize();
	mLeft = mUp % mForward; // already normalized

	// Divide everything by 'foward' and take make sure they are positive.
	// This normalizes the forward component (adjacent side) of all the
	// triangles to have length=1.0, which is important for our trigonometry
	// math later.
	//
	// up  left			|
	//  | /				| /
	//  |/				 |/
	//  @------------------+
	//		 1.0		/|
	//					 |
	up = fabsf(up / forward);
	left = fabsf(left / forward);
	down = fabsf(down / forward);
	right = fabsf(right / forward);

	// These are the indices of the directions and quadrants.
	// With 'forward' pointing into the page.
	//			 up
	//			  |
	//		  1   |   0
	//			  |
	//  left ------(x)------ right
	//			  |
	//		  2   |   3
	//			  |
	//			down
	//
	// When projecting vectors onto the ellipsoidal surface we will always
	// scale the left-axis into the frame in which the ellipsoid is circular.
	// We cache the necessary scale coefficients now:
	//
	mQuadrantScales[0] = up / right;
	mQuadrantScales[1] = up / left;
	mQuadrantScales[2] = down / left;
	mQuadrantScales[3] = down / right;

	// When determining whether a direction is inside or outside the
	// ellipsoid we will need the cosine and cotangent of the cone angles in
	// the scaled frames. We cache them now:
	//	 cosine = adjacent / hypotenuse
	//	 cotangent = adjacent / opposite
	mQuadrantCosAngles[0] = 1.f / sqrtf(up * up + 1.f);
	mQuadrantCotAngles[0] = 1.f / up;
	mQuadrantCosAngles[1] = mQuadrantCosAngles[0];
	mQuadrantCotAngles[1] = mQuadrantCotAngles[0];
	mQuadrantCosAngles[2] = 1.f / sqrtf(down * down + 1.f);
	mQuadrantCotAngles[2] = 1.f / down;
	mQuadrantCosAngles[3] = mQuadrantCosAngles[2];
	mQuadrantCotAngles[3] = mQuadrantCotAngles[2];
}

AcuteEllipsoidalCone::AcuteEllipsoidalCone(const LLSD& parameters)
:	Constraint(Constraint::ACUTE_ELLIPSOIDAL_CONE_CONSTRAINT, parameters)
{
	mUp = LLVector3(parameters["up_axis"]);
	mUp.normalize();
	mForward = (mUp % mForward) % mUp;
	mForward.normalize();
	mLeft = mUp % mForward; // already normalized

	mXForward = parameters["forward"].asReal();
	mXUp = parameters["up"].asReal();
	mXDown = parameters["down"].asReal();
	mXLeft = parameters["left"].asReal();
	mXRight = parameters["right"].asReal();

	F32 up = fabsf(mXUp / mXForward);
	F32 left = fabsf(mXLeft / mXForward);
	F32 down = fabsf(mXDown / mXForward);
	F32 right = fabsf(mXRight / mXForward);

	mQuadrantScales[0] = up / right;
	mQuadrantScales[1] = up / left;
	mQuadrantScales[2] = down / left;
	mQuadrantScales[3] = down / right;

	mQuadrantCosAngles[0] = 1.f / sqrtf(up * up + 1.f);
	mQuadrantCotAngles[0] = 1.f / up;
	mQuadrantCosAngles[1] = mQuadrantCosAngles[0];
	mQuadrantCotAngles[1] = mQuadrantCotAngles[0];
	mQuadrantCosAngles[2] = 1.f / sqrtf(down * down + 1.f);
	mQuadrantCotAngles[2] = 1.f / down;
	mQuadrantCosAngles[3] = mQuadrantCosAngles[2];
	mQuadrantCotAngles[3] = mQuadrantCotAngles[2];
}

//virtual
LLSD AcuteEllipsoidalCone::asLLSD() const
{
	LLSD data = Constraint::asLLSD();
	data["up_axis"] = mUp.getValue();
	data["forward"] = mXForward;
	data["down"] = mXDown;
	data["left"] = mXLeft;
	data["right"] = mXRight;
	return data;
}

//virtual
U64 AcuteEllipsoidalCone::getHash() const
{
	return HBXXH64::digest((const void*)this, sizeof(AcuteEllipsoidalCone));
}

LLQuaternion AcuteEllipsoidalCone::computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const
{
	// Rotate mForward into joint-frame
	LLVector3 joint_forward = mForward * j_loc_rot;
	// joint_forward is normalized

	// Determine its quadrant
	F32 up_component = joint_forward * mUp;
	F32 left_component = joint_forward * mLeft;
	U32 q = 0;
	if (up_component < 0.f)
	{
		if (left_component < 0.f)
		{
			q = 2;
		}
		else
		{
			q = 3;
		}
	}
	else if (left_component < 0.f)
	{
		q = 1;
	}

	// Scale left axis to frame in which ellipse is a circle
	F32 scaled_left_comp = left_component * mQuadrantScales[q];

	// Reassemble in scaled frame
	F32 forward_component = joint_forward * mForward;
	LLVector3 new_j_forw = forward_component * mForward + up_component * mUp +
						   scaled_left_comp * mLeft;
	// new_j_forw is not normalized
	// which means we must adjust its the forward_component when
	// checking for violation in scaled frame
	if (forward_component / new_j_forw.length() < mQuadrantCosAngles[q])
	{
		// joint violates constraint --> project onto cone
		//
		// violates	  projected
		//	   +		+
		//		.	  /|
		//		 .	/ |
		//		  .  // |
		//		   .//  |
		//			@---+----
		//			 `
		//			  `
		//
		// Orthogonal components remain unchanged but we need to compute
		// a corrected forward_component (adjacent leg of the right triangle)
		// in the scaled frame. We can use the formula:
		//	 adjacent = opposite * cos(angle) / sin(angle)
		//	 adjacent = opposite * cot(angle)
		//
		F32 orthogonal_component = sqrtf(scaled_left_comp * scaled_left_comp +
										 up_component * up_component);
		forward_component = orthogonal_component * mQuadrantCotAngles[q];

		// Re-assemble the projected direction in the non-scaled frame:
		new_j_forw = forward_component * mForward + up_component * mUp +
					 left_component * mLeft;
		// new_j_forw is not normalized, but it doesn't matter

		// Compute adjusted_loc_rot
		LLQuaternion adjustment;
		adjustment.shortestArc(joint_forward, new_j_forw);
		LLQuaternion adjusted_loc_rot = j_loc_rot * adjustment;
		adjusted_loc_rot.normalize();
		return adjusted_loc_rot;
	}

	return j_loc_rot;
}

///////////////////////////////////////////////////////////////////////////////
// LLIK::DoubleLimitedHinge class
///////////////////////////////////////////////////////////////////////////////

DoubleLimitedHinge::DoubleLimitedHinge(const LLVector3& forward_axis,
									   const LLVector3& up_axis,
									   F32 min_yaw, F32 max_yaw,
									   F32 min_pitch, F32 max_pitch)
{
	mType = DOUBLE_LIMITED_HINGE_CONSTRAINT;

	mForward = forward_axis;
	mForward.normalize();
	mUp = mForward % (up_axis % mForward);
	mUp.normalize();
	mLeft = mUp % mForward;

	mMinYaw = min_yaw;
	mMaxYaw = max_yaw;
	compute_angle_limits(mMinYaw, mMaxYaw);

	// Keep pitch in range [-PI/2, PI/2]
	F32 HALF_PI = 0.5f * F_PI;
	mMinPitch = remove_multiples_of_two_pi(min_pitch);
	if (mMinPitch > HALF_PI)
	{
		mMinPitch = HALF_PI;
	}
	else if (mMinPitch < -HALF_PI)
	{
		mMinPitch = -HALF_PI;
	}
	mMaxPitch = remove_multiples_of_two_pi(max_pitch);
	if (mMaxPitch > HALF_PI)
	{
		mMaxPitch = HALF_PI;
	}
	else if (mMaxPitch < -HALF_PI)
	{
		mMaxPitch = -HALF_PI;
	}
	if (mMinPitch > mMaxPitch)
	{
		F32 temp = mMinPitch;
		mMinPitch = mMaxPitch;
		mMaxPitch = temp;
	}
}

DoubleLimitedHinge::DoubleLimitedHinge(const LLSD& parameters)
:	Constraint(Constraint::DOUBLE_LIMITED_HINGE_CONSTRAINT, parameters)
{
	mUp = mForward % (LLVector3(parameters["up_axis"]) % mForward);
	mUp.normalize();
	mLeft = mUp % mForward;

	mMinYaw = parameters["min_yaw"].asReal() * DEG_TO_RAD;
	mMaxYaw = parameters["max_yaw"].asReal() * DEG_TO_RAD;
	compute_angle_limits(mMinYaw, mMaxYaw);

	// Keep pitch in range [-PI/2, PI/2]
	F32 HALF_PI = 0.5f * F_PI;
	F32 min_pitch = parameters["min_pitch"].asReal() * DEG_TO_RAD;
	mMinPitch = remove_multiples_of_two_pi(min_pitch);
	if (mMinPitch > HALF_PI)
	{
		mMinPitch = HALF_PI;
	}
	else if (mMinPitch < -HALF_PI)
	{
		mMinPitch = -HALF_PI;
	}
	F32 max_pitch = parameters["max_pitch"].asReal() * DEG_TO_RAD;
	mMaxPitch = remove_multiples_of_two_pi(max_pitch);
	if (mMaxPitch > HALF_PI)
	{
		mMaxPitch = HALF_PI;
	}
	else if (mMaxPitch < -HALF_PI)
	{
		mMaxPitch = -HALF_PI;
	}
	if (mMinPitch > mMaxPitch)
	{
		F32 temp = mMinPitch;
		mMinPitch = mMaxPitch;
		mMaxPitch = temp;
	}
}

//virtual
LLSD DoubleLimitedHinge::asLLSD() const
{
	LLSD data = Constraint::asLLSD();
	data["up_axis"] = mUp.getValue();
	data["min_yaw"] = mMinYaw * RAD_TO_DEG;
	data["max_yaw"] = mMaxYaw * RAD_TO_DEG;
	data["min_pitch"] = mMinPitch * RAD_TO_DEG;
	data["max_pitch"] = mMaxPitch * RAD_TO_DEG;
	return data;
}

//virtual
U64 DoubleLimitedHinge::getHash() const
{
	return HBXXH64::digest((const void*)this, sizeof(DoubleLimitedHinge));
}

LLQuaternion DoubleLimitedHinge::computeAdjustedLocalRot(const LLQuaternion& j_loc_rot) const
{
	// Twist: eliminate twist by adjusting the rotated mLeft axis to remain in
	// horizontal plane
	LLVector3 joint_left = mLeft * j_loc_rot;
	LLQuaternion adjustment;
	adjustment.shortestArc(joint_left, joint_left - (joint_left * mUp) * mUp);
	LLQuaternion adjusted_loc_rot = j_loc_rot * adjustment;

	LLVector3 joint_forward = mForward * adjusted_loc_rot;

	// Yaw
	F32 up_component = joint_forward * mUp;
	LLVector3 horizontal_axis = joint_forward - up_component * mUp;
	F32 yaw = atan2f(horizontal_axis * mLeft, horizontal_axis * mForward);
	if (yaw > mMaxYaw || yaw < mMinYaw)
	{
		yaw = compute_clamped_angle(yaw, mMinYaw, mMaxYaw);
		horizontal_axis = cosf(yaw) * mForward + sinf(yaw) * mLeft;
	}
	else
	{
		horizontal_axis.normalize();
	}

	// Pitch. Note: the minus-sign in the "opposite" (sin) term here is because
	// our pitch-axis is mLeft and according to the right-hand-rule positive
	// pitch drops the forward axis down.
	F32 horiz_comp = sqrtf(llmax(1.f - up_component * up_component, 0.f));
	F32 pitch = atan2f(-up_component, horiz_comp);
	if (pitch > mMaxPitch || pitch < mMinPitch)
	{
		pitch = compute_clamped_angle(pitch, mMinPitch, mMaxPitch);
		up_component = -sinf(pitch);
		horiz_comp = sqrtf(llmax(1.f - up_component * up_component, 0.f));
	}

	LLVector3 new_j_forw = horiz_comp * horizontal_axis + up_component * mUp;
	new_j_forw.normalize();
	if (dist_vec(joint_forward, new_j_forw) > 1.0e-3f)
	{
		// Compute adjusted_loc_rot
		adjustment.shortestArc(joint_forward, new_j_forw);
		adjusted_loc_rot = adjusted_loc_rot * adjustment;
	}
	adjusted_loc_rot.normalize();
	return adjusted_loc_rot;
}

// Eliminates twist by adjusting the rotated mLeft axis to remain in horizontal
// plane
LLQuaternion DoubleLimitedHinge::minimizeTwist(const LLQuaternion& j_loc_rot) const
{
	LLVector3 joint_left = mLeft * j_loc_rot;
	LLQuaternion adjustment;
	adjustment.shortestArc(joint_left, joint_left - (joint_left * mUp) * mUp);
	LLQuaternion adjusted_loc_rot = j_loc_rot * adjustment;
	adjusted_loc_rot.normalize();
	return adjusted_loc_rot;
}

///////////////////////////////////////////////////////////////////////////////
// LLIK::Joint::Config sub-class
///////////////////////////////////////////////////////////////////////////////

void Joint::Config::updateFrom(const Config& other_config)
{
	if (mFlags == other_config.mFlags)
	{
		*this = other_config;	// other_config updates everything
	}
	else	// Find and apply all parameters in other_config
	{
		if (other_config.hasLocalPos())
		{
			setLocalPos(other_config.mLocalPos);
		}
		if (other_config.hasLocalRot())
		{
			setLocalRot(other_config.mLocalRot);
		}
		if (other_config.hasTargetPos())
		{
			setTargetPos(other_config.mTargetPos);
		}
		if (other_config.hasTargetRot())
		{
			setTargetRot(other_config.mTargetRot);
		}
		if (other_config.hasLocalScale())
		{
			setLocalScale(other_config.mLocalScale);
		}
		if (other_config.constraintIsDisabled())
		{
			disableConstraint();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLIK::Joint class
///////////////////////////////////////////////////////////////////////////////

Joint::Joint(LLJoint* info_ptr)
:	mInfoPtr(info_ptr),
	mID(info_ptr->getJointNum()),
	mConfig(NULL),
	mConfigFlags(0),
	mIkFlags(0)
{
	resetFromInfo();
}

void Joint::resetFromInfo()
{
	const LLVector3& scale = mInfoPtr->getScale();
	mLocalPos = mInfoPtr->getPosition().scaledVec(scale);
	mBone = mInfoPtr->getEnd().scaledVec(scale);
	mLocalPosLength = mLocalPos.length();
	// This is correct: we do NOT store info scale in mLocalScale which
	// represents Puppetry's tweak on top of whatever is set in the info.
	mLocalScale.set(1.f, 1.f, 1.f);
}

void Joint::addChild(const ptr_t& child)
{
	if (child)
	{
		mChildren.push_back(child);
	}
}

void Joint::setTargetPos(const LLVector3& pos)
{
	if (hasPosTarget())
	{
		// *HACK: cast mConfig to non-const pointer so we can modify it
		Config* config = const_cast<Config*>(mConfig);
		config->setTargetPos(pos);
	}
}

void Joint::setParent(const ptr_t& parent)
{
	mParent = parent;
	if (!mParent)
	{
		// The root's local orientation is never updated by the IK algorithm.
		// Whatever orientation it has at the start of IK will be its final,
		// which is why we flag it as "locked". This also simplifies logic
		// elsewhere: in a few places we assume any non-locked Joint has a parent.
		mIkFlags = IK_FLAG_LOCAL_ROT_LOCKED;
	}
	reset();
}

void Joint::reset()
{
	resetFromInfo();
	// Note: we do not bother to enforce localRotLocked() here because any call
	// to reset() is expected to be outside the Solver IK iterations.
	mLocalRot = LLQuaternion::DEFAULT;
	if (mParent)
	{
		mPos = mParent->mPos + mLocalPos * mParent->mRot;
		mRot = mParent->mRot;
	}
	else
	{
		mPos = mLocalPos;
		mRot = mLocalRot;
	}
}

void Joint::relaxRot(F32 blend_factor)
{
	if (!localRotLocked())
	{
		mLocalRot = lerp(blend_factor, mLocalRot, LLQuaternion::DEFAULT);
	}
	if (mParent)
	{
		// We always re-compute world-frame transform because parent may have
		// relaxed.
		mRot = mLocalRot * mParent->mRot;
		mRot.normalize();
		mPos = mParent->mPos + mLocalPos * mParent->mRot;
	}
	else
	{
		mRot = mLocalRot;
		mPos = mLocalPos;
	}
}

void Joint::resetRecursively()
{
	reset();
	for (auto& child : mChildren)
	{
		child->resetRecursively();
	}
}

void Joint::relaxRotationsRecursively(F32 blend_factor)
{
	blend_factor = llclamp(blend_factor, 0.f, 1.f);
	relaxRot(blend_factor);

	for (auto& child : mChildren)
	{
		if (child->isActive())
		{
			child->relaxRotationsRecursively(blend_factor);
		}
	}
}

F32 Joint::recursiveComputeLongestChainLength(F32 length) const
{
	length += mLocalPosLength;
	F32 longest_length = length;
	if (mChildren.empty())
	{
		longest_length += mBone.length();
	}
	else
	{
		for (const auto& child : mChildren)
		{
			F32 child_len = child->recursiveComputeLongestChainLength(length);
			if (child_len > longest_length)
			{
				longest_length = child_len;
			}
		}
	}
	return longest_length;
}

LLVector3 Joint::computeEndTargetPos() const
{
	// Note: we expect this Joint has either: a target, or at least one
	// active child
	if (hasPosTarget())
	{
		return mConfig->getTargetPos();
	}
	LLVector3 target_pos;
	S32 num_active_children = 0;
	for (const auto& child : mChildren)
	{
		if (child->isActive())
		{
			target_pos += child->mPos;
			++num_active_children;
		}
	}
	if (!num_active_children)
	{
		llwarns_sparse << "No active children !" << llendl;
		return target_pos;
	}
	return (1.f / F32(num_active_children)) * target_pos;
}

LLVector3 Joint::computeWorldTipOffset() const
{
	LLVector3 offset = mPos;
	if (mParent)
	{
		offset -= mParent->mPos + mLocalPos * mParent->mRot;
	}
	return offset;
}

void Joint::updateEndInward()
{
	// Note: during FABRIK we DO NOT enforce constraints.
	if (hasRotTarget())
	{
		mRot = mConfig->getTargetRot();
		if (hasPosTarget())
		{
			mPos = mConfig->getTargetPos() - mBone * mRot;
		}
	}
	else
	{
		std::vector<LLVector3> local_targets, world_targets;
		collectTargetPositions(local_targets, world_targets);
		size_t num_targets = local_targets.size();
		if (num_targets == 1)
		{
			// Special handling for the most common num_targets == 1 case
			// compute mPos
			LLVector3 bone_dir = world_targets[0] - mPos;
			bone_dir.normalize();
			mPos = world_targets[0] - (local_targets[0].length() * bone_dir);

			// Compute new mRot
			LLVector3 old_bone = local_targets[0] * mRot;
			LLQuaternion adjustment;
			adjustment.shortestArc(old_bone, bone_dir);
			mRot = mRot * adjustment;
			mRot.normalize();
		}
		else
		{
			LLVector3 new_pos;
			// Origin in quaternion space
			LLQuaternion avg_adjustment(0.f, 0.f, 0.f, 0.f);
			for (size_t i = 0; i < num_targets; ++i)
			{
				// mPos
				LLVector3 new_bone = world_targets[i] - mPos;
				new_bone.normalize();
				new_bone *= local_targets[i].length();
				new_pos += world_targets[i] - new_bone;

				// mRot
				LLVector3 old_bone = local_targets[i] * mRot;
				LLQuaternion adjustment;
				adjustment.shortestArc(old_bone, new_bone);
				if (adjustment.mQ[VW] < 0.f)
				{
					// Negate to keep all arithmetic on the same hypersphere
					avg_adjustment = avg_adjustment - adjustment;
				}
				else
				{
					avg_adjustment = avg_adjustment + adjustment;
				}
			}
			if (mParent && mParent->isActive())
			{
				// Compute mPos
				mPos = new_pos / (F32)(num_targets);
			}

			// Compute mRot
			avg_adjustment.normalize();
			mRot = mRot * avg_adjustment;
			mRot.normalize();
		}
	}
	// Note: mLocalRot will be updated later when we know mParent's location

	// Now that we know mRot --> update children mLocalRot
	for (auto& child : mChildren)
	{
		if (child->isActive())
		{
			child->updateLocalRot();
		}
	}
}

void Joint::updateEndOutward()
{
	// Note: during FABRIK we DO NOT enforce constraints.
	// mParent is expected to be non-null.
	mPos = mParent->mPos + mLocalPos * mParent->mRot;

	// mRot
	if (localRotLocked())
	{
		mRot = mLocalRot * mParent->mRot;
		return;
	}

	if (hasRotTarget())
	{
		mRot = mConfig->getTargetRot();
		if (hasPosTarget())
		{
			mPos = mConfig->getTargetPos() - mBone * mRot;
		}
	}
	else
	{
		std::vector<LLVector3> local_targets, world_targets;
		collectTargetPositions(local_targets, world_targets);
		size_t num_targets = local_targets.size();
		if (num_targets == 1)
		{
			// Special handling for the most common num_targets == 1 case
			LLVector3 new_bone = world_targets[0] - mPos;
			LLVector3 old_bone = local_targets[0] * mRot;
			LLQuaternion adjustment;
			adjustment.shortestArc(old_bone, new_bone);
			mRot = mRot * adjustment;
		}
		else
		{
			// Origin in quaternion space
			LLQuaternion avg_adjustment(0.f, 0.f, 0.f, 0.f);
			LLQuaternion adjustment;
			for (size_t i = 0; i < num_targets; ++i)
			{
				LLVector3 new_bone = world_targets[i] - mPos;
				LLVector3 old_bone = local_targets[i] * mRot;
				adjustment.shortestArc(old_bone, new_bone);
				if (adjustment.mQ[VW] < 0.f)
				{
					// Negate to keep all Quaternion arithmetic on one
					// hypersphere
					avg_adjustment = avg_adjustment - adjustment;
				}
				else
				{
					avg_adjustment = avg_adjustment + adjustment;
				}
			}
			avg_adjustment.normalize();
			mRot = mRot * avg_adjustment;
		}
		mRot.normalize();
	}

	updateLocalRot();
}

// This Joint's child is specified in argument in case this Joint has multiple
// children.
void Joint::updateInward(const Joint::ptr_t& child)
{
	// Note: during FABRIK we DO NOT enforce constraints. mParent is expected
	// to be non-null.
	// Compute mPos
	LLVector3 old_pos = mPos;
	LLVector3 bone_dir = child->mPos - old_pos;
	bone_dir.normalize();
	mPos = child->mPos - child->mLocalPosLength * bone_dir;
	// Compute mRot
	LLVector3 old_bone = child->mLocalPos * mRot;
	LLQuaternion adjustment;
	adjustment.shortestArc(old_bone, bone_dir);
	mRot = mRot * adjustment;
	mRot.normalize();
	// Compute child->mLocalRot
	child->updateLocalRot();
	// this->mLocalRot will be updated later
}

void Joint::updatePosAndRotFromParent()
{
	if (mParent)
	{
		mPos = mParent->mPos + mLocalPos * mParent->mRot;
		mRot = mLocalRot * mParent->mRot;
		mRot.normalize();
	}
}

void Joint::updateOutward()
{
	// Note: during FABRIK we DO NOT enforce constraints.
	// mParent is expected to be non-null.
	LLVector3 old_end_pos = mPos + mBone * mRot;

	// mPos
	mPos = mParent->mPos + mLocalPos * mParent->mRot;

	// mRot
	LLVector3 new_bone = old_end_pos - mPos;
	LLVector3 old_bone = mBone * mRot;
	LLQuaternion dQ;
	dQ.shortestArc(old_bone, new_bone);
	mRot = mRot * dQ;
	mRot.normalize();

	updateLocalRot();
}

void Joint::applyLocalRot()
{
	if (!mParent)
	{
		return;
	}
	if (hasRotTarget())
	{
		// Apply backpressure by lerping toward new_rot
		LLQuaternion new_rot = mLocalRot * mParent->mRot;
		constexpr F32 WORLD_ROT_TARGET_BACKPRESSURE_COEF = 0.5f;
		mRot = lerp(WORLD_ROT_TARGET_BACKPRESSURE_COEF,
					mConfig->getTargetRot(), new_rot);

		// Recompute mLocalRot
		LLQuaternion inv_parent_rot = mParent->mRot;
		inv_parent_rot.transpose();
		mLocalRot = mRot * inv_parent_rot;
		mLocalRot.normalize();
	}
	else
	{
		mRot = mLocalRot * mParent->mRot;
		mRot.normalize();
	}
}

void Joint::updateLocalRot()
{
	if (!localRotLocked())
	{
		// mPos and mRot are expected to be correct and mParent is expected to
		// be valid
		LLQuaternion inv_parent_rot = mParent->mRot;
		inv_parent_rot.transpose();
		mLocalRot = mRot * inv_parent_rot;
		mLocalRot.normalize();
	}
}

LLQuaternion Joint::computeParentRot() const
{
	// Formula is:
	//	 mRot = mLocalRot * mParent->mRot
	// Solving for mParent->mRot gives:
	//	 mParent->mRot = mLocalRotInv * mRot
	LLQuaternion q = mLocalRot;
	q.transpose();
	q = q * mRot;
	q.normalize();
	return q;
}

void Joint::updateChildLocalRots() const
{
	// Now that we know mRot we can update the childrens' mLocalRot
	for (const Joint::ptr_t& child : mChildren)
	{
		if (child->isActive())
		{
			child->updateLocalRot();
		}
	}
}

void Joint::lockLocalRot(const LLQuaternion& local_rot)
{
	mLocalRot = local_rot;
	mIkFlags |= IK_FLAG_LOCAL_ROT_LOCKED;
	activate();
	if (!mParent)
	{
		mRot = local_rot;
	}
}

bool Joint::enforceConstraint()
{
	if (localRotLocked())
	{
		// A fixed mLocalRot is effectively like a fixed Constraint so we
		// always return 'true' here: the Constraint is in effect and mRot may
		// have been optimistically modified but mLocalRot was not.
		return true;
	}
	if (mConstraint && !hasDisabledConstraint())
	{
		return mConstraint->enforce(*this);
	}
	return false;
}

void Joint::updateWorldTransformsRecursively()
{
	updatePosAndRotFromParent();
	for (Joint::ptr_t& child : mChildren)
	{
		if (child->isActive())
		{
			child->updateWorldTransformsRecursively();
		}
	}
}

// Returns valid Joint::ptr_t to child iff only one child is active, else
// returns null Joint::ptr_t
Joint::ptr_t Joint::getSingleActiveChild()
{
	Joint::ptr_t active_child;
	for (Joint::ptr_t& child : mChildren)
	{
		if (child->isActive())
		{
			if (active_child)
			{
				// Second child --> this Joint is not a "false" sub-base
				active_child.reset();
				break;
			}
			active_child = child;
		}
	}
	return active_child;
}

void Joint::setLocalRot(const LLQuaternion& new_local_rot)
{
	if (!localRotLocked())
	{
		constexpr F32 BLEND_COEF = 0.25f;
		mLocalRot = lerp(BLEND_COEF, mLocalRot, new_local_rot);
	}
}

// Only call this if you know what you are doing; this should only be called
// once before starting IK algorithm iterations.
void Joint::setLocalScale(const LLVector3& scale)
{
	// Compute final scale adustment to applly to mLocalPos and mBone
	constexpr F32 MIN_INVERTABLE_SCALE = 1.0e-15f;
	LLVector3 re_scale;
	for (U32 i = 0; i < 3; ++i)
	{
		// Verify mLocalScale component to avoid introducing NaN
		if (mLocalScale[i] > MIN_INVERTABLE_SCALE)
		{
			re_scale[i] = scale[i] / mLocalScale[i];
		}
		else
		{
			re_scale[i] = 0.f;
		}
	}
	// We remember the final scale adjustment for later...
	mLocalScale = scale;
	// ...and apply it immediately onto mLocalPos and mBone.
	mBone.scaleVec(re_scale);
	mLocalPos.scaleVec(re_scale);
	mLocalPosLength = mLocalPos.length();
}

LLVector3 Joint::getPreScaledLocalPos() const
{
	LLVector3 pos = mLocalPos;
	// We inverse-scale mLocalPos because we already applied the info's scale
	// to mLocalPos so we could perform IK without constantly recomputing it,
	// and now we are being asked for mLocalPos in the info's pre-scaled frame.
	LLVector3 inv_scale = mInfoPtr->getScale();
	constexpr F32 MIN_INVERTABLE_SCALE = 1.0e-15f;
	for (U32 i = 0; i < 3; ++i)
	{
		// Verify mLocalScale component to avoid introducing NaN
		if (inv_scale[i] > MIN_INVERTABLE_SCALE)
		{
			inv_scale[i] = 1.f / inv_scale[i];
		}
		else
		{
			inv_scale[i] = 0.f;
		}
	}
	pos.scaleVec(inv_scale);
	return pos;
}

void Joint::adjustWorldRot(const LLQuaternion& adjustment)
{
	mRot = mRot * adjustment;
	updateLocalRot();
	if (enforceConstraint())
	{
		applyLocalRot();
	}
}

void Joint::collectTargetPositions(std::vector<LLVector3>& local_targets,
								   std::vector<LLVector3>& world_targets) const
{
	// The "target positions" are points in the Joint local-frame which
	// correspond to points in other frames: either child positions or a target
	// end-effector. We need to know these positions in both local and world
	// frames.
	//
	// Note: it is expected this Joint has either: a target, or at least one
	// active child
	if (hasPosTarget())
	{
		local_targets.emplace_back(mBone);
		world_targets.emplace_back(mConfig->getTargetPos());
	}
	else
	{
		// *TODO: local_centroid and its length are invarient for the lifetime
		// of the Chains so we could pre-compute and cache them and simplify
		// the logic which consumes this info.
		for (const auto& child : mChildren)
		{
			if (child->isActive())
			{
				local_targets.emplace_back(child->mLocalPos);
				world_targets.emplace_back(child->mPos);
			}
		}
	}
}

void Joint::transformTargetsToParentLocal(std::vector<LLVector3>& local) const
{
	if (mParent)
	{
		LLQuaternion world_to_parent = mParent->mRot;
		world_to_parent.transpose();
		for (auto& target : local)
		{
			LLVector3 world_target = (mPos + target * mRot) - mParent->mPos;
			target = world_target * world_to_parent;
		}
	}
}

bool Joint::swingTowardTargets(const std::vector<LLVector3>& local_targets,
							   const std::vector<LLVector3>& world_targets)
{
	if (localRotLocked())
	{
		// Nothing to do, but we assume targets are not yet aligned and return
		// 'true'
		return true;
	}

	constexpr F32 MIN_SWING_ANGLE = 0.001f * F_PI;
	bool something_changed = false;
	if (hasRotTarget())
	{
		mRot = mConfig->getTargetRot();
		something_changed = true;
	}
	else
	{
		size_t num_targets = local_targets.size();
		LLQuaternion adjustment;
		if (num_targets == 1)
		{
			LLVector3 old_bone = local_targets[0] * mRot;
			LLVector3 new_bone = world_targets[0] - mPos;
			adjustment.shortestArc(old_bone, new_bone);
		}
		else
		{
			adjustment.mQ[VW] = 0.f;
			for (size_t i = 0; i < num_targets; ++i)
			{
				LLVector3 old_bone = local_targets[i] * mRot;
				LLVector3 new_bone = world_targets[i] - mPos;
				LLQuaternion adj;
				adj.shortestArc(old_bone, new_bone);
				if (adj.mQ[VW] < 0.f)
				{
					// Negate to keep all arithmetic on the same hypersphere
					adjustment = adjustment - adj;
				}
				else
				{
					adjustment = adjustment + adj;
				}
			}
			adjustment.normalize();
		}

		if (!LLQuaternion::almost_equal(adjustment,
										LLQuaternion::DEFAULT, MIN_SWING_ANGLE))
		{
			// lerp the adjustment instead of using the full rotation: this
			// allows swing to distribute along the length of the chain.
			constexpr F32 SWING_FACTOR = 0.25f;
			adjustment = lerp(SWING_FACTOR, LLQuaternion::DEFAULT, adjustment);

			// compute mRot
			mRot = mRot * adjustment;
			mRot.normalize();
			something_changed = true;
		}
	}
	if (something_changed)
	{
		// Compute mLocalRot. Instead of calling updateLocalRot() which has
		// extra checks unnecessary in this context, we do the math explicitly.
		LLQuaternion inv_parent_rot = mParent->mRot;
		inv_parent_rot.transpose();
		mLocalRot = mRot * inv_parent_rot;
		mLocalRot.normalize();

		if (enforceConstraint())
		{
			applyLocalRot();
#if LLIK_EXPERIMENTAL
			// EXPERIMENTAL: we hit the constraint during the swing; perhaps
			// some twist can get us closer
			twistTowardTargets(local_targets, world_targets);
#endif
		}
	}
	return something_changed;
}

#if LLIK_EXPERIMENTAL
void Joint::twistTowardTargets(const std::vector<LLVector3>& local_targets,
							   const std::vector<LLVector3>& world_targets)
{
	if (!mConstraint->allowsTwist())
	{
		return;
	}
	// Always twist about mConstraint->mForward axis
	LLVector3 axis = mConstraint->getForwardAxis() * mRot;
	LLQuaternion adjustment;
	size_t num_targets = local_targets.size();
	if (num_targets == 1)
	{
		// Transform to the world-frame with mPos as origin
		LLVector3 local_target = local_targets[0] * mRot;
		LLVector3 world_target = world_targets[0] - mPos;
		F32 target_length = local_target.length();
		constexpr F32 MIN_TARGET_LENGTH = 1.0e-2f;
		if (target_length < MIN_TARGET_LENGTH)
		{
			// Bone is too short
			return;
		}

		// Remove components parallel to axis
		local_target -= (local_target * axis) * axis;
		world_target -= (world_target * axis) * axis;

		if (local_target * world_target < 0.f)
		{
			// This discrepancy is better served with a swing
			return;
		}

		F32 radius = local_target.length();
		constexpr F32 MIN_RADIUS_FRACTION = 1.0e-2f;
		const F32 MIN_RADIUS = MIN_RADIUS_FRACTION * target_length;
		if (radius < MIN_RADIUS || world_target.length() < MIN_RADIUS)
		{
			// Twist movement too small to bother
			return;
		}

		// Compute the adjustment
		adjustment.shortestArc(local_target, world_target);
	}
	else
	{
		adjustment.mQ[VW] = 0.f;
		U32 num_adjustments = 0;
		for (size_t i = 0; i < local_targets.size(); ++i)
		{
			LLQuaternion adj;
			// Transform to the world-frame with mPos as origin
			LLVector3 local_target = local_targets[i] * mRot;
			LLVector3 world_target = world_targets[i] - mPos;
			F32 target_length = local_target.length();
			constexpr F32 MIN_TARGET_LENGTH = 1.0e-2f;
			if (target_length < MIN_TARGET_LENGTH)
			{
				// bone is too short
				adjustment = adjustment + adj;
				return;
			}

			// Remove components parallel to axis
			local_target -= (local_target * axis) * axis;
			world_target -= (world_target * axis) * axis;

			if (local_target * world_target < 0.f)
			{
				// This discrepancy is better served with a swing
				adjustment = adjustment + adj;
				return;
			}

			F32 radius = local_target.length();
			constexpr F32 MIN_RADIUS_FRACTION = 1.0e-2f;
			const F32 MIN_RADIUS = MIN_RADIUS_FRACTION * target_length;
			if (radius < MIN_RADIUS || world_target.length() < MIN_RADIUS)
			{
				// Twist movement will be too small
				adjustment = adjustment + adj;
				return;
			}

			// Compute the adjustment
			adj.shortestArc(local_target, world_target);
			adjustment = adjustment + adj;
			++num_adjustments;
		}
		if (num_adjustments == 0)
		{
			return;
		}
		adjustment.normalize();
	}

	// lerp the adjustment instead of using the full rotation; this allows
	// twist to distribute along the length of the chain.
	constexpr F32 TWIST_BLEND = 0.4f;
	adjustment = lerp(TWIST_BLEND, LLQuaternion::DEFAULT, adjustment);

	// Compute mRot
	mRot = mRot * adjustment;
	mRot.normalize();

	// Compute mLocalRot. Instead of calling updateLocalRot() which has extra
	// checks unnecessary in this context, we do the math explicitly.
	LLQuaternion inv_parent_rot = mParent->mRot;
	inv_parent_rot.transpose();
	mLocalRot = mRot * inv_parent_rot;
	mLocalRot.normalize();

	if (enforceConstraint())
	{
		applyLocalRot();
	}
}
#endif	// LLIK_EXPERIMENTAL

void Joint::untwist()
{
	if (hasRotTarget())
	{
		mRot = mConfig->getTargetRot();
		updateLocalRot();
	}
	else if (!localRotLocked())
	{
		// Compute new_local_rot
		LLQuaternion new_local_rot = LLQuaternion::DEFAULT;
		if (mConstraint && !hasDisabledConstraint())
		{
			new_local_rot = mConstraint->minimizeTwist(mLocalRot);
		}
		else
		{
			LLVector3 bone = mBone;
			bone.normalize();
			LLVector3 new_bone = bone * mLocalRot;
			LLVector3 swing_axis = bone % new_bone;
			constexpr F32 MIN_SWING_AXIS_LENGTH = 1.0e-3f;
			if (swing_axis.length() > MIN_SWING_AXIS_LENGTH)
			{
				F32 swing_angle = acosf(new_bone * bone);
				new_local_rot.setAngleAxis(swing_angle, swing_axis);
			}
		}

		// Blend toward new_local_rot
		constexpr F32 UNTWIST_BLEND = 0.25f;
		mLocalRot = lerp(UNTWIST_BLEND, mLocalRot, new_local_rot);
		// Note: if UNTWIST_BLEND is increased here the consequence will be
		// more noticeable occasional pops in some joints. It is an interaction
		// with transitions in/out of the
		//	 if (swing_axis.length() > MIN_SWING_AXIS_LENGTH)
		// condition above.

		// Apply new mLocalRot
		LLQuaternion new_rot = mLocalRot * mParent->mRot;
		if (!mParent->localRotLocked())
		{
			// Check to see if new mLocalRot would change world-frame bone
			// (which only happens for some Constraints)
			LLVector3 old_bone = mBone * mRot;
			LLVector3 new_bone = mBone * new_rot;
			constexpr F32 MIN_DELTA_COEF = 0.01f;
			if ((new_bone - old_bone).length() >
					MIN_DELTA_COEF * mBone.length())
			{
				// The new mLocalRot would change the world-frame bone
				// direction so we counter-rotate mParent to compensate.

				// Compute axis of correction
				LLVector3 axis = mParent->mBone * mParent->mRot;
				axis.normalize();

				// Project child bones to plane
				old_bone = old_bone - (old_bone * axis) * axis;
				new_bone = new_bone - (new_bone * axis) * axis;

				// Compute correction from new_bone back to old_bone
				LLQuaternion twist;
				twist.shortestArc(new_bone, old_bone);

				// Compute new parent rot
				LLQuaternion new_parent_rot = mParent->mRot * twist;
				new_parent_rot.normalize();
				mParent->setWorldRot(new_parent_rot);
				mParent->updateLocalRot();

				// Compute new rot
				new_rot = mLocalRot * mParent->mRot;
			}
		}
		mRot = new_rot;
		mRot.normalize();
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLIK::Solver class
///////////////////////////////////////////////////////////////////////////////

void Solver::resetSkeleton()
{
	mSkeleton.begin()->second->resetRecursively();
}

// Computes the offset from the "tip" of from_id to the "end" of to_id or the
// negative when from_id > to_id
LLVector3 Solver::computeReach(S16 to_id, S16 from_id) const
{
	S16 ancestor = from_id;
	S16 descendent = to_id;
	bool swapped = false;
	if (ancestor > descendent)
	{
		ancestor = to_id;
		descendent = from_id;
		swapped = true;
	}
	LLVector3 reach;
	// Start at descendent and traverse up the limb until we find the ancestor
	joint_map_t::const_iterator itr = mSkeleton.find(descendent);
	if (itr != mSkeleton.end())
	{
		Joint::ptr_t joint = itr->second;
		LLVector3 chain_reach = joint->getBone();
		while (joint)
		{
			chain_reach += joint->getLocalPos();
			joint = joint->getParent();
			if (joint && joint->getID() == ancestor)
			{
				// Success !
				reach = chain_reach;
				break;
			}
		}
	}
	if (swapped)
	{
		reach = - reach;
	}
	return reach;
}

void Solver::addJoint(S16 joint_id, S16 parent_id, LLJoint* joint_info,
					  const Constraint::ptr_t& constraint)
{
	if (!joint_info)
	{
		llwarns_sparse << "Cannot add with NULL joint info." << llendl;
		return;
	}
	// Note: parent Joints must be added BEFORE their children.
	if (joint_id < 0)
	{
		llwarns << "Failed to add invalid joint_id=" << joint_id << llendl;
		return;
	}
	joint_map_t::iterator itr = mSkeleton.find(joint_id);
	if (itr != mSkeleton.end())
	{
		llwarns << "Failed to add joint_id=" << joint_id << ": already exists"
				<< llendl;
		return;
	}

	Joint::ptr_t parent;
	itr = mSkeleton.find(parent_id);
	if (itr == mSkeleton.end())
	{
		if (parent_id >= mRootID)
		{
			llwarns << "failed to add joint_id=" << joint_id
					<< ": could not find parent_id=" << parent_id << llendl;
			return;
		}
	}
	else
	{
		parent = itr->second;
	}
	Joint::ptr_t joint = std::make_shared<Joint>(joint_info);
	joint->setParent(parent);
	if (parent)
	{
		parent->addChild(joint);
	}
	mSkeleton.emplace(joint_id, joint);

	joint->setConstraint(constraint);
}

void Solver::addWristID(S16 wrist_id)
{
	auto joint_itr = mSkeleton.find(wrist_id);
	if (joint_itr == mSkeleton.end())
	{
		llwarns << "Failed to find wrist_id=" << wrist_id << llendl;
		return;
	}
	mWristJoints.push_back(joint_itr->second);
}

#if LLIK_EXPERIMENTAL
void Solver::adjustTargets(joint_config_map_t& targets)
{
	// When an end-effector has both target_position and target_orientation
	// the IK problem can be reduced by giving the parent a target_position.
	// We scan targets for such conditions and when found: add/update the
	// parent's Target with target_position.
	for (auto& data_pair : targets)
	{
		Joint::Config& target = data_pair.second;
		U8 mask = target.getFlags();
		if (!target.hasWorldPos() || target.hasLocalRot() ||
			!target.hasWorldRot())
		{
			// Target does not match our needs
			continue;
		}

		S16 id = data_pair.first;
		auto joint_itr = mSkeleton.find(id);
		if (joint_itr == mSkeleton.end())
		{
			// Joint does not exist
			continue;
		}

		Joint::ptr_t& joint = joint_itr->second;
		const Joint::ptr_t& parent = joint->getParent();
		if (!parent)
		{
			// No parent
			continue;
		}

		// Compute parent's target pos. Note: we assume joint->mLocalPos ==
		// parent_joint->mBone (e.g. parent's end is same position as joint's
		// tip) which is not true in general, but is true for elbow->wrist.
		LLVector3 parent_target_pos = target.getPos() -
									  joint->getBone() * target.getRot();

		auto parent_target_itr = targets.find(parent->getID());
		if (parent_target_itr != targets.end())
		{
			// parent already has a target --> modify it
			parent_target_itr->second.setPos(parent_target_pos);
		}
		else
		{
			// Parent does not have a target yet, so give it one.
			Joint::Config parent_target;
			parent_target.setPos(parent_target_pos);
			targets.insert({parent->getID(), parent_target});
		}
		// Delegate joint's target but set the joint active. The joint's world
		// transform will be updated during the IK iterations after all chains
		// have been processed.
		target.delegate();
		joint->activate();
	}
}
#endif	// LLIK_EXPERIMENTAL

// The Skeleton relaxes toward the T-pose and the IK solution will tend to put
// the elbows higher than normal for a humanoid character.  The dropElbow()
// method tries to orient the elbows lower to achieve a more natural pose.
void Solver::dropElbow(const Joint::ptr_t& wrist_joint)
{
	const Joint::ptr_t& elbow_joint = wrist_joint->getParent();
	const Joint::ptr_t& shoulder_joint = elbow_joint->getParent();
	if (shoulder_joint->hasPosTarget())
	{
		// Remember: end-of-shoulder is tip-of-elbow. Assume whoever is setting
		// the shoulder's target position knows what they are doing.
		return;
	}

	// Compute some geometry
	LLVector3 shoulder_tip = shoulder_joint->getWorldTipPos();
	LLVector3 elbow_tip = elbow_joint->getWorldTipPos();
	LLVector3 elbow_end = elbow_joint->computeWorldEndPos();
	LLVector3 axis = elbow_end - shoulder_tip;
	axis.normalize();

	// Compute rotation of shoulder to bring upper-arm down
	LLVector3 down = (LLVector3::z_axis % axis) % axis;
	LLVector3 shoulder_bone = elbow_tip - shoulder_tip;
	LLVector3 projection = shoulder_bone - (shoulder_bone * axis) * axis;
	LLQuaternion adjustment;
	adjustment.shortestArc(projection, down);

	// Adjust shoulder to bring upper-arm down
	shoulder_joint->adjustWorldRot(adjustment);

	// elbow_joint's mLocalRot remains unchanged, but we need to update its
	// world-frame transforms
	elbow_joint->updatePosAndRotFromParent();

	if (wrist_joint->isActive())
	{
		// In theory: only wrist_joint's mLocalRot has changed, not its
		// world-frame transform.
		wrist_joint->updateLocalRot();

		// *TODO ?  Enforce twist of wrist's Constraint and back-rotate the
		// elbow-drop to compensate
	}
}

bool Solver::updateJointConfigs(const joint_config_map_t& configs)
{
	bool something_changed = configs.size() != mJointConfigs.size();
	// Check to see if configs changed since last iteration.
	if (!something_changed)
	{
		for (const auto& data_pair : mJointConfigs)
		{
			joint_config_map_t::const_iterator itr =
				configs.find(data_pair.first);
			if (itr == configs.end())
			{
				something_changed = true;
				break;
			}

			// Found old target in current configs.
			const Joint::Config& old_target = data_pair.second;
			const Joint::Config& new_target = itr->second;

			U8 mask = old_target.getFlags();
			if (mask != new_target.getFlags())
			{
				something_changed = true;
				break;
			}
			if ((mask & CONFIG_FLAG_TARGET_POS) &&
				dist_vec(old_target.getTargetPos(),
						 new_target.getTargetPos()) > mAcceptableError)
			{
				something_changed = true;
				break;
			}
			if ((mask & CONFIG_FLAG_TARGET_ROT) &&
				!LLQuaternion::almost_equal(old_target.getTargetRot(),
											new_target.getTargetRot()))
			{
				something_changed = true;
				break;
			}
			if ((mask & CONFIG_FLAG_LOCAL_POS) &&
				dist_vec(old_target.getLocalPos(),
						 new_target.getLocalPos()) > mAcceptableError)
			{
				something_changed = true;
				break;
			}
			if ((mask & CONFIG_FLAG_LOCAL_ROT) &&
				!LLQuaternion::almost_equal(old_target.getLocalRot(),
											new_target.getLocalRot()))
			{
				something_changed = true;
				break;
			}
		}
	}
	if (something_changed)
	{
		mJointConfigs = configs;
	}
	return something_changed;
}

void Solver::rebuildAllChains()
{
	// Before recompute chains: clear active status on old chains
	for (const auto& data_pair : mChainMap)
	{
		const joint_list_t& chain = data_pair.second;
		for (const Joint::ptr_t& joint : chain)
		{
			joint->resetFlags();
		}
	}
	mChainMap.clear();
	mActiveRoots.clear();

	// makeChains
	//
	// Consider the following hypothetical skeleton, where each Joint tip
	// has a numerical ID and each end-effector tip is denoted with
	// bracketed [ID]:
	//					 8			 [11]
	//					/			  /
	//				   7---14--[15]   10
	//				  /			  /
	//				 6---12---13	9
	//				/			  /
	//	  0----1---2----3----4---[5]--16---17--[18]
	//				`
	//				 19
	//				  `
	//				  [20]
	//
	// The target ID list is: [5,11,15,18,20].
	// IK would need to solve all joints except for [8,12,13].
	// In other words: all Joints are "active" except [8,12,13].
	//
	// We divide the Skeleton into "chain segments" that start at a targeted
	// Joint and continue up until: root (0), end-effector ([ID]), or
	// sub-base (Joint with multiple children).
	//
	// Inward passes operate on the Chains in order such that when it is time
	// to update a sub-base all of its active children will have already been
	// updated: it will be able to compute the centroid of its mWorldEndPos.
	//
	// Outward passes also only operate on the Chains.  This simplifies
	// the logic because there will be no need to check for target or sub-base
	// until the end of a Chain is reached.  Any Joint not on a Chain (e.g.
	// non-active) will keep its parent-relative rotation.
	//
	// The initial chain list would be:
	//	 {  5:[5,4,3,2]
	//	   11:[11,10,9,5]
	//	   15:[15,14,7]
	//	   18:[18,17,16,5]
	//	   20:[20,19,2] }
	// Where all chains include their end_point and also sub-base.
	// The remaining active non-targeted sub_base_map would be:
	//	 { 2:[2,1,0]
	//	   7:[7,6]
	//	   6:[6,2] }
	// In this scenario Joints (6) and (7) are "false" sub-bases: they
	// don't have targets and have multiple children but only one of them is "active".
	// We can condense the chains to be:
	//	 {  5:[5,4,3,2]
	//	   11:[11,10,9,5]
	//	   15:[15,14,7,6,2]
	//	   18:[18,17,16,5]
	//	   20:[20,19,2] }
	// and:
	//	 { 2:[2,1,0] }
	//

	std::set<S16> sub_bases;
	// mJointConfigs is sorted by joint_id low-to-high and we rely on this in
	// buildChain().
	for (auto& data_pair : mJointConfigs)
	{
		// Make sure joint_id is valid
		S16 joint_id = data_pair.first;
		joint_map_t::iterator itr = mSkeleton.find(joint_id);
		if (itr == mSkeleton.end())
		{
			continue;
		}
		Joint::ptr_t joint = itr->second;

		// Joint caches a pointer to the Target and the Joint::Config will
		// remain valid for the duration of the IK iterations.
		Joint::Config& config = data_pair.second;
		joint->setConfig(config);

		if (joint->getID() == mRootID)
		{
			// For root world-frame == local-frame
			U8 flags = joint->getConfigFlags();
			if (flags & MASK_ROT)
			{
				LLQuaternion q =
					(flags & CONFIG_FLAG_LOCAL_ROT) ? config.getLocalRot()
													: config.getTargetRot();
				joint->lockLocalRot(q);
				joint->activate();
				mActiveRoots.insert(joint);
			}
			if (flags & MASK_POS)
			{
				LLVector3 p =
					(flags & CONFIG_FLAG_LOCAL_POS) ? config.getLocalPos()
													: config.getTargetPos();
				joint->setLocalPos(p);
				joint->activate();
			}
			if (flags & CONFIG_FLAG_LOCAL_SCALE)
			{
				joint->setLocalScale(config.getLocalScale());
			}
			continue;
		}
		if (config.hasLocalRot())
		{
			joint->lockLocalRot(config.getLocalRot());
		}
#if LLIK_EXPERIMENTAL
		if (config.hasDelegated())
		{
			// Do not build chain for delegated Target
			continue;
		}
#endif
		if (config.hasTargetPos())
		{
			// Add and build chain
			mChainMap[joint_id] = joint_list_t();
			buildChain(joint, mChainMap[joint_id], sub_bases);

			// *HACK or FIX ?  If we have sequential end effectors, we are not
			// guaranteed the expression module has sent us positions that can
			// be solved. We will instead assume that the child's position is
			// higher prioriy than the parent, get direction from child to
			// parent and move the parent's target to the exact bone length.
			// *TODO: will not work correctly for a parent with multiple direct
			// children with effector targets. Because we create the targets
			// form low to high we will know if the parent is an end-effector.
			Joint::ptr_t parent = joint->getParent();
			if (parent->hasPosTarget())
			{
				// Sequential targets detected
				LLVector3 child_target_pos = config.getTargetPos();
				LLVector3 parent_target_pos = parent->getTargetPos();
				LLVector3 direction = parent_target_pos - child_target_pos;
				direction.normalize();
				direction *= joint->getLocalPosLength();
				parent_target_pos = child_target_pos + direction;
				parent->setTargetPos(parent_target_pos);
			}
		}
		else if (config.hasLocalPos())
		{
			joint->setLocalPos(config.getLocalPos());
			joint->activate();
		}
		if (config.hasLocalScale())
		{
			joint->setLocalScale(config.getLocalScale());
			joint->activate();
		}
	}

	// Each sub_base gets its own Chain
	while (sub_bases.size() > 0)
	{
		std::set<S16> new_sub_bases;
		for (S16 joint_id : sub_bases)
		{
			// Add and build chain
			Joint::ptr_t joint = mSkeleton[joint_id];
			mChainMap[joint_id] = joint_list_t();
			buildChain(joint, mChainMap[joint_id], new_sub_bases);
		}
		sub_bases = std::move(new_sub_bases);
	}

	// Eliminate "false" sub-bases and condense the Chains; search for
	// Chain-joins.
	std::vector<U16> joins;
	for (const auto& data_pair : mChainMap)
	{
		const Joint::ptr_t& outer_end = data_pair.second[0];
		if (!outer_end->hasPosTarget() && !isSubBase(outer_end->getID()))
		{
			Joint::ptr_t active_child = outer_end->getSingleActiveChild();
			if (active_child)
			{
				// outer_end does not have a target, is not flagged as subbase,
				// and has only one active_child --> it is a "false" sub-base
				// and we will try to "join" this Chain to another.
				joins.push_back(outer_end->getID());
			}
		}
	}
	// Make the joins
	for (U16 id : joins)
	{
		// Hunt for recipient chain
		for (auto& data_pair : mChainMap)
		{
			auto& recipient = data_pair.second;
			const Joint::ptr_t& inner_end = recipient[recipient.size() - 1];
			if (inner_end->getID() == id)
			{
				// Copy donor to recipient
				const auto& donor = mChainMap[id];
				recipient.insert(recipient.end(), ++(donor.begin()),
								 donor.end());
				// Erase donor
				mChainMap.erase(id);
				break;
			}
		}
	}

	// Cache the set of active branch roots
	for (auto& data_pair : mChainMap)
	{
		auto& chain = data_pair.second;
		size_t last_index = chain.size() - 1;
		Joint::ptr_t chain_base = chain[last_index];
		Joint::ptr_t parent = chain_base->getParent();
		if (!parent || !parent->isActive())
		{
			mActiveRoots.insert(chain_base);
		}
	}

	// Cache the list of all active joints
	mActiveJoints.clear();
	for (auto& data_pair : mSkeleton)
	{
		if (data_pair.second->isActive())
		{
			mActiveJoints.push_back(data_pair.second);
			data_pair.second->flagForHarvest();
		}
	}
}

////////////////////////////////////// Solvers ////////////////////////////////

F32 Solver::solve()
{
	rebuildAllChains();

	// Before each solve: we relax a fraction toward the reset pose. This
	// provides return pressure that removes floating-point drift that would
	// otherwise wander around within the valid zones of the constraints.
	constexpr F32 INITIAL_RELAXATION_FACTOR = 0.25f;
	for (auto& root : mActiveRoots)
	{
		root->relaxRotationsRecursively(INITIAL_RELAXATION_FACTOR);
	}

	constexpr U32 MAX_FABRIK_ITERATIONS = 16;
	constexpr U32 MIN_FABRIK_ITERATIONS = 4;
	F32 max_error = F32_MAX;
	for (U32 loop = 0;
		 loop < MIN_FABRIK_ITERATIONS ||
		 (loop < MAX_FABRIK_ITERATIONS && max_error > mAcceptableError);
		 ++loop)
	{
		max_error = solveOnce();
	}
	mLastError = max_error;

	return mLastError;
}

F32 Solver::solveOnce()
{
	constexpr bool constrain = true;
	constexpr bool drop_elbow = true;
	constexpr bool untwist = true;
#if LLIK_EXPERIMENTAL
	executeCcd(constrain, drop_elbow, untwist);
#endif
	executeFabrik(constrain, drop_elbow, untwist);
	return measureMaxError();
}

void Solver::executeFabrik(bool constrain, bool drop_elbow, bool untwist)
{
	executeFabrikPass();

	// Pull elbows downward toward a more natual pose
	for (const auto& wrist_joint : mWristJoints)
	{
		dropElbow(wrist_joint);
	}

	if (!constrain)
	{
		return;
	}

	// Since our FABRIK implementation does not enforce constraints during the
	// forward/backward passes, we do it here.
	enforceConstraintsOutward();

	if (!untwist)
	{
		return;
	}

	// It is often possible to remove excess twist between the Joints without
	// swinging their bones in the world-frame. We try this now to help reduce
	// the "spin drift" that can occur where Joint orientations pick up
	// systematic and floating-point errors and drift within the twist-limits
	// of their constraints.
	for (const auto& data_pair : mChainMap)
	{
		const joint_list_t& chain = data_pair.second;
		untwistChain(chain);
	}

	executeFabrikPass();
	// Note: we do not bother enforcing constraints after untwisting.
}

LLVector3 Solver::getJointLocalPos(S16 joint_id) const
{
	LLVector3 pos;
	auto itr = mSkeleton.find(joint_id);
	if (itr != mSkeleton.end())
	{
		pos = itr->second->getLocalPos();
	}
	return pos;
}

bool Solver::getJointLocalTransform(S16 joint_id, LLVector3& pos,
									LLQuaternion& rot) const
{
	auto itr = mSkeleton.find(joint_id);
	if (itr == mSkeleton.end())
	{
		return false;
	}
	pos = itr->second->getLocalPos();
	rot = itr->second->getLocalRot();
	return true;
}

LLVector3 Solver::getJointWorldEndPos(S16 joint_id) const
{
	LLVector3 pos;
	joint_map_t::const_iterator itr = mSkeleton.find(joint_id);
	if (itr != mSkeleton.end())
	{
		pos = itr->second->computeWorldEndPos();
	}
	return pos;
}

LLQuaternion Solver::getJointWorldRot(S16 joint_id) const
{
	LLQuaternion rot;
	joint_map_t::const_iterator itr = mSkeleton.find(joint_id);
	if (itr != mSkeleton.end())
	{
		rot = itr->second->getWorldRot();
	}
	return rot;
}

void Solver::resetJointGeometry(S16 joint_id,
								const Constraint::ptr_t& constraint)
{
	joint_map_t::iterator itr = mSkeleton.find(joint_id);
	if (itr == mSkeleton.end())
	{
		llwarns << "Failed update unknown joint_id=" << joint_id << llendl;
		return;
	}
	const Joint::ptr_t& joint = itr->second;
	joint->resetFromInfo();
	joint->setConstraint(constraint);
	// Note: will need to call computeReach() after all Joints geometries are
	// reset.
}

void Solver::buildChain(Joint::ptr_t joint, joint_list_t& chain,
						std::set<S16>& sub_bases)
{
	// Builds a Chain in descending order (inward) from end-effector or sub-
	// base. Stops at next end-effector (has target), sub-base (more than one
	// active child), or root. Side effect: sets each Joint on chain "active".
	chain.push_back(joint);
	joint->activate();
	// Walk up the chain of ancestors and add to chain but stop at: end-effector,
	// sub-base, or root. When a sub-base is encountered push its id onto
	// sub_bases.
	joint = joint->getParent();
	while (joint)
	{
		chain.push_back(joint);
		joint->activate();
		S16 joint_id = joint->getID();
		// Yes, add the joint to the chain before the break checks below
		// because we want to include the final joint (e.g. root, sub-base, or
		// previously targeted joint) at the end of the chain.
		if (isSubRoot(joint_id))
		{
			// AURA hack to deal with lack of constraints in spine
			break;
		}
		if (joint_id == mRootID)
		{
			break;
		}
		if (joint->hasPosTarget())
		{
			// Truncate this chain at targeted ancestor joint
			break;
		}
		if ((mSubBaseIds.empty() && joint->getNumChildren() > 1) ||
			isSubBase(joint_id))
		{
			sub_bases.insert(joint_id);
			break;
		}
		joint = joint->getParent();
	}
}

void Solver::executeFabrikInward(const joint_list_t& chain)
{
	// Chain starts at end-effector or sub-base. Do not forget: chain is
	// organized in descending order: for inward pass we traverse the chain
	// forward.

	// Outer end of chain is special: it either has a target or is a sub-base
	// with active children
	chain[0]->updateEndInward();

	// Traverse Chain forward. Skip first Joint in chain (the "outer end"): we
	// just handled it. Also skip last Joint in chain (the "inner end"): it is
	// either the outer end of another chain (and will be updated then) or it
	// is one of the "active roots" and will be handled after all chains.
	S32 last_index = (S32)(chain.size()) - 1;
	for (S32 i = 1; i < last_index; ++i)
	{
		chain[i]->updateInward(chain[i - 1]);
	}
}

void Solver::executeFabrikOutward(const joint_list_t& chain)
{
	// Chain starts at a end-effector or sub-base. Do not forget: chain is
	// organized in descending order: for outward pass we traverse the chain
	// in reverse.
	S32 last_index = (S32)(chain.size()) - 1;

	// Skip the Joint at last_index: chain's inner-end does not move at this
	// stage. Traverse the middle of chain in reverse
	for (S32 i = last_index - 1; i > 0; --i)
	{
		chain[i]->updateOutward();
	}

	// Outer end of chain is special: it either has a target or is a sub-base
	// with active children
	chain[0]->updateEndOutward();
}

void Solver::shiftChainToBase(const joint_list_t& chain)
{
	size_t last_index = (S32)(chain.size()) - 1;
	const Joint::ptr_t& inner_end_child = chain[last_index - 1];
	LLVector3 offset = inner_end_child->computeWorldTipOffset();
	if (offset.lengthSquared() > mAcceptableError * mAcceptableError)
	{
		for (size_t i = 0; i < last_index; ++i)
		{
			chain[i]->shiftPos(-offset);
		}
	}
}

void Solver::executeFabrikPass()
{
	// FABRIK = Forward And Backward Reching Inverse Kinematics
	// http://andreasaristidou.com/FABRIK.html

	// mChainMap is sorted by outer_end joint_id, low-to-high so for the inward
	// pass we traverse the chains in reverse order.
	for (chain_map_t::const_reverse_iterator itr = mChainMap.rbegin(),
											 rend = mChainMap.rend();
		 itr != rend; ++itr)
	{
		executeFabrikInward(itr->second);
	}

	// executeFabrikInward() does not update child mLocalRots for the inner_end
	// so we must do it manually for each active root
	for (auto& root : mActiveRoots)
	{
		root->updateChildLocalRots();
	}

	// The outward pass must solve the combined set of chains from-low-to-high
	// so we process them in forward order.
	for (const auto& data_pair : mChainMap)
	{
		const joint_list_t& chain = data_pair.second;
		executeFabrikOutward(chain);
	}
}

void Solver::enforceConstraintsOutward()
{
	for (const auto& data_pair : mChainMap)
	{
		const joint_list_t& chain = data_pair.second;

		// Chain starts at a end-effector or sub-base. Do not forget: chain is
		// organized in descending order: for outward pass we traverse the
		// chain in reverse.
		S32 last_index = (S32)(chain.size()) - 1;

		// Skip the Joint at last_index: chain's inner-end does not move at
		// this stage. Traverse the middle of chain in reverse.
		for (S32 i = last_index - 1; i > -1; --i)
		{
			const Joint::ptr_t& joint = chain[i];
			joint->updatePosAndRotFromParent();
			if (joint->enforceConstraint())
			{
				joint->applyLocalRot();
			}
		}
		chain[0]->updateChildLocalRots();
	}
}

#if LLIK_EXPERIMENTAL
void Solver::executeCcd(bool constrain, bool drop_elbow, bool untwist)
{
	// *TODO:
	//	- modify executeCcdPass() to handle enforce_constraints;
	//	- handle drop_elbow before CCD pass;
	//	- handle untwist ?
	executeCcdPass(constrain);
}

// Cyclic Coordinate Descend (CCD) is an alternative IK algorithm.
// http://rodolphe-vaillant.fr/entry/114/cyclic-coordonate-descent-inverse-kynematic-ccd-ik
//
// It converges well however is more susceptible than FABRIK to solution
// instability when Constraints are being enforced. We keep it around just in
// case we want to try it, or for when we figure out how to enforce Constraints
// without making CCD unstable.
void Solver::executeCcdPass(bool constrain)
{
	// mChainMap is sorted by outer_end joint_id, low-to-high and CCD is an
	// inward pass, so we traverse the map in reverse
	for (chain_map_t::const_reverse_iterator itr = mChainMap.rbegin(),
											 rend = mChainMap.rend();
		 itr != rend; ++itr)
	{
		executeCcdInward(itr->second, constrain);
	}

	// executeCcdInward(chain) recomputes world-frame transform of all Joints
	// in chain... except the child of the chain's inner_end. Now that all
	// chains are solved we shift each chain to connect with its sub-base.
	for (auto& data_pair: mChainMap)
	{
		shiftChainToBase(data_pair.second);
	}
}

void Solver::executeCcdInward(const joint_list_t& chain, bool constrain)
{
	// 'chain' starts at a end-effector or sub-base. Do not forget: 'chain' is
	// organized in descending order: for inward pass we traverse the Chain
	// forward.
	const Joint::ptr_t& outer_end = chain[0];

	// outer_end has one or more targets known in both local and world frames.
	// For CCD we'll be swinging each joint of the Chain as we traverse inward
	// in attempts to get the local-frame targets to align with their world-
	//frame counterparts.
	std::vector<LLVector3> local_targets, world_targets;
	outer_end->collectTargetPositions(local_targets, world_targets);

	if (!outer_end->swingTowardTargets(local_targets, world_targets))
	{
		// Targets are close enough
		return;
	}

	// Traverse Chain forward and swing each part. Skip first Joint in 'chain'
	// (the "outer end"): we just handled it. Also skip last Joint in 'chain'
	// (the "inner end"): it is either the outer end of another Chain (and will
	// be updated as part of a subsequent Chain) or it is one of the "active
	// roots" and is not moved.
	S32 last_index = (S32)(chain.size()) - 1;
	S32 last_swung_index = 0;
	for (S32 i = 1; i < last_index; ++i)
	{
		chain[i - 1]->transformTargetsToParentLocal(local_targets);
		if (!chain[i]->swingTowardTargets(local_targets, world_targets))
		{
			break;
		}
		last_swung_index = i;
	}

	// Update the world-frame transforms of swung Joints
	for (S32 i = last_swung_index - 1; i > -1; --i)
	{
		chain[i]->updatePosAndRotFromParent();
	}

	// Finally: make sure to update outer_end's childrens' mLocalRots. Note: we
	// do not bother to enforce constraints in this step.
	outer_end->updateChildLocalRots();
}
#endif	// LLIK_EXPERIMENTAL

void Solver::untwistChain(const joint_list_t& chain)
{
	S32 last_index = (S32)(chain.size()) - 1;
	// Note: we start at last_index-1 becuase Joint::untwist() will affect its
	// parent's twist and we don't want to mess with the inner_end of the chain
	// since it will be handled later in another chain.
	for (S32 i = last_index - 1; i > -1; --i)
	{
		chain[i]->untwist();
	}
	chain[0]->updateChildLocalRots();
}

F32 Solver::measureMaxError()
{
	F32 max_error = 0.f;
	for (auto& data_pair : mJointConfigs)
	{
		S16 joint_id = data_pair.first;
		if (joint_id == mRootID)
		{
			// Skip error measure of root joint: should always be zero.
			continue;
		}
		Joint::Config& target = data_pair.second;
#if LLIK_EXPERIMENTAL
		if (target.hasTargetPos() && !target.hasDelegated())
#else
		if (target.hasTargetPos())
#endif
		{
			LLVector3 end_pos = mSkeleton[joint_id]->computeWorldEndPos();
			F32 dist = dist_vec(end_pos, target.getTargetPos());
			if (dist > max_error)
			{
				max_error = dist;
			}
		}
	}
	return max_error;
}

}	// namespace LLIK

///////////////////////////////////////////////////////////////////////////////
// LLIKConstraintFactory class
///////////////////////////////////////////////////////////////////////////////

void LLIKConstraintFactory::initSingleton()
{
	std::string filename =
		gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER,
									   "avatar_constraint.llsd");
	llifstream file(filename.c_str());
	if (!file.is_open())
	{
		llwarns << "Error opening the IK constraints file: " << filename
				<< llendl;
		return;
	}

	LLSD map;
	if (!LLSDSerialize::deserialize(map, file, LLSDSerialize::SIZE_UNLIMITED))
	{
		llwarns << "Unable to load and parse IK constraints from: " << filename
				<< llendl;
		return;
	}

	for (LLSD::map_const_iterator it = map.beginMap(), end = map.endMap();
		 it != end; ++it)
	{
		const std::string& joint_name = it->first;
		const LLSD& data = it->second;
		LLIK::Constraint::ptr_t ptr = getConstraint(data);
		if (ptr)
		{
			mJointMapping.emplace(joint_name, ptr);
		}
	}
}

LLIK::Constraint::ptr_t
LLIKConstraintFactory::getConstrForJoint(const std::string& joint_name) const
{
	auto it = mJointMapping.find(joint_name);
	return it != mJointMapping.end() ? it->second : LLIK::Constraint::ptr_t();
}

LLIK::Constraint::ptr_t LLIKConstraintFactory::getConstraint(const LLSD& data)
{
	LLIK::Constraint::ptr_t ptr = create(data);
	if (ptr)
	{
		U64 hash = ptr->getHash();
		auto it = mConstraints.find(hash);
		if (it != mConstraints.end())
		{
			ptr = it->second;
		}
		else
		{
			mConstraints[hash] = ptr;
		}
	}
	return ptr;
}

//static
LLIK::Constraint::ptr_t LLIKConstraintFactory::create(const LLSD& data)
{
	std::string type = data["type"].asString();
	LLStringUtil::toUpper(type);

	LLIK::Constraint::ptr_t ptr;
	if (type == SIMPLE_CONE_NAME)
	{
		ptr = std::make_shared<LLIK::SimpleCone>(data);
	}
	else if (type == TWIST_LIMITED_CONE_NAME)
	{
		ptr = std::make_shared<LLIK::TwistLimitedCone>(data);
	}
	else if (type == ELBOW_NAME)
	{
		ptr = std::make_shared<LLIK::ElbowConstraint>(data);
	}
	else if (type == KNEE_NAME)
	{
		ptr = std::make_shared<LLIK::KneeConstraint>(data);
	}
	else if (type == ACUTE_ELLIPSOIDAL_NAME)
	{
		ptr = std::make_shared<LLIK::AcuteEllipsoidalCone>(data);
	}
	else if (type == DOUBLE_LIMITED_HINGE_NAME)
	{
		ptr = std::make_shared<LLIK::DoubleLimitedHinge>(data);
	}
	return ptr;
}
