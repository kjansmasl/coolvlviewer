/**
 * @file llvector4.h
 * @brief LLVector4 class header file.
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

#ifndef LL_V4MATH_H
#define LL_V4MATH_H

#include "llvector2.h"

class LLMatrix3;
class LLMatrix4;
class LLQuaternion;

//  LLVector4 = |x y z w|

constexpr U32 LENGTHOFVECTOR4 = 4;

class LLVector4
{
public:
	// Initializes LLVector4 to (0, 0, 0, 1):
	LL_INLINE LLVector4() noexcept			{ mV[VX] = mV[VY] = mV[VZ] = 0.f; mV[VW] = 1.f; }

	// Initializes LLVector4 to (x. y, z, 1):
	LL_INLINE LLVector4(F32 x, F32 y, F32 z) noexcept
	{
		mV[VX] = x;
		mV[VY] = y;
		mV[VZ] = z;
		mV[VW] = 1.f;
	}

	LL_INLINE LLVector4(F32 x, F32 y, F32 z, F32 w) noexcept
	{
		mV[VX] = x;
		mV[VY] = y;
		mV[VZ] = z;
		mV[VW] = w;
	}

	LL_INLINE explicit LLVector4(const F32* vec) noexcept
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
		mV[VZ] = vec[VZ];
		mV[VW] = vec[VW];
	}

	LL_INLINE explicit LLVector4(const F64* vec) noexcept
	{
		mV[VX] = (F32)vec[VX];
		mV[VY] = (F32)vec[VY];
		mV[VZ] = (F32)vec[VZ];
		mV[VW] = (F32)vec[VW];
	}

	LL_INLINE explicit LLVector4(const LLVector2& vec) noexcept
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
		mV[VZ] = mV[VW] = 0.f;
	}

	LL_INLINE explicit LLVector4(const LLVector2& vec, F32 z, F32 w) noexcept
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
		mV[VZ] = z;
		mV[VW] = w;
	}

	// Initializes LLVector4 to (vec, 1):
	LL_INLINE explicit LLVector4(const LLVector3& vec) noexcept
	{
		mV[VX] = vec.mV[VX];
		mV[VY] = vec.mV[VY];
		mV[VZ] = vec.mV[VZ];
		mV[VW] = 1.f;
	}

	// Initializes LLVector4 to (vec, w):
	LL_INLINE explicit LLVector4(const LLVector3& vec, F32 w) noexcept
	{
		mV[VX] = vec.mV[VX];
		mV[VY] = vec.mV[VY];
		mV[VZ] = vec.mV[VZ];
		mV[VW] = w;
	}

	LL_INLINE explicit LLVector4(const LLSD& sd)
	{
		mV[0] = sd[0].asReal();
		mV[1] = sd[1].asReal();
		mV[2] = sd[2].asReal();
		mV[3] = sd[3].asReal();
	}

	// Allow the use of the default C++11 move constructor and assignation
	LLVector4(LLVector4&& other) noexcept = default;
	LLVector4& operator=(LLVector4&& other) noexcept = default;

	LLVector4(const LLVector4& other) = default;
	LLVector4& operator=(const LLVector4& other) = default;

	LL_INLINE void setValue(const LLSD& sd)
	{
		mV[0] = sd[0].asReal();
		mV[1] = sd[1].asReal();
		mV[2] = sd[2].asReal();
		mV[3] = sd[3].asReal();
	}

	LL_INLINE LLSD getValue() const
	{
		LLSD ret;
		ret[0] = mV[0];
		ret[1] = mV[1];
		ret[2] = mV[2];
		ret[3] = mV[3];
		return ret;
	}

	// Checks to see if all values of LLVector3 are finite
	LL_INLINE bool isFinite() const
	{
		return llfinite(mV[VX]) && llfinite(mV[VY]) && llfinite(mV[VZ]) &&
			   llfinite(mV[VW]);
	}

	// Clears LLVector4 to (0, 0, 0, 1)
	LL_INLINE void clear()					{ mV[VX] = mV[VY] = mV[VZ] = 0.f; mV[VW] = 1.f; }

	// Clears LLVector4 to (0, 0, 0, 0)
	LL_INLINE void setZero()				{ mV[VX] = mV[VY] = mV[VZ] = mV[VW] = 0.f; }

	// Sets LLVector4 to (x, y, z, 1)
	LL_INLINE void set(F32 x, F32 y, F32 z)
	{
		mV[VX] = x;
		mV[VY] = y;
		mV[VZ] = z;
		mV[VW] = 1.f;
	}

	LL_INLINE void set(F32 x, F32 y, F32 z, F32 w)
	{
		mV[VX] = x;
		mV[VY] = y;
		mV[VZ] = z;
		mV[VW] = w;
	}

	LL_INLINE void set(const LLVector4& vec)
	{
		mV[VX] = vec.mV[VX];
		mV[VY] = vec.mV[VY];
		mV[VZ] = vec.mV[VZ];
		mV[VW] = vec.mV[VW];
	}

	LL_INLINE void set(const LLVector3& vec, F32 w = 1.f)
	{
		mV[VX] = vec.mV[VX];
		mV[VY] = vec.mV[VY];
		mV[VZ] = vec.mV[VZ];
		mV[VW] = w;
	}

	LL_INLINE void set(const F32* vec)
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
		mV[VZ] = vec[VZ];
		mV[VW] = vec[VW];
	}

	// Returns magnitude of LLVector4
	LL_INLINE F32 length() const
	{
		return sqrtf(mV[VX] * mV[VX] + mV[VY] * mV[VY] + mV[VZ] * mV[VZ]);
	}

	// Returns magnitude squared of LLVector4
	LL_INLINE F32 lengthSquared() const
	{
		return mV[VX] * mV[VX] + mV[VY] * mV[VY] + mV[VZ] * mV[VZ];
	}

	// Normalizes and returns the magnitude
	LL_INLINE F32 normalize()
	{
		F32 mag = sqrtf(mV[VX] * mV[VX] + mV[VY] * mV[VY] + mV[VZ] * mV[VZ]);
		if (mag > FP_MAG_THRESHOLD)
		{
			F32 oomag = 1.f / mag;
			mV[VX] *= oomag;
			mV[VY] *= oomag;
			mV[VZ] *= oomag;
		}
		else
		{
			mV[0] = mV[1] = mV[2] = mag = 0.f;
		}
		return mag;
	}

	// Sets all values to absolute value of their original values
	// Returns true if data changed
	bool abs();

	LL_INLINE bool isExactlyClear() const	{ return mV[VW] == 1.f && !mV[VX] && !mV[VY] && !mV[VZ]; }
	LL_INLINE bool isExactlyZero() const	{ return !mV[VW] && !mV[VX] && !mV[VY] && !mV[VZ]; }

	// Rotates about vec by angle radians
	const LLVector4& rotVec(F32 angle, const LLVector4& vec);
	// Rotates about x,y,z by angle radians
	const LLVector4& rotVec(F32 angle, F32 x, F32 y, F32 z);
	// Rotates by MAT4 mat
	const LLVector4& rotVec(const LLMatrix4& mat);
	// Rotates by QUAT q
	const LLVector4& rotVec(const LLQuaternion& q);

	// Scales component-wise by vec
	const LLVector4& scaleVec(const LLVector4& vec);

	LL_INLINE F32 operator[](int idx) const	{ return mV[idx]; }
	LL_INLINE F32& operator[](int idx)		{ return mV[idx]; }

	friend std::ostream& operator<<(std::ostream& s, const LLVector4& a);	// Prints a
	friend LLVector4 operator+(const LLVector4& a, const LLVector4& b);	// Returns vector a + b
	friend LLVector4 operator-(const LLVector4& a, const LLVector4& b);	// Returns vector a minus b
	friend F32  operator*(const LLVector4& a, const LLVector4& b);		// Returns a dot b
	friend LLVector4 operator%(const LLVector4& a, const LLVector4& b);	// Returns a cross b
	friend LLVector4 operator/(const LLVector4& a, F32 k);				// Returns a divided by scaler k
	friend LLVector4 operator*(const LLVector4& a, F32 k);				// Returns a times scaler k
	friend LLVector4 operator*(F32 k, const LLVector4& a);				// Returns a times scaler k
	friend bool operator==(const LLVector4& a, const LLVector4& b);		// Returns a == b
	friend bool operator!=(const LLVector4& a, const LLVector4& b);		// Returns a != b

	friend const LLVector4& operator+=(LLVector4& a, const LLVector4& b);	// Returns vector a + b
	friend const LLVector4& operator-=(LLVector4& a, const LLVector4& b);	// Returns vector a minus b
	friend const LLVector4& operator%=(LLVector4& a, const LLVector4& b);	// Returns a cross b
	friend const LLVector4& operator*=(LLVector4& a, F32 k);				// Returns a times scaler k
	friend const LLVector4& operator/=(LLVector4& a, F32 k);				// Returns a divided by scaler k

	friend LLVector4 operator-(const LLVector4& a);	// Returns vector -a

public:
	F32 mV[LENGTHOFVECTOR4];
};

// Non-member functions

// Returns distance between a and b
LL_INLINE F32 dist_vec(const LLVector4& a, const LLVector4& b)
{
	LLVector4 vec = a - b;
	return vec.length();
}

// Returns distance squared between a and b
LL_INLINE F32 dist_vec_squared(const LLVector4& a, const LLVector4& b)
{
	LLVector4 vec = a - b;
	return vec.lengthSquared();
}

// Returns a vector that is a linear interpolation between a and b
LL_INLINE LLVector4 lerp(const LLVector4& a, const LLVector4& b, F32 u)
{
	return LLVector4(a.mV[VX] + (b.mV[VX] - a.mV[VX]) * u,
					 a.mV[VY] + (b.mV[VY] - a.mV[VY]) * u,
					 a.mV[VZ] + (b.mV[VZ] - a.mV[VZ]) * u,
					 a.mV[VW] + (b.mV[VW] - a.mV[VW]) * u);
}

// Returns angle (radians) between a and b
F32 angle_between(const LLVector4& a, const LLVector4& b);

// Returns true if a and b are very close to parallel
bool are_parallel(const LLVector4& a, const LLVector4& b,
				  F32 epsilon = F_APPROXIMATELY_ZERO);

LLVector3 vec4to3(const LLVector4& vec);
LLVector4 vec3to4(const LLVector3& vec);

// LLVector4 Operators

LL_INLINE LLVector4 operator+(const LLVector4& a, const LLVector4& b)
{
	LLVector4 c(a);
	return c += b;
}

LL_INLINE LLVector4 operator-(const LLVector4& a, const LLVector4& b)
{
	LLVector4 c(a);
	return c -= b;
}

LL_INLINE F32 operator*(const LLVector4& a, const LLVector4& b)
{
	return a.mV[VX] * b.mV[VX] + a.mV[VY] * b.mV[VY] + a.mV[VZ] * b.mV[VZ];
}

LL_INLINE LLVector4 operator%(const LLVector4& a, const LLVector4& b)
{
	return LLVector4(a.mV[VY] * b.mV[VZ] - b.mV[VY] * a.mV[VZ],
					 a.mV[VZ] * b.mV[VX] - b.mV[VZ] * a.mV[VX],
					 a.mV[VX] * b.mV[VY] - b.mV[VX] * a.mV[VY]);
}

LL_INLINE LLVector4 operator/(const LLVector4& a, F32 k)
{
	F32 t = 1.f / k;
	return LLVector4(a.mV[VX] * t, a.mV[VY] * t, a.mV[VZ] * t);
}

LL_INLINE LLVector4 operator*(const LLVector4& a, F32 k)
{
	return LLVector4(a.mV[VX] * k, a.mV[VY] * k, a.mV[VZ] * k);
}

LL_INLINE LLVector4 operator*(F32 k, const LLVector4& a)
{
	return LLVector4(a.mV[VX] * k, a.mV[VY] * k, a.mV[VZ] * k);
}

LL_INLINE bool operator==(const LLVector4& a, const LLVector4& b)
{
	return a.mV[VX] == b.mV[VX] && a.mV[VY] == b.mV[VY] &&
		   a.mV[VZ] == b.mV[VZ];
}

LL_INLINE bool operator!=(const LLVector4& a, const LLVector4& b)
{
	return a.mV[VX] != b.mV[VX] || a.mV[VY] != b.mV[VY] ||
		   a.mV[VZ] != b.mV[VZ] || a.mV[VW] != b.mV[VW];
}

LL_INLINE const LLVector4& operator+=(LLVector4& a, const LLVector4& b)
{
	a.mV[VX] += b.mV[VX];
	a.mV[VY] += b.mV[VY];
	a.mV[VZ] += b.mV[VZ];
	return a;
}

LL_INLINE const LLVector4& operator-=(LLVector4& a, const LLVector4& b)
{
	a.mV[VX] -= b.mV[VX];
	a.mV[VY] -= b.mV[VY];
	a.mV[VZ] -= b.mV[VZ];
	return a;
}

LL_INLINE const LLVector4& operator%=(LLVector4& a, const LLVector4& b)
{
	LLVector4 ret(a.mV[VY] * b.mV[VZ] - b.mV[VY] * a.mV[VZ],
				  a.mV[VZ] * b.mV[VX] - b.mV[VZ] * a.mV[VX],
				  a.mV[VX] * b.mV[VY] - b.mV[VX] * a.mV[VY]);
	a = ret;
	return a;
}

LL_INLINE const LLVector4& operator*=(LLVector4& a, F32 k)
{
	a.mV[VX] *= k;
	a.mV[VY] *= k;
	a.mV[VZ] *= k;
	return a;
}

LL_INLINE const LLVector4& operator/=(LLVector4& a, F32 k)
{
	F32 t = 1.f / k;
	a.mV[VX] *= t;
	a.mV[VY] *= t;
	a.mV[VZ] *= t;
	return a;
}

LL_INLINE LLVector4 operator-(const LLVector4& a)
{
	return LLVector4(-a.mV[VX], -a.mV[VY], -a.mV[VZ]);
}

LL_INLINE const LLVector4 srgbVector4(const LLVector4& a)
{
	return LLVector4(linearToSRGB(a.mV[0]), linearToSRGB(a.mV[1]),
					 linearToSRGB(a.mV[2]), a.mV[3]);
}

#endif	// LL_V4MATH_H
