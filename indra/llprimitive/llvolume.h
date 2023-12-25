/**
 * @file llvolume.h
 * @brief LLVolume base class.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLVOLUME_H
#define LL_LLVOLUME_H

#include <iostream>
#include <vector>

#if LL_JEMALLOC
# include "jemalloc/jemalloc.h"
# include "hbtracy.h"
#endif

class LLPath;
class LLPathParams;
class LLProfile;
class LLProfileParams;
class LLVolume;
class LLVolumeFace;
class LLVolumeParams;
class LLVolumeTriangle;
template<class T> class LLPointer;
template <class T, typename B> class _LLOctreeRoot;
template <class T, typename B> class _LLOctreeNode;

template<typename T>
using LLOctreeRootNoOwnership = _LLOctreeRoot<T, T*>;

template<class T>
using LLOctreeNodeNoOwnership = _LLOctreeNode<T, T*>;

#include "llalignedarray.h"
#include "llcolor4.h"
#include "llcolor4u.h"
#include "llfile.h"
#include "llmath.h"
#include "llmatrix4a.h"
#include "llmutex.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llstrider.h"
#include "lluuid.h"
#include "llvector2.h"
#include "llvector3.h"
#include "llvector3d.h"
#include "llvector4.h"

// Define to 1 in case we add support for on-the-wire tangents
#define LL_USE_TANGENTS 0

// Forward declarations
LL_INLINE void* allocate_volume_mem(size_t size);
LL_INLINE void free_volume_mem(void* addr) noexcept;

constexpr S32 MIN_DETAIL_FACES = 6;

// These are defined here but are not enforced at this level, rather they are
// here for the convenience of code that uses the LLVolume class.
constexpr F32 MIN_VOLUME_PROFILE_WIDTH	= 0.05f;
constexpr F32 MIN_VOLUME_PATH_WIDTH		= 0.05f;

constexpr F32 CUT_QUANTA	= 0.00002f;
constexpr F32 SCALE_QUANTA	= 0.01f;
constexpr F32 SHEAR_QUANTA	= 0.01f;
constexpr F32 TAPER_QUANTA	= 0.01f;
constexpr F32 REV_QUANTA	= 0.015f;
constexpr F32 HOLLOW_QUANTA	= 0.00002f;

constexpr S32 MAX_VOLUME_TRIANGLE_INDICES = 10000;

// Useful masks
constexpr LLPCode LL_PCODE_HOLLOW_MASK 	= 0x80;	// Has a thickness
constexpr LLPCode LL_PCODE_SEGMENT_MASK = 0x40;	// Segments (1 angle)
constexpr LLPCode LL_PCODE_PATCH_MASK 	= 0x20;	// Segmented segments (2 angles)
constexpr LLPCode LL_PCODE_HEMI_MASK 	= 0x10;	// Half-prims get their own type
constexpr LLPCode LL_PCODE_BASE_MASK 	= 0x0F;

// Primitive shapes
constexpr LLPCode LL_PCODE_CUBE			= 1;
constexpr LLPCode LL_PCODE_PRISM		= 2;
constexpr LLPCode LL_PCODE_TETRAHEDRON	= 3;
constexpr LLPCode LL_PCODE_PYRAMID		= 4;
constexpr LLPCode LL_PCODE_CYLINDER		= 5;
constexpr LLPCode LL_PCODE_CONE			= 6;
constexpr LLPCode LL_PCODE_SPHERE		= 7;
constexpr LLPCode LL_PCODE_TORUS		= 8;
constexpr LLPCode LL_PCODE_VOLUME		= 9;

// Surfaces (deprecated)
//constexpr LLPCode	LL_PCODE_SURFACE_TRIANGLE = 10;
//constexpr LLPCode	LL_PCODE_SURFACE_SQUARE = 11;
//constexpr LLPCode	LL_PCODE_SURFACE_DISC = 12;

// App specific pcode (for viewer/sim side only objects)
constexpr LLPCode LL_PCODE_APP			= 14;

constexpr LLPCode LL_PCODE_LEGACY		= 15;

// Pcodes for legacy objects

constexpr LLPCode LL_PCODE_LEGACY_AVATAR	= 0x20 | LL_PCODE_LEGACY;
constexpr LLPCode LL_PCODE_LEGACY_GRASS		= 0x50 | LL_PCODE_LEGACY;
constexpr LLPCode LL_PCODE_LEGACY_PART_SYS	= 0x80 | LL_PCODE_LEGACY;
constexpr LLPCode LL_PCODE_LEGACY_ROCK		= 0x90 | LL_PCODE_LEGACY;
constexpr LLPCode LL_PCODE_LEGACY_TREE		= 0xF0 | LL_PCODE_LEGACY;

// Deprecated
//constexpr LLPCode	LL_PCODE_LEGACY_ATOR	= 0x10 | LL_PCODE_LEGACY;
//constexpr LLPCode	LL_PCODE_LEGACY_BIRD	= 0x30 | LL_PCODE_LEGACY;
//constexpr LLPCode	LL_PCODE_LEGACY_DEMON	= 0x40 | LL_PCODE_LEGACY;
//constexpr LLPCode	LL_PCODE_TREE_NEW		= 0x60 | LL_PCODE_LEGACY;
//constexpr LLPCode	LL_PCODE_LEGACY_ORACLE	= 0x70 | LL_PCODE_LEGACY;
//constexpr LLPCode	LL_PCODE_LEGACY_SHOT	= 0xA0 | LL_PCODE_LEGACY;
//constexpr LLPCode	LL_PCODE_LEGACY_SHOT_BIG= 0xB0 | LL_PCODE_LEGACY;
//constexpr LLPCode	LL_PCODE_LEGACY_SMOKE	= 0xC0 | LL_PCODE_LEGACY;
//constexpr LLPCode	LL_PCODE_LEGACY_SPARK	= 0xD0 | LL_PCODE_LEGACY;
//constexpr LLPCode	LL_PCODE_LEGACY_TEXT_BUBBLE= 0xE0 | LL_PCODE_LEGACY;

// Hemis
constexpr LLPCode LL_PCODE_CYLINDER_HEMI = LL_PCODE_HEMI_MASK | LL_PCODE_CYLINDER;
constexpr LLPCode LL_PCODE_CONE_HEMI	 = LL_PCODE_HEMI_MASK | LL_PCODE_CONE;
constexpr LLPCode LL_PCODE_SPHERE_HEMI	 = LL_PCODE_HEMI_MASK | LL_PCODE_SPHERE;
constexpr LLPCode LL_PCODE_TORUS_HEMI	 = LL_PCODE_HEMI_MASK | LL_PCODE_TORUS;

// Volumes consist of a profile at the base that is swept around a path to make
// a volume.

// The profile code
constexpr U8 LL_PCODE_PROFILE_MASK		  = 0x0f;
constexpr U8 LL_PCODE_PROFILE_MIN		  = 0x00;
constexpr U8 LL_PCODE_PROFILE_CIRCLE	  = 0x00;
constexpr U8 LL_PCODE_PROFILE_SQUARE	  = 0x01;
constexpr U8 LL_PCODE_PROFILE_ISOTRI	  = 0x02;
constexpr U8 LL_PCODE_PROFILE_EQUALTRI	  = 0x03;
constexpr U8 LL_PCODE_PROFILE_RIGHTTRI	  = 0x04;
constexpr U8 LL_PCODE_PROFILE_CIRCLE_HALF = 0x05;
constexpr U8 LL_PCODE_PROFILE_MAX		  = 0x05;

// Stored in the profile byte
constexpr U8 LL_PCODE_HOLE_MASK		= 0xf0;
constexpr U8 LL_PCODE_HOLE_MIN		= 0x00;
constexpr U8 LL_PCODE_HOLE_SAME		= 0x00; // Same as outside profile
constexpr U8 LL_PCODE_HOLE_CIRCLE	= 0x10;
constexpr U8 LL_PCODE_HOLE_SQUARE	= 0x20;
constexpr U8 LL_PCODE_HOLE_TRIANGLE	= 0x30;
constexpr U8 LL_PCODE_HOLE_MAX		= 0x03; // Min/max needs to be >> 4 of real min/max

constexpr U8 LL_PCODE_PATH_IGNORE	= 0x00;
constexpr U8 LL_PCODE_PATH_MIN		= 0x01; // Min/max needs to be >> 4 of real min/max
constexpr U8 LL_PCODE_PATH_LINE		= 0x10;
constexpr U8 LL_PCODE_PATH_CIRCLE	= 0x20;
constexpr U8 LL_PCODE_PATH_CIRCLE2	= 0x30;
constexpr U8 LL_PCODE_PATH_TEST		= 0x40;
constexpr U8 LL_PCODE_PATH_FLEXIBLE	= 0x80;
constexpr U8 LL_PCODE_PATH_MAX		= 0x08;

// Face identifiers
typedef U16 LLFaceID;

constexpr LLFaceID LL_FACE_PATH_BEGIN		= 0x1 << 0;
constexpr LLFaceID LL_FACE_PATH_END			= 0x1 << 1;
constexpr LLFaceID LL_FACE_INNER_SIDE		= 0x1 << 2;
constexpr LLFaceID LL_FACE_PROFILE_BEGIN	= 0x1 << 3;
constexpr LLFaceID LL_FACE_PROFILE_END		= 0x1 << 4;
constexpr LLFaceID LL_FACE_OUTER_SIDE_0		= 0x1 << 5;
constexpr LLFaceID LL_FACE_OUTER_SIDE_1		= 0x1 << 6;
constexpr LLFaceID LL_FACE_OUTER_SIDE_2		= 0x1 << 7;
constexpr LLFaceID LL_FACE_OUTER_SIDE_3		= 0x1 << 8;

// Sculpt types + flags

constexpr U8 LL_SCULPT_TYPE_NONE      = 0;
constexpr U8 LL_SCULPT_TYPE_SPHERE    = 1;
constexpr U8 LL_SCULPT_TYPE_TORUS     = 2;
constexpr U8 LL_SCULPT_TYPE_PLANE     = 3;
constexpr U8 LL_SCULPT_TYPE_CYLINDER  = 4;
constexpr U8 LL_SCULPT_TYPE_MESH      = 5;
constexpr U8 LL_SCULPT_TYPE_MASK      = LL_SCULPT_TYPE_SPHERE |
									LL_SCULPT_TYPE_TORUS |
									LL_SCULPT_TYPE_PLANE |
									LL_SCULPT_TYPE_CYLINDER |
									LL_SCULPT_TYPE_MESH;

// For value checks, assign new value after adding new types
constexpr U8 LL_SCULPT_TYPE_MAX = LL_SCULPT_TYPE_MESH;

constexpr U8 LL_SCULPT_FLAG_INVERT    = 64;
constexpr U8 LL_SCULPT_FLAG_MIRROR    = 128;
constexpr U8 LL_SCULPT_FLAG_MASK = LL_SCULPT_FLAG_INVERT | LL_SCULPT_FLAG_MIRROR;

constexpr S32 LL_SCULPT_MESH_MAX_FACES = 8;

class LLProfileParams
{
protected:
	LOG_CLASS(LLProfileParams);

public:
	LLProfileParams()
	:	mCurveType(LL_PCODE_PROFILE_SQUARE),
		mBegin(0.f),
		mEnd(1.f),
		mHollow(0.f),
		mCRC(0)
	{
	}

	LLProfileParams(U8 curve, F32 begin, F32 end, F32 hollow)
	:	mCurveType(curve),
		mBegin(begin),
		mEnd(end),
		mHollow(hollow),
		mCRC(0)
	{
	}

	LLProfileParams(U8 curve, U16 begin, U16 end, U16 hollow)
	{
		mCurveType = curve;
		F32 temp_f32 = begin * CUT_QUANTA;
		if (temp_f32 > 1.f)
		{
			temp_f32 = 1.f;
		}
		mBegin = temp_f32;
		temp_f32 = end * CUT_QUANTA;
		if (temp_f32 > 1.f)
		{
			temp_f32 = 1.f;
		}
		mEnd = 1.f - temp_f32;
		temp_f32 = hollow * HOLLOW_QUANTA;
		if (temp_f32 > 1.f)
		{
			temp_f32 = 1.f;
		}
		mHollow = temp_f32;
		mCRC = 0;
	}

	bool operator==(const LLProfileParams& params) const;
	bool operator!=(const LLProfileParams& params) const;
	bool operator<(const LLProfileParams& params) const;

	void copyParams(const LLProfileParams& params);

	bool importFile(LLFILE* fp);
	bool exportFile(LLFILE* fp) const;

	bool importLegacyStream(std::istream& input_stream);
	bool exportLegacyStream(std::ostream& output_stream) const;

	LLSD asLLSD() const;
	LL_INLINE operator LLSD() const						{ return asLLSD(); }
	bool fromLLSD(LLSD& sd);

	LL_INLINE F32 getBegin() const						{ return mBegin; }
	LL_INLINE F32 getEnd() const						{ return mEnd; }
	LL_INLINE F32 getHollow() const						{ return mHollow; }
	LL_INLINE U8 getCurveType() const					{ return mCurveType; }

	LL_INLINE void setCurveType(U32 type)				{ mCurveType = type; }

	LL_INLINE void setBegin(F32 begin)
	{
		mBegin = begin >= 1.f ? 0.f : ((S32)(begin * 100000.f)) / 100000.f;
	}

	LL_INLINE void setEnd(F32 end)
	{
		mEnd = end <= 0.f ? 1.f : ((S32)(end * 100000.f)) / 100000.f;
	}

	LL_INLINE void setHollow(F32 hollow)
	{
		mHollow = ((S32)(hollow * 100000.f)) / 100000.f;
	}

	friend std::ostream& operator<<(std::ostream& s,
									const LLProfileParams& profile_params);

protected:
	// Profile params
	F32	mBegin;
	F32	mEnd;
	F32	mHollow;
	U32	mCRC;
	U8	mCurveType;
};

LL_INLINE bool LLProfileParams::operator==(const LLProfileParams& params) const
{
	return getCurveType() == params.getCurveType() &&
		   getBegin() == params.getBegin() &&
		   getEnd() == params.getEnd() &&
		   getHollow() == params.getHollow();
}

LL_INLINE bool LLProfileParams::operator!=(const LLProfileParams& params) const
{
	return getCurveType() != params.getCurveType() ||
		   getBegin() != params.getBegin() ||
		   getEnd() != params.getEnd() ||
		   getHollow() != params.getHollow();
}


LL_INLINE bool LLProfileParams::operator<(const LLProfileParams& params) const
{
	if (getCurveType() != params.getCurveType())
	{
		return getCurveType() < params.getCurveType();
	}
	if (getBegin() != params.getBegin())
	{
		return getBegin() < params.getBegin();
	}
	if (getEnd() != params.getEnd())
	{
		return getEnd() < params.getEnd();
	}
	return getHollow() < params.getHollow();
}

#define U8_TO_F32(x) (F32)(*((S8*)&x))

class LLPathParams
{
protected:
	LOG_CLASS(LLPathParams);

public:
	LLPathParams()
	:	mCurveType(LL_PCODE_PATH_LINE),
		mBegin(0.f),
		mEnd(1.f),
		mScale(1.f, 1.f),
		mShear(0.f, 0.f),
		mTwistBegin(0.f),
		mTwistEnd(0.f),
		mRadiusOffset(0.f),
		mTaper(0.f, 0.f),
		mRevolutions(1.f),
		mSkew(0.f),
		mCRC(0)
	{
	}

	LLPathParams(U8 curve, F32 begin, F32 end, F32 scx, F32 scy,
				 F32 shx, F32 shy, F32 twistend, F32 twistbegin,
				 F32 radiusoffset, F32 tx, F32 ty, F32 revolutions, F32 skew)
	:	mCurveType(curve),
		mBegin(begin),
		mEnd(end),
		mScale(scx, scy),
		mShear(shx, shy),
		mTwistBegin(twistbegin),
		mTwistEnd(twistend),
		mRadiusOffset(radiusoffset),
		mTaper(tx, ty),
		mRevolutions(revolutions),
		mSkew(skew),
		mCRC(0)
	{
	}

	LLPathParams(U8 curve, U16 begin, U16 end, U8 scx, U8 scy, U8 shx, U8 shy,
				 U8 twistend, U8 twistbegin, U8 radiusoffset, U8 tx, U8 ty,
				 U8 revolutions, U8 skew)
	{
		mCurveType = curve;
		mBegin = (F32)(begin * CUT_QUANTA);
		mEnd = (F32)(100.f - end) * CUT_QUANTA;
		if (mEnd > 1.f)
		{
			mEnd = 1.f;
		}
		mScale.set((F32)(200 - scx) * SCALE_QUANTA,
				   (F32)(200 - scy) * SCALE_QUANTA);
		mShear.set(U8_TO_F32(shx) * SHEAR_QUANTA,
				   U8_TO_F32(shy) * SHEAR_QUANTA);
		mTwistBegin = U8_TO_F32(twistbegin) * SCALE_QUANTA;
		mTwistEnd = U8_TO_F32(twistend) * SCALE_QUANTA;
		mRadiusOffset = U8_TO_F32(radiusoffset) * SCALE_QUANTA;
		mTaper.set(U8_TO_F32(tx) * TAPER_QUANTA,
				   U8_TO_F32(ty) * TAPER_QUANTA);
		mRevolutions = (F32)revolutions * REV_QUANTA + 1.f;
		mSkew = U8_TO_F32(skew) * SCALE_QUANTA;

		mCRC = 0;
	}

	bool operator==(const LLPathParams& params) const;
	bool operator!=(const LLPathParams& params) const;
	bool operator<(const LLPathParams& params) const;

	void copyParams(const LLPathParams& params);

	bool importFile(LLFILE* fp);
	bool exportFile(LLFILE* fp) const;

	bool importLegacyStream(std::istream& input_stream);
	bool exportLegacyStream(std::ostream& output_stream) const;

	LLSD asLLSD() const;
	LL_INLINE operator LLSD() const						{ return asLLSD(); }
	bool fromLLSD(LLSD& sd);

	LL_INLINE F32 getBegin() const						{ return mBegin; }
	LL_INLINE F32 getEnd() const						{ return mEnd; }
	LL_INLINE const LLVector2& getScale() const			{ return mScale; }
	LL_INLINE F32 getScaleX() const						{ return mScale.mV[0]; }
	LL_INLINE F32 getScaleY() const						{ return mScale.mV[1]; }
	LLVector2 getBeginScale() const;
	LLVector2 getEndScale() const;
	LL_INLINE const LLVector2& getShear() const			{ return mShear; }
	LL_INLINE F32 getShearX() const						{ return mShear.mV[0]; }
	LL_INLINE F32 getShearY() const						{ return mShear.mV[1]; }
	LL_INLINE U8 getCurveType () const					{ return mCurveType; }

	LL_INLINE F32 getTwistBegin() const					{ return mTwistBegin; }
	// Note: getTwist() has been deprecated in favour of getTwistEnd(()
	LL_INLINE F32 getTwistEnd() const					{ return mTwistEnd; }
	LL_INLINE F32 getRadiusOffset() const				{ return mRadiusOffset; }
	LL_INLINE const LLVector2& getTaper() const			{ return mTaper; }
	LL_INLINE F32 getTaperX() const						{ return mTaper.mV[0]; }
	LL_INLINE F32 getTaperY() const						{ return mTaper.mV[1]; }
	LL_INLINE F32 getRevolutions() const				{ return mRevolutions; }
	LL_INLINE F32 getSkew() const						{ return mSkew; }

	LL_INLINE void setCurveType(U8 type)				{ mCurveType = type; }
	LL_INLINE void setBegin(F32 begin)					{ mBegin = begin; }
	LL_INLINE void setEnd(F32 end)						{ mEnd = end; }

	LL_INLINE void setScale(F32 x, F32 y)				{ mScale.set(x, y); }
	LL_INLINE void setScaleX(F32 v)						{ mScale.mV[VX] = v; }
	LL_INLINE void setScaleY(F32 v)						{ mScale.mV[VY] = v; }
	LL_INLINE void setShear(F32 x, F32 y)				{ mShear.set(x, y); }
	LL_INLINE void setShearX(F32 v)						{ mShear.mV[VX] = v; }
	LL_INLINE void setShearY(F32 v)						{ mShear.mV[VY] = v; }

	LL_INLINE void setTwistBegin(F32 tbegin)			{ mTwistBegin = tbegin; }
	// Note: setTwist() has been deprecated in favour of setTwistEnd(()
	LL_INLINE void setTwistEnd(F32 tend)				{ mTwistEnd = tend; }
	LL_INLINE void setRadiusOffset(F32 roff)			{ mRadiusOffset = roff; }
	LL_INLINE void setTaper(F32 x, F32 y)				{ mTaper.set(x, y); }
	LL_INLINE void setTaperX(F32 v)						{ mTaper.mV[VX] = v; }
	LL_INLINE void setTaperY(F32 v)						{ mTaper.mV[VY] = v; }
	LL_INLINE void setRevolutions(F32 revol)			{ mRevolutions = revol; }
	LL_INLINE void setSkew(F32 skew)					{ mSkew = skew; }

	friend std::ostream& operator<<(std::ostream& s,
									const LLPathParams& path_params);

protected:
	// Path params
	LLVector2	  mScale;
	LLVector2     mShear;
	F32           mBegin;
	F32           mEnd;

	LLVector2	  mTaper;
	F32			  mRevolutions;
	F32			  mSkew;

	F32			  mTwistBegin;
	F32			  mTwistEnd;
	F32			  mRadiusOffset;

	U32           mCRC;

	U8			  mCurveType;
};

LL_INLINE bool LLPathParams::operator==(const LLPathParams& params) const
{
	return getCurveType() == params.getCurveType() &&
		   getScale() == params.getScale() &&
		   getBegin() == params.getBegin() &&
		   getEnd() == params.getEnd() &&
		   getShear() == params.getShear() &&
		   getTwistEnd() == params.getTwistEnd() &&
		   getTwistBegin() == params.getTwistBegin() &&
		   getRadiusOffset() == params.getRadiusOffset() &&
		   getTaper() == params.getTaper() &&
		   getRevolutions() == params.getRevolutions() &&
		   getSkew() == params.getSkew();
}

LL_INLINE bool LLPathParams::operator!=(const LLPathParams& params) const
{
	return getCurveType() != params.getCurveType() ||
		   getScale() != params.getScale() ||
		   getBegin() != params.getBegin() ||
		   getEnd() != params.getEnd() ||
		   getShear() != params.getShear() ||
		   getTwistEnd() != params.getTwistEnd() ||
		   getTwistBegin() !=params.getTwistBegin() ||
		   getRadiusOffset() != params.getRadiusOffset() ||
		   getTaper() != params.getTaper() ||
		   getRevolutions() != params.getRevolutions() ||
		   getSkew() != params.getSkew();
}


LL_INLINE bool LLPathParams::operator<(const LLPathParams& params) const
{
	if (getCurveType() != params.getCurveType())
	{
		return getCurveType() < params.getCurveType();
	}
	if (getScale() != params.getScale())
	{
		return getScale() < params.getScale();
	}
	if (getBegin() != params.getBegin())
	{
		return getBegin() < params.getBegin();
	}
	if (getEnd() != params.getEnd())
	{
		return getEnd() < params.getEnd();
	}
	if (getShear() != params.getShear())
	{
		return getShear() < params.getShear();
	}
	if (getTwistEnd() != params.getTwistEnd())
	{
		return getTwistEnd() < params.getTwistEnd();
	}
	if (getTwistBegin() != params.getTwistBegin())
	{
		return getTwistBegin() < params.getTwistBegin();
	}
	if (getRadiusOffset() != params.getRadiusOffset())
	{
		return getRadiusOffset() < params.getRadiusOffset();
	}
	if (getTaper() != params.getTaper())
	{
		return getTaper() < params.getTaper();
	}
	if (getRevolutions() != params.getRevolutions())
	{
		return getRevolutions() < params.getRevolutions();
	}
	return getSkew() < params.getSkew();
}

typedef LLVolumeParams* LLVolumeParamsPtr;
typedef const LLVolumeParams* const_LLVolumeParamsPtr;

class LLVolumeParams
{
protected:
	LOG_CLASS(LLVolumeParams);

public:
	LLVolumeParams()
	:	mSculptType(LL_SCULPT_TYPE_NONE)
	{
	}

	LLVolumeParams(LLProfileParams& profile, LLPathParams& path,
				   LLUUID sculpt_id = LLUUID::null,
				   U8 sculpt_type = LL_SCULPT_TYPE_NONE)
	:	mProfileParams(profile),
		mPathParams(path),
		mSculptID(sculpt_id),
		mSculptType(sculpt_type)
	{
	}

	bool operator==(const LLVolumeParams& params) const;
	bool operator!=(const LLVolumeParams& params) const;
	bool operator<(const LLVolumeParams& params) const;

	void copyParams(const LLVolumeParams& params);

	LL_INLINE const LLProfileParams& getProfileParams() const
	{
		return mProfileParams;
	}

	LL_INLINE LLProfileParams& getProfileParams()		{ return mProfileParams; }
	LL_INLINE const LLPathParams& getPathParams() const	{ return mPathParams; }
	LL_INLINE LLPathParams& getPathParams()				{ return mPathParams; }

	bool importFile(LLFILE* fp);
	bool exportFile(LLFILE* fp) const;

	bool importLegacyStream(std::istream& input_stream);
	bool exportLegacyStream(std::ostream& output_stream) const;

	LLSD sculptAsLLSD() const;
	bool sculptFromLLSD(LLSD& sd);

	LLSD asLLSD() const;
	LL_INLINE operator LLSD() const						{ return asLLSD(); }
	bool fromLLSD(LLSD& sd);

	bool setType(U8 profile, U8 path);

	// Both range from 0 to 1, begin must be less than end
	bool setBeginAndEndS(F32 begin, F32 end);
	bool setBeginAndEndT(F32 begin, F32 end);

	bool setHollow(F32 hollow);				// Range 0 to 1
	// 0 = point, 1 = same as base
	LL_INLINE bool setRatio(F32 x)						{ return setRatio(x, x); }
	// 0 = no movement,
	LL_INLINE bool setShear(F32 x)						{ return setShear(x, x); }
	bool setRatio(F32 x, F32 y);			// 0 = point, 1 = same as base
	bool setShear(F32 x, F32 y);			// 0 = no movement

	bool setTwistBegin(F32 twist_begin);	// Range -1 to 1
	bool setTwistEnd(F32 twist_end);		// Range -1 to 1

	LL_INLINE bool setTaper(F32 x, F32 y)
	{
		bool pass_x = setTaperX(x);
		bool pass_y = setTaperY(y);
		return pass_x && pass_y;
	}

	bool setTaperX(F32 v);						// -1 to 1
	bool setTaperY(F32 v);						// -1 to 1
	bool setRevolutions(F32 revolutions);		// 1 to 4
	bool setRadiusOffset(F32 radius_offset);
	bool setSkew(F32 skew);
	bool setSculptID(const LLUUID& sculpt_id, U8 sculpt_type);

	static bool validate(U8 prof_curve, F32 prof_begin, F32 prof_end,
						 F32 hollow,
						 U8 path_curve, F32 path_begin, F32 path_end,
						 F32 scx, F32 scy, F32 shx, F32 shy,
						 F32 twistend, F32 twistbegin, F32 radiusoffset,
						 F32 tx, F32 ty, F32 revolutions, F32 skew);

	LL_INLINE F32 getBeginS() const						{ return mProfileParams.getBegin(); }
 	LL_INLINE F32 getBeginT() const						{ return mPathParams.getBegin(); }
 	LL_INLINE F32 getEndS() const						{ return mProfileParams.getEnd(); }
 	LL_INLINE F32 getEndT() const						{ return mPathParams.getEnd(); }

 	LL_INLINE F32 getHollow() const						{ return mProfileParams.getHollow(); }
  	LL_INLINE F32 getRatio() const						{ return mPathParams.getScaleX(); }
 	LL_INLINE F32 getRatioX() const						{ return mPathParams.getScaleX(); }
 	LL_INLINE F32 getRatioY() const						{ return mPathParams.getScaleY(); }
 	LL_INLINE F32 getShearX() const						{ return mPathParams.getShearX(); }
 	LL_INLINE F32 getShearY() const						{ return mPathParams.getShearY(); }

	LL_INLINE F32 getTwistBegin()const					{ return mPathParams.getTwistBegin(); }
	// Note: getTwist() has been deprecated in favour of getTwistEnd(()
	LL_INLINE F32 getTwistEnd() const					{ return mPathParams.getTwistEnd(); }
	LL_INLINE F32 getRadiusOffset() const				{ return mPathParams.getRadiusOffset(); }
	LL_INLINE F32 getTaper() const 						{ return mPathParams.getTaperX(); }
	LL_INLINE F32 getTaperX() const						{ return mPathParams.getTaperX(); }
	LL_INLINE F32 getTaperY() const						{ return mPathParams.getTaperY(); }
	LL_INLINE F32 getRevolutions() const				{ return mPathParams.getRevolutions(); }
	LL_INLINE F32 getSkew() const						{ return mPathParams.getSkew(); }
	LL_INLINE const LLUUID& getSculptID() const			{ return mSculptID; }
	LL_INLINE U8 getSculptType() const					{ return mSculptType; }

	LL_INLINE bool isSculpt() const
	{
		return (mSculptType & LL_SCULPT_TYPE_MASK) != LL_SCULPT_TYPE_NONE;
	}

	LL_INLINE bool isMeshSculpt() const
	{
		return (mSculptType & LL_SCULPT_TYPE_MASK) == LL_SCULPT_TYPE_MESH;
	}

	bool isConvex() const;

	// 'begin' and 'end' should be in range [0, 1] (they will be clamped)
	// (begin, end) = (0, 1) will not change the volume
	// (begin, end) = (0, 0.5) will reduce the volume to the first half of its
	// profile/path (S/T)
	void reduceS(F32 begin, F32 end);
	void reduceT(F32 begin, F32 end);

	struct compare
	{
		LL_INLINE bool operator()(const const_LLVolumeParamsPtr& first,
								  const const_LLVolumeParamsPtr& second) const
		{
			return *first < *second;
		}
	};

	friend std::ostream& operator<<(std::ostream& s,
									const LLVolumeParams& volume_params);

	// debug helper functions
	void setCube();

protected:
	LLUUID			mSculptID;
	LLProfileParams	mProfileParams;
	// Note: LLProfileParams ends with an U8, so let's use the aligmenent gap with
	// another U8, at least... HB
	U8				mSculptType;
	LLPathParams	mPathParams;
};

class LLProfile
{
protected:
	LOG_CLASS(LLProfile);

public:
	LLProfile()
	:	mOpen(false),
		mConcave(false),
		mDirty(true),
		mTotalOut(0),
		mTotal(2)
	{
	}

	LL_INLINE S32 getTotal() const						{ return mTotal; }

	// Total number of outside points:
	LL_INLINE S32 getTotalOut() const					{ return mTotalOut; }

	LL_INLINE bool isFlat(S32 face) const				{ return mFaces[face].mCount == 2; }
	LL_INLINE bool isOpen() const						{ return mOpen; }
	LL_INLINE bool isConcave() const					{ return mConcave; }

	LL_INLINE void setDirty()							{ mDirty = true; }

	static S32 getNumPoints(const LLProfileParams& params, bool path_open,
							F32 detail = 1.f, S32 split = 0,
							bool is_sculpted = false, S32 sculpt_size = 0);

	bool generate(const LLProfileParams& params, bool path_open,
				  F32 detail = 1.f, S32 split = 0,
				  bool is_sculpted = false, S32 sculpt_size = 0);

	friend std::ostream& operator<<(std::ostream& s, const LLProfile& profile);

	struct Face
	{
		LLFaceID	mFaceID;
		S32			mIndex;
		S32			mCount;
		F32			mScaleU;
		bool		mCap;
		bool		mFlat;
	};

protected:
	static S32 getNumNGonPoints(const LLProfileParams& params, S32 sides,
								F32 ang_scale = 1.f, S32 split = 0);
	void genNGon(const LLProfileParams& params, S32 sides, F32 offset = 0.f,
				 F32 ang_scale = 1.f, S32 split = 0);

	Face* addHole(const LLProfileParams& params, bool flat, F32 sides,
				  F32 offset, F32 box_hollow, F32 ang_scale, S32 split = 0);
	Face* addCap(S16 face_id);
	Face* addFace(S32 index, S32 count, F32 u_scale, S16 face_id, bool flat);

public:
	LLAlignedArray<LLVector4a, 64>	mVertices;
	std::vector<Face>				mFaces;

protected:
	LLMutex							mMutex;
	S32								mTotalOut;
	S32								mTotal;
	bool							mOpen;
	bool							mConcave;
	bool							mDirty;
};

//-------------------------------------------------------------------
// SWEEP/EXTRUDE PATHS
//-------------------------------------------------------------------

class LLPath
{
protected:
	LOG_CLASS(LLPath);

public:
#if LL_JEMALLOC
	LL_INLINE void* operator new(size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}
#endif

	class alignas(16) PathPt
	{
	public:
		LLMatrix4a	mRot;
		LLVector4a	mPos;
		LLVector4a	mScale;
		F32			mTexT;
		F32			pad[3];	// For alignment

		PathPt()
		{
			mPos.clear();
			mTexT = 0;
			mScale.clear();
			mRot.setRows(LLVector4a(1, 0, 0, 0), LLVector4a(0, 1, 0, 0),
						 LLVector4a(0, 0, 1, 0));

			// Distinguished data in the pad for debugging
			pad[0] = F_PI;
			pad[1] = -F_PI;
			pad[2] = 0.585f;
		}
	};

public:
	LLPath()
	:	mOpen(false),
		mTotal(0),
		mDirty(true),
		mStep(1.f)
	{
	}

	virtual ~LLPath() = default;

	static S32 getNumPoints(const LLPathParams& params, F32 detail);
	static S32 getNumNGonPoints(const LLPathParams& params, S32 sides);

	void genNGon(const LLPathParams& params, S32 sides, F32 end_scale = 1.f,
				 F32 twist_scale = 1.f);
	virtual bool generate(const LLPathParams& params, F32 detail = 1.f,
						  S32 split = 0,
						  bool is_sculpted = false, S32 sculpt_size = 0);

	LL_INLINE bool isOpen() const						{ return mOpen; }
	LL_INLINE F32 getStep() const						{ return mStep; }
	LL_INLINE void setDirty()							{ mDirty = true; }

	LL_INLINE S32 getPathLength() const					{ return (S32)mPath.size(); }

	LL_INLINE void resizePath(S32 length)				{ mPath.resize(length); }

	friend std::ostream& operator<<(std::ostream& s, const LLPath& path);

protected:
	S32							mTotal;
	F32							mStep;
	bool						mOpen;
	bool						mDirty;

public:
	LLAlignedArray<PathPt, 64>	mPath;
};

class LLDynamicPath : public LLPath
{
protected:
	LOG_CLASS(LLDynamicPath);

public:
	bool generate(const LLPathParams& params, F32 detail = 1.f, S32 split = 0,
				  bool is_sculpted = false, S32 sculpt_size = 0) override;
};

// ----------------------------------------------------------------------------
// LLJointRiggingInfo class.
// Stores information related to associated rigged mesh vertices.
// ----------------------------------------------------------------------------

class alignas(16) LLJointRiggingInfo
{
public:
	LL_INLINE void* operator new(size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}

	LLJointRiggingInfo();

	LL_INLINE LLVector4a* getRiggedExtents()		{ return mRiggedExtents; }

	LL_INLINE const LLVector4a* getRiggedExtents() const
	{
		return mRiggedExtents;
	}

	LL_INLINE void setIsRiggedTo(bool val = true)	{  mIsRiggedTo = val; }
	LL_INLINE bool isRiggedTo() const				{ return mIsRiggedTo; }

	void merge(const LLJointRiggingInfo& other);

private:
	alignas(16) LLVector4a	mRiggedExtents[2];
	bool					mIsRiggedTo;
};

// ----------------------------------------------------------------------------
// LLJointRiggingInfoTab class.
// For storing all the rigging info associated with a given avatar or object,
// keyed by joint_key.
// ----------------------------------------------------------------------------

class LLJointRiggingInfoTab
{
public:
#if LL_JEMALLOC
	LL_INLINE void* operator new(size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}
#endif

	LLJointRiggingInfoTab();
	~LLJointRiggingInfoTab();

	void clear();
	void resize(U32 size);
	LL_INLINE U32 size() const						{ return mSize; }

	void merge(const LLJointRiggingInfoTab& src);

	LL_INLINE LLJointRiggingInfo& operator[](S32 i)	{ return mRigInfoPtr[i]; }

	LL_INLINE const LLJointRiggingInfo& operator[](S32 i) const
	{
		return mRigInfoPtr[i];
	}

	LL_INLINE bool needsUpdate()					{ return mNeedsUpdate; }

	LL_INLINE void setNeedsUpdate(bool val = true)	{ mNeedsUpdate = val; }

private:
	// Not implemented, on purpose.
	LLJointRiggingInfoTab& operator=(const LLJointRiggingInfoTab& src);
	LLJointRiggingInfoTab(const LLJointRiggingInfoTab& src);

private:
	LLJointRiggingInfo* mRigInfoPtr;
	U32					mSize;
	bool				mNeedsUpdate;
};

// ----------------------------------------------------------------------------
// LLVolumeFace class.
// Yet another "face" class - caches volume-specific, but not instance-specific
// data for faces)
// ----------------------------------------------------------------------------

class LLVolumeFace
{
protected:
	LOG_CLASS(LLVolumeFace);

public:
#if LL_JEMALLOC
	LL_INLINE void* operator new(size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}
#endif

	class VertexData
	{
	private:
		enum
		{
			POSITION = 0,
			NORMAL = 1
		};

		void init();

	public:
		LL_INLINE VertexData()							{ mData = NULL; init(); }
		LL_INLINE VertexData(const VertexData& rhs)		{ mData = NULL; *this = rhs; }

		const VertexData& operator=(const VertexData& rhs);

		~VertexData();

		LL_INLINE LLVector4a& getPosition()				{ return mData[POSITION]; }
		LL_INLINE LLVector4a& getNormal()				{ return mData[NORMAL]; }
		LL_INLINE const LLVector4a& getPosition() const	{ return mData[POSITION]; }
		LL_INLINE const LLVector4a& getNormal() const	{ return mData[NORMAL]; }
		LL_INLINE void setPosition(const LLVector4a& p)	{ mData[POSITION] = p; }
		LL_INLINE void setNormal(const LLVector4a& n)	{ mData[NORMAL] = n; }

		LLVector2 mTexCoord;

		bool operator<(const VertexData& rhs) const;
		LL_INLINE bool operator==(const VertexData& rhs) const;
		bool compareNormal(const VertexData& rhs, F32 angle_cutoff) const;

	private:
		LLVector4a* mData;
	};

	LLVolumeFace();
	LLVolumeFace(const LLVolumeFace& src);
	LLVolumeFace& operator=(const LLVolumeFace& rhs);

	~LLVolumeFace();

	static void initClass();

	bool create(LLVolume* volume, bool partial_build = false);
	void createTangents();

	bool resizeVertices(S32 num_verts);
	bool allocateTangents(S32 num_verts);
	bool allocateWeights(S32 num_verts);
	bool resizeIndices(S32 num_indices);
	void fillFromLegacyData(std::vector<LLVolumeFace::VertexData>& v,
							std::vector<U16>& idx);

	// Note: max_indice is the number of indices in the unoptimized face
	void pushVertex(const VertexData& cv, S32 max_indice);
	void pushVertex(const LLVector4a& pos, const LLVector4a& norm,
					const LLVector2& tc, S32 max_indice);
	void pushIndex(const U16& idx);

	void swapData(LLVolumeFace& rhs);

	void getVertexData(U16 indx, LLVolumeFace::VertexData& cv);

	class VertexMapData : public LLVolumeFace::VertexData
	{
	public:
		bool operator==(const LLVolumeFace::VertexData& rhs) const;

		struct ComparePosition
		{
			bool operator()(const LLVector3& a, const LLVector3& b) const;
		};

		typedef std::map<LLVector3, std::vector<VertexMapData>,
						 VertexMapData::ComparePosition> PointMap;
	public:
		U16 mIndex;
	};

	// Used to be a validate_face(const LLVolumeFace& face) global function in
	// llmodel.h. HB
	bool validate(bool check_nans = false) const;

	// Used to be a ll_is_degenerate() global function in llmodel.h. HB
	static bool isDegenerate(const LLVector4a& a, const LLVector4a& b,
							 const LLVector4a& c);

	// Eliminates non unique triangles, taking positions, normals and texture
	// coordinates into account.
	void remap();

	void optimize(F32 angle_cutoff = 2.f);
	bool cacheOptimize(bool gen_tangents = false);

	void createOctree(F32 scaler = 0.25f,
					  const LLVector4a& center = LLVector4a(0.f, 0.f, 0.f),
					  const LLVector4a& size = LLVector4a(0.5f, 0.5f, 0.5f));

	enum
	{
		SINGLE_MASK =	0x0001,
		CAP_MASK =		0x0002,
		END_MASK =		0x0004,
		SIDE_MASK =		0x0008,
		INNER_MASK =	0x0010,
		OUTER_MASK =	0x0020,
		HOLLOW_MASK =	0x0040,
		OPEN_MASK =		0x0080,
		FLAT_MASK =		0x0100,
		TOP_MASK =		0x0200,
		BOTTOM_MASK =	0x0400
	};

	void destroyOctree();

#if LL_JEMALLOC
	LL_INLINE static U32 getMallocxFlags16()			{ return sMallocxFlags16; }
	LL_INLINE static U32 getMallocxFlags64()			{ return sMallocxFlags64; }
#endif

private:
	void freeData();

    bool createUnCutCubeCap(LLVolume* volume, bool partial_build = false);
    bool createCap(LLVolume* volume, bool partial_build = false);
    bool createSide(LLVolume* volume, bool partial_build = false);

private:
#if LL_JEMALLOC
	static U32						sMallocxFlags16;
	static U32						sMallocxFlags64;
#endif

	// NOTE: some of the member variables below would better be private, but
	// this would forbid using the offsetof() macro on LLVolumeFace (gcc error
	// "'offsetof' within non-standard-layout type 'LLVolumeFace' is
	// conditionally-supported"), and we do need the latter... HB
public:
	// This octree stores raw pointer references to triangles in
	// mOctreeTriangles
	LLOctreeNodeNoOwnership<LLVolumeTriangle>*	mOctree;
	LLVolumeTriangle*				mOctreeTriangles;

	// List of skin weights for rigged volumes. Format is:
	// mWeights[vertex_index].mV[influence] = <joint_index>.<weight>
	// mWeights.size() should be empty or match mVertices.size()
	LLVector4a*						mWeights;

	std::vector<S32>				mEdge;

	LLJointRiggingInfoTab			mJointRiggingInfoTab;

	// If this is a mesh asset, scale and translation that were applied when
	// encoding the source mesh into a unit cube used for regenerating tangents
	LLVector3						mNormalizedScale;

	// Minimum and maximum of texture coordinates of the face:
	LLVector2						mTexCoordExtents[2];
	// Minimum and maximum point of face:
	LLVector4a*						mExtents;
	LLVector4a*						mCenter;

	// mPositions contains vertices, normals and texcoords
	LLVector4a*						mPositions;
	LLVector4a*						mNormals;
	LLVector4a*						mTangents;
	// Pointer into mPositions
	LLVector2*						mTexCoords;
	// mIndices contains mNumIndices amount of elements. It contains triangles,
	// each 3 indices describe one triangle.
	// If mIndices contains {0, 2, 3, 1, 2, 4}, it means there are 2 triangles
	// {0, 2, 3} and {1, 2, 4} with values being indexes for mPositions,
	// mNormals, mTexCoords.
	U16*							mIndices;

	S32								mID;
	U32								mTypeMask;

	// Only used for INNER/OUTER faces
	S32								mBeginS;
	S32								mBeginT;
	S32								mNumS;
	S32								mNumT;

	// mNumVertices == num vertices == num normals == num texcoords
	S32								mNumVertices;
	S32								mNumAllocatedVertices;
	S32								mNumIndices;

	mutable bool					mWeightsScrubbed;

	// Whether or not face has been cache optimized
	bool							mOptimized;
};

LL_INLINE bool LLVolumeFace::VertexData::operator==(const LLVolumeFace::VertexData& rhs) const
{
	return mData[POSITION].equals3(rhs.getPosition()) &&
		   mData[NORMAL].equals3(rhs.getNormal()) &&
		   mTexCoord == rhs.mTexCoord;
}

class LLVolume : public LLRefCount
{
	friend class LLVolumeLODGroup;

protected:
	LOG_CLASS(LLVolume);

	~LLVolume() override; // Uses unref

public:
#if LL_JEMALLOC
	LL_INLINE void* operator new(size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return allocate_volume_mem(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		free_volume_mem(ptr);
	}
#endif

	typedef std::vector<LLVolumeFace> face_list_t;

	struct FaceParams
	{
		LLFaceID mFaceID;
		S32 mBeginS;
		S32 mCountS;
		S32 mBeginT;
		S32 mCountT;
	};

	LLVolume(const LLVolumeParams& params, F32 detail,
			 bool generate_single_face = false, bool is_unique = false);

	LL_INLINE U8 getProfileType() const					{ return mParams.getProfileParams().getCurveType(); }
	LL_INLINE U8 getPathType() const					{ return mParams.getPathParams().getCurveType(); }

	LL_INLINE S32 getNumFaces() const
	{
		return mIsMeshAssetLoaded ? getNumVolumeFaces()
								  : (S32)mProfile.mFaces.size();
	}

	LL_INLINE S32 getNumVolumeFaces() const				{ return mVolumeFaces.size(); }
	LL_INLINE F32 getDetail() const						{ return mDetail; }
	LL_INLINE F32 getSurfaceArea() const				{ return mSurfaceArea; }
	LL_INLINE const LLVolumeParams& getParams() const	{ return mParams; }
	LL_INLINE LLVolumeParams getCopyOfParams() const	{ return mParams; }
	LL_INLINE const LLProfile& getProfile() const		{ return mProfile; }
	LL_INLINE LLPath& getPath() const					{ return *mPathp; }
	void resizePath(S32 length);

	LL_INLINE const LLAlignedArray<LLVector4a, 64>& getMesh() const
	{
		return mMesh;
	}

	LL_INLINE const LLVector4a& getMeshPt(U32 i) const
	{
		return mMesh[i];
	}

	LL_INLINE void setDirty()
	{
		mPathp->setDirty();
		mProfile.setDirty();
	}

	void regen();
	void genTangents(S32 face);

	// mParams.isConvex() may return false even though the final geometry is
	// actually convex due to LOD approximations.
	// *TODO: provide LLPath and LLProfile with isConvex() methods that
	// correctly determine convexity. -- Leviathan
	LL_INLINE bool isConvex() const						{ return mParams.isConvex(); }
	LL_INLINE bool isCap(S32 face)						{ return mProfile.mFaces[face].mCap; }
	LL_INLINE bool isFlat(S32 face)						{ return mProfile.mFaces[face].mFlat; }
	LL_INLINE bool isUnique() const						{ return mUnique; }

	LL_INLINE S32 getSculptLevel() const				{ return mSculptLevel; }
	LL_INLINE void setSculptLevel(S32 level)			{ mSculptLevel = level; }

	void getLoDTriangleCounts(S32* counts);

	S32 getNumTriangles(S32* vcount = NULL) const;

	void generateSilhouetteVertices(std::vector<LLVector3>& vertices,
									std::vector<LLVector3>& normals,
									const LLVector3& view_vec,
									const LLMatrix4& mat,
									const LLMatrix3& norm_mat,
									S32 face_index);

	// Gets the face index of the face that intersects with the given line
	// segment at the point closest to start. Moves end to the point of
	// intersection. Returns -1 if no intersection. Line segment must be in
	// volume space.
	S32 lineSegmentIntersect(const LLVector4a& start,
							 const LLVector4a& end,
							 // Which face to check, -1 = ALL_SIDES
							 S32 face = -1,
							 // Returns the intersection point
							 LLVector4a* intersection = NULL,
							 // Returns the texture coordinates at intersection
							 LLVector2* tex_coord = NULL,
							 // Returns the surface normal at intersection
							 LLVector4a* normal = NULL,
							 // Returns the surface tangent at intersection
							 LLVector4a* tangent = NULL);

	LLFaceID generateFaceMask();

	bool isFaceMaskValid(LLFaceID face_mask);
	friend std::ostream& operator<<(std::ostream& s, const LLVolume& volume);
	// *HACK to bypass Windoze confusion over conversion if *(LLVolume*) to
	// LLVolume&
	friend std::ostream& operator<<(std::ostream& s, const LLVolume* volumep);

	// ------------------------------------------------------------------------
	// DO NOT DELETE VOLUME WHILE USING THESE REFERENCES, OR HOLD A POINTER TO
	// THESE VOLUMEFACES

	LL_INLINE const LLVolumeFace& getVolumeFace(S32 f) const
	{
		return mVolumeFaces[f];
	}

	LL_INLINE LLVolumeFace& getVolumeFace(S32 f)
	{
		return mVolumeFaces[f];
	}

	LL_INLINE face_list_t& getVolumeFaces()				{ return mVolumeFaces; }

	// ------------------------------------------------------------------------

	void sculpt(U16 sculpt_width, U16 sculpt_height, S8 sculpt_components,
				const U8* sculpt_data, S32 sculpt_level,
				bool visible_placeholder = false);

	LL_INLINE void copyFacesTo(std::vector<LLVolumeFace>& faces) const
	{
		faces = mVolumeFaces;
	}

	LL_INLINE void copyFacesFrom(const std::vector<LLVolumeFace>& faces)
	{
		mVolumeFaces = faces;
		mSculptLevel = 0;
	}

	LL_INLINE void copyVolumeFaces(const LLVolume* volume)
	{
		if (volume)
		{
			mVolumeFaces = volume->mVolumeFaces;
			mSculptLevel = 0;
		}
	}

	bool cacheOptimize(bool gen_tangents = false);

	bool unpackVolumeFaces(std::istream& is, S32 size);
	bool unpackVolumeFaces(const U8* in, S32 size);

	LL_INLINE void setMeshAssetLoaded(bool b)			{ mIsMeshAssetLoaded = b; }
	LL_INLINE bool isMeshAssetLoaded()					{ return mIsMeshAssetLoaded; }

protected:
	bool generate();
	void createVolumeFaces();

private:
	bool unpackVolumeFaces(const LLSD& mdl);
	void sculptGenerateMapVertices(U16 sculpt_width, U16 sculpt_height,
								   S8 sculpt_components, const U8* sculpt_data,
								   U8 sculpt_type);
	F32 sculptGetSurfaceArea();
	void sculptGenerateEmptyPlaceholder();
	void sculptGenerateSpherePlaceholder();
	void sculptCalcMeshResolution(U16 width, U16 height, U8 type,
								  S32& s, S32& t);

protected:
	// Note: before these variables, we find the 32 bits counter from
	// LLRefCount... HB

	F32								mDetail;
	S32								mSculptLevel;
	F32								mSurfaceArea;	// Unscaled surface area

	LLPath*							mPathp;
	const LLVolumeParams			mParams;
	LLProfile						mProfile;

	LLAlignedArray<LLVector4a, 64>	mMesh;

	face_list_t						mVolumeFaces;

	struct TrianglesPerLODCache
	{
		LLProfileParams	mProfileParams;
		LLPathParams	mPathParams;
		S32				mTriangles[4];
	};
	TrianglesPerLODCache*			mTrianglesCache;

	bool							mUnique;
	bool							mGenerateSingleFace;
	bool							mIsMeshAssetLoaded;

public:
	// Bit array of which faces exist in this volume
	U32								mFaceMask;
	// Vector for biasing LOD based on scale
	LLVector3						mLODScaleBias;

	LLVector4a*						mHullPoints;
	U16*							mHullIndices;
	S32								mNumHullPoints;
	S32								mNumHullIndices;

	static U32						sLODCacheHit;
	static U32						sLODCacheMiss;
	static S32						sNumMeshPoints;
	static bool						sOptimizeCache;
};

std::ostream& operator<<(std::ostream& s, const LLVolumeParams& volume_params);

bool LLLineSegmentBoxIntersect(const F32* start, const F32* end,
							   const F32* center, const F32* size);
bool LLLineSegmentBoxIntersect(const LLVector3& start, const LLVector3& end,
							   const LLVector3& center, const LLVector3& size);
bool LLLineSegmentBoxIntersect(const LLVector4a& start, const LLVector4a& end,
							   const LLVector4a& center, const LLVector4a& size);

#if 0
bool LLTriangleRayIntersect(const LLVector3& vert0, const LLVector3& vert1,
							const LLVector3& vert2, const LLVector3& orig,
							const LLVector3& dir,
							F32& intersection_a, F32& intersection_b,
							F32& intersection_t, bool two_sided);
#endif

bool LLTriangleRayIntersect(const LLVector4a& vert0, const LLVector4a& vert1,
							const LLVector4a& vert2, const LLVector4a& orig,
							const LLVector4a& dir,
							F32& intersection_a, F32& intersection_b,
							F32& intersection_t);

bool LLTriangleRayIntersectTwoSided(const LLVector4a& vert0,
									const LLVector4a& vert1,
									const LLVector4a& vert2,
									const LLVector4a& orig,
									const LLVector4a& dir,
									F32& intersection_a,
									F32& intersection_b,
									F32& intersection_t);

F32 LLTriangleClosestPoint(const LLVector3& vert0, const LLVector3& vert1,
						   const LLVector3& vert2, const LLVector3& target,
						   F32& closest_a, F32& closest_b);

// NOTE: since the memory functions below use void* pointers instead of char*
// (because void* is the type used by malloc and jemalloc), strict aliasing is
// not possible on structures allocated with them. Make sure you forbid your
// compiler to optimize with strict aliasing assumption (i.e. for gcc, DO use
// the -fno-strict-aliasing option) !

LL_INLINE void* allocate_volume_mem(size_t size)
{
	if (LL_UNLIKELY(size <= 0)) return NULL;

	void* addr;
#if LL_JEMALLOC
	addr = mallocx(size, LLVolumeFace::getMallocxFlags16());
	LL_TRACY_ALLOC(addr, size, trc_mem_volume);
#else
	addr = ll_aligned_malloc_16(size);
#endif

	if (LL_UNLIKELY(addr == NULL))
	{
		LLMemory::allocationFailed(size);
	}

	return addr;
}

LL_INLINE void free_volume_mem(void* addr) noexcept
{
	if (LL_UNLIKELY(addr == NULL)) return;

#if LL_JEMALLOC
	LL_TRACY_FREE(addr, trc_mem_volume);
	dallocx(addr, 0);
#else
	ll_aligned_free_16(addr);
#endif
}

LL_INLINE void* realloc_volume_mem(void* ptr, size_t size, size_t old_size)
{
	if (LL_UNLIKELY(size <= 0))
	{
		free_volume_mem(ptr);
		return NULL;
	}

	if (LL_UNLIKELY(ptr == NULL))
	{
		return allocate_volume_mem(size);
	}

#if LL_JEMALLOC
	LL_TRACY_FREE(ptr, trc_mem_volume);
	void* addr = rallocx(ptr, size, LLVolumeFace::getMallocxFlags16());
	if (LL_UNLIKELY(addr == NULL))
	{
		LLMemory::allocationFailed(size);
	}
	LL_TRACY_ALLOC(addr, size, trc_mem_volume);
	return addr;
#else
	return ll_aligned_realloc_16(ptr, size, old_size);
#endif
}

LL_INLINE void* allocate_volume_mem_64(size_t size)
{
	if (LL_UNLIKELY(size <= 0)) return NULL;

	void* addr;

#if LL_JEMALLOC
	addr = mallocx(size, LLVolumeFace::getMallocxFlags64());
	LL_TRACY_ALLOC(addr, size, trc_mem_volume64);
#else
	addr = ll_aligned_malloc(size, 64);
#endif

	if (LL_UNLIKELY(addr == NULL))
	{
		LLMemory::allocationFailed(size);
	}

	return addr;
}

LL_INLINE void free_volume_mem_64(void* addr) noexcept
{
	if (LL_LIKELY(addr))
	{
#if LL_JEMALLOC
		LL_TRACY_FREE(addr, trc_mem_volume64);
		dallocx(addr, 0);
#else
		ll_aligned_free(addr);
#endif
	}
}

#endif
