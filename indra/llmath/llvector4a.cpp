/**
 * @file llvector4a.cpp
 * @brief SIMD vector implementation
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "llmath.h"
#include "llmemory.h"
#include "llquantize.h"

extern const LLQuad F_ZERO_4A = { 0.f, 0.f, 0.f, 0.f };
extern const LLQuad F_APPROXIMATELY_ZERO_4A =
{
	F_APPROXIMATELY_ZERO,
	F_APPROXIMATELY_ZERO,
	F_APPROXIMATELY_ZERO,
	F_APPROXIMATELY_ZERO
};

extern const LLVector4a LL_V4A_ZERO = reinterpret_cast<const LLVector4a&> (F_ZERO_4A);
extern const LLVector4a LL_V4A_EPSILON = reinterpret_cast<const LLVector4a&> (F_APPROXIMATELY_ZERO_4A);

//static
void LLVector4a::memcpyNonAliased16(F32* __restrict dst,
									const F32* __restrict src,
									size_t bytes)
{
	ll_memcpy_nonaliased_aligned_16((char*)dst, (char*)src, bytes);
}

void LLVector4a::setRotated(const LLRotation& rot, const LLVector4a& vec)
{
	const LLVector4a col0 = rot.getColumn(0);
	const LLVector4a col1 = rot.getColumn(1);
	const LLVector4a col2 = rot.getColumn(2);

	LLVector4a result = _mm_load_ss(vec.getF32ptr());
	result.splat<0>(result);
	result.mul(col0);

	{
		LLVector4a yyyy = _mm_load_ss(vec.getF32ptr() +  1);
		yyyy.splat<0>(yyyy);
		yyyy.mul(col1);
		result.add(yyyy);
	}

	{
		LLVector4a zzzz = _mm_load_ss(vec.getF32ptr() +  2);
		zzzz.splat<0>(zzzz);
		zzzz.mul(col2);
		result.add(zzzz);
	}

	*this = result;
}

void LLVector4a::setRotated(const LLQuaternion2& quat, const LLVector4a& vec)
{
	const LLVector4a& quatVec = quat.getVector4a();
	LLVector4a temp; temp.setCross3(quatVec, vec);
	temp.add(temp);

	const LLVector4a realPart(quatVec.getScalarAt<3>());
	LLVector4a tempTimesReal; tempTimesReal.setMul(temp, realPart);

	mQ = vec;
	add(tempTimesReal);

	LLVector4a imagCrossTemp; imagCrossTemp.setCross3(quatVec, temp);
	add(imagCrossTemp);
}

void LLVector4a::quantize8(const LLVector4a& low, const LLVector4a& high)
{
	LLVector4a val(mQ);
	LLVector4a delta; delta.setSub(high, low);

	{
		val.clamp(low, high);
		val.sub(low);

		// 8-bit quantization means we can do with just 12 bits of reciprocal accuracy
		const LLVector4a oneOverDelta = _mm_rcp_ps(delta.mQ);
// 		{
// 			thread_local alignas(16) const F32 F_TWO_4A[4] = { 2.f, 2.f, 2.f, 2.f };
// 			LLVector4a two; two.load4a(F_TWO_4A);
//
// 			// Here we use _mm_rcp_ps plus one round of newton-raphson
// 			// We wish to find 'x' such that x = 1/delta
// 			// As a first approximation, we take x0 = _mm_rcp_ps(delta)
// 			// Then x1 = 2 * x0 - a * x0^2 or x1 = x0 * (2 - a * x0)
// 			// See Intel AP-803 http://ompf.org/!/Intel_application_note_AP-803.pdf
// 			const LLVector4a recipApprox = _mm_rcp_ps(delta.mQ);
// 			oneOverDelta.setMul(delta, recipApprox);
// 			oneOverDelta.setSub(two, oneOverDelta);
// 			oneOverDelta.mul(recipApprox);
// 		}

		val.mul(oneOverDelta);
		val.mul(*reinterpret_cast<const LLVector4a*>(F_U8MAX_4A));
	}

	val = _mm_cvtepi32_ps(_mm_cvtps_epi32(val.mQ));

	{
		val.mul(*reinterpret_cast<const LLVector4a*>(F_OOU8MAX_4A));
		val.mul(delta);
		val.add(low);
	}

	{
		LLVector4a maxError; maxError.setMul(delta, *reinterpret_cast<const LLVector4a*>(F_OOU8MAX_4A));
		LLVector4a absVal; absVal.setAbs(val);
		setSelectWithMask(absVal.lessThan(maxError), F_ZERO_4A, val);
	}
}

void LLVector4a::quantize16(const LLVector4a& low, const LLVector4a& high)
{
	LLVector4a val(mQ);
	LLVector4a delta; delta.setSub(high, low);

	{
		val.clamp(low, high);
		val.sub(low);

		// 16-bit quantization means we need a round of Newton-Raphson
		LLVector4a oneOverDelta;
		{
			alignas(16) thread_local const F32 F_TWO_4A[4] = { 2.f, 2.f, 2.f, 2.f };
			ll_assert_aligned(F_TWO_4A, 16);

			LLVector4a two; two.load4a(F_TWO_4A);

			// Here we use _mm_rcp_ps plus one round of newton-raphson
			// We wish to find 'x' such that x = 1/delta
			// As a first approximation, we take x0 = _mm_rcp_ps(delta)
			// Then x1 = 2 * x0 - a * x0^2 or x1 = x0 * (2 - a * x0)
			// See Intel AP-803 http://ompf.org/!/Intel_application_note_AP-803.pdf
			const LLVector4a recipApprox = _mm_rcp_ps(delta.mQ);
			oneOverDelta.setMul(delta, recipApprox);
			oneOverDelta.setSub(two, oneOverDelta);
			oneOverDelta.mul(recipApprox);
		}

		val.mul(oneOverDelta);
		val.mul(*reinterpret_cast<const LLVector4a*>(F_U16MAX_4A));
	}

	val = _mm_cvtepi32_ps(_mm_cvtps_epi32(val.mQ));

	{
		val.mul(*reinterpret_cast<const LLVector4a*>(F_OOU16MAX_4A));
		val.mul(delta);
		val.add(low);
	}

	{
		LLVector4a maxError; maxError.setMul(delta, *reinterpret_cast<const LLVector4a*>(F_OOU16MAX_4A));
		LLVector4a absVal; absVal.setAbs(val);
		setSelectWithMask(absVal.lessThan(maxError), F_ZERO_4A, val);
	}
}
