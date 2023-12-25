/**
 * @file llcamera.h
 * @brief Header file for the LLCamera class.
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

#ifndef LL_CAMERA_H
#define LL_CAMERA_H

#include "llcoordframe.h"
#include "llmath.h"
#include "llplane.h"

constexpr F32 DEFAULT_FIELD_OF_VIEW	= 60.f * DEG_TO_RAD;
constexpr F32 DEFAULT_ASPECT_RATIO 	= 640.f / 480.f;
constexpr F32 DEFAULT_NEAR_PLANE 	= 0.25f;
// Far reaches across two horizontal, not diagonal, regions:
constexpr F32 DEFAULT_FAR_PLANE 	= 64.f;

constexpr F32 MAX_ASPECT_RATIO 	= 50.0f;
// Clamp the near plane just before the skybox ends
constexpr F32 MAX_NEAR_PLANE 	= 1023.9f;
// Max allowed. Not good Z precision though:
constexpr F32 MAX_FAR_PLANE 	= 100000.0f;
constexpr F32 MAX_FAR_CLIP		= 512.0f;

constexpr F32 MIN_ASPECT_RATIO 	= 0.02f;
constexpr F32 MIN_NEAR_PLANE 	= 0.1f;
constexpr F32 MIN_FAR_PLANE 	= 0.2f;

// Min/Max FOV values for square views. Call getMin/MaxView to get extremes
// based on current aspect ratio.
constexpr F32 MIN_FIELD_OF_VIEW = 5.f * DEG_TO_RAD;
constexpr F32 MAX_FIELD_OF_VIEW = 175.f * DEG_TO_RAD;

// An LLCamera is an LLCoorFrame with a view frustum. This means that it has
// several methods for moving it around that are inherited from the
// LLCoordFrame() class :
//
// setOrigin(), setAxes()
// translate(), rotate()
// roll(), pitch(), yaw()
// etc...

class alignas(16) LLCamera : public LLCoordFrame
{
public:
	LLCamera(const LLCamera& rhs)
	{
		*this = rhs;
	}

	enum
	{
		PLANE_LEFT			= 0,
		PLANE_RIGHT			= 1,
		PLANE_BOTTOM		= 2,
		PLANE_TOP			= 3,
		PLANE_NUM			= 4,
		PLANE_MASK_NONE 	= 0xff	// Disable this plane
	};

	enum
	{
		PLANE_LEFT_MASK		= 1 << PLANE_LEFT,
		PLANE_RIGHT_MASK	= 1 << PLANE_RIGHT,
		PLANE_BOTTOM_MASK	= 1 << PLANE_BOTTOM,
		PLANE_TOP_MASK		= 1 << PLANE_TOP,
		PLANE_ALL_MASK		= 0xf
	};

	// Indexes to mAgentPlanes[] and mPlaneMask[]
	enum
	{
		AGENT_PLANE_LEFT		= 0,
		AGENT_PLANE_RIGHT		= 1,
		AGENT_PLANE_NEAR		= 2,
		AGENT_PLANE_BOTTOM		= 3,
		AGENT_PLANE_TOP			= 4,
		AGENT_PLANE_FAR			= 5,
		AGENT_PLANE_USER_CLIP	= 6
	};

	// Sizes for mAgentPlanes[].  7th entry is special case for user clip
	enum
	{
		AGENT_PLANE_NO_USER_CLIP_NUM	= 6,
		AGENT_PLANE_USER_CLIP_NUM		= 7,
		PLANE_MASK_NUM					= 8	// 7 actually used, 8 is for alignment
	};

	enum
	{
		AGENT_FRUSTRUM_NUM	= 8
	};

	enum
	{
		HORIZ_PLANE_LEFT	= 0,
		HORIZ_PLANE_RIGHT	= 1,
		HORIZ_PLANE_NUM		= 2
	};

	enum
	{
		HORIZ_PLANE_LEFT_MASK	= 1 << HORIZ_PLANE_LEFT,
		HORIZ_PLANE_RIGHT_MASK	= 1 << HORIZ_PLANE_RIGHT,
		HORIZ_PLANE_ALL_MASK	= 0x3
	};

	LLCamera();
	LLCamera(F32 vertical_fov_rads, F32 aspect_ratio,
			 S32 view_height_in_pixels, F32 near_plane, F32 far_plane);

	bool isChanged();	// Checks if mAgentPlanes changed since last frame.

	LL_INLINE void setUserClipPlane(const LLPlane& plane)
	{
		mPlaneCount = AGENT_PLANE_USER_CLIP_NUM;
		mAgentPlanes[AGENT_PLANE_USER_CLIP] = plane;
		mPlaneMask[AGENT_PLANE_USER_CLIP] = plane.calcPlaneMask();
	}

	LL_INLINE void disableUserClipPlane()
	{
		mPlaneCount = AGENT_PLANE_NO_USER_CLIP_NUM;
	}

	LL_INLINE virtual void setView(F32 vert_fov_rads)
	{
		mView = llclamp(vert_fov_rads, MIN_FIELD_OF_VIEW, MAX_FIELD_OF_VIEW);
		calculateFrustumPlanes();
	}

	LL_INLINE void setViewHeightInPixels(S32 height)
	{
		mViewHeightInPixels = height;
		// We do not really need to do this, but update the pixel meter ratio
		// with it.
		calculateFrustumPlanes();
	}

	LL_INLINE void setAspect(F32 new_aspect)
	{
		mAspect = llclamp(new_aspect, MIN_ASPECT_RATIO, MAX_ASPECT_RATIO);
		calculateFrustumPlanes();
	}

	LL_INLINE void setNear(F32 new_near)
	{
		mNearPlane = llclamp(new_near, MIN_NEAR_PLANE, MAX_NEAR_PLANE);
		calculateFrustumPlanes();
	}

	LL_INLINE void setFar(F32 new_far)
	{
		mFarPlane = llclamp(new_far, MIN_FAR_PLANE, MAX_FAR_PLANE);
		calculateFrustumPlanes();
	}

	LL_INLINE const LLPlane& getAgentPlane(U32 idx) const
	{
		return mAgentPlanes[idx];
	}
	// Non-const version of the above (used only in llpipeline.cpp). HB
	LL_INLINE LLPlane& agentPlane(U32 idx)				{ return mAgentPlanes[idx]; }

	// Returns the vertical FOV in radians
	LL_INLINE F32 getView() const						{ return mView; }
	LL_INLINE S32 getViewHeightInPixels() const			{ return mViewHeightInPixels; }
	// Returns width / height
	LL_INLINE F32 getAspect() const						{ return mAspect; }
	// Distance in meters
	LL_INLINE F32 getNear() const						{ return mNearPlane; }
	// Distance in meters
	LL_INLINE F32 getFar() const						{ return mFarPlane; }

	// The values returned by the min/max view getters depend upon the aspect
	// ratio at the time they are called and therefore should not be cached.
	LL_INLINE F32 getMinView() const
	{
		// Minimum vertical FOV needs to be constrained in narrow windows.
		return mAspect > 1	? MIN_FIELD_OF_VIEW // wide views
							// Clamps minimum width in narrow views
							: MIN_FIELD_OF_VIEW / mAspect;
	}

	LL_INLINE F32 getMaxView() const
	{
		// Maximum vertical fov needs to be constrained in wide windows.
							 // Clamps maximum width in wide views
		return mAspect > 1 ? MAX_FIELD_OF_VIEW / mAspect
						   : MAX_FIELD_OF_VIEW; // Narrow views
	}

	LL_INLINE F32 getYaw() const
	{
		return atan2f(mXAxis[VY], mXAxis[VX]);
	}

	LL_INLINE F32 getPitch() const
	{
		F32 xylen = sqrtf(mXAxis[VX] * mXAxis[VX] + mXAxis[VY] * mXAxis[VY]);
		return atan2f(mXAxis[VZ], xylen);
	}

	// Copy mView, mAspect, mNearPlane, and mFarPlane to buffer.
	// Return number of bytes copied.
	size_t writeFrustumToBuffer(char* buffer) const;

	// Copy mView, mAspect, mNearPlane, and mFarPlane from buffer.
	// Return number of bytes copied.
	size_t readFrustumFromBuffer(const char* buffer);
	void calcAgentFrustumPlanes(LLVector3* frust);
	// Calculate regional planes from mAgentPlanes:
	void calcRegionFrustumPlanes(const LLVector3& shift,
								 F32 far_clip_distance);

	LL_INLINE void ignoreAgentFrustumPlane(S32 idx)
	{
		if (idx >= 0 && idx < (S32)mPlaneCount)
		{
			mPlaneMask[idx] = PLANE_MASK_NONE;
			mAgentPlanes[idx].clear();
		}
	}

	// Returns 1 if partly in, 2 if fully in; 'center' is in absolute frame.
	S32 sphereInFrustum(const LLVector3& center, F32 radius) const;

	LL_INLINE S32 pointInFrustum(const LLVector3& point) const
	{
		return sphereInFrustum(point, 0.0f);
	}

	LL_INLINE S32 sphereInFrustumFull(const LLVector3& center,
									  F32 radius) const
	{
		return sphereInFrustum(center, radius);
	}

	S32 AABBInFrustum(const LLVector4a& center, const LLVector4a& radius,
					  const LLPlane* planes = NULL);
	// Exactly same method as AABBInFrustum(...) except it uses mRegionPlanes
	// instead of mAgentPlanes.
	LL_INLINE S32 AABBInRegionFrustum(const LLVector4a& center,
									  const LLVector4a& radius)
	{
		return AABBInFrustum(center, radius, mRegionPlanes);
	}

	S32 AABBInFrustumNoFarClip(const LLVector4a& center,
							   const LLVector4a& radius,
							   const LLPlane* planes = NULL);
	// Exactly same method as AABBInFrustumNoFarClip(...) except it uses
	// mRegionPlanes instead of mAgentPlanes.
	LL_INLINE S32 AABBInRegionFrustumNoFarClip(const LLVector4a& center,
											   const LLVector4a& radius)
	{
		return AABBInFrustumNoFarClip(center, radius, mRegionPlanes);
	}

	// Does a quick'n dirty sphere-sphere check
	S32 sphereInFrustumQuick(const LLVector3& sphere_center, F32 radius);

	// Returns height of object in pixels (must be height because field of view
	// is based on window height).
	F32 heightInPixels(const LLVector3& center, F32 radius) const;

	LL_INLINE void setFixedDistance(F32 d)					{ mFixedDistance = d; }

	friend std::ostream& operator<<(std::ostream& s, const LLCamera& c);

protected:
	void calculateFrustumPlanes();
	void calculateFrustumPlanes(F32 left, F32 right, F32 top, F32 bottom);
	void calculateFrustumPlanesFromWindow(F32 x1, F32 y1, F32 x2, F32 y2);

private:
	// Frustum planes in agent space a la gluUnproject (I'm a bastard, I know)
	// - DaveP
	alignas(16) LLPlane	mAgentPlanes[AGENT_PLANE_USER_CLIP_NUM];

	// Frustum planes in a local region space, derived from mAgentPlanes
	alignas(16) LLPlane	mRegionPlanes[AGENT_PLANE_USER_CLIP_NUM];

	alignas(16) LLPlane	mLastAgentPlanes[AGENT_PLANE_USER_CLIP_NUM];

	// Center of frustum and radius squared for ultra-quick exclusion test
	LLVector3			mFrustCenter;

	// Defaults to 6, if setUserClipPlane is called, uses user supplied clip
	// plane in
	U32					mPlaneCount;

	// Angle between top and bottom frustum planes in radians.
	F32					mView;
	F32					mAspect;				// Width/height
	// For ViewHeightInPixels() only
	S32					mViewHeightInPixels;
	F32					mNearPlane;
	F32					mFarPlane;
	// Always return this distance, unless < 0
	F32					mFixedDistance;
	F32					mFrustRadiusSquared;

	U8					mPlaneMask[PLANE_MASK_NUM];

public:
	// 8 corners of 6-plane frustum
	LLVector3			mAgentFrustum[AGENT_FRUSTRUM_NUM];
	// Distance to corner of frustum against far clip plane
	F32					mFrustumCornerDist;
};

#endif
