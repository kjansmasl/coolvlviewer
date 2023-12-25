/**
 * @file llvector4a.h
 * @brief LLVector4a class header file - memory aligned and vectorized 4 component vector
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

#ifndef	LL_LLVECTOR4A_H
#define	LL_LLVECTOR4A_H

#include "llmemory.h"	// Also includes appropriate intrinsics headers

class LLRotation;

///////////////////////////////////
// FIRST TIME USERS PLEASE READ
//////////////////////////////////
// This is just the beginning of LLVector4a. There are many more useful
// functions yet to be implemented. For example, setNeg to negate a vector,
// rotate() to apply a matrix rotation, various functions to manipulate only
// the X, Y, and Z elements and many others (including a whole variety of
// accessors). So if you don't see a function here that you need, please
// contact Falcon or someone else with SSE experience (Richard, I think, has
// some and davep has a little as of the time of this writing, July 08, 2010)
// about getting it implemented before you resort to LLVector3/LLVector4.
/////////////////////////////////

class alignas(16) LLVector4a
{
public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr)
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr)
	{
		ll_aligned_free_16(ptr);
	}

	///////////////////////////////////
	// STATIC METHODS
	///////////////////////////////////

	// Call this method at startup to avoid 15,000+ cycle penalties from
	// denormalized numbers
	static void initClass()
	{
#if LL_WINDOWS
		unsigned int current_word = 0;
		_controlfp_s(&current_word, _DN_FLUSH, _MCW_DN);
#endif
#if defined(_MM_SET_DENORMALS_ZERO_MODE) && defined(_MM_DENORMALS_ZERO_ON)
		_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
#if defined(_MM_SET_FLUSH_ZERO_MODE) && defined(_MM_FLUSH_ZERO_ON)
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif
		_MM_SET_ROUNDING_MODE(_MM_ROUND_NEAREST);
	}

	// Returns a vector of all zeros
	static LL_INLINE const LLVector4a& getZero()
	{
		extern const LLVector4a LL_V4A_ZERO;
		return LL_V4A_ZERO;
	}

	// Returns a vector of all epsilon, where epsilon is a small float suitable
	// for approximate equality checks
	static LL_INLINE const LLVector4a& getEpsilon()
	{
		extern const LLVector4a LL_V4A_EPSILON;
		return LL_V4A_EPSILON;
	}

	// Copies 16 bytes from src to dst. Source and destination must be 16-byte
	// aligned
	static LL_INLINE void copy4a(F32* dst, const F32* src)
	{
		_mm_store_ps(dst, _mm_load_ps(src));
	}

	// Copies words 16-byte blocks from src to dst. Source and destination must
	// not overlap. Source and dest must be 16-byte aligned and size must be
	// multiple of 16.
	static void memcpyNonAliased16(F32* __restrict dst,
								   const F32* __restrict src, size_t bytes);

	////////////////////////////////////
	// CONSTRUCTORS
	////////////////////////////////////

	// DO NOT INITIALIZE: The overhead is completely unnecessary
	LL_INLINE LLVector4a()
	{
	}

	LL_INLINE LLVector4a(F32 x, F32 y, F32 z, F32 w = 0.f)
	{
		set(x, y, z, w);
	}

	LL_INLINE LLVector4a(F32 x)
	{
		splat(x);
	}

	LL_INLINE LLVector4a(const LLSimdScalar& x)
	{
		splat(x);
	}

	LL_INLINE LLVector4a(LLQuad q)
	{
		mQ = q;
	}

	////////////////////////////////////
	// LOAD/STORE
	////////////////////////////////////

	// Loads from 16-byte aligned src array (preferred method of loading)
	LL_INLINE void load4a(const F32* src)
	{
		mQ = _mm_load_ps(src);
	}

	// Loads from unaligned src array (NB: significantly slower than load4a)
	LL_INLINE void loadua(const F32* src)
	{
		mQ = _mm_loadu_ps(src);
	}

	// Loads only three floats beginning at address 'src'. Slowest method.
	LL_INLINE void load3(const F32* src, const F32 w = 0.f)
	{
		// mQ = { 0.f, src[2], src[1], src[0] } = { W, Z, Y, X }
		// NB: This differs from the convention of { Z, Y, X, W }
		mQ = _mm_set_ps(w, src[2], src[1], src[0]);
	}

	// Stores to a 16-byte aligned memory address
	LL_INLINE void store4a(F32* dst) const
	{
		_mm_store_ps(dst, mQ);
	}

	////////////////////////////////////
	// BASIC GET/SET
	////////////////////////////////////

	// Returns a "this" as an F32 pointer.
	LL_INLINE F32* getF32ptr()
	{
		return (F32*)&mQ;
	}

	// Returns a "this" as a const F32 pointer.
	LL_INLINE const F32* const getF32ptr() const
	{
		return (const F32* const)&mQ;
	}

	// Read-only access to single float in this vector. Do not use in proximity
	// to any function call that manipulates the data at the whole vector level
	// or you will incur a substantial penalty. Consider using the splat
	// functions instead
	LL_INLINE F32 operator[](const S32 idx) const
	{
		return ((F32*)&mQ)[idx];
	}

	// Prefer this method for read-only access to a single element. Prefer the
	// templated version if the elem is known at compile time.
	LL_INLINE LLSimdScalar getScalarAt(S32 idx) const;

	// Prefer this method for read-only access to a single element. Prefer the
	// templated version if the elem is known at compile time.
	template <int N> LL_INLINE LLSimdScalar getScalarAt() const;

	// Sets to an x, y, z and optional w provided
	LL_INLINE void set(F32 x, F32 y, F32 z, F32 w = 0.f)
	{
		mQ = _mm_set_ps(w, z, y, x);
	}

	// Sets to all zeros. This is preferred to using ::getZero()
	LL_INLINE void clear()
	{
		mQ = LLVector4a::getZero().mQ;
	}

	// Sets all elements to 'x'
	LL_INLINE void splat(F32 x)
	{
		mQ = _mm_set1_ps(x);
	}

	// Sets all elements to 'x'
	LL_INLINE void splat(const LLSimdScalar& x);

	// Sets all 4 elements to element N of src, with N known at compile time
	template <int N> LL_INLINE void splat(const LLVector4a& src);

	// Sets all 4 elements to element i of v, with i NOT known at compile time
	LL_INLINE void splat(const LLVector4a& v, U32 i);

	// Sets element N to that of src's element N. Much cleaner than
	// {LLVector4Logical mask; mask.clear(); mask.setElement<N>();
	// target.setSelectWithMask(mask,src,target);}
	template <int N> LL_INLINE void copyComponent(const LLVector4a& src);

	// Selects bits from src_if_true and src_if_false according to bits in mask
	LL_INLINE void setSelectWithMask(const LLVector4Logical& mask,
									 const LLVector4a& src_if_true,
									 const LLVector4a& src_if_false);

	////////////////////////////////////
	// ALGEBRAIC
	////////////////////////////////////

	// Sets this to the element-wise (a + b)
	LL_INLINE void setAdd(const LLVector4a& a, const LLVector4a& b)
	{
		mQ = _mm_add_ps(a.mQ, b.mQ);
	}

	// Sets this to element-wise (a - b)
	LL_INLINE void setSub(const LLVector4a& a, const LLVector4a& b)
	{
		mQ = _mm_sub_ps(a.mQ, b.mQ);
	}

	// Sets this to element-wise multiply (a * b)
	LL_INLINE void setMul(const LLVector4a& a, const LLVector4a& b)
	{
		mQ = _mm_mul_ps(a.mQ, b.mQ);
	}

	// Sets this to element-wise quotient (a / b)
	LL_INLINE void setDiv(const LLVector4a& a, const LLVector4a& b)
	{
		mQ = _mm_div_ps(a.mQ, b.mQ);
	}

	// Sets this to the element-wise absolute value of src
	LL_INLINE void setAbs(const LLVector4a& src)
	{
		alignas(16) thread_local const U32 F_ABS_MASK_4A[4] = {
			0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF
		};
		mQ = _mm_and_ps(src.mQ,
						*reinterpret_cast<const LLQuad*>(F_ABS_MASK_4A));
	}

	// Adds to each component in this vector the corresponding component in rhs
	LL_INLINE void add(const LLVector4a& rhs)
	{
		mQ = _mm_add_ps(mQ, rhs.mQ);
	}

	// Subtracts from each component in this vector the corresponding component
	// in rhs
	LL_INLINE void sub(const LLVector4a& rhs)
	{
		mQ = _mm_sub_ps(mQ, rhs.mQ);
	}

	// Multiplies each component in this vector by the corresponding component
	// in rhs
	LL_INLINE void mul(const LLVector4a& rhs)
	{
		mQ = _mm_mul_ps(mQ, rhs.mQ);
	}

	// Divides each component in this vector by the corresponding component in
	// rhs
	LL_INLINE void div(const LLVector4a& rhs)
	{
		// *TODO: Check accuracy, maybe add divFast
		mQ = _mm_div_ps(mQ, rhs.mQ);
	}

	// Multiplies this vector by x in a scalar fashion
	LL_INLINE void mul(F32 x)
	{
		LLVector4a t;
		t.splat(x);
		mQ = _mm_mul_ps(mQ, t.mQ);
	}

	// Sets this to (a x b) (geometric cross-product)
	LL_INLINE void setCross3(const LLVector4a& a, const LLVector4a& b);

	// Sets all elements to the dot product of the x, y, and z elements in a
	// and b
	LL_INLINE void setAllDot3(const LLVector4a& a, const LLVector4a& b);

	// Sets all elements to the dot product of the x, y, z, and w elements in a
	// and b
	LL_INLINE void setAllDot4(const LLVector4a& a, const LLVector4a& b);

	// Returns the 3D dot product of this vector and b
	LL_INLINE LLSimdScalar dot3(const LLVector4a& b) const;

	// Returns the 4D dot product of this vector and b
	LL_INLINE LLSimdScalar dot4(const LLVector4a& b) const;

	// Normalizes this vector with respect to the x, y, and z components only.
	// Accurate to 22 bits of precision. W component is destroyed.
	// Note that this does not consider zero length vectors !
	LL_INLINE void normalize3();

	// Same as normalize3() but with respect to all 4 components
	LL_INLINE void normalize4();

	// Same as normalize3(), but returns length as a SIMD scalar
	LL_INLINE LLSimdScalar normalize3withLength();

	// Normalizes this vector with respect to the x, y, and z components only.
	// Accurate only to 10-12 bits of precision. W component is destroyed.
	// Note that this does not consider zero length vectors !
	LL_INLINE void normalize3fast();

	LL_INLINE void normalize3fast_checked(LLVector4a* d = NULL);

	// Returns true if this vector is normalized with respect to x,y,z up to
	// tolerance
	LL_INLINE LLBool32 isNormalized3(F32 tolerance = 1e-3) const;

	// Returns true if this vector is normalized with respect to all components
	// up to tolerance
	LL_INLINE LLBool32 isNormalized4(F32 tolerance = 1e-3) const;

	// Sets all elements to the length of vector 'v'
	LL_INLINE void setAllLength3(const LLVector4a& v)
	{
		LLVector4a len_squared;
		len_squared.setAllDot3(v, v);
		mQ = _mm_sqrt_ps(len_squared.mQ);
	}

	// Gets this vector's length
	LL_INLINE LLSimdScalar getLength3() const
	{
		return _mm_sqrt_ss(dot3((const LLVector4a)mQ));
	}

	// Sets the components of this vector to the minimum of the corresponding
	// components of lhs and rhs
	LL_INLINE void setMin(const LLVector4a& lhs, const LLVector4a& rhs)
	{
		mQ = _mm_min_ps(lhs.mQ, rhs.mQ);
	}

	// Sets the components of this vector to the maximum of the corresponding
	// components of lhs and rhs
	LL_INLINE void setMax(const LLVector4a& lhs, const LLVector4a& rhs)
	{
		mQ = _mm_max_ps(lhs.mQ, rhs.mQ);
	}

	// Clamps this vector to be within the component-wise range low to high
	// (inclusive)
	LL_INLINE void clamp(const LLVector4a& low, const LLVector4a& high);

	// Sets this to  (c * lhs) + rhs * (1 - c)
	LL_INLINE void setLerp(const LLVector4a& lhs, const LLVector4a& rhs, F32 c)
	{
		LLVector4a t;
		t.setSub(rhs, lhs);
		t.mul(c);
		setAdd(lhs, t);
	}

	// Returns true (nonzero) if x, y, z (and w for Finite4) are all finite
	// floats
	LL_INLINE LLBool32 isFinite3() const;
	LL_INLINE LLBool32 isFinite4() const;

	// Sets this vector to 'vec' rotated by the LLRotation or LLQuaternion2
	// provided
	void setRotated(const LLRotation& rot, const LLVector4a& vec);
	void setRotated(const class LLQuaternion2& quat, const LLVector4a& vec);

	// Sets this vector to 'vec' rotated by the INVERSE of the LLRotation or
	// LLQuaternion2 provided
	LL_INLINE void setRotatedInv(const LLRotation& rot, const LLVector4a& vec);
	LL_INLINE void setRotatedInv(const class LLQuaternion2& quat,
								 const LLVector4a& vec);

	// Quantizes this vector to 8 or 16 bit precision
	void quantize8(const LLVector4a& low, const LLVector4a& high);
	void quantize16(const LLVector4a& low, const LLVector4a& high);

	LL_INLINE void negate()
	{
		alignas(16) thread_local const U32 sign_mask[] =
			{ 0x80000000, 0x80000000, 0x80000000, 0x80000000 };

		mQ = _mm_xor_ps(*reinterpret_cast<const LLQuad*>(sign_mask), mQ);
	}

	////////////////////////////////////
	// LOGICAL
	////////////////////////////////////
	// The methods in this section will compare the elements in this vector to
	// those in rhs and return an LLVector4Logical with all bits set in
	// elements where the comparison was true and all bits unset in elements
	// where the comparison was false. See llvector4logica.h
	////////////////////////////////////
	// WARNING: Other than equals3 and equals4, these functions do NOT account
	// for floating point tolerance. You should include the appropriate
	// tolerance in the inputs.
	////////////////////////////////////

	LL_INLINE LLVector4Logical greaterThan(const LLVector4a& rhs) const
	{
		return _mm_cmpgt_ps(mQ, rhs.mQ);
	}

	LL_INLINE LLVector4Logical lessThan(const LLVector4a& rhs) const
	{
		return _mm_cmplt_ps(mQ, rhs.mQ);
	}

	LL_INLINE LLVector4Logical greaterEqual(const LLVector4a& rhs) const
	{
		return _mm_cmpge_ps(mQ, rhs.mQ);
	}

	LL_INLINE LLVector4Logical lessEqual(const LLVector4a& rhs) const
	{
		return _mm_cmple_ps(mQ, rhs.mQ);
	}

	LL_INLINE LLVector4Logical equal(const LLVector4a& rhs) const
	{
		return _mm_cmpeq_ps(mQ, rhs.mQ);
	}

	// Returns true if this and rhs are componentwise equal up to the specified
	// absolute tolerance
	LL_INLINE bool equals4(const LLVector4a& rhs,
						   F32 tolerance = F_APPROXIMATELY_ZERO) const;

	LL_INLINE bool equals3(const LLVector4a& rhs,
						   F32 tolerance = F_APPROXIMATELY_ZERO) const;

	////////////////////////////////////
	// OPERATORS
	////////////////////////////////////

	// Do NOT add aditional operators without consulting someone with SSE
	// experience
	LL_INLINE const LLVector4a& operator=(const LLVector4a& rhs)
	{
		mQ = rhs.mQ;
		return *this;
	}

	LL_INLINE const LLVector4a& operator=(const LLQuad& rhs)
	{
		mQ = rhs;
		return *this;
	}

	LL_INLINE operator LLQuad() const
	{
		return mQ;
	}

private:
	LLQuad							mQ;
};

LL_INLINE void update_min_max(LLVector4a& min, LLVector4a& max,
							  const LLVector4a& p)
{
	min.setMin(min, p);
	max.setMax(max, p);
}

#endif
