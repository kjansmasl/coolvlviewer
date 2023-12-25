/**
 * @file llvector2.h
 * @brief LLVector2 class header file.
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

#ifndef LL_V2MATH_H
#define LL_V2MATH_H

#include "llmath.h"
#include "llvector3.h"

class LLVector4;
class LLMatrix3;
class LLQuaternion;

//  LLVector2 = |x y z w|

constexpr U32 LENGTHOFVECTOR2 = 2;

class LLVector2
{
public:
	LL_INLINE LLVector2() noexcept
	{
		mV[VX] = mV[VY] = 0.f;
	}

	LL_INLINE LLVector2(F32 x, F32 y) noexcept
	{
		mV[VX] = x;
		mV[VY] = y;
	}

	LL_INLINE LLVector2(const F32* vec) noexcept
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
	}

	LL_INLINE explicit LLVector2(const LLVector3& vec) noexcept
	{
		mV[VX] = vec.mV[VX];
		mV[VY] = vec.mV[VY];
	}

	LL_INLINE explicit LLVector2(const LLSD& sd) noexcept
	{
		mV[0] = (F32)sd[0].asReal();
		mV[1] = (F32)sd[1].asReal();
	}

	// Allow the use of the default C++11 move constructor and assignation
	LLVector2(LLVector2&& other) noexcept = default;
	LLVector2& operator=(LLVector2&& other) noexcept = default;

	LLVector2(const LLVector2& other) = default;
	LLVector2& operator=(const LLVector2& other) = default;

	// Clears LLVector2 to (0, 0).
	LL_INLINE void clear()						{ mV[VX] = mV[VY] = 0.f; }
	LL_INLINE void setZero()					{ mV[VX] = mV[VY] = 0.f; }

	LL_INLINE void set(F32 x, F32 y)
	{
		mV[VX] = x;
		mV[VY] = y;
	}

	LL_INLINE void set(const LLVector2& vec)
	{
		mV[VX] = vec.mV[VX];
		mV[VY] = vec.mV[VY];
	}

	LL_INLINE void set(const F32* vec)
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
	}

	LL_INLINE LLSD getValue() const
	{
		LLSD ret;
		ret[0] = mV[0];
		ret[1] = mV[1];
		return ret;
	}

	LL_INLINE void setValue(const LLSD& sd)
	{
		mV[0] = (F32)sd[0].asReal();
		mV[1] = (F32)sd[1].asReal();
	}

	// Checks to see if all values of LLVector2 are finite
	LL_INLINE bool isFinite() const				{ return llfinite(mV[VX]) && llfinite(mV[VY]); }

	// Returns magnitude of LLVector2
	LL_INLINE F32 length() const				{ return sqrtf(mV[0] * mV[0] + mV[1] * mV[1]); }

	// Returns magnitude squared of LLVector2
	LL_INLINE F32 lengthSquared() const			{ return mV[0] * mV[0] + mV[1] * mV[1]; }

	// Normalizes and returns the magnitude
	LL_INLINE F32 normalize()
	{
		F32 mag = sqrtf(mV[0] * mV[0] + mV[1] * mV[1]);
		if (mag > FP_MAG_THRESHOLD)
		{
			F32 oomag = 1.f / mag;
			mV[0] *= oomag;
			mV[1] *= oomag;
		}
		else
		{
			mV[0] = mV[1] = mag = 0.f;
		}
		return mag;
	}

	// scales per component by vec
	LL_INLINE const LLVector2& scaleVec(const LLVector2& vec)
	{
		mV[VX] *= vec.mV[VX];
		mV[VY] *= vec.mV[VY];
		return *this;
	}

	// Sets all values to absolute value of original value (first octant),
	// returns true if changed.
	bool abs();

	// Returns true if vector has a _very_small_ length
	LL_INLINE bool isNull()
	{
		return F_APPROXIMATELY_ZERO > mV[VX] * mV[VX] + mV[VY] * mV[VY];
	}

	LL_INLINE bool isExactlyZero() const		{ return !mV[VX] && !mV[VY]; }

	LL_INLINE F32 operator[](int idx) const		{ return mV[idx]; }
	LL_INLINE F32& operator[](int idx)			{ return mV[idx]; }

	friend bool operator<(const LLVector2& a, const LLVector2& b);		// For sorting. x is "more significant" than y
	friend LLVector2 operator+(const LLVector2& a, const LLVector2& b);	// Returns vector a + b
	friend LLVector2 operator-(const LLVector2& a, const LLVector2& b);	// Returns vector a minus b
	friend F32 operator*(const LLVector2& a, const LLVector2& b);		// Returns a dot b
	friend LLVector2 operator%(const LLVector2& a, const LLVector2& b);	// Returns a cross b
	friend LLVector2 operator/(const LLVector2& a, F32 k);				// Returns a divided by scaler k
	friend LLVector2 operator*(const LLVector2& a, F32 k);				// Returns a times scaler k
	friend LLVector2 operator*(F32 k, const LLVector2& a);				// Returns a times scaler k
	friend bool operator==(const LLVector2& a, const LLVector2& b);		// Returns a == b
	friend bool operator!=(const LLVector2& a, const LLVector2& b);		// Returns a != b

	friend const LLVector2& operator+=(LLVector2& a, const LLVector2& b);	// Returns vector a + b
	friend const LLVector2& operator-=(LLVector2& a, const LLVector2& b);	// Returns vector a minus b
	friend const LLVector2& operator%=(LLVector2& a, const LLVector2& b);	// Returns a cross b
	friend const LLVector2& operator*=(LLVector2& a, F32 k);				// Returns a times scaler k
	friend const LLVector2& operator/=(LLVector2& a, F32 k);				// Returns a divided by scaler k

	friend LLVector2 operator-(const LLVector2& a);	// Returns vector -a

	friend std::ostream& operator<<(std::ostream& s, const LLVector2& a);	// Stream a

public:
	F32 mV[LENGTHOFVECTOR2];

	static LLVector2 zero;
};

// Non-member functions

LL_INLINE void update_min_max(LLVector2& min, LLVector2& max,
							  const LLVector2& pos)
{
	for (U32 i = 0; i < 2; ++i)
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

// Returns angle (radians) between a and b
F32	angle_between(const LLVector2& a, const LLVector2& b);

// Returns true if a and b are very close to parallel
bool are_parallel(const LLVector2& a, const LLVector2& b,
				  F32 epsilon = F_APPROXIMATELY_ZERO);

// Returns distance between a and b
F32	dist_vec(const LLVector2& a, const LLVector2& b);

// Returns distance squared between a and b
F32	dist_vec_squared(const LLVector2& a, const LLVector2& b);

// Returns distance squared between a and b ignoring Z component
F32	dist_vec_squared2D(const LLVector2& a, const LLVector2& b);

// Returns a vector that is a linear interpolation between a and b
LLVector2 lerp(const LLVector2& a, const LLVector2& b, F32 u);

// LLVector2 Operators

// For sorting. By convention, x is "more significant" than y.
LL_INLINE bool operator<(const LLVector2& a, const LLVector2& b)
{
	if (a.mV[VX] == b.mV[VX])
	{
		return a.mV[VY] < b.mV[VY];
	}
	else
	{
		return a.mV[VX] < b.mV[VX];
	}
}

LL_INLINE LLVector2 operator+(const LLVector2& a, const LLVector2& b)
{
	LLVector2 c(a);
	return c += b;
}

LL_INLINE LLVector2 operator-(const LLVector2& a, const LLVector2& b)
{
	LLVector2 c(a);
	return c -= b;
}

LL_INLINE F32 operator*(const LLVector2& a, const LLVector2& b)
{
	return (a.mV[0]*b.mV[0] + a.mV[1]*b.mV[1]);
}

LL_INLINE LLVector2 operator%(const LLVector2& a, const LLVector2& b)
{
	return LLVector2(a.mV[0] * b.mV[1] - b.mV[0] * a.mV[1],
					 a.mV[1] * b.mV[0] - b.mV[1] * a.mV[0]);
}

LL_INLINE LLVector2 operator/(const LLVector2& a, F32 k)
{
	F32 t = 1.f / k;
	return LLVector2(a.mV[0] * t, a.mV[1] * t);
}

LL_INLINE LLVector2 operator*(const LLVector2& a, F32 k)
{
	return LLVector2(a.mV[0] * k, a.mV[1] * k);
}

LL_INLINE LLVector2 operator*(F32 k, const LLVector2& a)
{
	return LLVector2(a.mV[0] * k, a.mV[1] * k);
}

LL_INLINE bool operator==(const LLVector2& a, const LLVector2& b)
{
	return a.mV[0] == b.mV[0] && a.mV[1] == b.mV[1];
}

LL_INLINE bool operator!=(const LLVector2& a, const LLVector2& b)
{
	return a.mV[0] != b.mV[0] || a.mV[1] != b.mV[1];
}

LL_INLINE const LLVector2& operator+=(LLVector2& a, const LLVector2& b)
{
	a.mV[0] += b.mV[0];
	a.mV[1] += b.mV[1];
	return a;
}

LL_INLINE const LLVector2& operator-=(LLVector2& a, const LLVector2& b)
{
	a.mV[0] -= b.mV[0];
	a.mV[1] -= b.mV[1];
	return a;
}

LL_INLINE const LLVector2& operator%=(LLVector2& a, const LLVector2& b)
{
	LLVector2 ret(a.mV[0] * b.mV[1] - b.mV[0] * a.mV[1],
				  a.mV[1] * b.mV[0] - b.mV[1] * a.mV[0]);
	a = ret;
	return a;
}

LL_INLINE const LLVector2& operator*=(LLVector2& a, F32 k)
{
	a.mV[0] *= k;
	a.mV[1] *= k;
	return a;
}

LL_INLINE const LLVector2& operator/=(LLVector2& a, F32 k)
{
	F32 t = 1.f / k;
	a.mV[0] *= t;
	a.mV[1] *= t;
	return a;
}

LL_INLINE LLVector2 operator-(const LLVector2& a)
{
	return LLVector2(-a.mV[0], -a.mV[1]);
}

LL_INLINE std::ostream& operator<<(std::ostream& s, const LLVector2& a)
{
	s << "{ " << a.mV[VX] << ", " << a.mV[VY] << " }";
	return s;
}

#endif	// LL_V2MATH_H
