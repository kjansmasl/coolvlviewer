/**
 * @file llvector3.h
 * @brief LLVector3 class header file.
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

#ifndef LL_V3MATH_H
#define LL_V3MATH_H

#include "llmath.h"
#include "llsd.h"

class LLVector2;
class LLVector4;
class LLMatrix3;
class LLMatrix4;
class LLVector3d;
class LLQuaternion;

//  LLvector3 = |x y z w|

constexpr U32 LENGTHOFVECTOR3 = 3;

class LLVector3
{
public:
	LL_INLINE LLVector3() noexcept				{ mV[0] = mV[1] = mV[2] = 0.f; }

	LL_INLINE LLVector3(F32 x, F32 y, F32 z) noexcept
	{
		mV[VX] = x;
		mV[VY] = y;
		mV[VZ] = z;
	}

	LL_INLINE explicit LLVector3(const F32* vec) noexcept
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
		mV[VZ] = vec[VZ];
	}

#if 0
	LL_INLINE LLVector3(const LLVector3& copy) noexcept
	{
		mV[VX] = copy.mV[VX];
		mV[VY] = copy.mV[VY];
		mV[VZ] = copy.mV[VZ];
	}
#endif

	explicit LLVector3(const LLVector2& vec) noexcept;
	explicit LLVector3(const LLVector3d& vec) noexcept;
	explicit LLVector3(const LLVector4& vec) noexcept;

	// Allow the use of the default C++11 move constructor and assignation
	LLVector3(LLVector3&& other) noexcept = default;
	LLVector3& operator=(LLVector3&& other) noexcept = default;

	LLVector3(const LLVector3& other) = default;
	LLVector3& operator=(const LLVector3& other) = default;

	LL_INLINE explicit LLVector3(const LLSD& sd)	{ setValue(sd); }

	LL_INLINE LLSD getValue() const
	{
		LLSD ret;
		ret[0] = mV[0];
		ret[1] = mV[1];
		ret[2] = mV[2];
		return ret;
	}

	LL_INLINE void setValue(const LLSD& sd)
	{
		mV[0] = (F32)sd[0].asReal();
		mV[1] = (F32)sd[1].asReal();
		mV[2] = (F32)sd[2].asReal();
	}

	// checks to see if all values of LLVector3 are finite
	LL_INLINE bool isFinite() const
	{
		return llfinite(mV[VX]) && llfinite(mV[VY]) && llfinite(mV[VZ]);
	}

	// Clamps all values to (min, max), returns true if data changed
	bool clamp(F32 min, F32 max);
	// Scales vector by another vector
	bool clamp(const LLVector3& min_vec, const LLVector3& max_vec);
	// Scales vector to limit length to a value
	bool clampLength(F32 length_limit);

	// Change the vector to reflect quatization
	void quantize16(F32 lowerxy, F32 upperxy, F32 lowerz, F32 upperz);
	void quantize8(F32 lowerxy, F32 upperxy, F32 lowerz, F32 upperz);

	// Snaps x,y,z to sig_digits decimal places
	void snap(S32 sig_digits);

	// Sets all values to absolute value of original value (first octant),
	// returns true if changed.
	bool abs();

	// Clear LLVector3 to (0, 0, 0)
	LL_INLINE void clear()						{ mV[0] = mV[1] = mV[2] = 0.f; }
	LL_INLINE void setZero()					{ mV[0] = mV[1] = mV[2] = 0.f; }

	LL_INLINE void set(F32 x, F32 y, F32 z)
	{
		mV[VX] = x;
		mV[VY] = y;
		mV[VZ] = z;
	}

	LL_INLINE void set(const LLVector3& vec)
	{
		mV[0] = vec.mV[0];
		mV[1] = vec.mV[1];
		mV[2] = vec.mV[2];
	}

	LL_INLINE void set(const F32* vec)
	{
		mV[0] = vec[0];
		mV[1] = vec[1];
		mV[2] = vec[2];
	}

	const LLVector3& set(const LLVector4& vec);
	const LLVector3& set(const LLVector3d& vec);

	// Returns magnitude of LLVector3
	LL_INLINE F32 length() const
	{
		return sqrtf(mV[0] * mV[0] + mV[1] * mV[1] + mV[2] * mV[2]);
	}

	// Returns squared magnitude
	LL_INLINE F32 lengthSquared() const
	{
		return mV[0] * mV[0] + mV[1] * mV[1] + mV[2] * mV[2];
	}

	// Normalizes and returns the magnitude
	LL_INLINE F32 normalize()
	{
		F32 mag = sqrtf(mV[0] * mV[0] + mV[1] * mV[1] + mV[2] * mV[2]);
		if (mag > FP_MAG_THRESHOLD)
		{
			F32 oomag = 1.f / mag;
			mV[0] *= oomag;
			mV[1] *= oomag;
			mV[2] *= oomag;
		}
		else
		{
			mV[0] = mV[1] = mV[2] = mag = 0.f;
		}
		return mag;
	}

	// Returns true if all values of the vector are between min and max
	LL_INLINE bool inRange(F32 min, F32 max) const
	{
		return mV[0] >= min && mV[0] <= max && mV[1] >= min && mV[1] <= max &&
			   mV[2] >= min && mV[2] <= max;
	}

	// Rotates about vec by angle radians
	const LLVector3& rotVec(F32 angle, const LLVector3& vec);
	// Rotates about x,y,z by angle radians
	const LLVector3& rotVec(F32 angle, F32 x, F32 y, F32 z);
	// Rotates by LLMatrix4 mat
	const LLVector3& rotVec(const LLMatrix3& mat);
	// Rotates by LLQuaternion q
	const LLVector3& rotVec(const LLQuaternion& q);
	// Transforms by LLMatrix4 mat (mat * v)
	const LLVector3& transVec(const LLMatrix4& mat);

	// Scales per component by vec
	const LLVector3& scaleVec(const LLVector3& vec);
	// Gets a copy of this vector scaled by vec
	LLVector3 scaledVec(const LLVector3& vec) const;

	// Returns true if vector has a _very_small_ length
	LL_INLINE bool isNull() const
	{
		return F_APPROXIMATELY_ZERO > mV[VX] * mV[VX] +
									  mV[VY] * mV[VY] +
									  mV[VZ] * mV[VZ];
	}

	LL_INLINE bool isExactlyZero() const		{ return !mV[VX] && !mV[VY] && !mV[VZ]; }

	LL_INLINE F32 operator[](int idx) const		{ return mV[idx]; }
	LL_INLINE F32 &operator[](int idx)			{ return mV[idx]; }

	friend LLVector3 operator+(const LLVector3& a, const LLVector3& b);	// Returns vector a + b
	friend LLVector3 operator-(const LLVector3& a, const LLVector3& b);	// Returns vector a minus b
	friend F32 operator*(const LLVector3& a, const LLVector3& b);		// Returns a dot b
	friend LLVector3 operator%(const LLVector3& a, const LLVector3& b);	// Returns a cross b
	friend LLVector3 operator*(const LLVector3& a, F32 k);				// Returns a times scaler k
	friend LLVector3 operator/(const LLVector3& a, F32 k);				// Returns a divided by scaler k
	friend LLVector3 operator*(F32 k, const LLVector3& a);				// Returns a times scaler k
	friend bool operator==(const LLVector3& a, const LLVector3& b);		// Returns a == b
	friend bool operator!=(const LLVector3& a, const LLVector3& b);		// Returns a != b
	// less than operator useful for using vectors as std::map keys
	friend bool operator<(const LLVector3& a, const LLVector3& b);		// Returns a < b

	friend const LLVector3& operator+=(LLVector3& a, const LLVector3& b);	// Returns vector a + b
	friend const LLVector3& operator-=(LLVector3& a, const LLVector3& b);	// Returns vector a minus b
	friend const LLVector3& operator%=(LLVector3& a, const LLVector3& b);	// Returns a cross b
	friend const LLVector3& operator*=(LLVector3& a, const LLVector3& b);	// Returns a * b;
	friend const LLVector3& operator*=(LLVector3& a, F32 k);				// Returns a times scaler k
	friend const LLVector3& operator/=(LLVector3& a, F32 k);				// Returns a divided by scaler k
	friend const LLVector3& operator*=(LLVector3& a, const LLQuaternion& b);// Returns a * b;

	friend LLVector3 operator-(const LLVector3& a);							// Returns vector -a

	friend std::ostream& operator<<(std::ostream& s, const LLVector3& a);	// Streams a

	static bool parseVector3(const std::string& buf, LLVector3* value);

	static LLVector3 pointToBoxOffset(const LLVector3& pos,
									  const LLVector3* box);

	static bool boxValidAndNonZero(const LLVector3* box);

public:
	F32 mV[LENGTHOFVECTOR3];

	static const LLVector3 zero;
	static const LLVector3 x_axis;
	static const LLVector3 y_axis;
	static const LLVector3 z_axis;
	static const LLVector3 x_axis_neg;
	static const LLVector3 y_axis_neg;
	static const LLVector3 z_axis_neg;
	static const LLVector3 all_one;
};

typedef LLVector3 LLSimLocalVec;

// Non-member functions

LL_INLINE LLVector3 operator+(const LLVector3& a, const LLVector3& b)
{
	LLVector3 c(a);
	return c += b;
}

LL_INLINE LLVector3 operator-(const LLVector3& a, const LLVector3& b)
{
	LLVector3 c(a);
	return c -= b;
}

LL_INLINE F32 operator*(const LLVector3& a, const LLVector3& b)
{
	return (a.mV[0] * b.mV[0] + a.mV[1] * b.mV[1] + a.mV[2] * b.mV[2]);
}

LL_INLINE LLVector3 operator%(const LLVector3& a, const LLVector3& b)
{
	return LLVector3(a.mV[1] * b.mV[2] - b.mV[1] * a.mV[2],
					 a.mV[2] * b.mV[0] - b.mV[2] * a.mV[0],
					 a.mV[0] * b.mV[1] - b.mV[0] * a.mV[1]);
}

LL_INLINE LLVector3 operator/(const LLVector3& a, F32 k)
{
	F32 t = 1.f / k;
	return LLVector3(a.mV[0] * t, a.mV[1] * t, a.mV[2] * t);
}

LL_INLINE LLVector3 operator*(const LLVector3& a, F32 k)
{
	return LLVector3(a.mV[0] * k, a.mV[1] * k, a.mV[2] * k);
}

LL_INLINE LLVector3 operator*(F32 k, const LLVector3& a)
{
	return LLVector3(a.mV[0] * k, a.mV[1] * k, a.mV[2] * k);
}

LL_INLINE bool operator==(const LLVector3& a, const LLVector3& b)
{
	return a.mV[0] == b.mV[0] && a.mV[1] == b.mV[1] && a.mV[2] == b.mV[2];
}

LL_INLINE bool operator!=(const LLVector3& a, const LLVector3& b)
{
	return a.mV[0] != b.mV[0] || a.mV[1] != b.mV[1] || a.mV[2] != b.mV[2];
}

LL_INLINE bool operator<(const LLVector3& a, const LLVector3& b)
{
	return (a.mV[0] < b.mV[0] ||
			(a.mV[0] == b.mV[0] &&
			 (a.mV[1] < b.mV[1] ||
			  (a.mV[1] == b.mV[1] && a.mV[2] < b.mV[2]))));
}

LL_INLINE const LLVector3& operator+=(LLVector3& a, const LLVector3& b)
{
	a.mV[0] += b.mV[0];
	a.mV[1] += b.mV[1];
	a.mV[2] += b.mV[2];
	return a;
}

LL_INLINE const LLVector3& operator-=(LLVector3& a, const LLVector3& b)
{
	a.mV[0] -= b.mV[0];
	a.mV[1] -= b.mV[1];
	a.mV[2] -= b.mV[2];
	return a;
}

LL_INLINE const LLVector3& operator%=(LLVector3& a, const LLVector3& b)
{
	LLVector3 ret(a.mV[1] * b.mV[2] - b.mV[1] * a.mV[2],
				  a.mV[2] * b.mV[0] - b.mV[2] * a.mV[0],
				  a.mV[0] * b.mV[1] - b.mV[0] * a.mV[1]);
	a = ret;
	return a;
}

LL_INLINE const LLVector3& operator*=(LLVector3& a, F32 k)
{
	a.mV[0] *= k;
	a.mV[1] *= k;
	a.mV[2] *= k;
	return a;
}

LL_INLINE const LLVector3& operator*=(LLVector3& a, const LLVector3& b)
{
	a.mV[0] *= b.mV[0];
	a.mV[1] *= b.mV[1];
	a.mV[2] *= b.mV[2];
	return a;
}

LL_INLINE const LLVector3& operator/=(LLVector3& a, F32 k)
{
	F32 t = 1.f / k;
	a.mV[0] *= t;
	a.mV[1] *= t;
	a.mV[2] *= t;
	return a;
}

LL_INLINE LLVector3 operator-(const LLVector3& a)
{
	return LLVector3(-a.mV[0], -a.mV[1], -a.mV[2]);
}

// Returns distance between a and b
LL_INLINE F32 dist_vec(const LLVector3& a, const LLVector3& b)
{
	F32 x = a.mV[0] - b.mV[0];
	F32 y = a.mV[1] - b.mV[1];
	F32 z = a.mV[2] - b.mV[2];
	return sqrtf(x * x + y * y + z * z);
}

// Returns distance squared between a and b
LL_INLINE F32 dist_vec_squared(const LLVector3& a, const LLVector3& b)
{
	F32 x = a.mV[0] - b.mV[0];
	F32 y = a.mV[1] - b.mV[1];
	F32 z = a.mV[2] - b.mV[2];
	return x * x + y * y + z * z;
}

// Returns distance squared between a and b ignoring Z component
LL_INLINE F32 dist_vec_squared2D(const LLVector3& a, const LLVector3& b)
{
	F32 x = a.mV[0] - b.mV[0];
	F32 y = a.mV[1] - b.mV[1];
	return x * x + y * y;
}

// Returns vector a projected on vector b
LL_INLINE LLVector3 projected_vec(const LLVector3& a, const LLVector3& b)
{
	F32 bb = b * b;
	if (bb > FP_MAG_THRESHOLD * FP_MAG_THRESHOLD)
	{
		return ((a * b) / bb) * b;
	}
	else
	{
		return b.zero;
	}
}

// Returns vector a scaled such that:
//   projected_vec(inverse_projected_vec(a, b), b) == b;
LL_INLINE LLVector3 inverse_projected_vec(const LLVector3& a,
										  const LLVector3& b)
{
	LLVector3 normalized_a = a;
	normalized_a.normalize();
	LLVector3 normalized_b = b;
	F64 b_length = normalized_b.normalize();

	F64 dot_product = normalized_a * normalized_b;
	// NB: if a _|_ b, then returns an infinite vector
	return normalized_a * (b_length / dot_product);
}

// Returns vector a projected on vector b (same as projected_vec)
LL_INLINE LLVector3 parallel_component(const LLVector3& a, const LLVector3& b)
{
	return projected_vec(a, b);
}

// Returns component of vector a not parallel to vector b (same as
// projected_vec)
LL_INLINE LLVector3 orthogonal_component(const LLVector3& a,
										 const LLVector3& b)
{
	return a - projected_vec(a, b);
}

// Returns a vector that is a linear interpolation between a and b
LL_INLINE LLVector3 lerp(const LLVector3& a, const LLVector3& b, F32 u)
{
	return LLVector3(a.mV[VX] + (b.mV[VX] - a.mV[VX]) * u,
					 a.mV[VY] + (b.mV[VY] - a.mV[VY]) * u,
					 a.mV[VZ] + (b.mV[VZ] - a.mV[VZ]) * u);
}

LL_INLINE void update_min_max(LLVector3& min, LLVector3& max,
							  const LLVector3& pos)
{
	for (U32 i = 0; i < 3; ++i)
	{
		if (min.mV[i] > pos.mV[i])
		{
			min.mV[i] = pos.mV[i];
		}
		if (max.mV[i] < pos.mV[i])
		{
			max.mV[i] = pos.mV[i];
		}
	}
}

LL_INLINE void update_min_max(LLVector3& min, LLVector3& max, const F32* pos)
{
	for (U32 i = 0; i < 3; ++i)
	{
		if (min.mV[i] > pos[i])
		{
			min.mV[i] = pos[i];
		}
		if (max.mV[i] < pos[i])
		{
			max.mV[i] = pos[i];
		}
	}
}

// Returns angle (radians) between a and b
LL_INLINE F32 angle_between(const LLVector3& a, const LLVector3& b)
{
	F32 ab = a * b;						// dotproduct
	if (ab == -0.0f)
	{
		ab = 0.0f;						// get rid of negative zero
	}
	LLVector3 c = a % b;				// crossproduct
	return atan2f(sqrtf(c * c), ab);	// return the angle
}

// Returns true if a and b are very close to parallel
LL_INLINE bool are_parallel(const LLVector3& a, const LLVector3& b,
							F32 epsilon = F_APPROXIMATELY_ZERO)
{
	LLVector3 an = a;
	LLVector3 bn = b;
	an.normalize();
	bn.normalize();
	F32 dot = an * bn;
	return 1.0f - fabs(dot) < epsilon;
}

LL_INLINE std::ostream& operator<<(std::ostream& s, const LLVector3& a)
{
	s << "{ " << a.mV[VX] << ", " << a.mV[VY] << ", " << a.mV[VZ] << " }";
	return s;
}

#endif	// LL_V3MATH_H
