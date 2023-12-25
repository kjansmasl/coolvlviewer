/**
 * @file llcolor4u.h
 * @brief The LLColor4U class.
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

#ifndef LL_V4COLORU_H
#define LL_V4COLORU_H

#include "llmath.h"
#include "llcolor3.h"
#include "llcolor4.h"

class LLColor4;

//  LLColor4U = | red green blue alpha |

constexpr U32 LENGTHOFCOLOR4U = 4;

class LLColor4U
{
public:

	LL_INLINE LLColor4U() noexcept
	{
		mV[VX] = mV[VY] = mV[VZ] = 0;
		mV[VW] = 255;
	}

	LL_INLINE LLColor4U(U8 r, U8 g, U8 b) noexcept
	{
		mV[VX] = r;
		mV[VY] = g;
		mV[VZ] = b;
		mV[VW] = 255;
	}

	LL_INLINE LLColor4U(U8 r, U8 g, U8 b, U8 a) noexcept
	{
		mV[VX] = r;
		mV[VY] = g;
		mV[VZ] = b;
		mV[VW] = a;
	}

	LL_INLINE LLColor4U(const U8* vec) noexcept
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
		mV[VZ] = vec[VZ];
		mV[VW] = vec[VW];
	}

#if 0
	LL_INLINE LLColor4U(const LLColor3& vec) noexcept
	{
		mV[VX] = vec.mV[VX];
		mV[VY] = vec.mV[VY];
		mV[VZ] = vec.mV[VZ];
		mV[VW] = 255;
	}
#endif

	LL_INLINE void setValue(const LLSD& sd) noexcept
	{
		mV[0] = sd[0].asInteger();
		mV[1] = sd[1].asInteger();
		mV[2] = sd[2].asInteger();
		mV[3] = sd[3].asInteger();
	}

	LL_INLINE explicit LLColor4U(const LLSD& sd)		{ setValue(sd); }

	// Allow the use of the default C++11 move constructor and assignation
	LLColor4U(LLColor4U&& other) noexcept = default;
	LLColor4U& operator=(LLColor4U&& other) noexcept = default;

	LLColor4U(const LLColor4U& other) = default;
	LLColor4U& operator=(const LLColor4U& other) = default;

	LL_INLINE LLSD getValue() const
	{
		LLSD ret;
		ret[0] = mV[0];
		ret[1] = mV[1];
		ret[2] = mV[2];
		ret[3] = mV[3];
		return ret;
	}

	LL_INLINE U32 asRGBA() const
	{
		U32 rgba = 0;

		// Little endian: values are swapped in memory. The original code
		// access the array like a U32, so we need to swap here
		rgba |= mV[3];
		rgba <<= 8;
		rgba |= mV[2];
		rgba <<= 8;
		rgba |= mV[1];
		rgba <<= 8;
		rgba |= mV[0];

		return rgba;
	}

	LL_INLINE void fromRGBA(U32 rgba)
	{
		// Little endian: values are swapped in memory. The original code
		// access the array like a U32, so we need to swap here
		mV[0] = rgba & 0xFF;
		rgba >>= 8;
		mV[1] = rgba & 0xFF;
		rgba >>= 8;
		mV[2] = rgba & 0xFF;
		rgba >>= 8;
		mV[3] = rgba & 0xFF;
	}

	LL_INLINE const LLColor4U& setToBlack()
	{
		mV[VX] = mV[VY] = mV[VZ] = 0;
		mV[VW] = 255;
		return *this;
	}

	LL_INLINE const LLColor4U& setToWhite()
	{
		mV[VX] = mV[VY] = mV[VZ] = mV[VW] = 255;
		return *this;
	}


	LL_INLINE const LLColor4U& set(U8 x, U8 y, U8 z)
	{
		mV[VX] = x;
		mV[VY] = y;
		mV[VZ] = z;
#if 0	// no change to alpha !
		mV[VW] = 255;
#endif
		return *this;
	}

	LL_INLINE const LLColor4U& set(U8 r, U8 g, U8 b, U8 a)
	{
		mV[0] = r;
		mV[1] = g;
		mV[2] = b;
		mV[3] = a;
		return *this;
	}

	LL_INLINE const LLColor4U& set(const LLColor4U& vec)
	{
		mV[VX] = vec.mV[VX];
		mV[VY] = vec.mV[VY];
		mV[VZ] = vec.mV[VZ];
		mV[VW] = vec.mV[VW];
		return *this;
	}

	LL_INLINE const LLColor4U& set(const U8* vec)
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
		mV[VZ] = vec[VZ];
		mV[VW] = vec[VW];
		return *this;
	}

	LL_INLINE const LLColor4U& setAlpha(U8 a)
	{
		mV[VW] = a;
		return *this;
	}

	// Returns magnitude of LLColor4U
	LL_INLINE F32 length() const
	{
		return sqrtf(((F32)mV[VX]) * mV[VX] + ((F32)mV[VY]) * mV[VY] +
					 ((F32)mV[VZ]) * mV[VZ]);
	}

	// Returns squared magnitude of LLColor4U
	LL_INLINE F32 lengthSquared() const
	{
		return ((F32)mV[VX]) * mV[VX] + ((F32)mV[VY]) * mV[VY] +
			   ((F32)mV[VZ]) * mV[VZ];
	}

	// Prints a
	friend std::ostream& operator<<(std::ostream& s, const LLColor4U& a);
	// Returns vector a + b
	friend LLColor4U operator+(const LLColor4U& a, const LLColor4U& b);
	// Returns vector a minus b
	friend LLColor4U operator-(const LLColor4U& a, const LLColor4U& b);
	// Returns a * b
	friend LLColor4U operator*(const LLColor4U& a, const LLColor4U& b);
	// Returns a == b
	friend bool operator==(const LLColor4U& a, const LLColor4U& b);
	// Returns a != b
	friend bool operator!=(const LLColor4U& a, const LLColor4U& b);

	// Returns vector a + b
	friend const LLColor4U& operator+=(LLColor4U& a, const LLColor4U& b);
	// Returns vector a minus b
	friend const LLColor4U& operator-=(LLColor4U& a, const LLColor4U& b);
	// Returns rgb times scaler k (no alpha change)
	friend const LLColor4U& operator*=(LLColor4U& a, U8 k);
	// Returns alpha times scaler k (no rgb change)
	friend const LLColor4U& operator%=(LLColor4U& a, U8 k);

	LLColor4U addClampMax(const LLColor4U& color);	// Add and clamp the max

	LLColor4U multAll(F32 k);	// Multiplies ALL channels by scalar k
	const LLColor4U& combine();

	void setVecScaleClamp(const LLColor3& color);
	void setVecScaleClamp(const LLColor4& color);

	// Note: when 'strict' is false, a 3 numbers vector is accepted and the
	// missing alpha value is then set to 255 (opaque). HB
	static bool parseColor4U(const std::string& buf, LLColor4U* value,
							 bool strict = true);

	// Conversion
	LL_INLINE operator LLColor4() const
	{
		return LLColor4(*this);
	}

#if 0
	LL_INLINE operator LLColor4()
	{
		return LLColor4((F32)mV[VRED] / 255.f,
						(F32)mV[VGREEN] / 255.f,
						(F32)mV[VBLUE] / 255.f,
						(F32)mV[VALPHA] / 255.f));
	}
#endif

public:
	union
	{
		U8				mV[LENGTHOFCOLOR4U];
		U32				mAll;
	};		

public:
	static LLColor4U	white;
	static LLColor4U	black;
	static LLColor4U	red;
	static LLColor4U	green;
	static LLColor4U	blue;
};

LL_INLINE LLColor4U operator+(const LLColor4U& a, const LLColor4U& b)
{
	return LLColor4U(a.mV[VX] + b.mV[VX], a.mV[VY] + b.mV[VY],
					 a.mV[VZ] + b.mV[VZ], a.mV[VW] + b.mV[VW]);
}

LL_INLINE LLColor4U operator-(const LLColor4U& a, const LLColor4U& b)
{
	return LLColor4U(a.mV[VX] - b.mV[VX], a.mV[VY] - b.mV[VY],
					 a.mV[VZ] - b.mV[VZ], a.mV[VW] - b.mV[VW]);
}

LL_INLINE LLColor4U operator*(const LLColor4U& a, const LLColor4U& b)
{
	return LLColor4U(a.mV[VX] * b.mV[VX], a.mV[VY] * b.mV[VY],
					 a.mV[VZ] * b.mV[VZ], a.mV[VW] * b.mV[VW]);
}

LL_INLINE LLColor4U LLColor4U::addClampMax(const LLColor4U& color)
{
	return LLColor4U(llmin((S32)mV[VX] + color.mV[VX], 255),
					 llmin((S32)mV[VY] + color.mV[VY], 255),
					 llmin((S32)mV[VZ] + color.mV[VZ], 255),
					 llmin((S32)mV[VW] + color.mV[VW], 255));
}

LL_INLINE LLColor4U LLColor4U::multAll(F32 k)
{
	// Round to nearest
	return LLColor4U((U8)ll_roundp(mV[VX] * k), (U8)ll_roundp(mV[VY] * k),
					 (U8)ll_roundp(mV[VZ] * k), (U8)ll_roundp(mV[VW] * k));
}

#if 0
LL_INLINE LLColor4U operator*(const LLColor4U& a, U8 k)
{
	// Only affects rgb (not a !)
	return LLColor4U(a.mV[VX] * k, a.mV[VY] * k, a.mV[VZ] * k, a.mV[VW]);
}

LL_INLINE LLColor4U operator*(U8 k, const LLColor4U& a)
{
	// Only affects rgb (not a !)
	return LLColor4U(a.mV[VX] * k, a.mV[VY] * k, a.mV[VZ] * k, a.mV[VW]);
}

LL_INLINE LLColor4U operator%(U8 k, const LLColor4U& a)
{
	// Only affects alpha (not rgb !)
	return LLColor4U(a.mV[VX], a.mV[VY], a.mV[VZ], a.mV[VW] * k);
}

LL_INLINE LLColor4U operator%(const LLColor4U& a, U8 k)
{
	// Only affects alpha (not rgb !)
	return LLColor4U(a.mV[VX], a.mV[VY], a.mV[VZ], a.mV[VW] * k);
}

LL_INLINE LLColor4U operator=(const LLColor3& a)
{
	mV[VX] = a.mV[VX];
	mV[VY] = a.mV[VY];
	mV[VZ] = a.mV[VZ];

	// Converting from an rgb sets a=1 (opaque)
	mV[VW] = 255;
	return *this;
}
#endif

LL_INLINE bool operator==(const LLColor4U& a, const LLColor4U& b)
{
	return (a.mV[VX] == b.mV[VX] && a.mV[VY] == b.mV[VY] &&
			a.mV[VZ] == b.mV[VZ] && a.mV[VW] == b.mV[VW]);
}

LL_INLINE bool operator!=(const LLColor4U& a, const LLColor4U& b)
{
	return (a.mV[VX] != b.mV[VX] || a.mV[VY] != b.mV[VY] ||
			a.mV[VZ] != b.mV[VZ] || a.mV[VW] != b.mV[VW]);
}

LL_INLINE const LLColor4U& operator+=(LLColor4U& a, const LLColor4U& b)
{
	a.mV[VX] += b.mV[VX];
	a.mV[VY] += b.mV[VY];
	a.mV[VZ] += b.mV[VZ];
	a.mV[VW] += b.mV[VW];
	return a;
}

LL_INLINE const LLColor4U& operator-=(LLColor4U& a, const LLColor4U& b)
{
	a.mV[VX] -= b.mV[VX];
	a.mV[VY] -= b.mV[VY];
	a.mV[VZ] -= b.mV[VZ];
	a.mV[VW] -= b.mV[VW];
	return a;
}

LL_INLINE const LLColor4U& operator*=(LLColor4U& a, U8 k)
{
	// Only affects rgb (not a !)
	a.mV[VX] *= k;
	a.mV[VY] *= k;
	a.mV[VZ] *= k;
	return a;
}

LL_INLINE const LLColor4U& operator%=(LLColor4U& a, U8 k)
{
	// Only affects alpha (not rgb !)
	a.mV[VW] *= k;
	return a;
}

// Non-member functions

// Returns distance between a and b
LL_INLINE F32 distVec(const LLColor4U& a, const LLColor4U& b)
{
	LLColor4U vec = a - b;
	return vec.length();
}

// Returns distance squared between a and b
LL_INLINE F32 distVec_squared(const LLColor4U& a, const LLColor4U& b)
{
	LLColor4U vec = a - b;
	return vec.lengthSquared();
}

#endif	// LL_V4COLORU_H
