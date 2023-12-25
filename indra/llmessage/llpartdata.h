/**
 * @file llpartdata.h
 * @brief Particle system data packing
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLPARTDATA_H
#define LL_LLPARTDATA_H

#include "llpreprocessor.h"
#include "lluuid.h"
#include "llvector2.h"
#include "llvector3d.h"
#include "llvector3.h"
#include "llcolor4.h"

class LLDataPacker;
class LLMessageSystem;

constexpr S32 PS_CUR_VERSION = 18;

//
// These constants are used by the script code, not by the particle system
// itself
//

enum LLPSScriptFlags
{
	// Flags for the different parameters of individual particles
	LLPS_PART_FLAGS,
	LLPS_PART_START_COLOR,
	LLPS_PART_START_ALPHA,
	LLPS_PART_END_COLOR,
	LLPS_PART_END_ALPHA,
	LLPS_PART_START_SCALE,
	LLPS_PART_END_SCALE,
	LLPS_PART_MAX_AGE,

	// Flags for the different parameters of the particle source
	LLPS_SRC_ACCEL,
	LLPS_SRC_PATTERN,
	LLPS_SRC_INNERANGLE,
	LLPS_SRC_OUTERANGLE,
	LLPS_SRC_TEXTURE,
	LLPS_SRC_BURST_RATE,
	LLPS_SRC_BURST_DURATION,
	LLPS_SRC_BURST_PART_COUNT,
	LLPS_SRC_BURST_RADIUS,
	LLPS_SRC_BURST_SPEED_MIN,
	LLPS_SRC_BURST_SPEED_MAX,
	LLPS_SRC_MAX_AGE,
	LLPS_SRC_TARGET_UUID,
	LLPS_SRC_OMEGA,
	LLPS_SRC_ANGLE_BEGIN,
	LLPS_SRC_ANGLE_END,

	LLPS_PART_BLEND_FUNC_SOURCE,
	LLPS_PART_BLEND_FUNC_DEST,
	LLPS_PART_START_GLOW,
	LLPS_PART_END_GLOW
};

class LLPartData
{
public:
	LLPartData()
	:	mFlags(0),
		mMaxAge(0.f),
		mParameter(0.f)
	{
	}

	// Need virtual destructor so that derived classes are properly destroyed:
	virtual ~LLPartData()
	{
	}

	bool unpackLegacy(LLDataPacker& dp);
	bool unpack(LLDataPacker& dp);

	LL_INLINE bool hasGlow() const					{ return mStartGlow > 0.f || mEndGlow > 0.f; }

	LL_INLINE bool hasBlendFunc() const
	{
		return mBlendFuncSource != LLPartData::LL_PART_BF_SOURCE_ALPHA ||
			   mBlendFuncDest != LLPartData::LL_PART_BF_ONE_MINUS_SOURCE_ALPHA;
	}

	// Masks for the different particle flags
	enum
	{
		LL_PART_INTERP_COLOR_MASK		= 0x01,
		LL_PART_INTERP_SCALE_MASK		= 0x02,
		LL_PART_BOUNCE_MASK				= 0x04,
		LL_PART_WIND_MASK				= 0x08,
		LL_PART_FOLLOW_SRC_MASK			= 0x10,		// Follows source, no rotation following (expensive!)
		LL_PART_FOLLOW_VELOCITY_MASK	= 0x20,		// Particles orient themselves with velocity
		LL_PART_TARGET_POS_MASK			= 0x40,
		LL_PART_TARGET_LINEAR_MASK		= 0x80,		// Particle uses a direct linear interpolation
		LL_PART_EMISSIVE_MASK			= 0x100,	// Particle is "emissive", instead of being lit
		LL_PART_BEAM_MASK				= 0x200,	// Particle is a "beam" connecting source and target
		LL_PART_RIBBON_MASK				= 0x400,	// Particles are joined together into one continuous triangle strip

		// Not implemented yet !
		//LL_PART_RANDOM_ACCEL_MASK		= 0x100,	// Particles have random acceleration
		//LL_PART_RANDOM_VEL_MASK		= 0x200,	// Particles have random velocity shifts"
		//LL_PART_TRAIL_MASK			= 0x400,	// Particles have historical "trails"

		// SYSTEM SET FLAGS
		LL_PART_DATA_GLOW				= 0x10000,
		LL_PART_DATA_BLEND				= 0x20000,

		// Viewer side use only !
		LL_PART_HUD						= 0x40000000,
		LL_PART_DEAD_MASK				= 0x80000000,
	};

	// WARNING: this MUST match the LLRender::eBlendFactor enum !
	enum
	{
		LL_PART_BF_ONE						= 0,
		LL_PART_BF_ZERO						= 1,
		LL_PART_BF_DEST_COLOR				= 2,
		LL_PART_BF_SOURCE_COLOR				= 3,
		LL_PART_BF_ONE_MINUS_DEST_COLOR		= 4,
		LL_PART_BF_ONE_MINUS_SOURCE_COLOR	= 5,
		UNSUPPORTED_DEST_ALPHA				= 6,
		LL_PART_BF_SOURCE_ALPHA				= 7,
		UNSUPPORTED_ONE_MINUS_DEST_ALPHA	= 8,
		LL_PART_BF_ONE_MINUS_SOURCE_ALPHA	= 9,
		LL_PART_BF_COUNT					= 10
	};

	static LL_INLINE bool validBlendFunc(S32 func)
	{
		return func >= 0 && func < LL_PART_BF_COUNT &&
			   func != UNSUPPORTED_DEST_ALPHA &&
			   func != UNSUPPORTED_ONE_MINUS_DEST_ALPHA;
	}

	LL_INLINE void setFlags(U32 flags)				{ mFlags = flags; }
	LL_INLINE void setMaxAge(F32 max_age)			{ mMaxAge = llclamp(max_age, 0.f, 30.f); }

	LL_INLINE void setStartScale(F32 xs, F32 ys)
	{
		constexpr F32 MAX_PART_SCALE = 4.f;
		mStartScale.mV[VX] = llmin(xs, MAX_PART_SCALE);
		mStartScale.mV[VY] = llmin(ys, MAX_PART_SCALE);
	}

	LL_INLINE void setEndScale(F32 xs, F32 ys)
	{
		constexpr F32 MAX_PART_SCALE = 4.f;
		mEndScale.mV[VX] = llmin(xs, MAX_PART_SCALE);
		mEndScale.mV[VY] = llmin(ys, MAX_PART_SCALE);
	}

	LL_INLINE void setStartColor(const LLVector3& rgb)
	{
		mStartColor.set(rgb.mV[0], rgb.mV[1], rgb.mV[2]);
	}

	LL_INLINE void setEndColor(const LLVector3& rgb)
	{
		mEndColor.set(rgb.mV[0], rgb.mV[1], rgb.mV[2]);
	}

	LL_INLINE void setStartAlpha(F32 alpha)			{ mStartColor.mV[3] = alpha; }
	LL_INLINE void setEndAlpha(F32 alpha)			{ mEndColor.mV[3] = alpha; }

	friend class LLPartSysData;
	friend class LLViewerPartSourceScript;

private:
	S32 getSize() const;

public:	// These are public because I'm really lazy...
	U32			mFlags;			// Particle state/interpolators in effect
	F32			mMaxAge;		// Maximum age of the particle
	LLColor4	mStartColor;	// Start color
	LLColor4	mEndColor;		// End color
	LLVector2	mStartScale;	// Start scale
	LLVector2	mEndScale;		// End scale

	LLVector3	mPosOffset;		// Offset from source if using FOLLOW_SOURCE
	F32			mParameter;		// A single floating point parameter

	F32			mStartGlow;
	F32			mEndGlow;

	U8			mBlendFuncSource;
	U8			mBlendFuncDest;
};

class LLPartSysData
{
protected:
	LOG_CLASS(LLPartSysData);

public:
	LLPartSysData();

	bool unpack(LLDataPacker& dp);
	bool unpackLegacy(LLDataPacker& dp);
	bool unpackBlock(S32 block_num);

	// Returns false if this is a "NULL" particle system (i.e. no system):
	static bool isNullPS(S32 block_num);

	LL_INLINE bool isLegacyCompatible() const	{ return !mPartData.hasGlow() &&
														 !mPartData.hasBlendFunc(); }

	// Different masks for effects on the source
	enum
	{
		// Accel and velocity for particles relative object rotation:
		LL_PART_SRC_OBJ_REL_MASK	= 0x01,
		// Particles uses new 'correct' angle parameters:
		LL_PART_USE_NEW_ANGLE 		= 0x02,
	};

	// The different patterns for how particles are created
	enum
	{
		LL_PART_SRC_PATTERN_DROP				= 0x01,
		LL_PART_SRC_PATTERN_EXPLODE				= 0x02,
		// Not implemented fully yet
		LL_PART_SRC_PATTERN_ANGLE				= 0x04,
		LL_PART_SRC_PATTERN_ANGLE_CONE			= 0x08,
		LL_PART_SRC_PATTERN_ANGLE_CONE_EMPTY	= 0x10,
	};

	LL_INLINE void setBurstSpeedMin(F32 spd)		{ mBurstSpeedMin = llclamp(spd, -100.f, 100.f); }
	LL_INLINE void setBurstSpeedMax(F32 spd)		{ mBurstSpeedMax = llclamp(spd, -100.f, 100.f); }
	LL_INLINE void setBurstRadius(F32 rad)			{ mBurstRadius = llclamp(rad, 0.f, 50.f); }

	LL_INLINE void setPartAccel(const LLVector3& accel)
	{
		mPartAccel.mV[VX] = llclamp(accel.mV[VX], -100.f, 100.f);
		mPartAccel.mV[VY] = llclamp(accel.mV[VY], -100.f, 100.f);
		mPartAccel.mV[VZ] = llclamp(accel.mV[VZ], -100.f, 100.f);
	}

	LL_INLINE void setUseNewAngle()					{ mFlags |=  LL_PART_USE_NEW_ANGLE; }
	LL_INLINE void unsetUseNewAngle()				{ mFlags &= ~LL_PART_USE_NEW_ANGLE; }

	// Since the actual particle creation rate is a combination of multiple
	// parameters, we need to clamp it using a separate method instead of an
	// accessor.
	void clampSourceParticleRate();

	friend std::ostream& operator<<(std::ostream& s,
									const LLPartSysData& data);

private:
	bool unpackSystem(LLDataPacker& dp);

public:	// Public because I'm lazy....

	// There are two kinds of data for the particle system
	// 1. Parameters which specify parameters of the source (mSource*)
	// 2. Parameters which specify parameters of the particles generated by the
	//    source (mPart*)

	U32			mCRC;
	U32			mFlags;

	U8			mPattern;			// Pattern for particle velocity/output
	F32			mInnerAngle;		// Inner angle for PATTERN_ANGLE
	F32			mOuterAngle;		// Outer angle for PATTERN_ANGLE
	LLVector3	mAngularVelocity;	// Angular velocity for emission axis (for
									// PATTERN_ANGLE)

	F32			mBurstRate;			// How often to do a burst of particles
	U8			mBurstPartCount;	// How many particles in a burst
	F32			mBurstRadius;
	F32			mBurstSpeedMin;		// Minimum particle velocity
	F32			mBurstSpeedMax;		// Maximum particle velocity

	F32			mMaxAge;			// Maximum lifetime of this particle source

	LLUUID		mTargetUUID;		// Target UUID for the particle system

	F32			mStartAge;			// Age at which to start the particle
									// system (for an update after the particle
									// system has started)

	// These are actually particle properties, but can be mutated by the
	// source, so are stored here instead

	LLVector3	mPartAccel;
	LLUUID		mPartImageID;

	// The "template" partdata where we actually store the non-mutable particle
	// parameters

	LLPartData	mPartData;

protected:
	S32			mNumParticles;		// Number of particles generated
};

#endif // LL_LLPARTDATA_H
