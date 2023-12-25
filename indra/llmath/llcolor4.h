/**
 * @file llcolor4.h
 * @brief LLColor4 class header file.
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

#ifndef LL_V4COLOR_H
#define LL_V4COLOR_H

#include <string.h>

#include "llerror.h"
#include "llmath.h"
#include "llsd.h"

class LLColor3;
class LLColor4U;
class LLVector4;

//  LLColor4 = |x y z w|

constexpr U32 LENGTHOFCOLOR4 = 4;

// Give plenty of room for additional colors...
constexpr U32 MAX_LENGTH_OF_COLOR_NAME = 15;

class LLColor4
{
protected:
	LOG_CLASS(LLColor4);

public:
	LL_INLINE LLColor4() noexcept
	{
		mV[VX] = mV[VY] = mV[VZ] = 0.f;
		mV[VW] = 1.f;
	}

	LL_INLINE LLColor4(F32 r, F32 g, F32 b) noexcept
	{
		mV[VX] = r;
		mV[VY] = g;
		mV[VZ] = b;
		mV[VW] = 1.f;
	}

	LL_INLINE LLColor4(F32 r, F32 g, F32 b, F32 a) noexcept
	{
		mV[VX] = r;
		mV[VY] = g;
		mV[VZ] = b;
		mV[VW] = a;
	}

	LL_INLINE LLColor4(U32 clr) noexcept
	{
		constexpr F32 SCALE = 1.f / 255.f;
		mV[VX] = (clr & 0xff) * SCALE;
		mV[VY] = ((clr >> 8) & 0xff) * SCALE;
		mV[VZ] = ((clr >> 16) & 0xff) * SCALE;
		mV[VW] = (clr >> 24) * SCALE;
	}

	LL_INLINE LLColor4(const F32* vec) noexcept
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
		mV[VZ] = vec[VZ];
		mV[VW] = vec[VW];
	}

	// Initializes LLColor4 to (vec, a)
	LLColor4(const LLColor3& vec, F32 a = 1.f) noexcept;

	 // "explicit" to avoid automatic conversions

	LL_INLINE explicit LLColor4(const LLSD& sd)
	{
		setValue(sd);
	}

	explicit LLColor4(const LLColor4U& color4u) noexcept;
	explicit LLColor4(const LLVector4& vector4) noexcept;

	// Allow the use of the default C++11 move constructor and assignation
	LLColor4(LLColor4&& other) noexcept = default;
	LLColor4& operator=(LLColor4&& other) noexcept = default;

	LLColor4(const LLColor4& other) = default;
	LLColor4& operator=(const LLColor4& other) = default;

	LL_INLINE LLSD getValue() const
	{
		LLSD ret;
		ret[0] = mV[0];
		ret[1] = mV[1];
		ret[2] = mV[2];
		ret[3] = mV[3];
		return ret;
	}

	void setValue(const LLSD& sd);

	void setHSL(F32 hue, F32 saturation, F32 luminance);
	void calcHSL(F32* hue, F32* saturation, F32* luminance) const;

	LL_INLINE const LLColor4& setToBlack()
	{
		mV[VX] = mV[VY] = mV[VZ] = 0.f;
		mV[VW] = 1.f;
		return *this;
	}

	LL_INLINE const LLColor4& setToWhite()
	{
		mV[VX] = mV[VY] = mV[VZ] = mV[VW] = 1.f;
		return *this;
	}

	LL_INLINE const LLColor4& set(F32 r, F32 g, F32 b, F32 a)
	{
		mV[VX] = r;
		mV[VY] = g;
		mV[VZ] = b;
		mV[VW] = a;
		return *this;
	}

	// Sets color without touching alpha
	LL_INLINE const LLColor4& set(F32 r, F32 g, F32 b)
	{
		mV[VX] = r;
		mV[VY] = g;
		mV[VZ] = b;
		return *this;
	}

	LL_INLINE const LLColor4& set(const LLColor4& vec)
	{
		mV[VX] = vec.mV[VX];
		mV[VY] = vec.mV[VY];
		mV[VZ] = vec.mV[VZ];
		mV[VW] = vec.mV[VW];
		return *this;
	}

	LL_INLINE const LLColor4& set(const F32* vec)
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
		mV[VZ] = vec[VZ];
		mV[VW] = vec[VW];
		return *this;
	}

	LL_INLINE const LLColor4& set(const F64* vec)
	{
		mV[VX] = F32(vec[VX]);
		mV[VY] = F32(vec[VY]);
		mV[VZ] = F32(vec[VZ]);
		mV[VW] = F32(vec[VW]);
		return *this;
	}

	// Sets LLColor4 to LLColor3 vec (no change in alpha)
	const LLColor4& set(const LLColor3& vec);
	// Sets LLColor4 to LLColor3 vec, with alpha specified
	LL_INLINE const LLColor4& set(const LLColor3& vec, F32 a);
	// Sets LLColor4 to color4u, rescaled.
	const LLColor4& set(const LLColor4U& color4u);

	LL_INLINE const LLColor4& setAlpha(F32 a)
	{
		mV[VW] = a;
		return *this;
	}

	// Sets from a vector of unknown type and size; may leave some data
	// unmodified.
	template<typename T>
	LL_INLINE const LLColor4& set(const std::vector<T>& v)
	{
		for (S32 i = 0, count = llmin((S32)v.size(), 4); i < count; ++i)
		{
			mV[i] = v[i];
		}
		return *this;
	}

	// Writes to a vector of unknown type and size; may leave some data
	// unmodified.
	template<typename T>
	LL_INLINE const LLColor4& write(std::vector<T>& v) const
	{
		for (S32 i = 0, count = llmin((S32)v.size(), 4); i < count; ++i)
		{
			v[i] = mV[i];
		}
		return *this;
	}

	// Returns magnitude of LLColor4
	LL_INLINE F32 length() const
	{
		return sqrtf(mV[VX] * mV[VX] + mV[VY] * mV[VY] + mV[VZ] * mV[VZ]);
	}

	// Returns magnitude squared of LLColor4, which is faster than length()
	LL_INLINE F32 lengthSquared() const
	{
		return mV[VX] * mV[VX] + mV[VY] * mV[VY] + mV[VZ] * mV[VZ];
	}

	LL_INLINE F32 normalize()
	{
		F32 mag = sqrtf(mV[VX] * mV[VX] + mV[VY] * mV[VY] + mV[VZ] * mV[VZ]);
		if (mag)
		{
			F32 oomag = 1.f / mag;
			mV[VX] *= oomag;
			mV[VY] *= oomag;
			mV[VZ] *= oomag;
		}
		return mag;
	}

	LL_INLINE bool isOpaque()					{ return mV[VALPHA] == 1.f; }

	LL_INLINE F32 operator[](int idx) const		{ return mV[idx]; }
	LL_INLINE F32& operator[](int idx)			{ return mV[idx]; }

	// Assigns vec3 to vec4 and returns vec4
    const LLColor4& operator=(const LLColor3& a);

	bool operator<(const LLColor4& rhs) const;
	friend std::ostream& operator<<(std::ostream& s, const LLColor4& a);

	// Returns vector a + b
	friend LLColor4 operator+(const LLColor4& a, const LLColor4& b);
	// Returns vector a minus b
	friend LLColor4 operator-(const LLColor4& a, const LLColor4& b);
	// Returns component wise a * b
	friend LLColor4 operator*(const LLColor4& a, const LLColor4& b);
	// Returns rgb times scaler k (no alpha change)
	friend LLColor4 operator*(const LLColor4& a, F32 k);
	// Returns rgb divided by scalar k (no alpha change)
	friend LLColor4 operator/(const LLColor4& a, F32 k);
	// Returns rgb times scaler k (no alpha change)
	friend LLColor4 operator*(F32 k, const LLColor4& a);
	// Returns alpha times scaler k (no rgb change)
	friend LLColor4 operator%(const LLColor4& a, F32 k);
	// Returns alpha times scaler k (no rgb change)
	friend LLColor4 operator%(F32 k, const LLColor4& a);

	friend bool operator==(const LLColor4& a, const LLColor4& b);
	friend bool operator!=(const LLColor4& a, const LLColor4& b);

	friend bool operator==(const LLColor4& a, const LLColor3& b);
	friend bool operator!=(const LLColor4& a, const LLColor3& b);

	// Returns vector a + b
	friend const LLColor4& operator+=(LLColor4& a, const LLColor4& b);
	// Returns vector a minus b
	friend const LLColor4& operator-=(LLColor4& a, const LLColor4& b);
	// Returns rgb times scaler k (no alpha change)
	friend const LLColor4& operator*=(LLColor4& a, F32 k);
	// Returns alpha times scaler k (no rgb change)
	friend const LLColor4& operator%=(LLColor4& a, F32 k);
	// Does not multiply alpha (used for lighting) !
	friend const LLColor4& operator*=(LLColor4& a, const LLColor4& b);

	// Conversion
	operator LLColor4U() const;

	LL_INLINE void clamp();

	static bool parseColor(const std::string& buf, LLColor4* color);
	static bool parseColor4(const std::string& buf, LLColor4* color);

public:
	F32 mV[LENGTHOFCOLOR4];

	// Basic color values.
	static LLColor4 red;
	static LLColor4 green;
	static LLColor4 blue;
	static LLColor4 black;
	static LLColor4 white;
	static LLColor4 yellow;
	static LLColor4 magenta;
	static LLColor4 cyan;
	static LLColor4 smoke;
	static LLColor4 grey;
	static LLColor4 orange;
	static LLColor4 purple;
	static LLColor4 pink;
	static LLColor4 transparent;

	// Extra color values.
	static LLColor4 grey1;
	static LLColor4 grey2;
	static LLColor4 grey3;
	static LLColor4 grey4;
	static LLColor4 grey5;

	static LLColor4 red0;
	static LLColor4 red1;
	static LLColor4 red2;
	static LLColor4 red3;
	static LLColor4 red4;
	static LLColor4 red5;

	static LLColor4 green0;
	static LLColor4 green1;
	static LLColor4 green2;
	static LLColor4 green3;
	static LLColor4 green4;
	static LLColor4 green5;
	static LLColor4 green6;
	static LLColor4 green7;
	static LLColor4 green8;
	static LLColor4 green9;

	static LLColor4 blue0;
	static LLColor4 blue1;
	static LLColor4 blue2;
	static LLColor4 blue3;
	static LLColor4 blue4;
	static LLColor4 blue5;
	static LLColor4 blue6;
	static LLColor4 blue7;

	static LLColor4 yellow1;
	static LLColor4 yellow2;
	static LLColor4 yellow3;
	static LLColor4 yellow4;
	static LLColor4 yellow5;
	static LLColor4 yellow6;
	static LLColor4 yellow7;
	static LLColor4 yellow8;
	static LLColor4 yellow9;

	static LLColor4 orange1;
	static LLColor4 orange2;
	static LLColor4 orange3;
	static LLColor4 orange4;
	static LLColor4 orange5;
	static LLColor4 orange6;

	static LLColor4 magenta1;
	static LLColor4 magenta2;
	static LLColor4 magenta3;
	static LLColor4 magenta4;

	static LLColor4 purple1;
	static LLColor4 purple2;
	static LLColor4 purple3;
	static LLColor4 purple4;
	static LLColor4 purple5;
	static LLColor4 purple6;

	static LLColor4 pink1;
	static LLColor4 pink2;

	static LLColor4 cyan1;
	static LLColor4 cyan2;
	static LLColor4 cyan3;
	static LLColor4 cyan4;
	static LLColor4 cyan5;
	static LLColor4 cyan6;
};

// LLColor4 Operators

LL_INLINE LLColor4 operator+(const LLColor4& a, const LLColor4& b)
{
	return LLColor4(a.mV[VX] + b.mV[VX], a.mV[VY] + b.mV[VY],
					a.mV[VZ] + b.mV[VZ], a.mV[VW] + b.mV[VW]);
}

LL_INLINE LLColor4 operator-(const LLColor4& a, const LLColor4& b)
{
	return LLColor4(a.mV[VX] - b.mV[VX], a.mV[VY] - b.mV[VY],
					a.mV[VZ] - b.mV[VZ], a.mV[VW] - b.mV[VW]);
}

LL_INLINE LLColor4  operator*(const LLColor4& a, const LLColor4& b)
{
	return LLColor4(a.mV[VX] * b.mV[VX], a.mV[VY] * b.mV[VY],
					a.mV[VZ] * b.mV[VZ], a.mV[VW] * b.mV[VW]);
}

// Only affects rgb (not a !)
LL_INLINE LLColor4 operator*(const LLColor4& a, F32 k)
{
	return LLColor4(a.mV[VX] * k, a.mV[VY] * k, a.mV[VZ] * k, a.mV[VW]);
}

// Only affects rgb (not a !)
LL_INLINE LLColor4 operator/(const LLColor4& a, F32 k)
{
	const F32 ik = 1.f / k;
	return LLColor4(a.mV[VX] * ik, a.mV[VY] * ik, a.mV[VZ] * ik, a.mV[VW]);
}

LL_INLINE LLColor4 operator*(F32 k, const LLColor4& a)
{
	// Only affects rgb (not a !)
	return LLColor4(a.mV[VX] * k, a.mV[VY] * k, a.mV[VZ] * k, a.mV[VW]);
}

LL_INLINE LLColor4 operator%(F32 k, const LLColor4& a)
{
	// Only affects alpha (not rgb!)
	return LLColor4(a.mV[VX], a.mV[VY], a.mV[VZ], a.mV[VW] * k);
}

// Only affects alpha (not rgb !)
LL_INLINE LLColor4 operator%(const LLColor4& a, F32 k)
{
	return LLColor4(a.mV[VX], a.mV[VY], a.mV[VZ], a.mV[VW] * k);
}

LL_INLINE bool operator==(const LLColor4& a, const LLColor4& b)
{
	return a.mV[VX] == b.mV[VX] && a.mV[VY] == b.mV[VY] &&
		   a.mV[VZ] == b.mV[VZ] && a.mV[VW] == b.mV[VW];
}

LL_INLINE bool operator!=(const LLColor4& a, const LLColor4& b)
{
	return a.mV[VX] != b.mV[VX] || a.mV[VY] != b.mV[VY] ||
		   a.mV[VZ] != b.mV[VZ] || a.mV[VW] != b.mV[VW];
}

LL_INLINE const LLColor4& operator+=(LLColor4& a, const LLColor4& b)
{
	a.mV[VX] += b.mV[VX];
	a.mV[VY] += b.mV[VY];
	a.mV[VZ] += b.mV[VZ];
	a.mV[VW] += b.mV[VW];
	return a;
}

LL_INLINE const LLColor4& operator-=(LLColor4& a, const LLColor4& b)
{
	a.mV[VX] -= b.mV[VX];
	a.mV[VY] -= b.mV[VY];
	a.mV[VZ] -= b.mV[VZ];
	a.mV[VW] -= b.mV[VW];
	return a;
}

// Only affects rgb (not a !)
LL_INLINE const LLColor4& operator*=(LLColor4& a, F32 k)
{
	a.mV[VX] *= k;
	a.mV[VY] *= k;
	a.mV[VZ] *= k;
	return a;
}

// Only affects rgb (not a !)
LL_INLINE const LLColor4& operator*=(LLColor4& a, const LLColor4& b)
{
	a.mV[VX] *= b.mV[VX];
	a.mV[VY] *= b.mV[VY];
	a.mV[VZ] *= b.mV[VZ];
	return a;
}

LL_INLINE const LLColor4& operator%=(LLColor4& a, F32 k)
{
	// Only affects alpha (not rgb!)
	a.mV[VW] *= k;
	return a;
}

// Non-member functions

LL_INLINE F32 distVec(const LLColor4& a, const LLColor4& b)
{
	LLColor4 vec = a - b;
	return vec.length();
}

LL_INLINE F32 distVec_squared(const LLColor4& a, const LLColor4& b)
{
	LLColor4 vec = a - b;
	return vec.lengthSquared();
}

LL_INLINE LLColor4 lerp(const LLColor4& a, const LLColor4& b, F32 u)
{
	return LLColor4(a.mV[VX] + (b.mV[VX] - a.mV[VX]) * u,
					a.mV[VY] + (b.mV[VY] - a.mV[VY]) * u,
					a.mV[VZ] + (b.mV[VZ] - a.mV[VZ]) * u,
					a.mV[VW] + (b.mV[VW] - a.mV[VW]) * u);
}

LL_INLINE bool LLColor4::operator<(const LLColor4& rhs) const
{
	if (mV[0] != rhs.mV[0])
	{
		return mV[0] < rhs.mV[0];
	}
	if (mV[1] != rhs.mV[1])
	{
		return mV[1] < rhs.mV[1];
	}
	if (mV[2] != rhs.mV[2])
	{
		return mV[2] < rhs.mV[2];
	}

	return mV[3] < rhs.mV[3];
}

LL_INLINE void LLColor4::clamp()
{
	// Clamp the color...
	if (mV[0] < 0.f)
	{
		mV[0] = 0.f;
	}
	else if (mV[0] > 1.f)
	{
		mV[0] = 1.f;
	}
	if (mV[1] < 0.f)
	{
		mV[1] = 0.f;
	}
	else if (mV[1] > 1.f)
	{
		mV[1] = 1.f;
	}
	if (mV[2] < 0.f)
	{
		mV[2] = 0.f;
	}
	else if (mV[2] > 1.f)
	{
		mV[2] = 1.f;
	}
	if (mV[3] < 0.f)
	{
		mV[3] = 0.f;
	}
	else if (mV[3] > 1.f)
	{
		mV[3] = 1.f;
	}
}

LL_INLINE F32 color_max(const LLColor4& col)
{
	return llmax(col.mV[0], col.mV[1], col.mV[2]);
}

LL_INLINE const LLColor4 srgbColor4(const LLColor4& a)
{
	return LLColor4(linearToSRGB(a.mV[0]), linearToSRGB(a.mV[1]),
					linearToSRGB(a.mV[2]), a.mV[3]);
}

LL_INLINE const LLColor4 linearColor4(const LLColor4& a)
{
	return LLColor4(sRGBtoLinear(a.mV[0]), sRGBtoLinear(a.mV[1]),
					sRGBtoLinear(a.mV[2]), a.mV[3]);
}

// Returns distance between a and b
F32 distVec(const LLColor4& a, const LLColor4& b);
// Returns distance squared between a and b
F32 distVec_squared(const LLColor4& a, const LLColor4& b);

LLColor3 vec4to3(const LLColor4& vec);
LLColor4 vec3to4(const LLColor3& vec);
LLColor4 lerp(const LLColor4& a, const LLColor4& b, F32 u);

#endif
