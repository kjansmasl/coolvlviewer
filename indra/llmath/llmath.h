/**
 * @file llmath.h
 * @brief Useful math constants and macros.
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

#ifndef LLMATH_H
#define LLMATH_H

#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

#if !LL_WINDOWS
# include <stdint.h>
#endif

#include "llmemory.h"	// Also includes appropriate intrinsics headers. HB
// llsd.cpp uses llisnan() and llsdutil.cpp uses is_approx_equal_fraction(),
// so these were moved from llmath.h into llcommonmath.h so that llcommon does
// not depend on llmath. HB
#include "llcommonmath.h"

constexpr F32 GRAVITY = -9.8f;

// Mathematical constants
constexpr F32 F_PI					= 3.1415926535897932384626433832795f;
constexpr F32 F_TWO_PI				= 6.283185307179586476925286766559f;
constexpr F32 F_PI_BY_TWO			= 1.5707963267948966192313216916398f;
constexpr F32 F_SQRT_TWO_PI			= 2.506628274631000502415765284811f;
constexpr F32 F_E					= 2.71828182845904523536f;
constexpr F32 F_SQRT2				= 1.4142135623730950488016887242097f;
constexpr F32 F_SQRT3				= 1.73205080756888288657986402541f;
constexpr F32 OO_SQRT2				= 0.7071067811865475244008443621049f;
constexpr F32 DEG_TO_RAD			= 0.017453292519943295769236907684886f;
constexpr F32 RAD_TO_DEG			= 57.295779513082320876798154814105f;
constexpr F32 F_LN10				= 2.3025850929940456840179914546844f;
constexpr F32 OO_LN10				= 0.43429448190325182765112891891661f;
constexpr F32 F_LN2					= 0.69314718056f;
constexpr F32 OO_LN2				= 1.4426950408889634073599246810019f;
constexpr F32 F_APPROXIMATELY_ZERO	= 0.00001f;
constexpr F32 F_ALMOST_ZERO			= 0.0001f;
constexpr F32 F_ALMOST_ONE			= 1.f - F_ALMOST_ZERO;

// Sets the gimballock threshold 0.025 away from +/-90 degrees.
// Formula: GIMBAL_THRESHOLD = sinf(DEG_TO_RAD * gimbal_threshold_angle);
constexpr F32 GIMBAL_THRESHOLD		= 0.000436f;

// BUG: Eliminate in favor of F_APPROXIMATELY_ZERO above ?
constexpr F32 FP_MAG_THRESHOLD		= 0.0000001f;

// *TODO: replace with logic like is_approx_equal
LL_INLINE bool is_approx_zero(F32 f)
{
	return -F_APPROXIMATELY_ZERO < f && f < F_APPROXIMATELY_ZERO;
}

// These functions work by interpreting sign+exp+mantissa as an unsigned
// integer.
// For example:
// x = <sign>1 <exponent>00000010 <mantissa>00000000000000000000000
// y = <sign>1 <exponent>00000001 <mantissa>11111111111111111111111
//
// interpreted as ints =
// x = 10000001000000000000000000000000
// y = 10000000111111111111111111111111
// which is clearly a different of 1 in the least significant bit.
// Values with the same exponent can be trivially shown to work.
//
// WARNING: Denormals of opposite sign do not work
// x = <sign>1 <exponent>00000000 <mantissa>00000000000000000000001
// y = <sign>0 <exponent>00000000 <mantissa>00000000000000000000001
// Although these values differ by 2 in the LSB, the sign bit makes the int
// comparison fail.
//
// WARNING: NaNs can compare equal
// There is no special treatment of exceptional values like NaNs
//
// WARNING: Infinity is comparable with F32_MAX and negative infinity is
// comparable with F32_MIN

LL_INLINE bool is_zero(F32 x)
{
	return (*(U32*)(&x) & 0x7fffffff) == 0;
}

LL_INLINE bool is_approx_equal(F32 x, F32 y)
{
	constexpr S32 COMPARE_MANTISSA_UP_TO_BIT = 0x02;
	return (std::abs((S32)((U32&)x - (U32&)y)) < COMPARE_MANTISSA_UP_TO_BIT);
}

LL_INLINE bool is_approx_equal(F64 x, F64 y)
{
	constexpr S64 COMPARE_MANTISSA_UP_TO_BIT = 0x02;
	return (std::abs((S32)((U64&)x - (U64&)y)) < COMPARE_MANTISSA_UP_TO_BIT);
}

LL_INLINE S32 lltrunc(F32 f)
{
	return (S32)f;
}

LL_INLINE S32 lltrunc(F64 f)
{
	return (S32)f;
}

LL_INLINE S32 llfloor(F32 f)
{
	return (S32)floorf(f);
}

LL_INLINE S32 llceil(F32 f)
{
	// This could probably be optimized, but this works.
	return (S32)ceilf(f);
}

// Does an arithmetic round (0.5 always rounds up)
LL_INLINE S32 ll_round(F32 val)
{
	return llfloor(val + 0.5f);
}

LL_INLINE F32 ll_round(F32 val, F32 nearest)
{
	return F32(floorf(val * (1.f / nearest) + 0.5f)) * nearest;
}

LL_INLINE F64 ll_round(F64 val, F64 nearest)
{
	return F64(floor(val * (1.0 / nearest) + 0.5)) * nearest;
}

LL_INLINE S32 ll_roundp(F32 val)
{
	return val + 0.5f;
}

LL_INLINE S32 ll_roundp(F64 val)
{
	return val + 0.5;
}

LL_INLINE F32 snap_to_sig_figs(F32 foo, S32 sig_figs)
{
	// compute the power of ten
	F32 bar = 1.f;
	for (S32 i = 0; i < sig_figs; ++i)
	{
		bar *= 10.f;
	}

#if 0	// The ll_round() implementation sucks. Do not use it.
	F32 new_foo = (F32)ll_round(foo * bar);
#else
	F32 sign = (foo > 0.f) ? 1.f : -1.f;
	F32 new_foo = F32(S64(foo * bar + sign * 0.5f));
	new_foo /= bar;
#endif
	return new_foo;
}

LL_INLINE F32 lerp(F32 a, F32 b, F32 u)
{
	return a + (b - a) * u;
}

LL_INLINE F32 ramp(F32 x, F32 a, F32 b)
{
	return a == b ? 0.f : (a - x) / (a - b);
}

LL_INLINE F32 rescale(F32 x, F32 x1, F32 x2, F32 y1, F32 y2)
{
	return lerp(y1, y2, ramp(x, x1, x2));
}

LL_INLINE F32 clamp_rescale(F32 x, F32 x1, F32 x2, F32 y1, F32 y2)
{
	if (y1 < y2)
	{
		return llclamp(rescale(x, x1, x2, y1, y2), y1, y2);
	}
	return llclamp(rescale(x, x1, x2, y1, y2), y2, y1);
}

LL_INLINE F32 cubic_step(F32 x)
{
	x = llclampf(x);

	return (x * x) * (3.f - 2.f * x);
}

// SDK - Renamed this to get_lower_power_two, since this is what this actually
// does.
LL_INLINE U32 get_lower_power_two(U32 val, U32 max_power_two)
{
	if (!max_power_two)
	{
		max_power_two = 1 << 31;
	}
	if (max_power_two & (max_power_two - 1))
	{
		return 0;
	}

	for ( ; val < max_power_two; max_power_two >>= 1) ;

	return max_power_two;
}

// Calculate next highest power of two, limited by max_power_two. This is taken
// from a brilliant little code snipped on:
// http://acius2.blogspot.fr/2007/11/calculating-next-power-of-2.html
// Basically we convert the binary to a solid string of 1's with the same
// number of digits, then add one.  We subtract 1 initially to handle the case
// where the number passed in is actually a power of two.
// WARNING: this only works with 32 bits integers.
LL_INLINE U32 get_next_power_two(U32 val, U32 max_power_two)
{
	if (!max_power_two)
	{
		max_power_two = 1 << 31;
	}

	if (val >= max_power_two)
	{
		return max_power_two;
	}

	--val;
	val = (val >> 1) | val;
	val = (val >> 2) | val;
	val = (val >> 4) | val;
	val = (val >> 8) | val;
	val = (val >> 16) | val;

	return ++val;
}

// Get the gaussian value given the linear distance from axis x and guassian
// value o
LL_INLINE F32 llgaussian(F32 x, F32 o)
{
	return 1.f / (F_SQRT_TWO_PI * o) * powf(F_E, - (x * x) / (2 * o * o));
}

// This converts a linear value to a SRGB non-linear value. Useful for gamma
// correction and such.
LL_INLINE F32 linearToSRGB(F32 val)
{
	if (val < 0.0031308f)
	{
		return val * 12.92f;
	}

	return 1.055f * powf(val, 1.f / 2.4f) - 0.055f;
}

// This converts a linear value to a SRGB non-linear value. Useful for gamma
// correction and such.
LL_INLINE F32 sRGBtoLinear(F32 val)
{
	if (val < 0.04045f)
	{
		constexpr F32 k1 = 1.f / 12.92f;
		return val * k1;
	}

	constexpr F32 k2 = 1.f / 1.055f;
	return powf((val + 0.055f) * k2, 2.4f);
}

///////////////////////////////////////////////////////////////////////////////
// Fast exp
// Implementation of fast expf() approximation (from a paper by Nicol N.
// Schraudolph: http://www.inf.ethz.ch/~schraudo/pubs/exp.pdf

#ifndef LL_BIG_ENDIAN
# error Unknown endianness. Did you omit to include llpreprocessor.h ?
#endif

static union
{
	double d;
	struct
	{
#if LL_BIG_ENDIAN
		S32 i, j;
#else
		S32 j, i;
#endif
	} n;
} LLECO; // Not sure what the name means

#define LL_EXP_A (1048576 * OO_LN2) // Use 1512775 for integer
#define LL_EXP_C (60801)			// This value of C good for -4 < y < 4

#define LL_FAST_EXP(y) (LLECO.n.i = ll_round(F32(LL_EXP_A*(y))) + (1072693248 - LL_EXP_C), LLECO.d)

LL_INLINE F32 llfastpow(F32 x, F32 y)
{
	return (F32)(LL_FAST_EXP(y * logf(x)));
}

///////////////////////////////////////////////////////////////////////////////
// Include the simd math headers

#if LL_GNUC
// gcc v5.0+ spews stupid warnings about "maybe uninitialized" variables
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include "llsimdtypes.h"
#include "llquaternion.h"
#include "llvector4logical.h"
#include "llvector4a.h"
#include "llmatrix3a.h"
#include "llmatrix3.h"
#include "llquaternion2.h"
#define LL_INL_INCLUDE
#include "llvector4a.inl"
#include "llmatrix3a.inl"
#include "llquaternion2.inl"
#undef LL_INL_INCLUDE

#if LL_GNUC
# pragma GCC diagnostic pop
#endif

#endif	// LLMATH_H
