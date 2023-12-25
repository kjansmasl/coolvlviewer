/**
 * @file llmatrix3.h
 * @brief LLMatrix3 class header file.
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

// Rotation matrix hints...

// Inverse of Rotation Matrices
// ----------------------------
// If R is a rotation matrix that rotate vectors from Frame-A to Frame-B,
// then the transpose of R will rotate vectors from Frame-B to Frame-A.

// Creating Rotation Matricies From Object Axes
// --------------------------------------------
// Suppose you know the three axes of some object in some "absolute-frame".
// If you take those three vectors and throw them into the rows of
// a rotation matrix what do you get?
//
// R = | X0  X1  X2 |
//     | Y0  Y1  Y2 |
//     | Z0  Z1  Z2 |
//
// Yeah, but what does it mean?
//
// Transpose the matrix and have it operate on a vector...
//
// V * R_transpose = [ V0  V1  V2 ] * | X0  Y0  Z0 |
//                                    | X1  Y1  Z1 |
//                                    | X2  Y2  Z2 |
//
//                 = [ V*X  V*Y  V*Z ]
//
//                 = components of V that are parallel to the three object axes
//
//                 = transformation of V into object frame
//
// Since the transformation of a rotation matrix is its inverse, then
// R must rotate vectors from the object-frame into the absolute-frame.

#ifndef LL_M3MATH_H
#define LL_M3MATH_H

#include "llerror.h"
#include "stdtypes.h"

class LLVector4;
class LLVector3;
class LLVector3d;
class LLQuaternion;

// NOTA BENE: Currently assuming a right-handed, z-up universe

//			     ji
// LLMatrix3 = | 00 01 02 |
//			   | 10 11 12 |
//			   | 20 21 22 |

// LLMatrix3 = | fx fy fz |	forward-axis
//			   | lx ly lz |	left-axis
//			   | ux uy uz |	up-axis

// NOTE: The world of computer graphics uses column-vectors and matricies that
// "operate to the left".

constexpr U32 NUM_VALUES_IN_MAT3 = 3;

class LLMatrix3
{
public:
	LLMatrix3() noexcept;	/// Initializes Matrix to identity matrix

	// Initializes Matrix to values in mat
	explicit LLMatrix3(const F32* mat) noexcept;
	// Initializes Matrix with rotation q
	explicit LLMatrix3(const LLQuaternion& q) noexcept;

	// Initializes Matrix with axis angle
	LLMatrix3(F32 angle, const LLVector3& vec) noexcept;
	// Initializes Matrix with axis angle
	LLMatrix3(F32 angle, const LLVector3d& vec) noexcept;
	// Initializes Matrix with axis angle
	LLMatrix3(F32 angle, const LLVector4& vec) noexcept;
	// Initializes Matrix with Euler angles
	LLMatrix3(F32 roll, F32 pitch, F32 yaw) noexcept;

	// Allow the use of the default C++11 move constructor and assignation
	LLMatrix3(LLMatrix3&& other) noexcept = default;
	LLMatrix3& operator=(LLMatrix3&& other) noexcept = default;

	LLMatrix3(const LLMatrix3& other) = default;
	LLMatrix3& operator=(const LLMatrix3& other) = default;

	// Returns a "this" as an F32 pointer.
	LL_INLINE F32* getF32ptr()
	{
		return (F32*)mMatrix;
	}

	// Returns a "this" as a const F32 pointer.
	LL_INLINE const F32* const getF32ptr() const
	{
		return (const F32* const)mMatrix;
	}

	//////////////////////////////
	// Matrix initializers - these replace any existing values in the matrix

	// Various useful matrix functions
	const LLMatrix3& setIdentity();		// Load identity matrix
	const LLMatrix3& clear();			// Clears Matrix to zero
	const LLMatrix3& setZero();			// Clears Matrix to zero

	///////////////////////////
	// Matrix setters - set some properties without modifying others
	// These functions take Rotation arguments
	// Calculate rotation matrix for rotating angle radians about vec
	const LLMatrix3& setRot(F32 angle, const LLVector3& vec);
	// Calculate rotation matrix from Euler angles
	const LLMatrix3& setRot(F32 roll, F32 pitch, F32 yaw);
	// Transform matrix by Euler angles and translating by pos
	const LLMatrix3& setRot(const LLQuaternion& q);

	const LLMatrix3& setRows(const LLVector3& x_axis,
							 const LLVector3& y_axis,
							 const LLVector3& z_axis);
	const LLMatrix3& setRow(U32 rowIndex, const LLVector3& row);
	const LLMatrix3& setCol(U32 col_idx, const LLVector3& col);

	///////////////////////////
	// Get properties of a matrix

	// Returns quaternion from mat
	LLQuaternion quaternion() const;
	// Returns Euler angles, in radians
	void getEulerAngles(F32* roll, F32* pitch, F32* yaw) const;

	// Axis extraction routines
	LLVector3 getFwdRow() const;
	LLVector3 getLeftRow() const;
	LLVector3 getUpRow() const;
	// Returns determinant
	F32	 determinant() const;

	///////////////////////////
	// Operations on an existing matrix

	const LLMatrix3& transpose();		// Transpose MAT4
	const LLMatrix3& orthogonalize();	// Orthogonalizes X, then Y, then Z
	void invert();						// Invert MAT4
	// returns transpose of matrix adjoint, for multiplying normals
	const LLMatrix3& adjointTranspose();


	// Rotate existing matrix. Note: the two lines below are equivalent:
	//	foo.rotate(bar)
	//	foo = foo * bar
	// That is, foo.rotate(bar) multiplies foo by bar FROM THE RIGHT

	// Rotate matrix by rotating angle radians about vec
	const LLMatrix3& rotate(F32 angle, const LLVector3& vec);
	// Rotate matrix by roll (about x), pitch (about y), and yaw (about z)
	const LLMatrix3& rotate(F32 roll, F32 pitch, F32 yaw);
	// Transform matrix by Euler angles and translating by pos
	const LLMatrix3& rotate(const LLQuaternion& q);

	void add(const LLMatrix3& other_matrix);	// add other_matrix to this one

#if 0	// This operator is misleading as to operation direction
	// Apply rotation a to vector b
	friend LLVector3 operator*(const LLMatrix3& a, const LLVector3& b);
#endif
	// Applies rotation b to vector a
	friend LLVector3 operator*(const LLVector3& a, const LLMatrix3& b);
	// Applies rotation b to vector a
	friend LLVector3d operator*(const LLVector3d& a, const LLMatrix3& b);
	// Returns a * b
	friend LLMatrix3 operator*(const LLMatrix3& a, const LLMatrix3& b);

	friend bool operator==(const LLMatrix3& a, const LLMatrix3& b);
	friend bool operator!=(const LLMatrix3& a, const LLMatrix3& b);

	// Returns a * b
	friend const LLMatrix3& operator*=(LLMatrix3& a, const LLMatrix3& b);
	// Returns a * scalar
	friend const LLMatrix3& operator*=(LLMatrix3& a, F32 scalar);
	// Streams a
	friend std::ostream& operator<<(std::ostream& s, const LLMatrix3& a);

public:
	F32	mMatrix[NUM_VALUES_IN_MAT3][NUM_VALUES_IN_MAT3];
};

LL_INLINE LLMatrix3::LLMatrix3() noexcept
{
	mMatrix[0][0] = 1.f;
	mMatrix[0][1] = 0.f;
	mMatrix[0][2] = 0.f;

	mMatrix[1][0] = 0.f;
	mMatrix[1][1] = 1.f;
	mMatrix[1][2] = 0.f;

	mMatrix[2][0] = 0.f;
	mMatrix[2][1] = 0.f;
	mMatrix[2][2] = 1.f;
}

LL_INLINE LLMatrix3::LLMatrix3(const F32* mat) noexcept
{
	mMatrix[0][0] = mat[0];
	mMatrix[0][1] = mat[1];
	mMatrix[0][2] = mat[2];

	mMatrix[1][0] = mat[3];
	mMatrix[1][1] = mat[4];
	mMatrix[1][2] = mat[5];

	mMatrix[2][0] = mat[6];
	mMatrix[2][1] = mat[7];
	mMatrix[2][2] = mat[8];
}

#endif
