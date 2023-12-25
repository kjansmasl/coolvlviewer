/**
 * @file llvector3d.h
 * @brief High precision 3 dimensional vector.
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

#ifndef LL_V3DMATH_H
#define LL_V3DMATH_H

#include "llvector3.h"

class LLVector3d
{
public:
	LL_INLINE LLVector3d() noexcept			{ mdV[0] = mdV[1] = mdV[2] = 0.0; }

	LL_INLINE LLVector3d(F64 x, F64 y, F64 z) noexcept
	{
		mdV[VX] = x;
		mdV[VY] = y;
		mdV[VZ] = z;
	}

	LL_INLINE explicit LLVector3d(const F64* vec) noexcept
	{
		mdV[VX] = vec[VX];
		mdV[VY] = vec[VY];
		mdV[VZ] = vec[VZ];
	}

	LL_INLINE explicit LLVector3d(const LLVector3& vec) noexcept
	{
		mdV[VX] = vec.mV[VX];
		mdV[VY] = vec.mV[VY];
		mdV[VZ] = vec.mV[VZ];
	}

#if 0
	LL_INLINE LLVector3d(const LLVector3d& copy) noexcept
	{
		mdV[VX] = copy.mdV[VX];
		mdV[VY] = copy.mdV[VY];
		mdV[VZ] = copy.mdV[VZ];
	}
#endif

	LL_INLINE explicit LLVector3d(const LLSD& sd)
	{
		setValue(sd);
	}

	// Allow the use of the default C++11 move constructor and assignation
	LLVector3d(LLVector3d&& other) noexcept = default;
	LLVector3d& operator=(LLVector3d&& other) noexcept = default;

	LLVector3d(const LLVector3d& other) = default;
	LLVector3d& operator=(const LLVector3d& other) = default;

	LL_INLINE void setValue(const LLSD& sd)
	{
		mdV[0] = sd[0].asReal();
		mdV[1] = sd[1].asReal();
		mdV[2] = sd[2].asReal();
	}

	LL_INLINE LLSD getValue() const
	{
		LLSD ret;
		ret[0] = mdV[0];
		ret[1] = mdV[1];
		ret[2] = mdV[2];
		return ret;
	}

	// checks to see if all values of LLVector3d are finite
	LL_INLINE bool isFinite() const
	{
		return llfinite(mdV[VX]) && llfinite(mdV[VY]) && llfinite(mdV[VZ]);
	}

	// Clamps all values to (min,max), returns true if data changed
	bool clamp(F64 min, F64 max);

	// Sets all values to absolute value of original value (first octant),
	// returns true if changed.
	bool abs();

	// Zero LLVector3d to (0,0,0)
	LL_INLINE const LLVector3d& clear()		{ mdV[0] = mdV[1] = mdV[2] = 0.0; return *this; }
	LL_INLINE const LLVector3d& setZero()	{ mdV[0] = mdV[1] = mdV[2] = 0.0; return *this; }

	LL_INLINE const LLVector3d& set(F64 x, F64 y, F64 z)
	{
		mdV[VX] = x;
		mdV[VY] = y;
		mdV[VZ] = z;
		return *this;
	}

	LL_INLINE const LLVector3d& set(const LLVector3& vec)
	{
		mdV[0] = vec.mV[0];
		mdV[1] = vec.mV[1];
		mdV[2] = vec.mV[2];
		return *this;
	}

	LL_INLINE const LLVector3d& set(const LLVector3d& vec)
	{
		mdV[0] = vec.mdV[0];
		mdV[1] = vec.mdV[1];
		mdV[2] = vec.mdV[2];
		return *this;
	}

	LL_INLINE const LLVector3d& set(const F64* vec)
	{
		mdV[0] = vec[0];
		mdV[1] = vec[1];
		mdV[2] = vec[2];
		return *this;
	}

	// Returns magnitude of LLVector3d
	LL_INLINE F64 length() const
	{
		return sqrt(mdV[0] * mdV[0] + mdV[1] * mdV[1] + mdV[2] * mdV[2]);
	}

	// Returns magnitude squared of LLVector3d
	LL_INLINE F64 lengthSquared() const
	{
		return mdV[0] * mdV[0] + mdV[1] * mdV[1] + mdV[2] * mdV[2];
	}

	// Normalizes and returns the magnitude
	LL_INLINE F64 normalize()
	{
		F64 mag = sqrt(mdV[0] * mdV[0] + mdV[1] * mdV[1] + mdV[2] * mdV[2]);

		if (mag > FP_MAG_THRESHOLD)
		{
			F64 oomag = 1.0 / mag;
			mdV[0] *= oomag;
			mdV[1] *= oomag;
			mdV[2] *= oomag;
		}
		else
		{
			mdV[0] = mdV[1] = mdV[2] = mag = 0.0;
		}

		return mag;
	}

	// Rotates about vec by angle radians
	const LLVector3d& rotVec(F64 angle, const LLVector3d& vec);
	// Rotates about x,y,z by angle radians
	const LLVector3d& rotVec(F64 angle, F64 x, F64 y, F64 z);
	// Rotates by LLMatrix4 mat
	const LLVector3d& rotVec(const LLMatrix3& mat);
	// Rotates by LLQuaternion q
	const LLVector3d& rotVec(const LLQuaternion& q);

	// Returns true if vector has a _very_small_ length
	LL_INLINE bool isNull() const
	{
		return F_APPROXIMATELY_ZERO > mdV[VX] * mdV[VX] +
									  mdV[VY] * mdV[VY] +
									  mdV[VZ] * mdV[VZ];
	}

	LL_INLINE bool isExactlyZero() const			{ return !mdV[VX] && !mdV[VY] && !mdV[VZ]; }

	const LLVector3d& operator=(const LLVector4& a);

	LL_INLINE F64 operator[](int idx) const			{ return mdV[idx]; }
	LL_INLINE F64& operator[](int idx)				{ return mdV[idx]; }

	friend LLVector3d operator+(const LLVector3d& a, const LLVector3d& b);	// Returns vector a + b
	friend LLVector3d operator-(const LLVector3d& a, const LLVector3d& b);	// Returns vector a minus b
	friend F64 operator*(const LLVector3d& a, const LLVector3d& b);			// Returns a dot b
	friend LLVector3d operator%(const LLVector3d& a, const LLVector3d& b);	// Returns a cross b
	friend LLVector3d operator*(const LLVector3d& a, F64 k);				// Returns a times scaler k
	friend LLVector3d operator/(const LLVector3d& a, F64 k);				// Returns a divided by scaler k
	friend LLVector3d operator*(F64 k, const LLVector3d& a);				// Returns a times scaler k
	friend bool operator==(const LLVector3d& a, const LLVector3d& b);		// Returns a == b
	friend bool operator!=(const LLVector3d& a, const LLVector3d& b);		// Returns a != b

	friend const LLVector3d& operator+=(LLVector3d& a, const LLVector3d& b);// Returns vector a + b
	friend const LLVector3d& operator-=(LLVector3d& a, const LLVector3d& b);// Returns vector a minus b
	friend const LLVector3d& operator%=(LLVector3d& a, const LLVector3d& b);// Returns a cross b
	friend const LLVector3d& operator*=(LLVector3d& a, F64 k);				// Returns a times scaler k
	friend const LLVector3d& operator/=(LLVector3d& a, F64 k);				// Returns a divided by scaler k

	// Returns vector -a
	friend LLVector3d operator-(const LLVector3d& a);

	// Streams a
	friend std::ostream& operator<<(std::ostream& s, const LLVector3d& a);

	static bool parseVector3d(const std::string& buf, LLVector3d* value);

public:
	F64 mdV[3];

	const static LLVector3d zero;
	const static LLVector3d x_axis;
	const static LLVector3d y_axis;
	const static LLVector3d z_axis;
	const static LLVector3d x_axis_neg;
	const static LLVector3d y_axis_neg;
	const static LLVector3d z_axis_neg;
};

typedef LLVector3d LLGlobalVec;

// Non-member functions

LL_INLINE LLVector3d operator+(const LLVector3d& a, const LLVector3d& b)
{
	LLVector3d c(a);
	return c += b;
}

LL_INLINE LLVector3d operator-(const LLVector3d& a, const LLVector3d& b)
{
	LLVector3d c(a);
	return c -= b;
}

LL_INLINE F64 operator*(const LLVector3d& a, const LLVector3d& b)
{
	return a.mdV[0] * b.mdV[0] + a.mdV[1] * b.mdV[1] + a.mdV[2] * b.mdV[2];
}

LL_INLINE LLVector3d operator%(const LLVector3d& a, const LLVector3d& b)
{
	return LLVector3d(a.mdV[1] * b.mdV[2] - b.mdV[1] * a.mdV[2],
					  a.mdV[2] * b.mdV[0] - b.mdV[2] * a.mdV[0],
					  a.mdV[0] * b.mdV[1] - b.mdV[0] * a.mdV[1]);
}

LL_INLINE LLVector3d operator/(const LLVector3d& a, F64 k)
{
	F64 t = 1.0 / k;
	return LLVector3d(a.mdV[0] * t, a.mdV[1] * t, a.mdV[2] * t);
}

LL_INLINE LLVector3d operator*(const LLVector3d& a, F64 k)
{
	return LLVector3d(a.mdV[0] * k, a.mdV[1] * k, a.mdV[2] * k);
}

LL_INLINE LLVector3d operator*(F64 k, const LLVector3d& a)
{
	return LLVector3d(a.mdV[0] * k, a.mdV[1] * k, a.mdV[2] * k);
}

LL_INLINE bool operator==(const LLVector3d& a, const LLVector3d& b)
{
	return a.mdV[0] == b.mdV[0] && a.mdV[1] == b.mdV[1] &&
		   a.mdV[2] == b.mdV[2];
}

LL_INLINE bool operator!=(const LLVector3d& a, const LLVector3d& b)
{
	return a.mdV[0] != b.mdV[0] || a.mdV[1] != b.mdV[1] ||
		   a.mdV[2] != b.mdV[2];
}

LL_INLINE const LLVector3d& operator+=(LLVector3d& a, const LLVector3d& b)
{
	a.mdV[0] += b.mdV[0];
	a.mdV[1] += b.mdV[1];
	a.mdV[2] += b.mdV[2];
	return a;
}

LL_INLINE const LLVector3d& operator-=(LLVector3d& a, const LLVector3d& b)
{
	a.mdV[0] -= b.mdV[0];
	a.mdV[1] -= b.mdV[1];
	a.mdV[2] -= b.mdV[2];
	return a;
}

LL_INLINE const LLVector3d& operator%=(LLVector3d& a, const LLVector3d& b)
{
	LLVector3d ret(a.mdV[1] * b.mdV[2] - b.mdV[1] * a.mdV[2],
				   a.mdV[2] * b.mdV[0] - b.mdV[2] * a.mdV[0],
				   a.mdV[0] * b.mdV[1] - b.mdV[0] * a.mdV[1]);
	a = ret;
	return a;
}

LL_INLINE const LLVector3d& operator*=(LLVector3d& a, F64 k)
{
	a.mdV[0] *= k;
	a.mdV[1] *= k;
	a.mdV[2] *= k;
	return a;
}

LL_INLINE const LLVector3d& operator/=(LLVector3d& a, F64 k)
{
	F64 t = 1.0 / k;
	a.mdV[0] *= t;
	a.mdV[1] *= t;
	a.mdV[2] *= t;
	return a;
}

LL_INLINE LLVector3d operator-(const LLVector3d& a)
{
	return LLVector3d(-a.mdV[0], -a.mdV[1], -a.mdV[2]);
}

LL_INLINE F64 dist_vec(const LLVector3d& a, const LLVector3d& b)
{
	F64 x = a.mdV[0] - b.mdV[0];
	F64 y = a.mdV[1] - b.mdV[1];
	F64 z = a.mdV[2] - b.mdV[2];
	return sqrt(x * x + y * y + z * z);
}

LL_INLINE F64 dist_vec_squared(const LLVector3d& a, const LLVector3d& b)
{
	F64 x = a.mdV[0] - b.mdV[0];
	F64 y = a.mdV[1] - b.mdV[1];
	F64 z = a.mdV[2] - b.mdV[2];
	return x * x + y * y + z * z;
}

LL_INLINE F64 dist_vec_squared2D(const LLVector3d& a, const LLVector3d& b)
{
	F64 x = a.mdV[0] - b.mdV[0];
	F64 y = a.mdV[1] - b.mdV[1];
	return x * x + y * y;
}

LL_INLINE LLVector3d lerp(const LLVector3d& a, const LLVector3d& b, F64 u)
{
	return LLVector3d(a.mdV[VX] + (b.mdV[VX] - a.mdV[VX]) * u,
					  a.mdV[VY] + (b.mdV[VY] - a.mdV[VY]) * u,
					  a.mdV[VZ] + (b.mdV[VZ] - a.mdV[VZ]) * u);
}

LL_INLINE F64 angle_between(const LLVector3d& a, const LLVector3d& b)
{
	LLVector3d an = a;
	LLVector3d bn = b;
	an.normalize();
	bn.normalize();
	F64 cosine = an * bn;
	F64 angle = cosine >= 1.0 ? 0.0
							  : cosine <= -1.0 ? F_PI
											   : acos(cosine);
	return angle;
}

LL_INLINE bool are_parallel(const LLVector3d& a, const LLVector3d& b,
							F64 epsilon)
{
	LLVector3d an = a;
	LLVector3d bn = b;
	an.normalize();
	bn.normalize();
	F64 dot = an * bn;
	return 1.0 - fabs(dot) < epsilon;
}

LL_INLINE LLVector3d projected_vec(const LLVector3d& a, const LLVector3d& b)
{
	LLVector3d project_axis = b;
	project_axis.normalize();
	return project_axis * (a * project_axis);
}

LL_INLINE LLVector3d inverse_projected_vec(const LLVector3d& a,
										   const LLVector3d& b)
{
	LLVector3d normalized_a = a;
	normalized_a.normalize();
	LLVector3d normalized_b = b;
	F64 b_length = normalized_b.normalize();

	F64 dot_product = normalized_a * normalized_b;
	return normalized_a * (b_length / dot_product);
}

#endif // LL_V3DMATH_H
