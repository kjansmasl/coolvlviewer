/**
 * @file llflexibleobject.cpp
 * @brief Flexible object implementation
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llflexibleobject.h"

#include "llfasttimer.h"
#include "llglheaders.h"

#include "llagent.h"
#include "lldrawpoolbump.h"
#include "llface.h"
#include "llpipeline.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llvoavatar.h"
#include "llworld.h"

//static
F32 LLVolumeImplFlexible::sUpdateFactor = 1.f;
LLVolumeImplFlexible::instances_list_t LLVolumeImplFlexible::sInstanceList;

constexpr F32 FLEXI_FPS = 60.f; // 60 flexi updates per second

// LLFlexibleObjectData::pack/unpack now in llprimitive.cpp

LLVolumeImplFlexible::LLVolumeImplFlexible(LLViewerObject* vo,
										   LLFlexibleObjectData* attributes)
:	mVO(vo),
	mAttributes(attributes),
	mLastFrameNum(0),
	mLastUpdatePeriod(0),
	mInitializedRes(-1),
	mSimulateRes(0),
	mRenderRes(1),
	mCollisionSphereRadius(0.f),
	mInitialized(false),
	mUpdated(false)
{
	static U32 seed = 0;
	mID = seed++;

	if (mVO->mDrawable.notNull())
	{
		mVO->mDrawable->makeActive();
	}

	mInstanceIndex = sInstanceList.size();
	sInstanceList.push_back(this);
}

//virtual
LLVolumeImplFlexible::~LLVolumeImplFlexible()
{
	S32 end_idx = sInstanceList.size() - 1;

	if (end_idx != mInstanceIndex)
	{
		sInstanceList[mInstanceIndex] = sInstanceList[end_idx];
		sInstanceList[mInstanceIndex]->mInstanceIndex = mInstanceIndex;
	}

	sInstanceList.pop_back();
}

//static
void LLVolumeImplFlexible::initClass()
{
	// Let's avoid memory fragmentation over time...
	sInstanceList.reserve(8192);
}

//static
void LLVolumeImplFlexible::updateClass()
{
	U64 virtual_frame = LLTimer::getElapsedSeconds() * FLEXI_FPS;
	for (std::vector<LLVolumeImplFlexible*>::iterator
			iter = sInstanceList.begin(), end = sInstanceList.end();
		 iter != end; ++iter)
	{
		LLVolumeImplFlexible* impl = *iter;
		// Note: by now update period might have changed
		if (impl->mRenderRes == -1 ||
			impl->mLastFrameNum +
			impl->mLastUpdatePeriod <= virtual_frame ||
			impl->mLastFrameNum > virtual_frame) // time issues, overflow
			
		{
			impl->doIdleUpdate();
		}
	}
}

//static
void LLVolumeImplFlexible::dumpStats()
{
	llinfos << "sInstanceList capacity reached: " << sInstanceList.capacity()
			<< llendl;
}

LLVector3 LLVolumeImplFlexible::getFramePosition() const
{
	return mVO->getRenderPosition();
}

LLQuaternion LLVolumeImplFlexible::getFrameRotation() const
{
	return mVO->getRenderRotation();
}

//virtual
void LLVolumeImplFlexible::onParameterChanged(U16 param_type,
											  LLNetworkData* data,
											  bool in_use,
											  bool local_origin)
{
	if (param_type == LLNetworkData::PARAMS_FLEXIBLE)
	{
		mAttributes = (LLFlexibleObjectData*)data;
		setAttributesOfAllSections();
	}
}

//virtual
void LLVolumeImplFlexible::onShift(const LLVector4a& shift_vector)
{
	// VECTORIZE THIS
	LLVector3 shift(shift_vector.getF32ptr());
	for (S32 section = 0; section < (1 << FLEXIBLE_OBJECT_MAX_SECTIONS) + 1;
		 ++section)
	{
		mSection[section].mPosition += shift;
	}
}

void LLVolumeImplFlexible::setParentPositionAndRotationDirectly(LLVector3 p,
																LLQuaternion r)
{
	mParentPosition = p;
	mParentRotation = r;
}

void LLVolumeImplFlexible::remapSections(LLFlexibleObjectSection* source,
										 S32 source_sections,
										 LLFlexibleObjectSection* dest,
										 S32 dest_sections)
{
	S32 num_output_sections = 1 << dest_sections;
	LLVector3 scale = mVO->mDrawable->getScale();
	F32 source_section_length = scale.mV[VZ] / (F32)(1 << source_sections);
	F32 section_length = scale.mV[VZ] / (F32)num_output_sections;
	if (source_sections == -1)
	{
		// Generate all from section 0
		dest[0] = source[0];
		for (S32 section = 0; section < num_output_sections; ++section)
		{
			dest[section + 1] = dest[section];
			dest[section + 1].mPosition += dest[section].mDirection *
										   section_length;
			dest[section + 1].mVelocity.setZero();
		}
	}
	else if (source_sections > dest_sections)
	{
		// Copy, skipping sections
		S32 num_steps = 1 << (source_sections - dest_sections);

		// Copy from left to right since it may be an in-place computation
		for (S32 section = 0; section < num_output_sections; ++section)
		{
			dest[section + 1] = source[(section + 1) * num_steps];
		}
		dest[0] = source[0];
	}
	else if (source_sections < dest_sections)
	{
		// Interpolate section info
		// Iterate from right to left since it may be an in-place computation
		S32 step_shift = dest_sections - source_sections;
		S32 num_steps = 1 << step_shift;
		for (S32 section = num_output_sections - num_steps; section >= 0;
			 section -= num_steps)
		{
			LLFlexibleObjectSection* last_source_section =
				&source[section >> step_shift];
			LLFlexibleObjectSection* source_section =
				&source[(section >> step_shift) + 1];

			// Cubic interpolation of position
			// At^3 + Bt^2 + Ct + D = f(t)
			LLVector3 d = last_source_section->mPosition;
			LLVector3 c = last_source_section->mdPosition *
						  source_section_length;
			LLVector3 y = source_section->mdPosition * source_section_length -
						  c;
			LLVector3 x = source_section->mPosition - d - c;
			LLVector3 a = y - 2 * x;
			LLVector3 b = x - a;

			F32 t_inc = 1.f / F32(num_steps);
			F32 t = t_inc;
			for (S32 step = 1; step < num_steps; ++step)
			{
				dest[section + step].mScale = lerp(last_source_section->mScale,
												   source_section->mScale, t);
				dest[section + step].mAxisRotation =
					slerp(t, last_source_section->mAxisRotation,
						  source_section->mAxisRotation);

				// Evaluate output interpolated values
				F32 t_sq = t * t;
				dest[section + step].mPosition = t_sq * (t * a + b) +
												 t * c + d;
				dest[section + step].mRotation =
					slerp(t, last_source_section->mRotation,
						  source_section->mRotation);
				dest[section + step].mVelocity =
					lerp(last_source_section->mVelocity,
						 source_section->mVelocity, t);
				dest[section + step].mDirection =
					lerp(last_source_section->mDirection,
						 source_section->mDirection, t);
				dest[section + step].mdPosition =
					lerp(last_source_section->mdPosition,
						 source_section->mdPosition, t);
				dest[section + num_steps] = *source_section;
				t += t_inc;
			}
		}
		dest[0] = source[0];
	}
	else
	{
		// Numbers are equal. Copy info
		for (S32 section = 0; section <= num_output_sections; ++section)
		{
			dest[section] = source[section];
		}
	}
}

void LLVolumeImplFlexible::setAttributesOfAllSections(LLVector3* in_scale)
{
	LLVector2 bottom_scale, top_scale;
	F32 begin_rot = 0, end_rot = 0;
	LLVolume* volumep = mVO->getVolume();
	if (volumep)
	{
		const LLPathParams& params = volumep->getParams().getPathParams();
		bottom_scale = params.getBeginScale();
		top_scale = params.getEndScale();
		begin_rot = F_PI * params.getTwistBegin();
		end_rot = F_PI * params.getTwistEnd();
	}

	if (!mVO->mDrawable)
	{
		return;
	}

	S32 num_sections = 1 << mSimulateRes;

	LLVector3 scale = in_scale ? *in_scale : mVO->mDrawable->getScale();

	mSection[0].mPosition = getAnchorPosition();
	mSection[0].mDirection = LLVector3::z_axis * getFrameRotation();
	mSection[0].mdPosition = mSection[0].mDirection;
	mSection[0].mScale.set(scale.mV[VX] * bottom_scale.mV[0],
						   scale.mV[VY] * bottom_scale.mV[1]);
	mSection[0].mVelocity.setZero();
	mSection[0].mAxisRotation.setAngleAxis(begin_rot, 0.f, 0.f, 1.f);

	remapSections(mSection, mInitializedRes, mSection, mSimulateRes);
	mInitializedRes = mSimulateRes;

	F32 t_inc = 1.f / F32(num_sections);
	F32 t = t_inc;

	for (S32 i = 1; i <= num_sections; ++i)
	{
		mSection[i].mAxisRotation.setAngleAxis(lerp(begin_rot, end_rot, t),
											   0.f, 0.f, 1.f);
		mSection[i].mScale = LLVector2(scale.mV[VX] * lerp(bottom_scale.mV[0],
									   top_scale.mV[0], t),
									   scale.mV[VY] * lerp(bottom_scale.mV[1],
									   top_scale.mV[1], t));
		t += t_inc;
	}
}

void LLVolumeImplFlexible::updateRenderRes()
{
	if (!mAttributes) return;

	LLDrawable* drawablep = mVO->mDrawable;

	S32 new_res = mAttributes->getSimulateLOD();

#if 1 // Optimal approximation of previous behavior that does not rely on atan2
	F32 app_angle = mVO->getScale().mV[2] / drawablep->mDistanceWRTCamera;

	// Rendering sections increases with visible angle on the screen
	mRenderRes = (S32)(12.f * app_angle);
#else // legacy behavior
	// Number of segments only cares about z axis
	F32 app_angle = ll_round(atan2f(mVO->getScale().mV[2] * 2.f,
									drawablep->mDistanceWRTCamera) *
							 RAD_TO_DEG, 0.01f);

 	// Rendering sections increases with visible angle on the screen
	mRenderRes = (S32)(FLEXIBLE_OBJECT_MAX_SECTIONS * 4 * app_angle *
					   DEG_TO_RAD / gViewerCamera.getView());
#endif

	mRenderRes = llclamp(mRenderRes, new_res - 1,
						 (S32)FLEXIBLE_OBJECT_MAX_SECTIONS);

	// Throttle back simulation of segments we're not rendering
	if (mRenderRes < new_res)
	{
		new_res = mRenderRes;
	}

	if (!mInitialized || mSimulateRes != new_res)
	{
		mSimulateRes = new_res;
		setAttributesOfAllSections();
		mInitialized = true;
	}
}

//-----------------------------------------------------------------------------
// This calculates the physics of the flexible object. Note that it has to be 0
// updated every time step. In the future, perhaps there could be an
// optimization similar to what Havok does for objects that are stationary.
//-----------------------------------------------------------------------------
//virtual
void LLVolumeImplFlexible::doIdleUpdate()
{
	LLDrawable* drawablep = mVO->mDrawable;
	if (!drawablep) return;

	LL_FAST_TIMER(FTM_FLEXIBLE_UPDATE);

	// Ensure drawable is active
	drawablep->makeActive();

	if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_FLEXIBLE))
	{
		bool visible = drawablep->isVisible();
		if (!mInitialized || (mSimulateRes == 0 && visible))
		{
			updateRenderRes();
			gPipeline.markRebuild(drawablep, LLDrawable::REBUILD_POSITION);
		}
		else
		{
			F32 pixel_area = mVO->getPixelArea();
			U32 update_period = (U32)(gViewerCamera.getScreenPixelArea() *
									  0.01f / (pixel_area *
											   (sUpdateFactor + 1.f))) + 1;
			// Note: flexies afar will be rarely updated, closer ones will be
			// updated more frequently. But frequency differences are extremely
			// noticeable, so consider modifying update factor, or at least
			// clamping value a bit more from both sides.
			update_period = llclamp(update_period, 1U, 32U);

			// We control how fast flexies update, buy splitting updates among
			// frames
			U64 virtual_frame = LLTimer::getElapsedSeconds() * FLEXI_FPS;
			if (visible)
			{
				if (!drawablep->isState(LLDrawable::IN_REBUILD_QUEUE) &&
					mVO->getPixelArea() > 256.f)
				{
					U32 id;
					if (mVO->isRootEdit())
					{
						id = mID;
					}
					else
					{
						LLVOVolume* parent = (LLVOVolume*)mVO->getParent();
						if (!parent) return;	// Paranoia
						id = parent->getVolumeInterfaceID();
					}

					// Throttle flexies and spread load by preventing them from
					// updating in same frame. Reflects how many frames we need 
					//to wait before next update.
					U64 throttling_delay = (virtual_frame + id) %
										   update_period;
					if ((throttling_delay == 0 &&
						 // One or more virtual frames per frame
						 mLastFrameNum < virtual_frame) ||
						// ... or if we missed a virtual frame
						(mLastFrameNum + update_period < virtual_frame) ||
						// ... or in case of an overflow
						mLastFrameNum > virtual_frame)
					{
						// We need mLastFrameNum to compensate for 'unreliable
						// time' and to filter 'duplicate' frames. If it
						// happened too late, subtract throttling_delay (it is
						// zero otherwise)
						mLastFrameNum = virtual_frame - throttling_delay;

						// Store update period for updateClass().
						// Note: consider substituting update_period with
						// mLastUpdatePeriod everywhere.
						mLastUpdatePeriod = update_period;

						updateRenderRes();

						mVO->shrinkWrap();
						gPipeline.markRebuild(drawablep,
											  LLDrawable::REBUILD_POSITION);
					}
				}
			}
			else
			{
				mLastFrameNum = virtual_frame;
				mLastUpdatePeriod = update_period;
			}
		}
	}
}

LL_INLINE S32 log2(S32 x)
{
	S32 ret = 0;
	while (x > 1)
	{
		++ret;
		x >>= 1;
	}
	return ret;
}

void LLVolumeImplFlexible::doFlexibleUpdate()
{
	LL_FAST_TIMER(FTM_DO_FLEXIBLE_UPDATE);
	LLVolume* volume = mVO->getVolume();
	LLPath* path = &volume->getPath();
	if ((mSimulateRes == 0 || !mInitialized) && mVO->mDrawable->isVisible())
	{
		doIdleUpdate();

		if (mSimulateRes != 0 ||
			!gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_FLEXIBLE))
		{
			// We did not get updated or initialized, proceeding without can be
			// dangerous
			return;
		}
	}

	if (!mInitialized || !mAttributes)
	{
		// The object is not visible
		return;
	}

	// Fix for MAINT-1894
	// Skipping the flexible update if render res is negative. If we were to
	// continue with a negative value, the subsequent
	// S32 num_render_sections = 1 << mRenderRes;
	// code would result in a really large number of render sections which
	// would then create a length exception in the std::vector::resize()
	// method.
	if (mRenderRes < 0)
	{
		return;
	}

	S32 num_sections = 1 << mSimulateRes;

	LLVector3 base_pos = getFramePosition();
	LLQuaternion base_rot = getFrameRotation();
	LLQuaternion parent_segment_rot = base_rot;
	LLVector3 anchor_dir_rotated = LLVector3::z_axis * parent_segment_rot;
	LLVector3 anchor_scale = mVO->mDrawable->getScale();

	F32 section_length = anchor_scale.mV[VZ] / (F32)num_sections;
	F32 inv_section_length = 1.f / section_length;

	S32 i;

	// ANCHOR position is offset from BASE position (centroid) by half the
	// length
	LLVector3 anchor_pos = base_pos -
						   anchor_scale.mV[VZ] * 0.5f * anchor_dir_rotated;
	mSection[0].mPosition = anchor_pos;
	mSection[0].mDirection = anchor_dir_rotated;
	mSection[0].mRotation = base_rot;

	// Coefficients which are constant across sections
    F32 seconds_this_frame = llmin(mTimer.getElapsedTimeAndResetF32(), 0.2f);
	F32 t_factor = mAttributes->getTension() * 0.1f;
	t_factor = llmin(t_factor *
					 (1.f - powf(0.85f, seconds_this_frame * 30.f)),
					 FLEXIBLE_OBJECT_MAX_INTERNAL_TENSION_FORCE);

	F32 friction_coeff = mAttributes->getAirFriction() * 2.f + 1.f;
	friction_coeff = llmax(powf(10.f, friction_coeff * seconds_this_frame),
						   1.f);
	F32 momentum = 1.f / friction_coeff;

	F32 wind_factor = mAttributes->getWindSensitivity() * 0.1f *
					  section_length * seconds_this_frame;
	F32 max_angle = atanf(section_length * 2.f);

	F32 force_factor = section_length * seconds_this_frame;

	// Update simulated sections
	LLVector3 parent_section_vec, parent_section_pos, parent_dir, last_pos;
	LLQuaternion delta_rot;
	for (i = 1; i <= num_sections; ++i)
	{
		// Save value of position as last_pos
		last_pos = mSection[i].mPosition;

		// Gravity
		mSection[i].mPosition.mV[2] -= mAttributes->getGravity() * force_factor;

		// Wind force
		if (mAttributes->getWindSensitivity() > 0.001f && gAgent.getRegion())
		{
			mSection[i].mPosition +=
				gAgent.getRegion()->mWind.getVelocity(mSection[i].mPosition) *
				wind_factor;
		}

		// User-defined force
		mSection[i].mPosition += mAttributes->getUserForce() * force_factor;

		// Tension (rigidity, stiffness)
		parent_section_pos = mSection[i - 1].mPosition;
		parent_dir = mSection[i - 1].mDirection;

		if (i == 1)
		{
			parent_section_vec = mSection[0].mDirection;
		}
		else
		{
			parent_section_vec = mSection[i - 2].mDirection;
		}

		mSection[i].mPosition += (parent_section_vec * section_length -
								  (mSection[i].mPosition -
								   parent_section_pos)) * t_factor;

#if 0	// Sphere collision, currently not used
		if (mAttributes->mUsingCollisionSphere)
		{
			LLVector3 center_collision_sphere_vec =
				mCollisionSpherePosition - mSection[i].mPosition;
			if (center_collision_sphere_vec.lengthSquared() <
					mCollisionSphereRadius * mCollisionSphereRadius)
			{
				F32 center_collision_sphere_dist =
					center_collision_sphere_vec.length();
				F32 penetration = mCollisionSphereRadius -
								  center_collision_sphere_dist;

				LLVector3 center_collision_sphere_norm;
				if (center_collision_sphere_dist > 0.f)
				{
					center_collision_sphere_norm =
						center_collision_sphere_vec /
						center_collision_sphere_dist;
				}
				else // rare
				{
					// Arbitrary
					center_collision_sphere_norm = LLVector3::x_axis;
				}

				// Push the position out to the surface of the collision sphere
				mSection[i].mPosition -= center_collision_sphere_norm *
										 penetration;
			}
		}
#endif

		// Inertia
		mSection[i].mPosition += mSection[i].mVelocity * momentum;

		// Clamp length & rotation
		mSection[i].mDirection = mSection[i].mPosition - parent_section_pos;
		mSection[i].mDirection.normalize();
		delta_rot.shortestArc(parent_dir, mSection[i].mDirection);

		F32 angle;
		LLVector3 axis;
		delta_rot.getAngleAxis(&angle, axis);
		if (angle > F_PI)
		{
			angle -= 2.f * F_PI;
		}
		else if (angle < -F_PI)
		{
			angle += 2.f * F_PI;
		}
		if (angle > max_angle)
		{
			// Angle = 0.5f * (angle + max_angle);
			delta_rot.setAngleAxis(max_angle, axis);
		}
		else if (angle < -max_angle)
		{
			// Angle = 0.5f * (angle - max_angle);
			delta_rot.setAngleAxis(-max_angle, axis);
		}
		parent_segment_rot = parent_segment_rot * delta_rot;

		mSection[i].mDirection = parent_dir * delta_rot;
		mSection[i].mPosition = parent_section_pos + mSection[i].mDirection *
								section_length;
		mSection[i].mRotation = parent_segment_rot;

		if (i > 1)
		{
			// Propogate half the rotation up to the parent
			LLQuaternion half_delta_rot(angle * 0.5f, axis);
			mSection[i - 1].mRotation = mSection[i - 1].mRotation *
										half_delta_rot;
		}

		// Calculate velocity
		mSection[i].mVelocity = mSection[i].mPosition - last_pos;
		if (mSection[i].mVelocity.lengthSquared() > 1.f)
		{
			mSection[i].mVelocity.normalize();
		}
	}

	// Calculate derivatives (not necessary until normals are automagically
	// generated)
	mSection[0].mdPosition = (mSection[1].mPosition - mSection[0].mPosition) *
							 inv_section_length;
	// i = 1..NumSections-1
	LLVector3 a, b;
	for (i = 1; i < num_sections; ++i)
	{
		// Quadratic numerical derivative of position
		// f(-L1) = aL1^2 - bL1 + c = f1
		// f(0)   =               c = f2
		// f(L2)  = aL2^2 + bL2 + c = f3
		// f = ax^2 + bx + c
		// d/dx f = 2ax + b
		// d/dx f(0) = b
		// c = f2
		// a = [(f1-c)/L1 + (f3-c)/L2] / (L1+L2)
		// b = (f3-c-aL2^2)/L2

		a = (mSection[i - 1].mPosition - mSection[i].mPosition +
			 mSection[i + 1].mPosition - mSection[i].mPosition) * 0.5f *
			inv_section_length * inv_section_length;
		b = mSection[i + 1].mPosition - mSection[i].mPosition -
			a * (section_length * section_length);
		b *= inv_section_length;

		mSection[i].mdPosition = b;
	}

	// i = NumSections
	mSection[i].mdPosition = (mSection[i].mPosition -
							  mSection[i - 1].mPosition) * inv_section_length;

	// Create points
	S32 num_render_sections = 1 << mRenderRes;
	if (path->getPathLength() != num_render_sections + 1)
	{
		((LLVOVolume*)mVO)->mVolumeChanged = true;
		volume->resizePath(num_render_sections + 1);
	}

	LLFlexibleObjectSection new_section[(1 << FLEXIBLE_OBJECT_MAX_SECTIONS) + 1];
	remapSections(mSection, mSimulateRes, new_section, mRenderRes);

	// Generate transform from global to prim space

	delta_rot = ~getFrameRotation();
	LLVector3 delta_pos = -getFramePosition() * delta_rot;

	// Vertex transform (4x4)
	LLVector3 x_axis = LLVector3::x_axis * delta_rot;
	LLVector3 y_axis = LLVector3::y_axis * delta_rot;
	LLVector3 z_axis = LLVector3::z_axis * delta_rot;
	LLMatrix4 rel_xform;
	rel_xform.initRows(LLVector4(x_axis, 0.f), LLVector4(y_axis, 0.f),
					   LLVector4(z_axis, 0.f), LLVector4(delta_pos, 1.f));

	LLQuaternion rot;
	for (i = 0; i <= num_render_sections; ++i)
	{
		LLPath::PathPt* new_point = &path->mPath[i];
		LLVector3 pos = new_section[i].mPosition * rel_xform;
		rot = mSection[i].mAxisRotation * new_section[i].mRotation * delta_rot;

		LLVector3 np(new_point->mPos.getF32ptr());

		if (!mUpdated ||
			(np - pos).length() / mVO->mDrawable->mDistanceWRTCamera > .001f)
		{
			new_point->mPos.load3((new_section[i].mPosition * rel_xform).mV);
			mUpdated = false;
		}

		new_point->mRot.loadu(LLMatrix3(rot));
		new_point->mScale.set(new_section[i].mScale.mV[0],
							  new_section[i].mScale.mV[1], 0.f, 1.f);
		new_point->mTexT = (F32)i / num_render_sections;
	}

	mLastSegmentRotation = parent_segment_rot;
}

//virtual
void LLVolumeImplFlexible::preRebuild()
{
	if (!mUpdated)
	{
		doFlexibleRebuild(false);
	}
}

void LLVolumeImplFlexible::doFlexibleRebuild(bool rebuild_volume)
{
	LL_FAST_TIMER(FTM_FLEXIBLE_REBUILD);

	mUpdated = true;

	LLVolume* volume = mVO->getVolume();
	if (!volume)
	{
		return;
	}

	if (rebuild_volume)
	{
		volume->setDirty();
	}
	volume->regen();
}

//virtual
void LLVolumeImplFlexible::onSetScale(const LLVector3& scale, bool damped)
{
	setAttributesOfAllSections((LLVector3*)&scale);
}

//virtual
bool LLVolumeImplFlexible::doUpdateGeometry(LLDrawable* drawable)
{
	LLVOVolume* volume = (LLVOVolume*)mVO;
	if (!volume || volume->isDead() || volume->mDrawable.isNull() ||
		volume->mDrawable->isDead())
	{
		return true; // No update to complete
	}

	if (mVO->isAttachment())
	{
		// Do not update flexible attachments for impostored avatars unless the
		// impostor is being updated this frame.
		LLViewerObject* parent = (LLViewerObject*)mVO->getParent();
		while (parent && !parent->isAvatar())
		{
			parent = (LLViewerObject*)parent->getParent();
		}

		if (parent)
		{
			LLVOAvatar* avatar = (LLVOAvatar*)parent;
			if (avatar->isImpostor() && !avatar->needsImpostorUpdate())
			{
				return true;
			}
		}
	}

	if (volume->mLODChanged)
	{
		LLVolumeParams volume_params = volume->getVolume()->getParams();
		volume->setVolume(volume_params, 0);
		mUpdated = false;
	}

	volume->updateRelativeXform();

	doFlexibleUpdate();

	// Object may have been rotated, which means it needs a rebuild.
	bool rotated = false;
	LLQuaternion cur_rotation = getFrameRotation();
	if (cur_rotation != mLastFrameRotation)
	{
		mLastFrameRotation = cur_rotation;
		rotated = true;
	}

	if (volume->mLODChanged || volume->mFaceMappingChanged ||
		volume->mVolumeChanged ||
		drawable->isState(LLDrawable::REBUILD_MATERIAL))
	{
		volume->regenFaces();
		volume->mDrawable->setState(LLDrawable::REBUILD_VOLUME);
		volume->dirtySpatialGroup();
		doFlexibleRebuild(volume->mVolumeChanged);
		volume->genBBoxes(isVolumeGlobal());
	}
	else if (!mUpdated || rotated)
	{
		volume->mDrawable->setState(LLDrawable::REBUILD_POSITION);
		LLSpatialGroup* group = volume->mDrawable->getSpatialGroup();
		if (group)
		{
			group->dirtyMesh();
		}
		volume->genBBoxes(isVolumeGlobal());
	}

	volume->mVolumeChanged = false;
	volume->mLODChanged = false;
	volume->mFaceMappingChanged = false;

	// Clear UV flag
	drawable->clearState(LLDrawable::UV);

	return true;
}

LLVector3 LLVolumeImplFlexible::getEndPosition()
{
	S32 num_sections = 1 << mAttributes->getSimulateLOD();
	return mSection[num_sections].mPosition;
}

LLVector3 LLVolumeImplFlexible::getNodePosition(S32 node_idx)
{
	S32 num_sections = 1 << mAttributes->getSimulateLOD();
	if (node_idx > num_sections - 1)
	{
		node_idx = num_sections - 1;
	}
	else if (node_idx < 0)
	{
		node_idx = 0;
	}

	return mSection[node_idx].mPosition;
}

LLVector3 LLVolumeImplFlexible::getPivotPosition() const
{
	return getAnchorPosition();
}

LLVector3 LLVolumeImplFlexible::getAnchorPosition() const
{
	LLVector3 anchor_dir_rotated = LLVector3::z_axis * getFrameRotation();
	LLVector3 anchor_scale = mVO->mDrawable->getScale();
	return getFramePosition() - anchor_scale.mV[VZ] * 0.5f *
		   anchor_dir_rotated;
}

LLQuaternion LLVolumeImplFlexible::getEndRotation()
{
	return mLastSegmentRotation;
}

//virtual
void LLVolumeImplFlexible::updateRelativeXform(bool force_identity)
{
	LLVOVolume* vo = (LLVOVolume*)mVO;
	if (!vo || !vo->mDrawable) return;	// Paranoia

	// Matrix from local space to parent relative/global space
	LLQuaternion delta_rot;
	LLVector3 delta_pos;
	bool use_identity = force_identity || vo->mDrawable->isSpatialRoot();
	if (!use_identity)
	{
		delta_rot = vo->mDrawable->getRotation();
		delta_pos = vo->mDrawable->getPosition();
	}

	// Vertex transform (4x4)
	LLVector3 x_axis = LLVector3::x_axis * delta_rot;
	LLVector3 y_axis = LLVector3::y_axis * delta_rot;
	LLVector3 z_axis = LLVector3::z_axis * delta_rot;
	vo->mRelativeXform.initRows(LLVector4(x_axis, 0.f), LLVector4(y_axis, 0.f),
								LLVector4(z_axis, 0.f),
								LLVector4(delta_pos, 1.f));

	x_axis.normalize();
	y_axis.normalize();
	z_axis.normalize();

	vo->mRelativeXformInvTrans.setRows(x_axis, y_axis, z_axis);
}

//virtual
const LLMatrix4& LLVolumeImplFlexible::getWorldMatrix(LLXformMatrix* xform) const
{
	return xform->getWorldMatrix();
}
