/**
 * @file llquaternion.h
 * @brief LLQuaternion class header file.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LLQUATERNION_H
#define LLQUATERNION_H

#include <iostream>

#include "llmath.h"
#include "llsd.h"

class LLMatrix3;
class LLMatrix4;
class LLVector3;
class LLVector3d;
class LLVector4;

// IMPORTANT: LLQuaternion code is written assuming *unit* quaternions !
//			  Moreover, it is written assuming that all vectors and matricies
//			  passed as arguments are normalized and unitary respectively.
//			  VERY VERY VERY BAD THINGS will happen if these assumptions fail.

constexpr U32 LENGTHOFQUAT = 4;

class LLQuaternion
{
public:
	LL_INLINE LLQuaternion() noexcept
	{
		mQ[VX] = mQ[VY] = mQ[VZ] = 0.f;
		mQ[VS] = 1.f;
	}

	// Initializes Quaternion to (x, y, z, w).
	// Note: we do not normalize this case as it is used mainly for temporaries
	// during calculations
	LL_INLINE LLQuaternion(F32 x, F32 y, F32 z, F32 w) noexcept
	{
		mQ[VX] = x;
		mQ[VY] = y;
		mQ[VZ] = z;
		mQ[VS] = w;
	}

	// Initializes Quaternion to normalize(x, y, z, w)
	LL_INLINE LLQuaternion(const F32* q) noexcept
	{
		mQ[VX] = q[VX];
		mQ[VY] = q[VY];
		mQ[VZ] = q[VZ];
		mQ[VS] = q[VW];

		normalize();
	}

	// Initializes Quaternion to axis_angle2quat(angle, vec)
	LLQuaternion(F32 angle, const LLVector4& vec) noexcept;
	// Initializes Quaternion to axis_angle2quat(angle, vec)
	LLQuaternion(F32 angle, const LLVector3& vec) noexcept;
	// Initializes Quaternion from Matrix3 = [x_axis ; y_axis ; z_axis]
	LLQuaternion(const LLVector3& x_axis, const LLVector3& y_axis,
				 const LLVector3& z_axis) noexcept;
	explicit LLQuaternion(const LLMatrix4& mat) noexcept;
	explicit LLQuaternion(const LLMatrix3& mat) noexcept;
	explicit LLQuaternion(const LLSD& sd);

	// Allow the use of the default C++11 move constructor and assignation
	LLQuaternion(LLQuaternion&& other) noexcept = default;
	LLQuaternion& operator=(LLQuaternion&& other) noexcept = default;

	LLQuaternion(const LLQuaternion& other) = default;
	LLQuaternion& operator=(const LLQuaternion& other) = default;

	LL_INLINE void setValue(const LLSD& sd)
	{
		mQ[0] = sd[0].asReal();
		mQ[1] = sd[1].asReal();
		mQ[2] = sd[2].asReal();
		mQ[3] = sd[3].asReal();
	}

	LL_INLINE LLSD getValue() const
	{
		LLSD ret;
		ret[0] = mQ[0];
		ret[1] = mQ[1];
		ret[2] = mQ[2];
		ret[3] = mQ[3];
		return ret;
	}

	LL_INLINE bool isIdentity() const
	{
		return  mQ[VX] == 0.f && mQ[VY] == 0.f && mQ[VZ] == 0.f &&
				mQ[VS] == 1.f;
	}

	LL_INLINE bool isNotIdentity() const
	{
		return mQ[VX] != 0.f || mQ[VY] != 0.f || mQ[VZ] != 0.f ||
			   mQ[VS] != 1.f;
	}

	// Checks to see if all values of LLQuaternion are finite
	LL_INLINE bool isFinite() const
	{
		return llfinite(mQ[VX]) && llfinite(mQ[VY]) && llfinite(mQ[VZ]) &&
			   llfinite(mQ[VS]);
	}

	// Changes the vector to reflect quatization
	void quantize16(F32 lower, F32 upper);
	// Changes the vector to reflect quatization
	void quantize8(F32 lower, F32 upper);

	// Loads the quaternion that represents the identity rotation
	LL_INLINE void loadIdentity()
	{
		mQ[VX] = mQ[VY] = mQ[VZ] = 0.f;
		mQ[VW] = 1.f;
	}

	bool isEqualEps(const LLQuaternion& quat, F32 epsilon) const;
	bool isNotEqualEps(const LLQuaternion& quat, F32 epsilon) const;

	// Sets Quaternion to normalize(x, y, z, w)
	const LLQuaternion&	set(F32 x, F32 y, F32 z, F32 w);
	// Copies Quaternion
	const LLQuaternion&	set(const LLQuaternion& quat);
	// Sets Quaternion to normalize(quat[VX], quat[VY], quat[VZ], quat[VW])
	const LLQuaternion&	set(const F32* q);
	const LLQuaternion&	set(const LLMatrix3& mat);
	const LLQuaternion&	set(const LLMatrix4& mat);
	// Sets from azimuth and altitude in radians
	const LLQuaternion& setFromAzimuthAndAltitude(F32 azimuth, F32 altitude);

	// Sets Quaternion to axis_angle2quat(angle, x, y, z)
	const LLQuaternion&	setAngleAxis(F32 angle, F32 x, F32 y, F32 z);
	// Sets Quaternion to axis_angle2quat(angle, vec)
	const LLQuaternion&	setAngleAxis(F32 angle, const LLVector3& vec);
	// Sets Quaternion to axis_angle2quat(angle, vec)
	const LLQuaternion&	setAngleAxis(F32 angle, const LLVector4& vec);
	// Sets Quaternion to euler2quat(pitch, yaw, roll)
	const LLQuaternion&	setEulerAngles(F32 roll, F32 pitch, F32 yaw);

	// Returns the Matrix4 equivalent of Quaternion
	LLMatrix4 getMatrix4() const;
	// Returns the Matrix3 equivalent of Quaternion
	LLMatrix3 getMatrix3() const;
	// Returns rotation in radians about axis x,y,z
	void getAngleAxis(F32* angle, F32* x, F32* y, F32* z) const;
	void getAngleAxis(F32* angle, LLVector3& vec) const;
	void getEulerAngles(F32* roll, F32* pitch, F32* yaw) const;
	void getAzimuthAndAltitude(F32& azimuth, F32& altitude);
	void getAzimuthAndElevation(F32& azimuth, F32& elevation);

	// Normalizes Quaternion and returns magnitude
	F32	normalize();

	// Transpose (AKA conjugate)
	const LLQuaternion&	transpose();

	// Other useful methods

	// Shortest rotation from a to b
	void shortestArc(const LLVector3& a, const LLVector3& b);
	// Constrains rotation to a cone angle specified in radians
	const LLQuaternion& constrain(F32 radians);

	// Standard operators

	friend std::ostream& operator<<(std::ostream& s, const LLQuaternion& a);
	friend LLQuaternion operator+(const LLQuaternion& a,
								  const LLQuaternion& b);
	friend LLQuaternion operator-(const LLQuaternion& a,
								  const LLQuaternion& b);
	friend LLQuaternion operator-(const LLQuaternion& a);			// Negation
	friend LLQuaternion operator*(F32 a, const LLQuaternion& q);	// Scale
	friend LLQuaternion operator*(const LLQuaternion& q, F32 b);	// Scale
	friend LLQuaternion operator*(const LLQuaternion& a,
								  const LLQuaternion& b);
	// Returns a* (transposed/conjugate of a):
	friend LLQuaternion operator~(const LLQuaternion& a);
	bool operator==(const LLQuaternion& b) const;
	bool operator!=(const LLQuaternion& b) const;

	friend const LLQuaternion& operator*=(LLQuaternion& a,
										  const LLQuaternion& b);
	// Rotations of a by rot
	friend LLVector4 operator*(const LLVector4& a, const LLQuaternion& rot);
	friend LLVector3 operator*(const LLVector3& a, const LLQuaternion& rot);
	friend LLVector3d operator*(const LLVector3d& a, const LLQuaternion& rot);

	// Non-standard operators

	friend F32 dot(const LLQuaternion& a, const LLQuaternion& b);
	// Linear interpolation (t = 0 to 1) from p to q
	friend LLQuaternion lerp(F32 t, const LLQuaternion& p,
							 const LLQuaternion& q);
	// Linear interpolation (t = 0 to 1) from identity to q
	friend LLQuaternion lerp(F32 t, const LLQuaternion& q);
	// Spherical linear interpolation from p to q
	friend LLQuaternion slerp(F32 t, const LLQuaternion& p,
							  const LLQuaternion& q);
	// Spherical linear interpolation from identity to q
	friend LLQuaternion slerp(F32 t, const LLQuaternion& q);
	// Normalized linear interpolation from p to q
	friend LLQuaternion nlerp(F32 t, const LLQuaternion& p,
							  const LLQuaternion& q);
	// Normalized linear interpolation from p to q
	friend LLQuaternion nlerp(F32 t, const LLQuaternion& q);

	// These two methods save space by using the fact that our quaternions are
	// normalized:
	LLVector3 packToVector3() const;
	void unpackFromVector3(const LLVector3& vec);

	enum Order {
		XYZ = 0,
		YZX = 1,
		ZXY = 2,
		XZY = 3,
		YXZ = 4,
		ZYX = 5
	};
	// Creates a quaternions from Maya's rotation representation, which is 3
	// rotations (in DEGREES) in the specified order
	friend LLQuaternion mayaQ(F32 x, F32 y, F32 z, Order order);

	// Conversions between Order and strings like "xyz" or "ZYX"
	friend const char* OrderToString(const Order order);
	friend Order StringToOrder(const char* str);

	static bool parseQuat(const std::string& buf, LLQuaternion* value);

	// Note 1: 1.0e-3f radians corresponds to about 0.0573 degrees
	// Note 2: this only works for well-normalized quaternions.
	static bool almost_equal(const LLQuaternion& a, const LLQuaternion& b,
							 F32 tolerance_angle = 1.0e-3f);

public:
	F32 mQ[LENGTHOFQUAT];

	static const LLQuaternion DEFAULT;
};

LL_INLINE bool LLQuaternion::isEqualEps(const LLQuaternion& quat,
										F32 epsilon) const
{
	return fabsf(mQ[VX] - quat.mQ[VX]) <= epsilon &&
		   fabsf(mQ[VY] - quat.mQ[VY]) <= epsilon &&
		   fabsf(mQ[VZ] - quat.mQ[VZ]) <= epsilon &&
		   fabsf(mQ[VS] - quat.mQ[VS]) <= epsilon;
}

LL_INLINE bool LLQuaternion::isNotEqualEps(const LLQuaternion& quat,
										   F32 epsilon) const
{
	return fabsf(mQ[VX] - quat.mQ[VX]) > epsilon ||
		   fabsf(mQ[VY] - quat.mQ[VY]) > epsilon ||
		   fabsf(mQ[VZ] - quat.mQ[VZ]) > epsilon ||
		   fabsf(mQ[VS] - quat.mQ[VS]) > epsilon;
}

LL_INLINE const LLQuaternion& LLQuaternion::set(F32 x, F32 y, F32 z, F32 w)
{
	mQ[VX] = x;
	mQ[VY] = y;
	mQ[VZ] = z;
	mQ[VS] = w;
	normalize();
	return *this;
}

LL_INLINE const LLQuaternion& LLQuaternion::set(const LLQuaternion& quat)
{
	mQ[VX] = quat.mQ[VX];
	mQ[VY] = quat.mQ[VY];
	mQ[VZ] = quat.mQ[VZ];
	mQ[VW] = quat.mQ[VW];
	normalize();
	return *this;
}

LL_INLINE const LLQuaternion& LLQuaternion::set(const F32* q)
{
	mQ[VX] = q[VX];
	mQ[VY] = q[VY];
	mQ[VZ] = q[VZ];
	mQ[VS] = q[VW];
	normalize();
	return *this;
}

LL_INLINE void LLQuaternion::getAngleAxis(F32* angle, F32* x, F32* y, F32* z) const
{
	// length of the vector-component
	F32 v = sqrtf(mQ[VX] * mQ[VX] + mQ[VY] * mQ[VY] + mQ[VZ] * mQ[VZ]);
	if (v > FP_MAG_THRESHOLD)
	{
		F32 oomag = 1.f / v;
		F32 w = mQ[VW];
		if (w < 0.f)
		{
			w = -w;						// make VW positive
			oomag = -oomag;				// invert the axis
		}
		*x = mQ[VX] * oomag;			// normalize the axis
		*y = mQ[VY] * oomag;
		*z = mQ[VZ] * oomag;
		*angle = 2.f * atan2f(v, w);	// get the angle
	}
	else
	{
		*angle = 0.f;					// no rotation
		*x = 0.f;						// around some dummy axis
		*y = 0.f;
		*z = 1.f;
	}
}

// Transpose
LL_INLINE const LLQuaternion& LLQuaternion::transpose()
{
	mQ[VX] *= -1.f;
	mQ[VY] *= -1.f;
	mQ[VZ] *= -1.f;
	return *this;
}

LL_INLINE LLQuaternion operator+(const LLQuaternion& a, const LLQuaternion& b)
{
	return LLQuaternion(a.mQ[VX] + b.mQ[VX], a.mQ[VY] + b.mQ[VY],
						a.mQ[VZ] + b.mQ[VZ], a.mQ[VW] + b.mQ[VW]);
}

LL_INLINE LLQuaternion operator-(const LLQuaternion& a, const LLQuaternion& b)
{
	return LLQuaternion(a.mQ[VX] - b.mQ[VX], a.mQ[VY] - b.mQ[VY],
						a.mQ[VZ] - b.mQ[VZ], a.mQ[VW] - b.mQ[VW]);
}

LL_INLINE LLQuaternion operator-(const LLQuaternion& a)
{
	return LLQuaternion(-a.mQ[VX], -a.mQ[VY], -a.mQ[VZ], -a.mQ[VW]);
}

LL_INLINE LLQuaternion operator*(F32 a, const LLQuaternion& q)
{
	return LLQuaternion(a * q.mQ[VX], a * q.mQ[VY], a * q.mQ[VZ],
						a * q.mQ[VW]);
}

LL_INLINE LLQuaternion operator*(const LLQuaternion& q, F32 a)
{
	return LLQuaternion(a * q.mQ[VX], a * q.mQ[VY], a * q.mQ[VZ],
						a * q.mQ[VW]);
}

LL_INLINE LLQuaternion	operator~(const LLQuaternion& a)
{
	LLQuaternion q(a);
	q.transpose();
	return q;
}

LL_INLINE bool	LLQuaternion::operator==(const LLQuaternion& b) const
{
	return mQ[VX] == b.mQ[VX] && mQ[VY] == b.mQ[VY] && mQ[VZ] == b.mQ[VZ] &&
		   mQ[VS] == b.mQ[VS];
}

LL_INLINE bool	LLQuaternion::operator!=(const LLQuaternion& b) const
{
	return mQ[VX] != b.mQ[VX] || mQ[VY] != b.mQ[VY] || mQ[VZ] != b.mQ[VZ] ||
		   mQ[VS] != b.mQ[VS];
}

LL_INLINE const LLQuaternion& operator*=(LLQuaternion& a, const LLQuaternion& b)
{
#if 1
	LLQuaternion q(b.mQ[3] * a.mQ[0] + b.mQ[0] * a.mQ[3] +
				   b.mQ[1] * a.mQ[2] - b.mQ[2] * a.mQ[1],
				   b.mQ[3] * a.mQ[1] + b.mQ[1] * a.mQ[3] +
				   b.mQ[2] * a.mQ[0] - b.mQ[0] * a.mQ[2],
				   b.mQ[3] * a.mQ[2] + b.mQ[2] * a.mQ[3] +
				   b.mQ[0] * a.mQ[1] - b.mQ[1] * a.mQ[0],
				   b.mQ[3] * a.mQ[3] - b.mQ[0] * a.mQ[0] -
				   b.mQ[1] * a.mQ[1] - b.mQ[2] * a.mQ[2]);
	a = q;
#else
	a = a * b;
#endif
	return a;
}

constexpr F32 ONE_PART_IN_A_MILLION = 0.000001f;

LL_INLINE F32 LLQuaternion::normalize()
{
	F32 mag = sqrtf(mQ[VX] * mQ[VX] + mQ[VY] * mQ[VY] + mQ[VZ] * mQ[VZ] +
					mQ[VS] * mQ[VS]);

	if (mag > FP_MAG_THRESHOLD)
	{
		// Floating point error can prevent some quaternions from achieving
		// exact unity length.  When trying to renormalize such quaternions we
		// can oscillate between multiple quantized states. To prevent such
		// drifts we only renomalize if the length is far enough from unity.
		if (fabsf(1.f - mag) > ONE_PART_IN_A_MILLION)
		{
			F32 oomag = 1.f / mag;
			mQ[VX] *= oomag;
			mQ[VY] *= oomag;
			mQ[VZ] *= oomag;
			mQ[VS] *= oomag;
		}
	}
	else
	{
		// We were given a very bad quaternion so we set it to identity
		mQ[VX] = mQ[VY] = mQ[VZ] = 0.f;
		mQ[VS] = 1.f;
	}

	return mag;
}

LLQuaternion::Order StringToOrder(const char* str);

// Some notes about Quaternions

// What is a Quaternion?
// ---------------------
// A quaternion is a point in 4-dimensional complex space.
// Q = { Qx, Qy, Qz, Qw }
//
//
// Why Quaternions ?
// -----------------
// The set of quaternions that make up the the 4-D unit sphere can be mapped to
// the set of all rotations in 3-D space. Sometimes it is easier to describe or
// manipulate rotations in quaternion space than rotation-matrix space.
//
//
// How Quaternions ?
// -----------------
// In order to take advantage of quaternions we need to know how to go from
// rotation-matricies to quaternions and back. We also have to agree what
// variety of rotations we are generating.
//
// Consider the equation...   v' = v * R
//
// There are two ways to think about rotations of vectors:
// 1) v' is the same vector in a different reference frame
// 2) v' is a new vector in the same reference frame
//
// bookmark -- which way are we using?
//
//
// Quaternion from Angle-Axis:
// ---------------------------
// Suppose we wanted to represent a rotation of some angle (theta) about some
// axis ({Ax, Ay, Az})...
//
// axis of rotation = {Ax, Ay, Az}
// angle_of_rotation = theta
//
// s = sinf(0.5 * theta)
// c = cosf(0.5 * theta)
// Q = { s * Ax, s * Ay, s * Az, c }
//
//
// 3x3 Matrix from Quaternion
// --------------------------
//
//     |                                                                    |
//     | 1 - 2 * (y^2 + z^2)   2 * (x * y + z * w)     2 * (y * w - x * z)  |
//     |                                                                    |
// M = | 2 * (x * y - z * w)   1 - 2 * (x^2 + z^2)     2 * (y * z + x * w)  |
//     |                                                                    |
//     | 2 * (x * z + y * w)   2 * (y * z - x * w)     1 - 2 * (x^2 + y^2)  |
//     |                                                                    |

#endif
