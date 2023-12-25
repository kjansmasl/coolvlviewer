/**
 * @file llvector4a.inl
 * @brief LLVector4a inline function implementations
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

#ifndef LL_INL_INCLUDE
# error "You cannot #include this file yourself, #include llmath.h instead !"
#endif

////////////////////////////////////
// GET/SET
////////////////////////////////////

// Prefer this method for read-only access to a single element. Prefer the
// templated version below if the element is known at compile time.
LL_INLINE LLSimdScalar LLVector4a::getScalarAt(S32 idx) const
{
	// Return appropriate LLQuad. It will be cast to LLSimdScalar automatically
	// (should be effectively a nop)
	switch (idx)
	{
		case 0:
			return mQ;

		case 1:
			return _mm_shuffle_ps(mQ, mQ, _MM_SHUFFLE(1, 1, 1, 1));

		case 2:
			return _mm_shuffle_ps(mQ, mQ, _MM_SHUFFLE(2, 2, 2, 2));

		default:
			return _mm_shuffle_ps(mQ, mQ, _MM_SHUFFLE(3, 3, 3, 3));
	}
}

template <int N> LL_INLINE LLSimdScalar LLVector4a::getScalarAt() const
{
	return _mm_shuffle_ps(mQ, mQ, _MM_SHUFFLE(N, N, N, N));
}

template<> LL_INLINE LLSimdScalar LLVector4a::getScalarAt<0>() const
{
	return mQ;
}

LL_INLINE void LLVector4a::splat(const LLSimdScalar& x)
{
	mQ = _mm_shuffle_ps(x.getQuad(), x.getQuad(), _MM_SHUFFLE(0, 0, 0, 0));
}

// Set all 4 elements to element N of src, with N known at compile time
template <int N> LL_INLINE void LLVector4a::splat(const LLVector4a& src)
{
	mQ = _mm_shuffle_ps(src.mQ, src.mQ, _MM_SHUFFLE(N, N, N, N));
}

// Set all 4 elements to element i of v, with i NOT known at compile time
LL_INLINE void LLVector4a::splat(const LLVector4a& v, U32 i)
{
	switch (i)
	{
		case 0:
			mQ = _mm_shuffle_ps(v.mQ, v.mQ, _MM_SHUFFLE(0, 0, 0, 0));
			break;

		case 1:
			mQ = _mm_shuffle_ps(v.mQ, v.mQ, _MM_SHUFFLE(1, 1, 1, 1));
			break;

		case 2:
			mQ = _mm_shuffle_ps(v.mQ, v.mQ, _MM_SHUFFLE(2, 2, 2, 2));
			break;

		case 3:
			mQ = _mm_shuffle_ps(v.mQ, v.mQ, _MM_SHUFFLE(3, 3, 3, 3));
	}
}

// Sets element N to that of src's element N
template <int N> LL_INLINE void LLVector4a::copyComponent(const LLVector4a& src)
{
	thread_local const LLVector4Logical mask =
		_mm_load_ps((F32*)&S_V4LOGICAL_MASK_TABLE[N * 4]);
	setSelectWithMask(mask, src, mQ);
}

// Select bits from src_if_true and src_if_false according to bits in mask
LL_INLINE void LLVector4a::setSelectWithMask(const LLVector4Logical& mask,
											 const LLVector4a& src_if_true,
											 const LLVector4a& src_if_false)
{
	// (((src_if_true ^ src_if_false) & mask) ^ src_if_false)
	// E.g., src_if_false = 1010b, src_if_true = 0101b, mask = 1100b
	// (src_if_true ^ src_if_false) = 1111b --> & mask = 1100b -->
	// ^ src_if_false = 0110b,
	// as expected (01 from src_if_true, 10 from src_if_false)
	// Courtesy of Mark++:
	//http://markplusplus.wordpress.com/2007/03/14/fast-sse-select-operation/
	mQ = _mm_xor_ps(src_if_false,
					_mm_and_ps(mask, _mm_xor_ps(src_if_true, src_if_false)));
}

////////////////////////////////////
// ALGEBRAIC
////////////////////////////////////

// Set this to (a x b) (geometric cross-product)
LL_INLINE void LLVector4a::setCross3(const LLVector4a& a, const LLVector4a& b)
{
#if 0	// Old code by LL
	// Vectors are stored in memory in w, z, y, x order from high to low
	// Set vector1 = { a[W], a[X], a[Z], a[Y] }
	const LLQuad vector1 = _mm_shuffle_ps(a.mQ, a.mQ, _MM_SHUFFLE(3, 0, 2, 1));
	// Set vector2 = { b[W], b[Y], b[X], b[Z] }
	const LLQuad vector2 = _mm_shuffle_ps(b.mQ, b.mQ, _MM_SHUFFLE(3, 1, 0, 2));
	// mQ = { a[W]*b[W], a[X]*b[Y], a[Z]*b[X], a[Y]*b[Z] }
	mQ = _mm_mul_ps(vector1, vector2);
	// vector3 = { a[W], a[Y], a[X], a[Z] }
	const LLQuad vector3 = _mm_shuffle_ps(a.mQ, a.mQ, _MM_SHUFFLE(3, 1, 0, 2));
	// vector4 = { b[W], b[X], b[Z], b[Y] }
	const LLQuad vector4 = _mm_shuffle_ps(b.mQ, b.mQ, _MM_SHUFFLE(3, 0, 2, 1));
	// mQ = { 0, a[X]*b[Y] - a[Y]*b[X], a[Z]*b[X] - a[X]*b[Z],
	// a[Y]*b[Z] - a[Z]*b[Y] }
	mQ = _mm_sub_ps(mQ, _mm_mul_ps(vector3, vector4));
#else	// Alchemy's version
	LLQuad tmp0 = _mm_shuffle_ps(b.mQ, b.mQ, _MM_SHUFFLE(3, 0, 2, 1));
	LLQuad tmp1 = _mm_shuffle_ps(a.mQ, a.mQ, _MM_SHUFFLE(3, 0, 2, 1));
	tmp0 = _mm_mul_ps(tmp0, a.mQ);
	tmp1 = _mm_mul_ps(tmp1, b.mQ);
	LLQuad tmp2 = _mm_sub_ps(tmp0, tmp1);
	mQ = _mm_shuffle_ps(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));
#endif
}

// Set all elements to the dot product of the x, y, and z elements in a and b
LL_INLINE void LLVector4a::setAllDot3(const LLVector4a& a, const LLVector4a& b)
{
#if defined(__SSE4_1__)
	mQ = _mm_dp_ps(a.mQ, b.mQ, 0x7f);
#else
	// ab = { a[W]*b[W], a[Z]*b[Z], a[Y]*b[Y], a[X]*b[X] }
	const LLQuad ab = _mm_mul_ps(a.mQ, b.mQ);
	// yzxw = { a[W]*b[W], a[Z]*b[Z], a[X]*b[X], a[Y]*b[Y] }
	const __m128i wzxy =
		_mm_shuffle_epi32(_mm_castps_si128(ab), _MM_SHUFFLE(3, 2, 0, 1));
	// xPlusY = { 2*a[W]*b[W], 2 * a[Z] * b[Z], a[Y]*b[Y] + a[X] * b[X],
	// a[X] * b[X] + a[Y] * b[Y] }
	const LLQuad xPlusY = _mm_add_ps(ab, _mm_castsi128_ps(wzxy));
	// xPlusYSplat = { a[Y]*b[Y] + a[X] * b[X], a[X] * b[X] + a[Y] * b[Y],
	// a[Y]*b[Y] + a[X] * b[X], a[X] * b[X] + a[Y] * b[Y] }
	const LLQuad xPlusYSplat = _mm_movelh_ps(xPlusY, xPlusY);
	// zSplat = { a[Z]*b[Z], a[Z]*b[Z], a[Z]*b[Z], a[Z]*b[Z] }
	const __m128i zSplat = _mm_shuffle_epi32(_mm_castps_si128(ab),
											 _MM_SHUFFLE(2, 2, 2, 2));
	// mQ = { a[Z] * b[Z] + a[Y] * b[Y] + a[X] * b[X], same, same, same }
	mQ = _mm_add_ps(_mm_castsi128_ps(zSplat), xPlusYSplat);
#endif
}

// Set all elements to the dot product of the x, y, z, and w elements in a and b
LL_INLINE void LLVector4a::setAllDot4(const LLVector4a& a, const LLVector4a& b)
{
#if defined(__SSE4_1__)
	mQ = _mm_dp_ps(a.mQ, b.mQ, 0xff);
#else
	// ab = { a[W]*b[W], a[Z]*b[Z], a[Y]*b[Y], a[X]*b[X] }
	const LLQuad ab = _mm_mul_ps(a.mQ, b.mQ);
	// yzxw = { a[W]*b[W], a[Z]*b[Z], a[X]*b[X], a[Y]*b[Y] }
	const __m128i zwxy = _mm_shuffle_epi32(_mm_castps_si128(ab),
										   _MM_SHUFFLE(2, 3, 0, 1));
	// zPlusWandXplusY = { a[W]*b[W] + a[Z]*b[Z], a[Z] * b[Z] + a[W]*b[W],
	// a[Y]*b[Y] + a[X] * b[X], a[X] * b[X] + a[Y] * b[Y] }
	const LLQuad zPlusWandXplusY = _mm_add_ps(ab, _mm_castsi128_ps(zwxy));
	// xPlusYSplat = { a[Y]*b[Y] + a[X] * b[X], a[X] * b[X] + a[Y] * b[Y],
	// a[Y]*b[Y] + a[X] * b[X], a[X] * b[X] + a[Y] * b[Y] }
	const LLQuad xPlusYSplat = _mm_movelh_ps(zPlusWandXplusY, zPlusWandXplusY);
	const LLQuad zPlusWSplat = _mm_movehl_ps(zPlusWandXplusY, zPlusWandXplusY);

	// mQ = { a[W]*b[W] + a[Z] * b[Z] + a[Y] * b[Y] + a[X] * b[X], same, same, same }
	mQ = _mm_add_ps(xPlusYSplat, zPlusWSplat);
#endif
}

// Return the 3D dot product of this vector and b
LL_INLINE LLSimdScalar LLVector4a::dot3(const LLVector4a& b) const
{
#if defined(__SSE4_1__)
	return _mm_dp_ps(mQ, b.mQ, 0x7f);
#else
	const LLQuad ab = _mm_mul_ps(mQ, b.mQ);
	const LLQuad splatY =
		_mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(ab),
										   _MM_SHUFFLE(1, 1, 1, 1)));
	const LLQuad splatZ =
		_mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(ab),
										   _MM_SHUFFLE(2, 2, 2, 2)));
	const LLQuad xPlusY = _mm_add_ps(ab, splatY);
	return _mm_add_ps(xPlusY, splatZ);
#endif
}

// Return the 4D dot product of this vector and b
LL_INLINE LLSimdScalar LLVector4a::dot4(const LLVector4a& b) const
{
#if defined(__SSE4_1__)
	return _mm_dp_ps(mQ, b.mQ, 0xff);
#else
	// ab = { w, z, y, x }
 	const LLQuad ab = _mm_mul_ps(mQ, b.mQ);
 	// upperProdsInLowerElems = { y, x, y, x }
	const LLQuad upperProdsInLowerElems = _mm_movehl_ps(ab, ab);
	// sumOfPairs = { w+y, z+x, 2y, 2x }
 	const LLQuad sumOfPairs = _mm_add_ps(upperProdsInLowerElems, ab);
	// shuffled = { z+x, z+x, z+x, z+x }
	const LLQuad shuffled =
		_mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(sumOfPairs),
						 _MM_SHUFFLE(1, 1, 1, 1)));
	return _mm_add_ss(sumOfPairs, shuffled);
#endif
}

// Normalize this vector with respect to the x, y, and z components only.
// Accurate to 22 bytes of precision. W component is destroyed. Note that this
// does not consider zero length vectors !
LL_INLINE void LLVector4a::normalize3()
{
	// len_squared = a dot a
	LLVector4a len_squared; len_squared.setAllDot3(*this, *this);
	// rsqrt = approximate reciprocal square (i.e., { ~1/len(a)^2, ~1/len(a)^2,
	// ~1/len(a)^2, ~1/len(a)^2 }
	const LLQuad rsqrt = _mm_rsqrt_ps(len_squared.mQ);
	thread_local const LLQuad half = { 0.5f, 0.5f, 0.5f, 0.5f };
	thread_local const LLQuad three = {3.f, 3.f, 3.f, 3.f };
	// Now we do one round of Newton-Raphson approximation to get full accuracy
	// According to the Newton-Raphson method, given a first 'w' for the root
	// of f(x) = 1/x^2 - a (i.e., x = 1/sqrt(a)) the next better approximation
	// w[i+1] = w - f(w)/f'(w) = w - (1/w^2 - a)/(-2*w^(-3))
	// w[i+1] = w + 0.5 * (1/w^2 - a) * w^3 = w + 0.5 * (w - a*w^3) =
	// 1.5 * w - 0.5 * a * w^3 = 0.5 * w * (3 - a*w^2)
	// Our first approx is w = rsqrt. We need out = a * w[i+1] (this is the
	// input vector 'a', not the 'a' from the above formula which is actually
	// len_squared). So out = a * [0.5 * rsqrt * (3 - len_squared * rsqrt * rsqrt)]
	const LLQuad AtimesRsqrt = _mm_mul_ps(len_squared.mQ, rsqrt);
	const LLQuad AtimesRsqrtTimesRsqrt = _mm_mul_ps(AtimesRsqrt, rsqrt);
	const LLQuad threeMinusAtimesRsqrtTimesRsqrt =
		_mm_sub_ps(three, AtimesRsqrtTimesRsqrt);
	const LLQuad nrApprox =
		_mm_mul_ps(half, _mm_mul_ps(rsqrt, threeMinusAtimesRsqrtTimesRsqrt));
	mQ = _mm_mul_ps(mQ, nrApprox);
}

// Normalize this vector with respect to all components. Accurate to 22 bytes
// of precision. Note that this does not consider zero length vectors !
LL_INLINE void LLVector4a::normalize4()
{
	// len_squared = a dot a
	LLVector4a len_squared; len_squared.setAllDot4(*this, *this);
	// rsqrt = approximate reciprocal square (i.e., { ~1/len(a)^2, ~1/len(a)^2,
	// ~1/len(a)^2, ~1/len(a)^2 }
	const LLQuad rsqrt = _mm_rsqrt_ps(len_squared.mQ);
	thread_local const LLQuad half = { 0.5f, 0.5f, 0.5f, 0.5f };
	thread_local const LLQuad three = { 3.f, 3.f, 3.f, 3.f };
	// Now we do one round of Newton-Raphson approximation to get full accuracy
	// According to the Newton-Raphson method, given a first 'w' for the root
	// of f(x) = 1/x^2 - a (i.e., x = 1/sqrt(a)) the next better approximation
	// w[i+1] = w - f(w)/f'(w) = w - (1/w^2 - a)/(-2*w^(-3))
	// w[i+1] = w + 0.5 * (1/w^2 - a) * w^3 = w + 0.5 * (w - a*w^3) =
	// 1.5 * w - 0.5 * a * w^3 = 0.5 * w * (3 - a*w^2)
	// Our first approx is w = rsqrt. We need out = a * w[i + 1] (this is the
	// input vector 'a', not the 'a' from the above formula which is actually
	// len_squared). So out = a * [0.5 * rsqrt * (3 - len_squared * rsqrt * rsqrt)]
	const LLQuad AtimesRsqrt = _mm_mul_ps(len_squared.mQ, rsqrt);
	const LLQuad AtimesRsqrtTimesRsqrt = _mm_mul_ps(AtimesRsqrt, rsqrt);
	const LLQuad threeMinusAtimesRsqrtTimesRsqrt =
		_mm_sub_ps(three, AtimesRsqrtTimesRsqrt);
	const LLQuad nrApprox =
		_mm_mul_ps(half, _mm_mul_ps(rsqrt, threeMinusAtimesRsqrtTimesRsqrt));
	mQ = _mm_mul_ps(mQ, nrApprox);
}

// Normalize this vector with respect to the x, y, and z components only.
// Accurate to 22 bytes of precision. W component is destroyed. Note that this
// does not consider zero length vectors !
LL_INLINE LLSimdScalar LLVector4a::normalize3withLength()
{
	// len_squared = a dot a
	LLVector4a len_squared; len_squared.setAllDot3(*this, *this);
	// rsqrt = approximate reciprocal square (i.e., { ~1/len(a)^2, ~1/len(a)^2,
	// ~1/len(a)^2, ~1/len(a)^2 }
	const LLQuad rsqrt = _mm_rsqrt_ps(len_squared.mQ);
	thread_local const LLQuad half = { 0.5f, 0.5f, 0.5f, 0.5f };
	thread_local const LLQuad three = { 3.f, 3.f, 3.f, 3.f };
	// Now we do one round of Newton-Raphson approximation to get full accuracy
	// According to the Newton-Raphson method, given a first 'w' for the root
	// of f(x) = 1/x^2 - a (i.e., x = 1/sqrt(a)) the next better approximation
	// w[i+1] = w - f(w)/f'(w) = w - (1/w^2 - a)/(-2*w^(-3))
	// w[i+1] = w + 0.5 * (1/w^2 - a) * w^3 = w + 0.5 * (w - a*w^3) =
	// 1.5 * w - 0.5 * a * w^3 = 0.5 * w * (3 - a*w^2)
	// Our first approx is w = rsqrt. We need out = a * w[i+1] (this is the
	// input vector 'a', not the 'a' from the above formula which is actually
	// len_squared). So out = a * [0.5 * rsqrt * (3 - len_squared * rsqrt * rsqrt)]
	const LLQuad AtimesRsqrt = _mm_mul_ps(len_squared.mQ, rsqrt);
	const LLQuad AtimesRsqrtTimesRsqrt = _mm_mul_ps(AtimesRsqrt, rsqrt);
	const LLQuad threeMinusAtimesRsqrtTimesRsqrt =
		_mm_sub_ps(three, AtimesRsqrtTimesRsqrt);
	const LLQuad nrApprox =
		_mm_mul_ps(half, _mm_mul_ps(rsqrt, threeMinusAtimesRsqrtTimesRsqrt));
	mQ = _mm_mul_ps(mQ, nrApprox);
	return _mm_sqrt_ss(len_squared);
}

// Normalize this vector with respect to the x, y, and z components only.
// Accurate only to 10-12 bits of precision. W component is destroyed.
// Note that this does not consider zero length vectors !
LL_INLINE void LLVector4a::normalize3fast()
{
	LLVector4a len_squared;
	len_squared.setAllDot3(*this, *this);
	const LLQuad approxRsqrt = _mm_rsqrt_ps(len_squared.mQ);
	mQ = _mm_mul_ps(mQ, approxRsqrt);
}

LL_INLINE void LLVector4a::normalize3fast_checked(LLVector4a* d)
{
	if (!isFinite3())
	{
		*this = d ? *d : LLVector4a(0, 1, 0, 1);
		return;
	}

	LLVector4a len_squared; len_squared.setAllDot3(*this, *this);
	if (len_squared.getF32ptr()[0] <= FLT_EPSILON)
	{
		*this = d ? *d : LLVector4a(0, 1, 0, 1);
		return;
	}

	const LLQuad approxRsqrt = _mm_rsqrt_ps(len_squared.mQ);
	mQ = _mm_mul_ps(mQ, approxRsqrt);
}

// Return true if this vector is normalized with respect to x,y,z up to
// tolerance
LL_INLINE LLBool32 LLVector4a::isNormalized3(F32 tolerance) const
{
	alignas(16) thread_local const U32 ones[4] = {
		0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	};
	LLSimdScalar tol = _mm_load_ss(&tolerance);
	tol = _mm_mul_ss(tol, tol);
	LLVector4a lenSquared; lenSquared.setAllDot3(*this, *this);
	lenSquared.sub(*reinterpret_cast<const LLVector4a*>(ones));
	lenSquared.setAbs(lenSquared);
	return _mm_comile_ss(lenSquared, tol);
}

// Return true if this vector is normalized with respect to all components up
// to tolerance
LL_INLINE LLBool32 LLVector4a::isNormalized4(F32 tolerance) const
{
	alignas(16) thread_local const U32 ones[4] = {
		0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	};
	LLSimdScalar tol = _mm_load_ss(&tolerance);
	tol = _mm_mul_ss(tol, tol);
	LLVector4a lenSquared; lenSquared.setAllDot4(*this, *this);
	lenSquared.sub(*reinterpret_cast<const LLVector4a*>(ones));
	lenSquared.setAbs(lenSquared);
	return _mm_comile_ss(lenSquared, tol);
}

LL_INLINE LLBool32 LLVector4a::isFinite3() const
{
	alignas(16) thread_local const U32 nanOrInfMask[4] = {
		0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000
	};
	ll_assert_aligned(nanOrInfMask, 16);
	const __m128i nanOrInfMaskV =
		*reinterpret_cast<const __m128i*>(nanOrInfMask);
	const __m128i maskResult = _mm_and_si128(_mm_castps_si128(mQ),
											 nanOrInfMaskV);
	const LLVector4Logical equalityCheck =
		_mm_castsi128_ps(_mm_cmpeq_epi32(maskResult, nanOrInfMaskV));
	return !equalityCheck.areAnySet(LLVector4Logical::MASK_XYZ);
}

LL_INLINE LLBool32 LLVector4a::isFinite4() const
{
	alignas(16) thread_local const U32 nanOrInfMask[4] = {
		0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000
	};
	const __m128i nanOrInfMaskV =
		*reinterpret_cast<const __m128i*>(nanOrInfMask);
	const __m128i maskResult = _mm_and_si128(_mm_castps_si128(mQ),
											 nanOrInfMaskV);
	const LLVector4Logical equalityCheck =
		_mm_castsi128_ps(_mm_cmpeq_epi32(maskResult, nanOrInfMaskV));
	return !equalityCheck.areAnySet(LLVector4Logical::MASK_XYZW);
}

LL_INLINE void LLVector4a::setRotatedInv(const LLRotation& rot,
										 const LLVector4a& vec)
{
	LLRotation inv; inv.setTranspose(rot);
	setRotated(inv, vec);
}

LL_INLINE void LLVector4a::setRotatedInv(const LLQuaternion2& quat,
										 const LLVector4a& vec)
{
	LLQuaternion2 invRot; invRot.setConjugate(quat);
	setRotated(invRot, vec);
}

LL_INLINE void LLVector4a::clamp(const LLVector4a& low, const LLVector4a& high)
{
	const LLVector4Logical highMask = greaterThan(high);
	const LLVector4Logical lowMask = lessThan(low);

	setSelectWithMask(highMask, high, *this);
	setSelectWithMask(lowMask, low, *this);
}

////////////////////////////////////
// LOGICAL
//
// The functions in this section will compare the elements in this vector to
// those in rhs and return an LLVector4Logical with all bits set in elements
// where the comparison was true and all bits unset in elements where the
// comparison was false.
//
// WARNING: Other than equals3 and equals4, these functions do NOT account for
// floating point tolerance. You should include the appropriate tolerance in
// the inputs.
////////////////////////////////////

// Returns true if this and rhs are componentwise equal up to the specified
// absolute tolerance
LL_INLINE bool LLVector4a::equals4(const LLVector4a& rhs, F32 tolerance) const
{
	LLVector4a diff; diff.setSub(*this, rhs);
	diff.setAbs(diff);
	const LLQuad tol = _mm_set1_ps(tolerance);
	const LLQuad cmp = _mm_cmplt_ps(diff, tol);
	return (_mm_movemask_ps(cmp) & LLVector4Logical::MASK_XYZW) ==
				LLVector4Logical::MASK_XYZW;
}

LL_INLINE bool LLVector4a::equals3(const LLVector4a& rhs, F32 tolerance) const
{
	LLVector4a diff; diff.setSub(*this, rhs);
	diff.setAbs(diff);
	const LLQuad tol = _mm_set1_ps(tolerance);
	const LLQuad t = _mm_cmplt_ps(diff, tol);
	return (_mm_movemask_ps(t) & LLVector4Logical::MASK_XYZ) ==
				LLVector4Logical::MASK_XYZ;
}
