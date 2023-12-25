/**
 * @file llviewercamera.h
 * @brief LLViewerCamera class header file
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

#ifndef LL_LLVIEWERCAMERA_H
#define LL_LLVIEWERCAMERA_H

#include "llcamera.h"
#include "lltimer.h"
#include "llstat.h"
#include "llmatrix4.h"

class LLCoordGL;
class LLViewerObject;

constexpr bool FOR_SELECTION = true;
constexpr bool NOT_FOR_SELECTION = false;

class alignas(16) LLViewerCamera final : public LLCamera
{
protected:
	LOG_CLASS(LLViewerCamera);

public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	enum eCameraID
	{
		CAMERA_WORLD = 0,
		CAMERA_SUN_SHADOW0,
		CAMERA_SUN_SHADOW1,
		CAMERA_SUN_SHADOW2,
		CAMERA_SUN_SHADOW3,
		CAMERA_SPOT_SHADOW0,
		CAMERA_SPOT_SHADOW2,
		CAMERA_WATER0,
		CAMERA_WATER1,
		NUM_CAMERAS
	};

	LLViewerCamera();

	void initClass();	// Called from LLAgent::init()

	void updateCameraLocation(const LLVector3& center,
							  const LLVector3& up_direction,
							  const LLVector3& point_of_interest);

	static void updateFrustumPlanes(LLCamera& camera, bool ortho = false,
									bool zflip = false, bool no_hacks = false);
	static void updateCameraAngle(void* user_data, const LLSD& value);
	void setPerspective(bool for_selection, S32 x, S32 y_from_bot, S32 width,
						S32 height, bool limit_select_distance,
						F32 z_near = 0, F32 z_far = 0);

	const LLMatrix4& getProjection() const;
	const LLMatrix4& getModelview() const;

	// Warning!  These assume the current global matrices are correct
	void projectScreenToPosAgent(S32 screen_x, S32 screen_y,
								 LLVector3* pos_agent) const;
	bool projectPosAgentToScreen(const LLVector3& pos_agent,
								 LLCoordGL& out_point,
								 bool clamp = true) const;
	bool projectPosAgentToScreenEdge(const LLVector3& pos_agent,
									 LLCoordGL& out_point) const;

	LL_INLINE const LLVector3* getVelocityDir() const	{ return &mVelocityDir; }
	LL_INLINE F32 getCosHalfFov()						{ return mCosHalfCameraFOV; }
	LL_INLINE F32 getAverageSpeed()						{ return mAverageSpeed; }
	LL_INLINE F32 getAverageAngularSpeed()				{ return mAverageAngularSpeed; }

	void getPixelVectors(const LLVector3& pos_agent, LLVector3& up,
						 LLVector3& right);
	LLVector3 roundToPixel(const LLVector3& pos_agent);

	// Sets the current matrix
	void setView(F32 vertical_fov_rads) override;

	// Sets FOV without broadcasting to simulator (for temporary local cameras)
	LL_INLINE void setViewNoBroadcast(F32 vertical_fov_rads)
	{
		LLCamera::setView(vertical_fov_rads);
	}

	void setDefaultFOV(F32 fov);
	LL_INLINE F32 getDefaultFOV()						{ return mCameraFOVDefault; }
	bool isDefaultFOVChanged();

	bool cameraUnderWater() const;

	LL_INLINE const LLVector3& getPointOfInterest()		{ return mLastPointOfInterest; }

	bool areVertsVisible(LLViewerObject* volumep, bool all_verts);

	LL_INLINE F32 getPixelMeterRatio() const			{ return mPixelMeterRatio; }
	LL_INLINE S32 getScreenPixelArea() const			{ return mScreenPixelArea; }

	LL_INLINE void setZoomParameters(F32 factor, S16 subregion)
	{
		mZoomFactor = factor;
		mZoomSubregion = subregion;
	}

	LL_INLINE F32 getZoomFactor()						{ return mZoomFactor; }
	LL_INLINE S16 getZoomSubRegion()					{ return mZoomSubregion; }

	LL_INLINE static const LLStat& getVelocityStat()	{ return sVelocityStat; }

	LL_INLINE static const LLStat& getAngularVelocityStat()
	{
		return sAngularVelocityStat;
	}

protected:
	void calcProjection(F32 far_distance) const;

protected:
	// Cache of perspective matrix
	mutable LLMatrix4	mProjectionMatrix;
	mutable LLMatrix4	mModelviewMatrix;

	LLVector3			mVelocityDir;
	F32					mAverageSpeed;
	F32					mAverageAngularSpeed;

	F32					mCameraFOVDefault;
	F32					mPrevCameraFOVDefault;
	F32					mCosHalfCameraFOV;
	LLVector3			mLastPointOfInterest;
	// Divide by distance from camera to get pixels per meter at that distance.
	F32					mPixelMeterRatio;
	// Pixel area of entire window
	S32					mScreenPixelArea;
	F32					mZoomFactor;
	S16					mZoomSubregion;

public:
	static LLStat		sVelocityStat;
	static LLStat		sAngularVelocityStat;
	static S32			sCurCameraID;
};

extern LLViewerCamera gViewerCamera;

#endif // LL_LLVIEWERCAMERA_H
