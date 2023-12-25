/**
 * @file llquaternion2.inl
 * @brief LLQuaternion2 inline definitions
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

thread_local const LLQuad LL_V4A_PLUS_ONE  = { 1.f, 1.f, 1.f, 1.f };
thread_local const LLQuad LL_V4A_MINUS_ONE = { -1.f, -1.f, -1.f, -1.f };

// Ctor from LLQuaternion
LL_INLINE LLQuaternion2::LLQuaternion2(const LLQuaternion& quat)
{
	mQ.set(quat.mQ[VX], quat.mQ[VY], quat.mQ[VZ], quat.mQ[VW]);
}

//////////////////////////
// Get/Set
//////////////////////////

// Return the internal LLVector4a representation of the quaternion
LL_INLINE const LLVector4a& LLQuaternion2::getVector4a() const
{
	return mQ;
}

LL_INLINE LLVector4a& LLQuaternion2::getVector4aRw()
{
	return mQ;
}

/////////////////////////
// Quaternion modification
/////////////////////////

// Set this quaternion to the conjugate of src
LL_INLINE void LLQuaternion2::setConjugate(const LLQuaternion2& src)
{
	alignas(16) thread_local const U32 F_QUAT_INV_MASK_4A[4] = {	0x80000000,
																	0x80000000,
																	0x80000000,
																	0x00000000 };

	mQ = _mm_xor_ps(src.mQ,
					*reinterpret_cast<const LLQuad*>(&F_QUAT_INV_MASK_4A));
}

// Renormalizes the quaternion. Assumes it has nonzero length.
LL_INLINE void LLQuaternion2::normalize()
{
	mQ.normalize4();
}

// Quantize this quaternion to 8 bit precision
LL_INLINE void LLQuaternion2::quantize8()
{
	mQ.quantize8(LL_V4A_MINUS_ONE, LL_V4A_PLUS_ONE);
	normalize();
}

// Quantize this quaternion to 16 bit precision
LL_INLINE void LLQuaternion2::quantize16()
{
	mQ.quantize16(LL_V4A_MINUS_ONE, LL_V4A_PLUS_ONE);
	normalize();
}

LL_INLINE void LLQuaternion2::mul(const LLQuaternion2& b)
{
	alignas(16) thread_local const unsigned int signMask[4] = {
		0x0, 0x0, 0x0, 0x80000000
	};

	LLVector4a sum1, sum2, prod1, prod2, prod3, prod4;
	const LLVector4a& va = mQ;
	const LLVector4a& vb = b.getVector4a();

	//			 [VX] [VY] [VZ] [VW]
	// prod1:	+wx  +wy  +wz  +ww  Bwwww*Axyzw
	// prod2:	+xw  +yw  +zw  -xx  Bxyzx*Awwwx	  [VW] sign flip
	// prod3:	+yz  +zx  +xy  -yy  Byzxy*Azxyy	  [VW] sign flip
	// prod4:	-zy  -xz  -yx  -zz  Bzxyz*Ayzzz

	const LLVector4a Bwwww = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(3, 3, 3, 3));
	const LLVector4a Bxyzx = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(0, 2, 1, 0));
	const LLVector4a Awwwx = _mm_shuffle_ps(va, va, _MM_SHUFFLE(0, 3, 3, 3));
	const LLVector4a Byzxy = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(1, 0, 2, 1));
	const LLVector4a Azxyy = _mm_shuffle_ps(va, va, _MM_SHUFFLE(1, 1, 0, 2));
	const LLVector4a Bzxyz = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(2, 1, 0, 2));
	const LLVector4a Ayzxz = _mm_shuffle_ps(va, va, _MM_SHUFFLE(2, 0, 2, 1));

	prod1.setMul(Bwwww, va);
	prod2.setMul(Bxyzx, Awwwx);
	prod3.setMul(Byzxy, Azxyy);
	prod4.setMul(Bzxyz, Ayzxz);

	sum1.setAdd(prod2, prod3);
	sum1 = _mm_xor_ps(sum1, _mm_load_ps((const float*)signMask));
	sum2.setSub(prod1, prod4);
	mQ.setAdd(sum1, sum2);
}

/////////////////////////
// Quaternion inspection
/////////////////////////

// Return true if this quaternion is equal to 'rhs'.
// Note: Quaternions exhibit "double-cover", so any rotation has two equally valid
// quaternion representations and they will NOT compare equal.
LL_INLINE bool LLQuaternion2::equals(const LLQuaternion2 &rhs, F32 tolerance) const
{
	return mQ.equals4(rhs.mQ, tolerance);
}

// Return true if all components are finite and the quaternion is normalized
LL_INLINE bool LLQuaternion2::isOkRotation() const
{
	return mQ.isFinite4() && mQ.isNormalized4();
}
