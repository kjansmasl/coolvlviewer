/**
 * @file llcolor3.h
 * @brief LLColor3 class header file.
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

#ifndef LL_V3COLOR_H
#define LL_V3COLOR_H

#include "llmath.h"
#include "llsd.h"

class LLColor4;
class LLVector4;

constexpr U32 LENGTHOFCOLOR3 = 3;

class LLColor3
{
public:
	LL_INLINE LLColor3() noexcept			{ mV[0] = mV[1] = mV[2] = 0.f; }

	LL_INLINE LLColor3(F32 r, F32 g, F32 b) noexcept
	{
		mV[VX] = r;
		mV[VY] = g;
		mV[VZ] = b;
	}

	LL_INLINE LLColor3(const F32* vec) noexcept
	{
		mV[VX] = vec[VX];
		mV[VY] = vec[VY];
		mV[VZ] = vec[VZ];
	}

	explicit LLColor3(const LLColor4& a) noexcept;
	explicit LLColor3(const LLVector4& a) noexcept;

	LL_INLINE LLColor3(const LLSD& sd)		{ setValue(sd); }

	// Allow the use of the default C++11 move constructor and assignation
	LLColor3(LLColor3&& other) noexcept = default;
	LLColor3& operator=(LLColor3&& other) noexcept = default;

	LLColor3(const LLColor3& other) = default;
	LLColor3& operator=(const LLColor3& other) = default;

	// Takes a string of format "RRGGBB" where RR is hex 00..FF
	LLColor3(const char* color_string) noexcept;

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

	void setHSL(F32 hue, F32 saturation, F32 luminance);
	void calcHSL(F32* hue, F32* saturation, F32* luminance) const;

	LL_INLINE const LLColor3& setToBlack()
	{
		mV[0] = mV[1] = mV[2] = 0.f;
		return *this;
	}

	LL_INLINE const LLColor3& setToWhite()
	{
		mV[0] = mV[1] = mV[2] = 1.f;
		return *this;
	}

	LL_INLINE const LLColor3& set(F32 r, F32 g, F32 b)
	{
		mV[0] = r;
		mV[1] = g;
		mV[2] = b;
		return *this;
	}

	LL_INLINE const LLColor3& set(const LLColor3& vec)
	{
		mV[0] = vec.mV[0];
		mV[1] = vec.mV[1];
		mV[2] = vec.mV[2];
		return *this;
	}

	LL_INLINE const LLColor3& set(const F32* vec)
	{
		mV[0] = vec[0];
		mV[1] = vec[1];
		mV[2] = vec[2];
		return *this;
	}

	// Sets from a vector of unknown type and size; may leave some data
	// unmodified.
	template<typename T>
	LL_INLINE const LLColor3& set(const std::vector<T>& v)
	{
		for (S32 i = 0, count = llmin((S32)v.size(), 3); i < count; ++i)
		{
			mV[i] = v[i];
		}
		return *this;
	}

	// Writes to a vector of unknown type and size; may leave some data
	// unmodified.
	template<typename T>
	LL_INLINE const LLColor3& write(std::vector<T>& v) const
	{
		for (S32 i = 0, count = llmin((S32)v.size(), 3); i < count; ++i)
		{
			v[i] = mV[i];
		}
		return *this;
	}

	// Returns magnitude of LLColor3
	LL_INLINE F32 length() const
	{
		return sqrtf(mV[0] * mV[0] + mV[1] * mV[1] + mV[2] * mV[2]);
	}

	// Returns magnitude squared of LLColor3
	LL_INLINE F32 lengthSquared() const
	{
		return mV[0] * mV[0] + mV[1] * mV[1] + mV[2] * mV[2];
	}

	// Normalizes and returns the magnitude of LLColor3
	LL_INLINE F32 normalize()
	{
		F32 mag = sqrtf(mV[0] * mV[0] + mV[1] * mV[1] + mV[2] * mV[2]);
		if (mag)
		{
			F32 oomag = 1.f / mag;
			mV[0] *= oomag;
			mV[1] *= oomag;
			mV[2] *= oomag;
		}
		return mag;
	}

	// Returns brightness of LLColor3
	LL_INLINE F32 brightness() const
	{
		constexpr F32 scaler = 1.f / 3.f;
		return (mV[0] + mV[1] + mV[2]) * scaler;
	}

	LL_INLINE LLColor3 divide(const LLColor3& col2)
	{
		return LLColor3(mV[0] / col2.mV[0], mV[1] / col2.mV[1],
						mV[2] / col2.mV[2]);
	}

	LL_INLINE LLColor3 color_norm()
	{
		F32 k = 1.f / length();
		return LLColor3(mV[0] * k, mV[1] * k, mV[2] * k);
	}

	const LLColor3& operator=(const LLColor4& a) noexcept;
	// Prints a
	friend std::ostream& operator<<(std::ostream& s, const LLColor3& a);
	// Returns vector a + b
	friend LLColor3 operator+(const LLColor3& a, const LLColor3& b);
	// Returns vector a minus b
	friend LLColor3 operator-(const LLColor3& a, const LLColor3& b);
	// Returns vector a + b
	friend const LLColor3& operator+=(LLColor3& a, const LLColor3& b);
	// Returns vector a minus b
	friend const LLColor3& operator-=(LLColor3& a, const LLColor3& b);
	// Returns component wise a * b
	friend const LLColor3& operator*=(LLColor3& a, const LLColor3& b);

	// Returns component wise a * b
	friend LLColor3 operator*(const LLColor3& a, const LLColor3& b);
	// Returns a times scaler k
	friend LLColor3 operator*(const LLColor3& a, F32 k);
	// Returns a times scaler k
	friend LLColor3 operator*(F32 k, const LLColor3& a);

	// Returns a == b
	friend bool operator==(const LLColor3& a, const LLColor3& b);
	// Returns a != b
	friend bool operator!=(const LLColor3& a, const LLColor3& b);

	// Returns a times scaler k
	friend const LLColor3& operator*=(LLColor3& a, F32 k);

	// Returns vector 1-rgb (inverse)
	friend LLColor3 operator-(const LLColor3& a);

	// Clamps the color...
	LL_INLINE void clamp()
	{
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
	}

public:
	F32 mV[LENGTHOFCOLOR3];

	static LLColor3 white;
	static LLColor3 black;
	static LLColor3 grey;
};

// Non-member functions

LLColor3 lerp(const LLColor3& a, const LLColor3& b, F32 u);

// Returns distance between a and b:
F32 distVec(const LLColor3& a, const LLColor3& b);

// Returns distance squared between a and b
F32 distVec_squared(const LLColor3& a, const LLColor3& b);

LL_INLINE LLColor3 operator+(const LLColor3& a, const LLColor3& b)
{
	return LLColor3(a.mV[0] + b.mV[0], a.mV[1] + b.mV[1], a.mV[2] + b.mV[2]);
}

LL_INLINE LLColor3 operator-(const LLColor3& a, const LLColor3& b)
{
	return LLColor3(a.mV[0] - b.mV[0], a.mV[1] - b.mV[1], a.mV[2] - b.mV[2]);
}

LL_INLINE LLColor3 operator*(const LLColor3& a, const LLColor3& b)
{
	return LLColor3(a.mV[0] * b.mV[0], a.mV[1] * b.mV[1], a.mV[2] * b.mV[2]);
}

LL_INLINE LLColor3 operator*(const LLColor3& a, F32 k)
{
	return LLColor3(a.mV[0] * k, a.mV[1] * k, a.mV[2] * k);
}

LL_INLINE LLColor3 operator*(F32 k, const LLColor3& a)
{
	return LLColor3(a.mV[0] * k, a.mV[1] * k, a.mV[2] * k);
}

LL_INLINE bool operator==(const LLColor3& a, const LLColor3& b)
{
	return a.mV[0] == b.mV[0] && a.mV[1] == b.mV[1] && a.mV[2] == b.mV[2];
}

LL_INLINE bool operator!=(const LLColor3& a, const LLColor3& b)
{
	return a.mV[0] != b.mV[0] || a.mV[1] != b.mV[1] || a.mV[2] != b.mV[2];
}

LL_INLINE const LLColor3& operator*=(LLColor3& a, const LLColor3& b)
{
	a.mV[0] *= b.mV[0];
	a.mV[1] *= b.mV[1];
	a.mV[2] *= b.mV[2];
	return a;
}

LL_INLINE const LLColor3& operator+=(LLColor3& a, const LLColor3& b)
{
	a.mV[0] += b.mV[0];
	a.mV[1] += b.mV[1];
	a.mV[2] += b.mV[2];
	return a;
}

LL_INLINE const LLColor3& operator-=(LLColor3& a, const LLColor3& b)
{
	a.mV[0] -= b.mV[0];
	a.mV[1] -= b.mV[1];
	a.mV[2] -= b.mV[2];
	return a;
}

LL_INLINE const LLColor3& operator*=(LLColor3& a, F32 k)
{
	a.mV[0] *= k;
	a.mV[1] *= k;
	a.mV[2] *= k;
	return a;
}

LL_INLINE LLColor3 operator-(const LLColor3& a)
{
	return LLColor3(1.f - a.mV[0], 1.f - a.mV[1], 1.f - a.mV[2]);
}

// Non-member functions

LL_INLINE F32 distVec(const LLColor3& a, const LLColor3& b)
{
	F32 x = a.mV[0] - b.mV[0];
	F32 y = a.mV[1] - b.mV[1];
	F32 z = a.mV[2] - b.mV[2];
	return sqrtf(x * x + y * y + z * z);
}

LL_INLINE F32 distVec_squared(const LLColor3& a, const LLColor3& b)
{
	F32 x = a.mV[0] - b.mV[0];
	F32 y = a.mV[1] - b.mV[1];
	F32 z = a.mV[2] - b.mV[2];
	return x * x + y * y + z * z;
}

LL_INLINE LLColor3 lerp(const LLColor3& a, const LLColor3& b, F32 u)
{
	return LLColor3(a.mV[VX] + (b.mV[VX] - a.mV[VX]) * u,
					a.mV[VY] + (b.mV[VY] - a.mV[VY]) * u,
					a.mV[VZ] + (b.mV[VZ] - a.mV[VZ]) * u);
}

LL_INLINE LLColor3 componentDiv(const LLColor3& left, const LLColor3& right)
{
	return LLColor3(left.mV[0] / right.mV[0],
					left.mV[1] / right.mV[1],
					left.mV[2] / right.mV[2]);
}

LL_INLINE LLColor3 componentMult(const LLColor3& left, const LLColor3& right)
{
	return LLColor3(left.mV[0] * right.mV[0],
					left.mV[1] * right.mV[1],
					left.mV[2] * right.mV[2]);
}

LL_INLINE LLColor3 componentExp(const LLColor3& v)
{
	return LLColor3(expf(v.mV[0]), expf(v.mV[1]), expf(v.mV[2]));
}

LL_INLINE LLColor3 componentPow(const LLColor3& v, F32 exponent)
{
	return LLColor3(powf(v.mV[0], exponent),
					powf(v.mV[1], exponent),
					powf(v.mV[2], exponent));
}

LL_INLINE LLColor3 componentSaturate(const LLColor3& v)
{
	return LLColor3(llmax(llmin(v.mV[0], 1.f), 0.f),
					llmax(llmin(v.mV[1], 1.f), 0.f),
					llmax(llmin(v.mV[2], 1.f), 0.f));
}

LL_INLINE LLColor3 componentSqrt(const LLColor3& v)
{
	return LLColor3(sqrtf(v.mV[0]), sqrtf(v.mV[1]), sqrtf(v.mV[2]));
}

LL_INLINE void componentMultBy(LLColor3& left, const LLColor3& right)
{
	left.mV[0] *= right.mV[0];
	left.mV[1] *= right.mV[1];
	left.mV[2] *= right.mV[2];
}

LL_INLINE LLColor3 colorMix(const LLColor3& left, const LLColor3& right,
							F32 amount)
{
	return left + (right - left) * amount;
}

LL_INLINE LLColor3 smear(F32 val)
{
	return LLColor3(val, val, val);
}

LL_INLINE F32 color_intens(const LLColor3& col)
{
	return col.mV[0] + col.mV[1] + col.mV[2];
}

LL_INLINE F32 color_max(const LLColor3& col)
{
	return llmax(col.mV[0], col.mV[1], col.mV[2]);
}

LL_INLINE F32 color_min(const LLColor3& col)
{
	return llmin(col.mV[0], col.mV[1], col.mV[2]);
}

LL_INLINE const LLColor3 srgbColor3(const LLColor3& a)
{
	return LLColor3(linearToSRGB(a.mV[0]), linearToSRGB(a.mV[1]),
					linearToSRGB(a.mV[2]));
}

LL_INLINE const LLColor3 linearColor3p(const F32* v)
{
	return LLColor3(sRGBtoLinear(v[0]), sRGBtoLinear(v[1]),
					sRGBtoLinear(v[2]));
}

// Avoids a trivial/pointless template and guarantees inlining. HB
#define linearColor3(a) linearColor3p((a).mV)

// Avoids a trivial/pointless template which would require the #inclusion of
// llvector3.h by this header file, and guarantees inlining. HB
#define linearColor3v(a) LLVector3(linearColor3p((a).mV).mV)

#endif
