/**
 * @file llquaternion.cpp
 * @brief LLQuaternion class implementation.
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

#include "linden_common.h"

#include "llmath.h"			// For F_PI and GIMBAL_THRESHOLD

#include "llquaternion.h"

#include "llquantize.h"
#include "llmatrix3.h"
#include "llmatrix4.h"
#include "llvector3d.h"
#include "llvector3.h"
#include "llvector4.h"

// WARNING: Do not use this for global const definitions !  Using this at the
// top of a *.cpp file might not give you what you think.
const LLQuaternion LLQuaternion::DEFAULT;

// Constructors

LLQuaternion::LLQuaternion(const LLMatrix4& mat) noexcept
{
	*this = mat.quaternion();
	normalize();
}

LLQuaternion::LLQuaternion(const LLMatrix3& mat) noexcept
{
	*this = mat.quaternion();
	normalize();
}

LLQuaternion::LLQuaternion(F32 angle, const LLVector4& vec) noexcept
{
	F32 mag = sqrtf(vec.mV[VX] * vec.mV[VX] +
					vec.mV[VY] * vec.mV[VY] +
					vec.mV[VZ] * vec.mV[VZ]);
	if (mag > FP_MAG_THRESHOLD)
	{
		angle *= 0.5;
		F32 c = cosf(angle);
		F32 s = sinf(angle) / mag;
		mQ[VX] = vec.mV[VX] * s;
		mQ[VY] = vec.mV[VY] * s;
		mQ[VZ] = vec.mV[VZ] * s;
		mQ[VW] = c;
	}
	else
	{
		loadIdentity();
	}
}

LLQuaternion::LLQuaternion(F32 angle, const LLVector3& vec) noexcept
{
	F32 mag = sqrtf(vec.mV[VX] * vec.mV[VX] +
					vec.mV[VY] * vec.mV[VY] +
					vec.mV[VZ] * vec.mV[VZ]);
	if (mag > FP_MAG_THRESHOLD)
	{
		angle *= 0.5;
		F32 c = cosf(angle);
		F32 s = sinf(angle) / mag;
		mQ[VX] = vec.mV[VX] * s;
		mQ[VY] = vec.mV[VY] * s;
		mQ[VZ] = vec.mV[VZ] * s;
		mQ[VW] = c;
	}
	else
	{
		loadIdentity();
	}
}

LLQuaternion::LLQuaternion(const LLVector3& x_axis, const LLVector3& y_axis,
						   const LLVector3& z_axis) noexcept
{
	LLMatrix3 mat;
	mat.setRows(x_axis, y_axis, z_axis);
	*this = mat.quaternion();
	normalize();
}

LLQuaternion::LLQuaternion(const LLSD& sd)
{
	setValue(sd);
}

// Quatizations
void LLQuaternion::quantize16(F32 lower, F32 upper)
{
	F32 x = mQ[VX];
	F32 y = mQ[VY];
	F32 z = mQ[VZ];
	F32 s = mQ[VS];

	x = U16_to_F32(F32_to_U16_ROUND(x, lower, upper), lower, upper);
	y = U16_to_F32(F32_to_U16_ROUND(y, lower, upper), lower, upper);
	z = U16_to_F32(F32_to_U16_ROUND(z, lower, upper), lower, upper);
	s = U16_to_F32(F32_to_U16_ROUND(s, lower, upper), lower, upper);

	mQ[VX] = x;
	mQ[VY] = y;
	mQ[VZ] = z;
	mQ[VS] = s;

	normalize();
}

void LLQuaternion::quantize8(F32 lower, F32 upper)
{
	mQ[VX] = U8_to_F32(F32_to_U8_ROUND(mQ[VX], lower, upper), lower, upper);
	mQ[VY] = U8_to_F32(F32_to_U8_ROUND(mQ[VY], lower, upper), lower, upper);
	mQ[VZ] = U8_to_F32(F32_to_U8_ROUND(mQ[VZ], lower, upper), lower, upper);
	mQ[VS] = U8_to_F32(F32_to_U8_ROUND(mQ[VS], lower, upper), lower, upper);

	normalize();
}

// LLVector3 Magnitude and Normalization Functions

// Set LLQuaternion routines

const LLQuaternion& LLQuaternion::setAngleAxis(F32 angle, F32 x, F32 y, F32 z)
{
	F32 mag = sqrtf(x * x + y * y + z * z);
	if (mag > FP_MAG_THRESHOLD)
	{
		angle *= 0.5;
		F32 c = cosf(angle);
		F32 s = sinf(angle) / mag;
		mQ[VX] = x * s;
		mQ[VY] = y * s;
		mQ[VZ] = z * s;
		mQ[VW] = c;
	}
	else
	{
		loadIdentity();
	}
	return *this;
}

const LLQuaternion& LLQuaternion::setAngleAxis(F32 angle, const LLVector3& vec)
{
	F32 mag = sqrtf(vec.mV[VX] * vec.mV[VX] +
					vec.mV[VY] * vec.mV[VY] +
					vec.mV[VZ] * vec.mV[VZ]);
	if (mag > FP_MAG_THRESHOLD)
	{
		angle *= 0.5;
		F32 c = cosf(angle);
		F32 s = sinf(angle) / mag;
		mQ[VX] = vec.mV[VX] * s;
		mQ[VY] = vec.mV[VY] * s;
		mQ[VZ] = vec.mV[VZ] * s;
		mQ[VW] = c;
	}
	else
	{
		loadIdentity();
	}
	return *this;
}

const LLQuaternion& LLQuaternion::setAngleAxis(F32 angle, const LLVector4& vec)
{
	F32 mag = sqrtf(vec.mV[VX] * vec.mV[VX] +
					vec.mV[VY] * vec.mV[VY] +
					vec.mV[VZ] * vec.mV[VZ]);
	if (mag > FP_MAG_THRESHOLD)
	{
		angle *= 0.5;
		F32 c = cosf(angle);
		F32 s = sinf(angle) / mag;
		mQ[VX] = vec.mV[VX] * s;
		mQ[VY] = vec.mV[VY] * s;
		mQ[VZ] = vec.mV[VZ] * s;
		mQ[VW] = c;
	}
	else
	{
		loadIdentity();
	}
	return *this;
}

const LLQuaternion& LLQuaternion::setEulerAngles(F32 roll, F32 pitch, F32 yaw)
{
	LLMatrix3 rot_mat(roll, pitch, yaw);
	rot_mat.orthogonalize();
	*this = rot_mat.quaternion();

	normalize();
	return *this;
}

const LLQuaternion& LLQuaternion::set(const LLMatrix3& mat)
{
	*this = mat.quaternion();
	normalize();
	return *this;
}

const LLQuaternion& LLQuaternion::set(const LLMatrix4& mat)
{
	*this = mat.quaternion();
	normalize();
	return *this;
}

// SJB: This code is correct for a logically stored (non-transposed) matrix;
// Our matrices are stored transposed, OpenGL style, so this generates the
// INVERSE matrix, or the CORRECT matrix form an INVERSE quaternion.
// Because we use similar logic in LLMatrix3::quaternion(), we are internally
// consistant so everything works OK :)
LLMatrix3 LLQuaternion::getMatrix3() const
{
	F32 xx = mQ[VX] * mQ[VX];
	F32 xy = mQ[VX] * mQ[VY];
	F32 xz = mQ[VX] * mQ[VZ];
	F32 xw = mQ[VX] * mQ[VW];

	F32 yy = mQ[VY] * mQ[VY];
	F32 yz = mQ[VY] * mQ[VZ];
	F32 yw = mQ[VY] * mQ[VW];

	F32 zz = mQ[VZ] * mQ[VZ];
	F32 zw = mQ[VZ] * mQ[VW];

	LLMatrix3 mat;
	mat.mMatrix[0][0] = 1.f - 2.f * (yy + zz);
	mat.mMatrix[0][1] = 2.f * (xy + zw);
	mat.mMatrix[0][2] = 2.f * (xz - yw);

	mat.mMatrix[1][0] = 2.f * (xy - zw);
	mat.mMatrix[1][1] = 1.f - 2.f * (xx + zz);
	mat.mMatrix[1][2] = 2.f * (yz + xw);

	mat.mMatrix[2][0] = 2.f * (xz + yw);
	mat.mMatrix[2][1] = 2.f * (yz - xw);
	mat.mMatrix[2][2] = 1.f - 2.f * (xx + yy);

	return mat;
}

LLMatrix4 LLQuaternion::getMatrix4() const
{
	F32 xx = mQ[VX] * mQ[VX];
	F32 xy = mQ[VX] * mQ[VY];
	F32 xz = mQ[VX] * mQ[VZ];
	F32 xw = mQ[VX] * mQ[VW];

	F32 yy = mQ[VY] * mQ[VY];
	F32 yz = mQ[VY] * mQ[VZ];
	F32 yw = mQ[VY] * mQ[VW];

	F32 zz = mQ[VZ] * mQ[VZ];
	F32 zw = mQ[VZ] * mQ[VW];

	LLMatrix4 mat;
	mat.mMatrix[0][0] = 1.f - 2.f * (yy + zz);
	mat.mMatrix[0][1] = 2.f * (xy + zw);
	mat.mMatrix[0][2] = 2.f * (xz - yw);

	mat.mMatrix[1][0] = 2.f * (xy - zw);
	mat.mMatrix[1][1] = 1.f - 2.f * (xx + zz);
	mat.mMatrix[1][2] = 2.f * (yz + xw);

	mat.mMatrix[2][0] = 2.f * (xz + yw);
	mat.mMatrix[2][1] = 2.f * (yz - xw);
	mat.mMatrix[2][2] = 1.f - 2.f * (xx + yy);

	// *TODO: should we set the translation portion to zero ?

	return mat;
}

// Other useful methods

// calculate the shortest rotation from a to b
void LLQuaternion::shortestArc(const LLVector3& a, const LLVector3& b)
{
	F32 ab = a * b;			// dotproduct
	LLVector3 c = a % b;	// crossproduct
	F32 cc = c * c;			// squared length of the crossproduct
	if (ab * ab + cc)		// test if the arguments have sufficient magnitude
	{
		// Test if the arguments are (anti)parallel
		if (cc > 0.f)
		{
			// Note: do not try to optimize this line
			F32 s = sqrtf(ab * ab + cc) + ab;
			// The inverted magnitude of the quaternion
			F32 m = 1.f / sqrtf(cc + s * s);
			mQ[VX] = c.mV[VX] * m;
			mQ[VY] = c.mV[VY] * m;
			mQ[VZ] = c.mV[VZ] * m;
			mQ[VW] = s * m;
			return;
		}

		// Test if the angle is bigger than PI/2 (anti parallel)
		if (ab < 0.f)
		{
			// The arguments are anti-parallel, we have to choose an axis
			c = a - b;
			// The length projected on the XY-plane
			F32 m = sqrtf(c.mV[VX] * c.mV[VX] +  c.mV[VY] * c.mV[VY]);
			if (m > FP_MAG_THRESHOLD)
			{
				// Return the quaternion with the axis in the XY-plane
				mQ[VX] = -c.mV[VY] / m;
				mQ[VY] =  c.mV[VX] / m;
				mQ[VZ] = mQ[VW] = 0.f;
				return;
			}
			else		// the vectors are parallel to the Z-axis
			{
				mQ[VX] = 1.f;	// rotate around the X-axis
				mQ[VY] = mQ[VZ] = mQ[VW] = 0.f;
				return;
			}
		}
	}
	loadIdentity();
}

// constrains rotation to a cone angle specified in radians
const LLQuaternion& LLQuaternion::constrain(F32 radians)
{
	const F32 cos_angle_lim = cosf(radians / 2);	// mQ[VW] limit
	const F32 sin_angle_lim = sinf(radians / 2);	// rotation axis length	limit

	if (mQ[VW] < 0.f)
	{
		mQ[VX] *= -1.f;
		mQ[VY] *= -1.f;
		mQ[VZ] *= -1.f;
		mQ[VW] *= -1.f;
	}

	// if rotation angle is greater than limit (cos is less than limit)
	if (mQ[VW] < cos_angle_lim)
	{
		mQ[VW] = cos_angle_lim;
		F32 axis_len = sqrtf(mQ[VX] * mQ[VX] + mQ[VY] * mQ[VY] +
							 mQ[VZ] * mQ[VZ]); // sinf(theta/2)
		F32 axis_mult_fact = sin_angle_lim / axis_len;
		mQ[VX] *= axis_mult_fact;
		mQ[VY] *= axis_mult_fact;
		mQ[VZ] *= axis_mult_fact;
	}

	return *this;
}

// Operators

std::ostream& operator<<(std::ostream& s, const LLQuaternion& a)
{
	s << "{ "
	  << a.mQ[VX] << ", " << a.mQ[VY] << ", " << a.mQ[VZ] << ", " << a.mQ[VW]
	  << " }";
	return s;
}

// Does NOT renormalize the result
LLQuaternion operator*(const LLQuaternion& a, const LLQuaternion& b)
{
	LLQuaternion q(b.mQ[3] * a.mQ[0] + b.mQ[0] * a.mQ[3] +
				   b.mQ[1] * a.mQ[2] - b.mQ[2] * a.mQ[1],
				   b.mQ[3] * a.mQ[1] + b.mQ[1] * a.mQ[3] +
				   b.mQ[2] * a.mQ[0] - b.mQ[0] * a.mQ[2],
				   b.mQ[3] * a.mQ[2] + b.mQ[2] * a.mQ[3] +
				   b.mQ[0] * a.mQ[1] - b.mQ[1] * a.mQ[0],
				   b.mQ[3] * a.mQ[3] - b.mQ[0] * a.mQ[0] -
				   b.mQ[1] * a.mQ[1] - b.mQ[2] * a.mQ[2]);
	return q;
}

#if 0
LLMatrix4 operator*(const LLMatrix4& m, const LLQuaternion& q)
{
	LLMatrix4 qmat(q);
	return (m*qmat);
}
#endif

LLVector4 operator*(const LLVector4& a, const LLQuaternion& rot)
{
	F32 rw = -rot.mQ[VX] * a.mV[VX] - rot.mQ[VY] * a.mV[VY] -
			 rot.mQ[VZ] * a.mV[VZ];
	F32 rx = rot.mQ[VW] * a.mV[VX] + rot.mQ[VY] * a.mV[VZ] -
			 rot.mQ[VZ] * a.mV[VY];
	F32 ry = rot.mQ[VW] * a.mV[VY] + rot.mQ[VZ] * a.mV[VX] -
			 rot.mQ[VX] * a.mV[VZ];
	F32 rz = rot.mQ[VW] * a.mV[VZ] + rot.mQ[VX] * a.mV[VY] -
			 rot.mQ[VY] * a.mV[VX];

	F32 nx = -rw * rot.mQ[VX] +  rx * rot.mQ[VW] - ry * rot.mQ[VZ] +
			 rz * rot.mQ[VY];
	F32 ny = -rw * rot.mQ[VY] +  ry * rot.mQ[VW] - rz * rot.mQ[VX] +
			 rx * rot.mQ[VZ];
	F32 nz = -rw * rot.mQ[VZ] +  rz * rot.mQ[VW] - rx * rot.mQ[VY] +
			 ry * rot.mQ[VX];

	return LLVector4(nx, ny, nz, a.mV[VW]);
}

LLVector3 operator*(const LLVector3& a, const LLQuaternion& rot)
{
	F32 rw = -rot.mQ[VX] * a.mV[VX] - rot.mQ[VY] * a.mV[VY] -
			 rot.mQ[VZ] * a.mV[VZ];
	F32 rx = rot.mQ[VW] * a.mV[VX] + rot.mQ[VY] * a.mV[VZ] -
			 rot.mQ[VZ] * a.mV[VY];
	F32 ry = rot.mQ[VW] * a.mV[VY] + rot.mQ[VZ] * a.mV[VX] -
			 rot.mQ[VX] * a.mV[VZ];
	F32 rz = rot.mQ[VW] * a.mV[VZ] + rot.mQ[VX] * a.mV[VY] -
			 rot.mQ[VY] * a.mV[VX];

	F32 nx = -rw * rot.mQ[VX] +  rx * rot.mQ[VW] - ry * rot.mQ[VZ] +
			 rz * rot.mQ[VY];
	F32 ny = -rw * rot.mQ[VY] +  ry * rot.mQ[VW] - rz * rot.mQ[VX] +
			 rx * rot.mQ[VZ];
	F32 nz = -rw * rot.mQ[VZ] +  rz * rot.mQ[VW] - rx * rot.mQ[VY] +
			 ry * rot.mQ[VX];

	return LLVector3(nx, ny, nz);
}

LLVector3d operator*(const LLVector3d& a, const LLQuaternion& rot)
{
	F64 rw = -rot.mQ[VX] * a.mdV[VX] - rot.mQ[VY] * a.mdV[VY] -
			 rot.mQ[VZ] * a.mdV[VZ];
	F64 rx = rot.mQ[VW] * a.mdV[VX] + rot.mQ[VY] * a.mdV[VZ] -
			 rot.mQ[VZ] * a.mdV[VY];
	F64 ry = rot.mQ[VW] * a.mdV[VY] + rot.mQ[VZ] * a.mdV[VX] -
			 rot.mQ[VX] * a.mdV[VZ];
	F64 rz = rot.mQ[VW] * a.mdV[VZ] + rot.mQ[VX] * a.mdV[VY] -
			 rot.mQ[VY] * a.mdV[VX];

	F64 nx = -rw * rot.mQ[VX] +  rx * rot.mQ[VW] - ry * rot.mQ[VZ] +
			 rz * rot.mQ[VY];
	F64 ny = -rw * rot.mQ[VY] +  ry * rot.mQ[VW] - rz * rot.mQ[VX] +
			 rx * rot.mQ[VZ];
	F64 nz = -rw * rot.mQ[VZ] +  rz * rot.mQ[VW] - rx * rot.mQ[VY] +
			 ry * rot.mQ[VX];

	return LLVector3d(nx, ny, nz);
}

F32 dot(const LLQuaternion& a, const LLQuaternion& b)
{
	return a.mQ[VX] * b.mQ[VX] + a.mQ[VY] * b.mQ[VY] + a.mQ[VZ] * b.mQ[VZ] +
		   a.mQ[VW] * b.mQ[VW];
}

#if 0	// DEMO HACK: This lerp is probably incorrect now due intermediate
		// normalization it should look more like the lerp below.
// linear interpolation
LLQuaternion lerp(F32 t, const LLQuaternion& p, const LLQuaternion& q)
{
	LLQuaternion r;
	r = t * (q - p) + p;
	r.normalize();
	return r;
}
#endif

// lerp from identity to q
LLQuaternion lerp(F32 t, const LLQuaternion& q)
{
	LLQuaternion r;
	r.mQ[VX] = t * q.mQ[VX];
	r.mQ[VY] = t * q.mQ[VY];
	r.mQ[VZ] = t * q.mQ[VZ];
	r.mQ[VW] = t * (q.mQ[VZ] - 1.f) + 1.f;
	r.normalize();
	return r;
}

LLQuaternion lerp(F32 t, const LLQuaternion& p, const LLQuaternion& q)
{
	F32 inv_t = 1.f - t;

	LLQuaternion r;
	r.mQ[VX] = t * q.mQ[VX] + inv_t * p.mQ[VX];
	r.mQ[VY] = t * q.mQ[VY] + inv_t * p.mQ[VY];
	r.mQ[VZ] = t * q.mQ[VZ] + inv_t * p.mQ[VZ];
	r.mQ[VW] = t * q.mQ[VW] + inv_t * p.mQ[VW];
	r.normalize();

	return r;
}

// spherical linear interpolation
LLQuaternion slerp(F32 u, const LLQuaternion& a, const LLQuaternion& b)
{
	// cosine theta = dot product of a and b
	F32 cos_t = a.mQ[0] * b.mQ[0] + a.mQ[1] * b.mQ[1] +
				a.mQ[2] * b.mQ[2] + a.mQ[3] * b.mQ[3];

	// if b is on opposite hemisphere from a, use -a instead
	bool bflip = false;
 	if (cos_t < 0.0f)
	{
		cos_t = -cos_t;
		bflip = true;
	}

	// If B is (within precision limits) the same as A, just linear interpolate
	// between A and B.
	F32 alpha;	// interpolant
	F32 beta;	// 1 - interpolant
	if (1.f - cos_t < 0.00001f)
	{
		beta = 1.f - u;
		alpha = u;
 	}
	else
	{
 		F32 theta = acosf(cos_t);
 		F32 sin_t = sinf(theta);
 		beta = sinf(theta - u * theta) / sin_t;
 		alpha = sinf(u * theta) / sin_t;
 	}

	if (bflip)
	{
		beta = -beta;
	}

	// interpolate
	LLQuaternion ret;
	ret.mQ[0] = beta * a.mQ[0] + alpha * b.mQ[0];
 	ret.mQ[1] = beta * a.mQ[1] + alpha * b.mQ[1];
 	ret.mQ[2] = beta * a.mQ[2] + alpha * b.mQ[2];
 	ret.mQ[3] = beta * a.mQ[3] + alpha * b.mQ[3];

	return ret;
}

// lerp whenever possible
LLQuaternion nlerp(F32 t, const LLQuaternion& a, const LLQuaternion& b)
{
	if (dot(a, b) < 0.f)
	{
		return slerp(t, a, b);
	}
	else
	{
		return lerp(t, a, b);
	}
}

LLQuaternion nlerp(F32 t, const LLQuaternion& q)
{
	if (q.mQ[VW] < 0.f)
	{
		return slerp(t, q);
	}
	else
	{
		return lerp(t, q);
	}
}

// slerp from identity quaternion to another quaternion
LLQuaternion slerp(F32 t, const LLQuaternion& q)
{
	F32 c = q.mQ[VW];
	if (t == 1.f || c == 1.f)
	{
		// the trivial cases
		return q;
	}

	LLQuaternion r;
	F32 angle, stq, stp;
	F32 s = sqrtf(1.f - c * c);

	if (c < 0.f)
	{
		// when c < 0.0 then theta > PI/2, since quat and -quat are the same
		// rotation we invert one of p or q to reduce unecessary spins. An
		// equivalent way to do it is to convert acosf(c) as if it had been
		// negative, and to negate stp
		F32 angle = acosf(-c);
		stp = -sinf(angle * (1.f - t));
		stq = sinf(angle * t);
	}
	else
	{
		angle = acosf(c);
		stp = sinf(angle * (1.f - t));
		stq = sinf(angle * t);
	}

	r.mQ[VX] = (q.mQ[VX] * stq) / s;
	r.mQ[VY] = (q.mQ[VY] * stq) / s;
	r.mQ[VZ] = (q.mQ[VZ] * stq) / s;
	r.mQ[VW] = (stp + q.mQ[VW] * stq) / s;

	return r;
}

LLQuaternion mayaQ(F32 xRot, F32 yRot, F32 zRot, LLQuaternion::Order order)
{
	LLQuaternion xQ(xRot * DEG_TO_RAD, LLVector3(1.f, 0.f, 0.f));
	LLQuaternion yQ(yRot * DEG_TO_RAD, LLVector3(0.f, 1.f, 0.f));
	LLQuaternion zQ(zRot * DEG_TO_RAD, LLVector3(0.f, 0.f, 1.f));
	LLQuaternion ret;
	switch (order)
	{
		case LLQuaternion::XYZ:
			ret = xQ * yQ * zQ;
			break;

		case LLQuaternion::YZX:
			ret = yQ * zQ * xQ;
			break;

		case LLQuaternion::ZXY:
			ret = zQ * xQ * yQ;
			break;

		case LLQuaternion::XZY:
			ret = xQ * zQ * yQ;
			break;

		case LLQuaternion::YXZ:
			ret = yQ * xQ * zQ;
			break;

		case LLQuaternion::ZYX:
			ret = zQ * yQ * xQ;
	}
	return ret;
}

const char* OrderToString(const LLQuaternion::Order order)
{
	switch (order)
	{
		default:
		case LLQuaternion::XYZ:
			return "XYZ";

		case LLQuaternion::YZX:
			return "YZX";

		case LLQuaternion::ZXY:
			return "ZXY";

		case LLQuaternion::XZY:
			return "XZY";

		case LLQuaternion::YXZ:
			return "YXZ";

		case LLQuaternion::ZYX:
			return "ZYX";
	}
}

LLQuaternion::Order StringToOrder(const char* str)
{
	if (strncmp(str, "XYZ", 3) == 0 || strncmp(str, "xyz", 3) == 0)
		return LLQuaternion::XYZ;

	if (strncmp(str, "YZX", 3) == 0 || strncmp(str, "yzx", 3) == 0)
		return LLQuaternion::YZX;

	if (strncmp(str, "ZXY", 3) == 0 || strncmp(str, "zxy", 3) == 0)
		return LLQuaternion::ZXY;

	if (strncmp(str, "XZY", 3) == 0 || strncmp(str, "xzy", 3) == 0)
		return LLQuaternion::XZY;

	if (strncmp(str, "YXZ", 3) == 0 || strncmp(str, "yxz", 3) == 0)
		return LLQuaternion::YXZ;

	if (strncmp(str, "ZYX", 3) == 0 || strncmp(str, "zyx", 3) == 0)
		return LLQuaternion::ZYX;

	return LLQuaternion::XYZ;
}

void LLQuaternion::getAngleAxis(F32* angle, LLVector3& vec) const
{
	// length of the vector-component
	F32 v = sqrtf(mQ[VX] * mQ[VX] + mQ[VY] * mQ[VY] + mQ[VZ] * mQ[VZ]);
	if (v > FP_MAG_THRESHOLD)
	{
		F32 oomag = 1.0f / v;
		F32 w = mQ[VW];
		if (mQ[VW] < 0.0f)
		{
			w = -w;						// make VW positive
			oomag = -oomag;				// invert the axis
		}
		vec.mV[VX] = mQ[VX] * oomag;	// normalize the axis
		vec.mV[VY] = mQ[VY] * oomag;
		vec.mV[VZ] = mQ[VZ] * oomag;
		*angle = 2.0f * atan2f(v, w);	// get the angle
	}
	else
	{
		*angle = 0.0f;					// no rotation
		vec.mV[VX] = 0.0f;				// around some dummy axis
		vec.mV[VY] = 0.0f;
		vec.mV[VZ] = 1.0f;
	}
}

const LLQuaternion& LLQuaternion::setFromAzimuthAndAltitude(F32 azimuth,
															F32 altitude)
{
	// Euler angle inputs are complements of azimuth/altitude which are
	// measured from zenith
	F32 pitch = llclamp(F_PI_BY_TWO - altitude, 0.f, F_PI_BY_TWO);
	F32 yaw = llclamp(F_PI_BY_TWO - azimuth, 0.f, F_PI_BY_TWO);
	setEulerAngles(0.f, pitch, yaw);
	return *this;
}

void LLQuaternion::getAzimuthAndAltitude(F32& azimuth, F32& altitude)
{
	F32 roll, pitch, yaw;
	getEulerAngles(&roll, &pitch, &yaw);
	// Make these measured from zenith
	altitude = llclamp(F_PI_BY_TWO - pitch, 0.f, F_PI_BY_TWO);
	azimuth = llclamp(F_PI_BY_TWO - yaw, 0.f, F_PI_BY_TWO);
}

void LLQuaternion::getAzimuthAndElevation(F32& azimuth, F32& elevation)
{
	const LLVector3 vector_zero(1.f, 0.f, 0.f);
	LLVector3 point = vector_zero * (*this);
	if (!is_approx_zero(point.mV[VX]) || !is_approx_zero(point.mV[VY]))
	{
		azimuth = atan2f(point.mV[VX], point.mV[VY]) - F_PI_BY_TWO;
		if (azimuth < 0.f)
		{
			azimuth += F_TWO_PI;
		}
	}
	else
	{
		azimuth = F_TWO_PI - F_PI_BY_TWO;
	}

	// While vector is normalized, F32 is not sufficiently precise and we can
	// get values like 1.0000012 which will result in -PI/2 angle instead of
	// PI/2...
	elevation = asinf(llclamp(point.mV[VZ], -1.f, 1.f));
}

// Quaternion does not need to be normalized
void LLQuaternion::getEulerAngles(F32* roll, F32* pitch, F32* yaw) const
{
	F32 sx = 2 * (mQ[VX] * mQ[VW] - mQ[VY] * mQ[VZ]);	// sine of the roll
	F32 sy = 2 * (mQ[VY] * mQ[VW] + mQ[VX] * mQ[VZ]);	// sine of the pitch
	F32 ys = mQ[VW] * mQ[VW] - mQ[VY] * mQ[VY];			// intermediate cosine 1
	F32 xz = mQ[VX] * mQ[VX] - mQ[VZ] * mQ[VZ];			// intermediate cosine 2
	F32 cx = ys - xz;									// cosine of the roll
	F32 cy = sqrtf(sx * sx + cx * cx);					// cosine of the pitch
	if (cy > GIMBAL_THRESHOLD)							// no gimbal lock
	{
		*roll  = atan2f(sx, cx);
		*pitch = atan2f(sy, cy);
		*yaw   = atan2f(2 * (mQ[VZ] * mQ[VW] - mQ[VX] * mQ[VY]), ys + xz);
	}
	else												// gimbal lock
	{
		if (sy > 0)
		{
			*pitch = F_PI_BY_TWO;
			*yaw = 2 * atan2f(mQ[VZ] + mQ[VX], mQ[VW] + mQ[VY]);
		}
		else
		{
			*pitch = -F_PI_BY_TWO;
			*yaw = 2 * atan2f(mQ[VZ] - mQ[VX], mQ[VW] - mQ[VY]);
		}
		*roll = 0;
	}
}

// Saves space by using the fact that our quaternions are normalized
LLVector3 LLQuaternion::packToVector3() const
{
	F32 x = mQ[VX];
	F32 y = mQ[VY];
	F32 z = mQ[VZ];
	F32 w = mQ[VW];
	F32 mag = sqrtf(x * x + y * y + z * z + w * w);
	if (mag > FP_MAG_THRESHOLD)
	{
		x /= mag;
		y /= mag;
		z /= mag;
		// no need to normalize w since it is not used
	}
	if (mQ[VW] >= 0)
	{
		return LLVector3(x, y , z);
	}
	else
	{
		return LLVector3(-x, -y, -z);
	}
}

// Saves space by using the fact that our quaternions are normalized
void LLQuaternion::unpackFromVector3(const LLVector3& vec)
{
	mQ[VX] = vec.mV[VX];
	mQ[VY] = vec.mV[VY];
	mQ[VZ] = vec.mV[VZ];
	F32 t = 1.f - vec.lengthSquared();
	if (t > 0)
	{
		mQ[VW] = sqrtf(t);
	}
	else
	{
		// Need this to avoid trying to find the square root of a negative
		// number due to floating point error.
		mQ[VW] = 0;
	}
}

bool LLQuaternion::parseQuat(const std::string& buf, LLQuaternion* value)
{
	if (buf.empty() || value == NULL)
	{
		return false;
	}

	LLQuaternion quat;
	S32 count = sscanf(buf.c_str(), "%f %f %f %f", quat.mQ + 0, quat.mQ + 1,
					   quat.mQ + 2, quat.mQ + 3);
	if (4 == count)
	{
		value->set(quat);
		return true;
	}

	return false;
}

//static
bool LLQuaternion::almost_equal(const LLQuaternion& a, const LLQuaternion& b,
								F32 tol_angle)
{
	// Use the small angle approximation of cos():
	// cos(angle) ~= 1.0 - 0.5 * angle^2
	return 8.f * fabsf(1.f - fabsf(dot(a, b))) < tol_angle * tol_angle;
}
