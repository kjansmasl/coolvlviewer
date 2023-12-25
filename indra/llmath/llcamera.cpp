/**
 * @file llcamera.cpp
 * @brief Implementation of the LLCamera class.
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

#include "linden_common.h"

#include "llmath.h"

#include "llcamera.h"

LLCamera::LLCamera()
:	mView(DEFAULT_FIELD_OF_VIEW),
	mAspect(DEFAULT_ASPECT_RATIO),
	mViewHeightInPixels(-1),			// Invalid height
	mNearPlane(DEFAULT_NEAR_PLANE),
	mFarPlane(DEFAULT_FAR_PLANE),
	mFixedDistance(-1.f),
	mPlaneCount(6),
	mFrustumCornerDist(0.f)
{
	for (U32 i = 0; i < PLANE_MASK_NUM; ++i)
	{
		mPlaneMask[i] = PLANE_MASK_NONE;
	}

	calculateFrustumPlanes();
}

LLCamera::LLCamera(F32 vertical_fov_rads, F32 aspect_ratio,
				   S32 view_height_in_pixels, F32 near_plane, F32 far_plane)
:	mViewHeightInPixels(view_height_in_pixels),
	mFixedDistance(-1.f),
	mPlaneCount(6),
	mFrustumCornerDist(0.f)
{
	for (U32 i = 0; i < PLANE_MASK_NUM; ++i)
	{
		mPlaneMask[i] = PLANE_MASK_NONE;
	}

	mAspect = llclamp(aspect_ratio, MIN_ASPECT_RATIO, MAX_ASPECT_RATIO);
	mNearPlane = llclamp(near_plane, MIN_NEAR_PLANE, MAX_NEAR_PLANE);

	if (far_plane < 0.f)
	{
		far_plane = DEFAULT_FAR_PLANE;
	}
	mFarPlane = llclamp(far_plane, MIN_FAR_PLANE, MAX_FAR_PLANE);

	setView(vertical_fov_rads);
}

size_t LLCamera::writeFrustumToBuffer(char* buffer) const
{
	memcpy(buffer, &mView, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(buffer, &mAspect, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(buffer, &mNearPlane, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(buffer, &mFarPlane, sizeof(F32));
	return 4 * sizeof(F32);
}

size_t LLCamera::readFrustumFromBuffer(const char* buffer)
{
	memcpy(&mView, buffer, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(&mAspect, buffer, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(&mNearPlane, buffer, sizeof(F32));
	buffer += sizeof(F32);
	memcpy(&mFarPlane, buffer, sizeof(F32));
	return 4 * sizeof(F32);
}

static const LLVector4a sFrustumScaler[] =
{
	LLVector4a(-1.f, -1.f, -1.f),
	LLVector4a(1.f, -1.f, -1.f),
	LLVector4a(-1.f, 1.f, -1.f),
	LLVector4a(1.f, 1.f, -1.f),
	LLVector4a(-1.f, -1.f, 1.f),
	LLVector4a(1.f, -1.f, 1.f),
	LLVector4a(-1.f, 1.f, 1.f),
	LLVector4a(1.f, 1.f, 1.f)
};

bool LLCamera::isChanged()
{
	bool changed = false;
	for (U32 i = 0; i < mPlaneCount; ++i)
	{
		U8 mask = mPlaneMask[i];
		if (mask != 0xff && !changed)
		{
			changed = !mAgentPlanes[i].equal(mLastAgentPlanes[i]);
		}
		mLastAgentPlanes[i].set(mAgentPlanes[i]);
	}

	return changed;
}

S32 LLCamera::AABBInFrustum(const LLVector4a& center, const LLVector4a& radius,
							const LLPlane* planes)
{
	if (!planes)
	{
		// Use agent space
		planes = mAgentPlanes;
	}

	U8 mask = 0;
	bool result = false;
	LLVector4a rscale, maxp, minp;
	LLSimdScalar d;
	// mAgentPlanes[] size is 7
	U32 max_planes = llmin(mPlaneCount, (U32)AGENT_PLANE_USER_CLIP_NUM);
	for (U32 i = 0; i < max_planes; ++i)
	{
		mask = mPlaneMask[i];
		if (mask < PLANE_MASK_NUM)
		{
			const LLPlane& p(planes[i]);
			p.getAt<3>(d);
			rscale.setMul(radius, sFrustumScaler[mask]);
			minp.setSub(center, rscale);
			d = -d;
			if (p.dot3(minp).getF32() > d)
			{
				return 0;
			}

			if (!result)
			{
				maxp.setAdd(center, rscale);
				result = (p.dot3(maxp).getF32() > d);
			}
		}
	}

	return result ? 1 : 2;
}

S32 LLCamera::AABBInFrustumNoFarClip(const LLVector4a& center,
									 const LLVector4a& radius,
									 const LLPlane* planes)
{
	if (!planes)
	{
		// Use agent space
		planes = mAgentPlanes;
	}

	U8 mask = 0;
	bool result = false;
	LLVector4a rscale, maxp, minp;
	LLSimdScalar d;
	// mAgentPlanes[] size is 7
	U32 max_planes = llmin(mPlaneCount, (U32)AGENT_PLANE_USER_CLIP_NUM);
	for (U32 i = 0; i < max_planes; ++i)
	{
		mask = mPlaneMask[i];
		if (i != 5 && mask < PLANE_MASK_NUM)
		{
			const LLPlane& p(planes[i]);
			p.getAt<3>(d);
			rscale.setMul(radius, sFrustumScaler[mask]);
			minp.setSub(center, rscale);
			d = -d;
			if (p.dot3(minp).getF32() > d)
			{
				return 0;
			}

			if (!result)
			{
				maxp.setAdd(center, rscale);
				result = p.dot3(maxp).getF32() > d;
			}
		}
	}

	return result ? 1 : 2;
}

S32 LLCamera::sphereInFrustumQuick(const LLVector3& sphere_center,
								   F32 radius)
{
	LLVector3 dist = sphere_center - mFrustCenter;
	F32 dsq = dist * dist;
	F32 rsq = mFarPlane * 0.5f + radius;
	rsq *= rsq;
	return dsq < rsq ? 1 : 0;
}

// Returns 1 if sphere is in frustum, 2 if fully in frustum, otherwise 0.
// NOTE: 'center' is in absolute frame.
S32 LLCamera::sphereInFrustum(const LLVector3& sphere_center,
							  F32 radius) const
{
	// Returns 1 if sphere is in frustum, 0 if not.
	bool res = false;
	for (S32 i = 0; i < 6; ++i)
	{
		if (mPlaneMask[i] != PLANE_MASK_NONE)
		{
			F32 d = mAgentPlanes[i].dist(sphere_center);

			if (d > radius)
			{
				return 0;
			}
			res = res || d > -radius;
		}
	}

	return res ? 1 : 2;
}

// Returns height of a sphere of given radius, located at center, in pixels
F32 LLCamera::heightInPixels(const LLVector3& center, F32 radius) const
{
	if (radius == 0.f)
	{
		return 0.f;
	}

	// If height initialized
	if (mViewHeightInPixels > -1)
	{
		// Convert sphere to coord system with 0,0,0 at camera
		LLVector3 vec = center - mOrigin;

		// Compute distance to sphere
		F32 dist = vec.length();

		// Calculate angle of whole object
		F32 angle = 2.f * atan2f(radius, dist);

		// Calculate fraction of field of view
		F32 fraction_of_fov = angle / mView;

		// Compute number of pixels tall, based on vertical field of view
		return (fraction_of_fov * mViewHeightInPixels);
	}

	// Return an invalid height
	return -1.f;
}

std::ostream& operator<<(std::ostream& s, const LLCamera& C)
{
	s << "{ \n  Center = " << C.getOrigin() << "\n";
	s << "  AtAxis = " << C.getXAxis() << "\n";
	s << "  LeftAxis = " << C.getYAxis() << "\n";
	s << "  UpAxis = " << C.getZAxis() << "\n";
	s << "  View = " << C.getView() << "\n";
	s << "  Aspect = " << C.getAspect() << "\n";
	s << "  NearPlane   = " << C.mNearPlane << "\n";
	s << "  FarPlane    = " << C.mFarPlane << "\n}";
	return s;
}

void LLCamera::calculateFrustumPlanes()
{
	// The planes only change when any of the frustum descriptions change.
	// They are not affected by changes of the position of the Frustum
	// because they are known in the view frame and the position merely
	// provides information on how to get from the absolute frame to the
	// view frame.
	F32 top = mFarPlane * tanf(0.5f * mView);
	F32 left = top * mAspect;
	calculateFrustumPlanes(left, -left, top, -top);
}

LL_INLINE static LLPlane plane_from_points(const LLVector3& p1,
										   const LLVector3& p2,
										   const LLVector3& p3)
{
	LLVector3 n = (p2 - p1) % (p3 - p1);
	n.normalize();
	return LLPlane(p1, n);
}

void LLCamera::calcAgentFrustumPlanes(LLVector3* frust)
{
	for (U32 i = 0; i < AGENT_FRUSTRUM_NUM; ++i)
	{
		mAgentFrustum[i] = frust[i];
	}

	mFrustumCornerDist = (frust[5] - getOrigin()).length();

	// Frust contains the 8 points of the frustum, calculate 6 planes

	// Order of planes is important, keep most likely to fail in the front of
	// the list

	// near - frust[0], frust[1], frust[2]
	mAgentPlanes[AGENT_PLANE_NEAR] = plane_from_points(frust[0], frust[1],
													   frust[2]);

	// Far
	mAgentPlanes[AGENT_PLANE_FAR] = plane_from_points(frust[5], frust[4],
													  frust[6]);

	// Left
	mAgentPlanes[AGENT_PLANE_LEFT] = plane_from_points(frust[4], frust[0],
													   frust[7]);

	// Right
	mAgentPlanes[AGENT_PLANE_RIGHT] = plane_from_points(frust[1], frust[5],
														frust[6]);

	// Top
	mAgentPlanes[AGENT_PLANE_TOP] = plane_from_points(frust[3], frust[2],
													  frust[6]);

	// Bottom
	mAgentPlanes[AGENT_PLANE_BOTTOM] = plane_from_points(frust[1], frust[0],
														 frust[4]);

	// Cache plane octant facing mask for use in AABBInFrustum
	for (U32 i = 0; i < mPlaneCount; ++i)
	{
		mPlaneMask[i] = mAgentPlanes[i].calcPlaneMask();
	}
}

// Calculate regional planes from mAgentPlanes. Vector "shift" is the vector of
// the region origin in the agent space.
void LLCamera::calcRegionFrustumPlanes(const LLVector3& shift,
									   F32 far_clip_distance)
{
	LLVector3 p = getOrigin();
	LLVector3 n(mAgentPlanes[5][0], mAgentPlanes[5][1], mAgentPlanes[5][2]);
	F32 dd = n * p;
	F32 far_w;
	if (dd + mAgentPlanes[5][3] < 0)	// Signed distance
	{
		far_w = -far_clip_distance - dd;
	}
	else
	{
		far_w = far_clip_distance - dd;
	}
	far_w += n * shift;

	F32 d;
	for (S32 i = 0 ; i < 7; ++i)
	{
		if (mPlaneMask[i] != 0xff)
		{
			n.set(mAgentPlanes[i][0], mAgentPlanes[i][1], mAgentPlanes[i][2]);

			if (i != 5)
			{
				d = mAgentPlanes[i][3] + n * shift;
			}
			else
			{
				d = far_w;
			}
			mRegionPlanes[i].setVec(n, d);
		}
	}
}

void LLCamera::calculateFrustumPlanes(F32 left, F32 right, F32 top, F32 bottom)
{
	// Calculate center and radius squared of frustum in world absolute
	// coordinates
	mFrustCenter = LLVector3::x_axis * mFarPlane * 0.5f;
	mFrustCenter = transformToAbsolute(mFrustCenter);
	mFrustRadiusSquared = mFarPlane * 0.5f;
	// Pad radius squared by 5%
	mFrustRadiusSquared *= mFrustRadiusSquared * 1.05f;
}

// x and y are in WINDOW space, so x = Y-Axis (left/right), y= Z-Axis(Up/Down)
void LLCamera::calculateFrustumPlanesFromWindow(F32 x1, F32 y1, F32 x2, F32 y2)
{
	F32 view_height = (F32)tanf(0.5f * mView) * mFarPlane;
	F32 view_width = view_height * mAspect;

	F32 left = x1 * -2.f * view_width;
	F32 right = x2 * -2.f * view_width;
	F32 bottom = y1 * 2.f * view_height;
	F32 top = y2 * 2.f * view_height;

	calculateFrustumPlanes(left, right, top, bottom);
}
