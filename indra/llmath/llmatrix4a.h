/**
 * @file llmatrix4a.h
 * @brief LLMatrix4a class header file - memory aligned and vectorized 4x4 matrix
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
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

#ifndef	LL_LLMATRIX4A_H
#define	LL_LLMATRIX4A_H

#include "llmath.h"
#include "llmatrix4.h"

class alignas(16) LLMatrix4a
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

	enum
	{
		ROW_FWD = 0,
		ROW_LEFT,
		ROW_UP,
		ROW_TRANS
	};

	LL_INLINE LLMatrix4a()
	{
	}

	LL_INLINE LLMatrix4a(const LLQuad& q1, const LLQuad& q2,
						 const LLQuad& q3, const LLQuad& q4)
	{
		mMatrix[0] = q1;
		mMatrix[1] = q2;
		mMatrix[2] = q3;
		mMatrix[3] = q4;
	}

	LL_INLINE LLMatrix4a(const LLQuaternion2& quat)
	{
		const LLVector4a& xyzw = quat.getVector4a();
		LLVector4a nyxwz = _mm_shuffle_ps(xyzw, xyzw, _MM_SHUFFLE(2, 3, 0, 1));
		nyxwz.negate();

		const LLVector4a xnyynx = _mm_unpacklo_ps(xyzw, nyxwz);
		const LLVector4a znwwnz = _mm_unpackhi_ps(xyzw, nyxwz);

		LLMatrix4a mata;
		mata.setRow<0>(_mm_shuffle_ps(xyzw, xnyynx, _MM_SHUFFLE(0, 1, 2, 3)));
		mata.setRow<1>(_mm_shuffle_ps(znwwnz, xyzw, _MM_SHUFFLE(1, 0, 2, 3)));
		mata.setRow<2>(_mm_shuffle_ps(xnyynx, xyzw, _MM_SHUFFLE(2, 3, 3, 2)));
		mata.setRow<3>(_mm_shuffle_ps(xnyynx, znwwnz, _MM_SHUFFLE(2, 3, 1, 3)));

		LLMatrix4a matb;
		matb.setRow<0>(_mm_shuffle_ps(xyzw, xnyynx, _MM_SHUFFLE(3, 1, 2, 3)));
		matb.setRow<1>(_mm_shuffle_ps(znwwnz, xnyynx, _MM_SHUFFLE(1, 0, 2, 3)));
		matb.setRow<2>(_mm_shuffle_ps(xnyynx, znwwnz, _MM_SHUFFLE(3, 2, 3, 2)));
		matb.setRow<3>(xyzw);

		setMul(matb, mata);
	}

	LL_INLINE explicit LLMatrix4a(const LLMatrix4& val)
	{
		loadu(val);
	}

	LL_INLINE F32* getF32ptr()
	{
		return mMatrix[0].getF32ptr();
	}

	LL_INLINE const F32* getF32ptr() const
	{
		return mMatrix[0].getF32ptr();
	}

	LL_INLINE void clear()
	{
		mMatrix[0].clear();
		mMatrix[1].clear();
		mMatrix[2].clear();
		mMatrix[3].clear();
	}

	LL_INLINE void setIdentity()
	{
		thread_local __m128 ones = _mm_set_ps(1.f, 0.f, 0.f, 1.f);
		thread_local __m128 zeroes = _mm_setzero_ps();
		mMatrix[0] = _mm_movelh_ps(ones, zeroes);
		mMatrix[1] = _mm_movehl_ps(zeroes, ones);
		mMatrix[2] = _mm_movelh_ps(zeroes, ones);
		mMatrix[3] = _mm_movehl_ps(ones, zeroes);
	}

	LL_INLINE void loadu(const LLMatrix4& src)
	{
		mMatrix[0] = _mm_loadu_ps(src.mMatrix[0]);
		mMatrix[1] = _mm_loadu_ps(src.mMatrix[1]);
		mMatrix[2] = _mm_loadu_ps(src.mMatrix[2]);
		mMatrix[3] = _mm_loadu_ps(src.mMatrix[3]);
	}

	LL_INLINE void loadu(const LLMatrix3& src)
	{
		mMatrix[0].load3(src.mMatrix[0]);
		mMatrix[1].load3(src.mMatrix[1]);
		mMatrix[2].load3(src.mMatrix[2]);
		mMatrix[3].set(0.f, 0.f, 0.f, 1.f);
	}

	LL_INLINE void loadu(const F32* src)
	{
		mMatrix[0] = _mm_loadu_ps(src);
		mMatrix[1] = _mm_loadu_ps(src + 4);
		mMatrix[2] = _mm_loadu_ps(src + 8);
		mMatrix[3] = _mm_loadu_ps(src + 12);
	}

	LL_INLINE void store4a(F32* dst) const
	{
		mMatrix[0].store4a(dst);
		mMatrix[1].store4a(dst + 4);
		mMatrix[2].store4a(dst + 8);
		mMatrix[3].store4a(dst + 12);
	}

	LL_INLINE void add(const LLMatrix4a& rhs)
	{
		mMatrix[0].add(rhs.mMatrix[0]);
		mMatrix[1].add(rhs.mMatrix[1]);
		mMatrix[2].add(rhs.mMatrix[2]);
		mMatrix[3].add(rhs.mMatrix[3]);
	}

	LL_INLINE void mul(const LLMatrix4a& rhs)
	{
		// Not using rotate4 to avoid extra copy of *this.
		LLVector4a x0, x1, x2, x3, y0, y1, y2, y3, z0, z1, z2, z3, w0, w1, w2,
				   w3;

		// 16 shuffles
		x0.splat<0>(rhs.mMatrix[0]);
		x1.splat<0>(rhs.mMatrix[1]);
		x2.splat<0>(rhs.mMatrix[2]);
		x3.splat<0>(rhs.mMatrix[3]);

		y0.splat<1>(rhs.mMatrix[0]);
		y1.splat<1>(rhs.mMatrix[1]);
		y2.splat<1>(rhs.mMatrix[2]);
		y3.splat<1>(rhs.mMatrix[3]);

		z0.splat<2>(rhs.mMatrix[0]);
		z1.splat<2>(rhs.mMatrix[1]);
		z2.splat<2>(rhs.mMatrix[2]);
		z3.splat<2>(rhs.mMatrix[3]);

		w0.splat<3>(rhs.mMatrix[0]);
		w1.splat<3>(rhs.mMatrix[1]);
		w2.splat<3>(rhs.mMatrix[2]);
		w3.splat<3>(rhs.mMatrix[3]);

		// 16 muls
		x0.mul(mMatrix[0]);
		x1.mul(mMatrix[0]);
		x2.mul(mMatrix[0]);
		x3.mul(mMatrix[0]);

		y0.mul(mMatrix[1]);
		y1.mul(mMatrix[1]);
		y2.mul(mMatrix[1]);
		y3.mul(mMatrix[1]);

		z0.mul(mMatrix[2]);
		z1.mul(mMatrix[2]);
		z2.mul(mMatrix[2]);
		z3.mul(mMatrix[2]);

		w0.mul(mMatrix[3]);
		w1.mul(mMatrix[3]);
		w2.mul(mMatrix[3]);
		w3.mul(mMatrix[3]);

		// 12 adds
		x0.add(y0);
		z0.add(w0);

		x1.add(y1);
		z1.add(w1);

		x2.add(y2);
		z2.add(w2);

		x3.add(y3);
		z3.add(w3);

		mMatrix[0].setAdd(x0, z0);
		mMatrix[1].setAdd(x1, z1);
		mMatrix[2].setAdd(x2, z2);
		mMatrix[3].setAdd(x3, z3);
	}

	LL_INLINE void setRows(const LLVector4a& r0, const LLVector4a& r1,
						   const LLVector4a& r2)
	{
		mMatrix[0] = r0;
		mMatrix[1] = r1;
		mMatrix[2] = r2;
	}

	template<int N>
	LL_INLINE void setRow(const LLVector4a& row)
	{
		mMatrix[N] = row;
	}

	template<int N>
	LL_INLINE const LLVector4a& getRow() const
	{
		return mMatrix[N];
	}

	template<int N>
	LL_INLINE LLVector4a& getRow()
	{
		return mMatrix[N];
	}

	template<int N>
	LL_INLINE void setColumn(const LLVector4a& col)
	{
		mMatrix[0].copyComponent<N>(col.getScalarAt<0>());
		mMatrix[1].copyComponent<N>(col.getScalarAt<1>());
		mMatrix[2].copyComponent<N>(col.getScalarAt<2>());
		mMatrix[3].copyComponent<N>(col.getScalarAt<3>());
	}

	template<int N>
	LL_INLINE LLVector4a getColumn()
	{
		LLVector4a v;
		v.clear();
		v.copyComponent<0>(mMatrix[0].getScalarAt<N>());
		v.copyComponent<1>(mMatrix[1].getScalarAt<N>());
		v.copyComponent<2>(mMatrix[2].getScalarAt<N>());
		v.copyComponent<3>(mMatrix[3].getScalarAt<N>());
		return v;
	}

	LL_INLINE void setMul(const LLMatrix4a& m, F32 s)
	{
		mMatrix[0].setMul(m.mMatrix[0], s);
		mMatrix[1].setMul(m.mMatrix[1], s);
		mMatrix[2].setMul(m.mMatrix[2], s);
		mMatrix[3].setMul(m.mMatrix[3], s);
	}

	LL_INLINE void setMul(const LLMatrix4a& m0, const LLMatrix4a& m1)
	{
		m0.rotate4(m1.mMatrix[0], mMatrix[0]);
		m0.rotate4(m1.mMatrix[1], mMatrix[1]);
		m0.rotate4(m1.mMatrix[2], mMatrix[2]);
		m0.rotate4(m1.mMatrix[3], mMatrix[3]);
	}

	LL_INLINE void setLerp(const LLMatrix4a& a, const LLMatrix4a& b, F32 w)
	{
		LLVector4a d0, d1, d2, d3;
		d0.setSub(b.mMatrix[0], a.mMatrix[0]);
		d1.setSub(b.mMatrix[1], a.mMatrix[1]);
		d2.setSub(b.mMatrix[2], a.mMatrix[2]);
		d3.setSub(b.mMatrix[3], a.mMatrix[3]);

		// this = a + d*w

		d0.mul(w);
		d1.mul(w);
		d2.mul(w);
		d3.mul(w);

		mMatrix[0].setAdd(a.mMatrix[0], d0);
		mMatrix[1].setAdd(a.mMatrix[1], d1);
		mMatrix[2].setAdd(a.mMatrix[2], d2);
		mMatrix[3].setAdd(a.mMatrix[3], d3);
	}

	LL_INLINE void rotate(const LLVector4a& v, LLVector4a& res) const
	{
		LLVector4a x, y, z;
		x.splat<0>(v);
		y.splat<1>(v);
		z.splat<2>(v);

		x.mul(mMatrix[0]);
		y.mul(mMatrix[1]);
		z.mul(mMatrix[2]);

		x.add(y);
		res.setAdd(x, z);
	}

	LL_INLINE void rotate4(const LLVector4a& v, LLVector4a& res) const
	{
		LLVector4a x, y, z, w;
		x.splat<0>(v);
		y.splat<1>(v);
		z.splat<2>(v);
		w.splat<3>(v);

		x.mul(mMatrix[0]);
		y.mul(mMatrix[1]);
		z.mul(mMatrix[2]);
		w.mul(mMatrix[3]);

		x.add(y);
		z.add(w);
		res.setAdd(x, z);
	}

	LL_INLINE void affineTransform(const LLVector4a& v, LLVector4a& res) const
	{
		LLVector4a x, y, z;
		x.splat<0>(v);
		y.splat<1>(v);
		z.splat<2>(v);

		x.mul(mMatrix[0]);
		y.mul(mMatrix[1]);
		z.mul(mMatrix[2]);

		x.add(y);
		z.add(mMatrix[3]);
		res.setAdd(x, z);
	}

	LL_INLINE const LLVector4a& getTranslation() const	{ return mMatrix[3]; }

	LL_INLINE void perspectiveTransform(const LLVector4a& v,
										LLVector4a& res) const
	{
		LLVector4a x, y, z, s, t, p, q;

		x.splat<0>(v);
		y.splat<1>(v);
		z.splat<2>(v);

		s.splat<3>(mMatrix[0]);
		t.splat<3>(mMatrix[1]);
		p.splat<3>(mMatrix[2]);
		q.splat<3>(mMatrix[3]);

		s.mul(x);
		t.mul(y);
		p.mul(z);
		q.add(s);
		t.add(p);
		q.add(t);

		x.mul(mMatrix[0]);
		y.mul(mMatrix[1]);
		z.mul(mMatrix[2]);

		x.add(y);
		z.add(mMatrix[3]);
		res.setAdd(x, z);
		res.div(q);
	}

	LL_INLINE void transpose()
	{
		__m128 q1 = _mm_unpackhi_ps(mMatrix[0], mMatrix[1]);
		__m128 q2 = _mm_unpacklo_ps(mMatrix[0], mMatrix[1]);
		__m128 q3 = _mm_unpacklo_ps(mMatrix[2], mMatrix[3]);
		__m128 q4 = _mm_unpackhi_ps(mMatrix[2], mMatrix[3]);

		mMatrix[0] = _mm_movelh_ps(q2, q3);
		mMatrix[1] = _mm_movehl_ps(q3, q2);
		mMatrix[2] = _mm_movelh_ps(q1, q4);
		mMatrix[3] = _mm_movehl_ps(q4, q1);
	}

	//  Following function is adapted from:
	//	http://software.intel.com/en-us/articles/optimized-matrix-library-for-use-with-the-intel-pentiumr-4-processors-sse2-instructions/
	//
	// License/Copyright Statement:
	//
	// Copyright (c) 2001 Intel Corporation.
	//
	// Permition is granted to use, copy, distribute and prepare derivative
	// works of this library for any purpose and without fee, provided, that
	// the above copyright notice and this statement appear in all copies.
	// Intel makes no representations about the suitability of this library for
	// any purpose, and specifically disclaims all warranties.
	// See LEGAL-intel_matrixlib.TXT for all the legal information.
	LL_INLINE F32 invert()
	{
		alignas(16) const unsigned int Sign_PNNP[4] = { 0x00000000,
														0x80000000,
														0x80000000,
														0x00000000 };

		// The inverse is calculated using "Divide and Conquer" technique. The
		// original matrix is divide into four 2x2 sub-matrices. Since each
		// register holds four matrix element, the smaller matrices are
		// represented as a registers. Hence we get a better locality of the
		// calculations.

	    // The four sub-matrices:
		LLVector4a A = _mm_movelh_ps(mMatrix[0], mMatrix[1]);
		LLVector4a B = _mm_movehl_ps(mMatrix[1], mMatrix[0]);
		LLVector4a C = _mm_movelh_ps(mMatrix[2], mMatrix[3]);
		LLVector4a D = _mm_movehl_ps(mMatrix[3], mMatrix[2]);
		// Partial inverse of the sub-matrices:
		LLVector4a iA, iB, iC, iD;
		LLVector4a DC, AB;
		// Determinant of the sub-matrices:
		LLSimdScalar dA, dB, dC, dD;
		LLSimdScalar det, d, d1, d2;
		LLVector4a rd;

		//  AB = A# * B
		AB.setMul(_mm_shuffle_ps(A, A, 0x0F), B);
		AB.sub(_mm_mul_ps(_mm_shuffle_ps(A, A, 0xA5),
			   _mm_shuffle_ps(B, B, 0x4E)));
		//  DC = D# * C
		DC.setMul(_mm_shuffle_ps(D,D,0x0F), C);
		DC.sub(_mm_mul_ps(_mm_shuffle_ps(D, D, 0xA5),
			   _mm_shuffle_ps(C, C, 0x4E)));

		//  dA = |A|
		dA = _mm_mul_ps(_mm_shuffle_ps(A, A, 0x5F), A);
		dA -= _mm_movehl_ps(dA, dA);
		//  dB = |B|
		dB = _mm_mul_ps(_mm_shuffle_ps(B, B, 0x5F), B);
		dB -= _mm_movehl_ps(dB, dB);

		//  dC = |C|
		dC = _mm_mul_ps(_mm_shuffle_ps(C, C, 0x5F), C);
		dC -= _mm_movehl_ps(dC, dC);
		//  dD = |D|
		dD = _mm_mul_ps(_mm_shuffle_ps(D, D, 0x5F), D);
		dD -= _mm_movehl_ps(dD, dD);

		//  d = trace(AB*DC) = trace(A#*B*D#*C)
		d = _mm_mul_ps(_mm_shuffle_ps(DC, DC, 0xD8), AB);

		//  iD = C*A#*B
		iD.setMul(_mm_shuffle_ps(C, C, 0xA0), _mm_movelh_ps(AB, AB));
		iD.add(_mm_mul_ps(_mm_shuffle_ps(C, C, 0xF5), _mm_movehl_ps(AB, AB)));
		//  iA = B*D#*C
		iA.setMul(_mm_shuffle_ps(B,B,0xA0), _mm_movelh_ps(DC,DC));
		iA.add(_mm_mul_ps(_mm_shuffle_ps(B, B, 0xF5), _mm_movehl_ps(DC, DC)));

		//  d = trace(AB*DC) = trace(A#*B*D#*C) [continue]
		d = _mm_add_ps(d, _mm_movehl_ps(d, d));
		d += _mm_shuffle_ps(d, d, 1);
		d1 = dA * dD;
		d2 = dB * dC;

		//  iD = D*|A| - C*A#*B
		iD.setSub(_mm_mul_ps(D, _mm_shuffle_ps(dA, dA, 0)), iD);

		//  iA = A*|D| - B*D#*C;
		iA.setSub(_mm_mul_ps(A, _mm_shuffle_ps(dD, dD, 0)), iA);

		//  det = |A|*|D| + |B|*|C| - trace(A#*B*D#*C)
		det = d1 + d2 - d;

		__m128 is_zero_mask = _mm_cmpeq_ps(det,_mm_setzero_ps());
		rd = _mm_div_ss(_mm_set_ss(1.f),
						_mm_or_ps(_mm_andnot_ps(is_zero_mask, det),
						_mm_and_ps(is_zero_mask, _mm_set_ss(1.f))));
#ifdef ZERO_SINGULAR
		rd = _mm_and_ps(_mm_cmpneq_ss(det, _mm_setzero_ps()), rd);
#endif

		//  iB = D * (A#B)# = D*B#*A
		iB.setMul(D, _mm_shuffle_ps(AB, AB, 0x33));
		iB.sub(_mm_mul_ps(_mm_shuffle_ps(D, D, 0xB1),
						  _mm_shuffle_ps(AB, AB, 0x66)));
		//  iC = A * (D#C)# = A*C#*D
		iC.setMul(A, _mm_shuffle_ps(DC, DC, 0x33));
		iC.sub(_mm_mul_ps(_mm_shuffle_ps(A, A, 0xB1),
						  _mm_shuffle_ps(DC, DC, 0x66)));

		rd = _mm_shuffle_ps(rd, rd, 0);
		rd = _mm_xor_ps(rd, _mm_load_ps((const F32*)Sign_PNNP));

		//  iB = C*|B| - D*B#*A
		iB.setSub(_mm_mul_ps(C, _mm_shuffle_ps(dB, dB, 0)), iB);

		//  iC = B*|C| - A*C#*D;
		iC.setSub(_mm_mul_ps(B, _mm_shuffle_ps(dC, dC, 0)), iC);

		//  iX = iX / det
		iA.mul(rd);
		iB.mul(rd);
		iC.mul(rd);
		iD.mul(rd);

		mMatrix[0] = _mm_shuffle_ps(iA, iB, 0x77);
		mMatrix[1] = _mm_shuffle_ps(iA, iB, 0x22);
		mMatrix[2] = _mm_shuffle_ps(iC, iD, 0x77);
		mMatrix[3] = _mm_shuffle_ps(iC, iD, 0x22);

		F32 ret;
		_mm_store_ss(&ret, det);
		return ret;
	}

	LL_INLINE LLVector4a rowMul(const LLVector4a& row) const
	{
		LLVector4a result;
		result = _mm_mul_ps(_mm_shuffle_ps(row, row, _MM_SHUFFLE(0, 0, 0, 0)),
									 	   mMatrix[0]);
		result = _mm_add_ps(result,
							_mm_mul_ps(_mm_shuffle_ps(row, row,
													  _MM_SHUFFLE(1, 1, 1, 1)),
							mMatrix[1]));
		result = _mm_add_ps(result,
							_mm_mul_ps(_mm_shuffle_ps(row, row,
													  _MM_SHUFFLE(2, 2, 2, 2)),
							mMatrix[2]));
		result = _mm_add_ps(result,
							_mm_mul_ps(_mm_shuffle_ps(row, row,
													  _MM_SHUFFLE(3, 3, 3, 3)),
							mMatrix[3]));
		return result;
	}

	LL_INLINE void matMul(const LLMatrix4a& a, const LLMatrix4a& b)
	{
		mMatrix[0] = b.rowMul(a.mMatrix[0]);
		mMatrix[1] = b.rowMul(a.mMatrix[1]);
		mMatrix[2] = b.rowMul(a.mMatrix[2]);
		mMatrix[3] = b.rowMul(a.mMatrix[3]);
	}

	void matMulBoundBox(const LLVector4a* in_extents,
						LLVector4a* out_extents) const;

	// ================== Affine transformation matrix only ===================

	// Multiply matrix with a pure translation matrix.
	LL_INLINE void applyTranslationAffine(F32 x, F32 y, F32 z)
	{
		const LLVector4a xyz0(x, y, z, 0.f);
		LLVector4a xxxx, yyyy, zzzz;
		xxxx.splat<0>(xyz0);
		yyyy.splat<1>(xyz0);
		zzzz.splat<2>(xyz0);

		LLVector4a sum1, sum2, sum3;
		sum1.setMul(xxxx, mMatrix[0]);
		sum2.setMul(yyyy, mMatrix[1]);
		sum3.setMul(zzzz, mMatrix[2]);

		mMatrix[3].add(sum1);
		mMatrix[3].add(sum2);
		mMatrix[3].add(sum3);
	}

	// Multiply matrix with a pure translation matrix.
	LL_INLINE void applyTranslationAffine(const LLVector3& trans)
	{
		applyTranslationAffine(trans.mV[VX], trans.mV[VY], trans.mV[VZ]);
	}

	// Multiply matrix with a pure scale matrix.
	LL_INLINE void applyScaleAffine(F32 x, F32 y, F32 z)
	{
		const LLVector4a xyz0(x, y, z, 0);
		LLVector4a xxxx, yyyy, zzzz;
		xxxx.splat<0>(xyz0);
		yyyy.splat<1>(xyz0);
		zzzz.splat<2>(xyz0);

		mMatrix[0].mul(xxxx);
		mMatrix[1].mul(yyyy);
		mMatrix[2].mul(zzzz);
	}

	// Multiply matrix with a pure scale matrix.
	LL_INLINE void applyScaleAffine(const LLVector3& scale)
	{
		applyScaleAffine(scale.mV[VX], scale.mV[VY], scale.mV[VZ]);
	}

	// Multiply matrix with a pure scale matrix.
	LL_INLINE void applyScaleAffine(F32 s)
	{
		const LLVector4a scale(s);
		mMatrix[0].mul(scale);
		mMatrix[1].mul(scale);
		mMatrix[2].mul(scale);
	}

	// Direct addition to row3.
	LL_INLINE void translateAffine(const LLVector3& trans)
	{
		LLVector4a translation;
		translation.load3(trans.mV);
		mMatrix[3].add(translation);
	}

	// Direct assignment of row3.
	LL_INLINE void setTranslateAffine(const LLVector3& trans)
	{
		thread_local const LLVector4Logical mask =
			_mm_load_ps((F32*)&S_V4LOGICAL_MASK_TABLE[12]);

		LLVector4a translation;
		translation.load3(trans.mV);

		mMatrix[3].setSelectWithMask(mask, mMatrix[3], translation);
	}

	LL_INLINE void mulAffine(const LLMatrix4a& rhs)
	{
		LLVector4a x0, y0, z0, x1, y1, z1, x2, y2, z2, x3, y3, z3;

		// 12 shuffles
		x0.splat<0>(rhs.mMatrix[0]);
		x1.splat<0>(rhs.mMatrix[1]);
		x2.splat<0>(rhs.mMatrix[2]);
		x3.splat<0>(rhs.mMatrix[3]);

		y0.splat<1>(rhs.mMatrix[0]);
		y1.splat<1>(rhs.mMatrix[1]);
		y2.splat<1>(rhs.mMatrix[2]);
		y3.splat<1>(rhs.mMatrix[3]);

		z0.splat<2>(rhs.mMatrix[0]);
		z1.splat<2>(rhs.mMatrix[1]);
		z2.splat<2>(rhs.mMatrix[2]);
		z3.splat<2>(rhs.mMatrix[3]);

		// 12 muls
		x0.mul(mMatrix[0]);
		x1.mul(mMatrix[0]);
		x2.mul(mMatrix[0]);
		x3.mul(mMatrix[0]);

		y0.mul(mMatrix[1]);
		y1.mul(mMatrix[1]);
		y2.mul(mMatrix[1]);
		y3.mul(mMatrix[1]);

		z0.mul(mMatrix[2]);
		z1.mul(mMatrix[2]);
		z2.mul(mMatrix[2]);
		z3.mul(mMatrix[2]);

		// 9 adds
		x0.add(y0);

		x1.add(y1);

		x2.add(y2);

		x3.add(y3);
		z3.add(mMatrix[3]);

		mMatrix[0].setAdd(x0, z0);
		mMatrix[1].setAdd(x1, z1);
		mMatrix[2].setAdd(x2, z2);
		mMatrix[3].setAdd(x3, z3);
	}

	LL_INLINE void extractRotationAffine()
	{
		thread_local const LLVector4Logical mask =
			_mm_load_ps((F32*)&S_V4LOGICAL_MASK_TABLE[12]);
		mMatrix[0].setSelectWithMask(mask, _mm_setzero_ps(), mMatrix[0]);
		mMatrix[1].setSelectWithMask(mask, _mm_setzero_ps(), mMatrix[1]);
		mMatrix[2].setSelectWithMask(mask, _mm_setzero_ps(), mMatrix[2]);
		mMatrix[3].setSelectWithMask(mask, LLVector4a(1.f), _mm_setzero_ps());
	}

	bool isIdentity() const;

public:
	alignas(16) LLVector4a mMatrix[4];
};

#endif
