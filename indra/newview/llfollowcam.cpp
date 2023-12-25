/**
 * @file llfollowcam.cpp
 * @author Jeffrey Ventrella
 * @brief LLFollowCam class implementation
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfollowcam.h"

#include "llagent.h"

//-----------------------------------------------------------------------------
// Static variables
//-----------------------------------------------------------------------------

LLFollowCamMgr::param_map_t LLFollowCamMgr::sParamMap;
LLFollowCamMgr::param_stack_t LLFollowCamMgr::sParamStack;

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

constexpr F32 FOLLOW_CAM_ZOOM_FACTOR			= 0.1f;
constexpr F32 FOLLOW_CAM_MIN_ZOOM_AMOUNT		= 0.1f;
constexpr F32 DISTANCE_EPSILON					= 0.0001f;
// this will be correctly set on me by my caller
constexpr F32 DEFAULT_MAX_DISTANCE_FROM_SUBJECT	= 1000.f;

// This is how slowly the camera position moves to its ideal position
constexpr F32 FOLLOW_CAM_MIN_POSITION_LAG		= 0.f;
constexpr F32 FOLLOW_CAM_DEFAULT_POSITION_LAG	= 0.1f;
constexpr F32 FOLLOW_CAM_MAX_POSITION_LAG		= 3.f;

// This is how slowly the camera focus moves to its subject
constexpr F32 FOLLOW_CAM_MIN_FOCUS_LAG		= 0.f;
constexpr F32 FOLLOW_CAM_DEFAULT_FOCUS_LAG	= 0.1f;
constexpr F32 FOLLOW_CAM_MAX_FOCUS_LAG		= 3.f;

// This is far the position can get from its IDEAL POSITION before it starts
// getting pulled
constexpr F32 FOLLOW_CAM_MIN_POSITION_THRESHOLD		= 0.f;
constexpr F32 FOLLOW_CAM_DEFAULT_POSITION_THRESHOLD	= 1.f;
constexpr F32 FOLLOW_CAM_MAX_POSITION_THRESHOLD		= 4.f;

// This is far the focus can get from the subject before it starts getting
// pulled
constexpr F32 FOLLOW_CAM_MIN_FOCUS_THRESHOLD		= 0.f;
constexpr F32 FOLLOW_CAM_DEFAULT_FOCUS_THRESHOLD	= 1.f;
constexpr F32 FOLLOW_CAM_MAX_FOCUS_THRESHOLD		= 4.f;

// This is the distance the camera wants to be from the subject
constexpr F32 FOLLOW_CAM_MIN_DISTANCE		= 0.5f;
constexpr F32 FOLLOW_CAM_DEFAULT_DISTANCE	= 3.f;

// This is an angluar value. It affects the angle that the camera rises
// (pitches) in relation to the horizontal plane
constexpr F32 FOLLOW_CAM_MIN_PITCH		= -45.f;
constexpr F32 FOLLOW_CAM_DEFAULT_PITCH	= 0.f;
// Keep under 90 degrees TO avoid gimbal lock !
constexpr F32 FOLLOW_CAM_MAX_PITCH		= 80.f;

// How high or low the camera considers its ideal focus to be (relative to its
// subject)
constexpr F32 FOLLOW_CAM_MIN_FOCUS_OFFSET		= -10.f;
const LLVector3 FOLLOW_CAM_DEFAULT_FOCUS_OFFSET	=  LLVector3(1.f, 0.f, 0.f);
constexpr F32 FOLLOW_CAM_MAX_FOCUS_OFFSET		=  10.f;

// This affects the rate at which the camera adjusts to stay behind the subject
constexpr F32 FOLLOW_CAM_MIN_BEHINDNESS_LAG		= 0.f;
constexpr F32 FOLLOW_CAM_DEFAULT_BEHINDNESS_LAG	= 0.f;
constexpr F32 FOLLOW_CAM_MAX_BEHINDNESS_LAG		= 3.f;

// In degrees: this is the size of the pie slice behind the subject matter
// within which the camera is free to move
constexpr F32 FOLLOW_CAM_MIN_BEHINDNESS_ANGLE		= 0.f;
constexpr F32 FOLLOW_CAM_DEFAULT_BEHINDNESS_ANGLE	= 10.f;
constexpr F32 FOLLOW_CAM_MAX_BEHINDNESS_ANGLE		= 180.f;
constexpr F32 FOLLOW_CAM_BEHINDNESS_EPSILON			= 1.f;

//-----------------------------------------------------------------------------
// LLFollowCamParams
//-----------------------------------------------------------------------------
LLFollowCamParams::LLFollowCamParams()
:	mMaxCameraDistantFromSubject(DEFAULT_MAX_DISTANCE_FROM_SUBJECT),
	mPositionLocked(false),
	mFocusLocked(false),
	mUsePosition(false),
	mUseFocus(false),
	mPositionLag(FOLLOW_CAM_DEFAULT_POSITION_LAG),
	mFocusLag(FOLLOW_CAM_DEFAULT_FOCUS_LAG),
	mPositionThreshold(FOLLOW_CAM_DEFAULT_POSITION_THRESHOLD),
	mFocusThreshold(FOLLOW_CAM_DEFAULT_FOCUS_THRESHOLD),
	mBehindnessLag(FOLLOW_CAM_DEFAULT_BEHINDNESS_LAG),
	mPitch(FOLLOW_CAM_DEFAULT_PITCH),
	mFocusOffset(FOLLOW_CAM_DEFAULT_FOCUS_OFFSET),
	mBehindnessMaxAngle(FOLLOW_CAM_DEFAULT_BEHINDNESS_ANGLE)
{
	setDistance(FOLLOW_CAM_DEFAULT_DISTANCE);
}

void LLFollowCamParams::setPositionLag(F32 p)
{
	mPositionLag = llclamp(p, FOLLOW_CAM_MIN_POSITION_LAG,
						   FOLLOW_CAM_MAX_POSITION_LAG);
}

void LLFollowCamParams::setFocusLag(F32 f)
{
	mFocusLag = llclamp(f, FOLLOW_CAM_MIN_FOCUS_LAG,
						FOLLOW_CAM_MAX_FOCUS_LAG);
}

void LLFollowCamParams::setPositionThreshold(F32 p)
{
	mPositionThreshold = llclamp(p, FOLLOW_CAM_MIN_POSITION_THRESHOLD,
								 FOLLOW_CAM_MAX_POSITION_THRESHOLD);
}

void LLFollowCamParams::setFocusThreshold(F32 f)
{
	mFocusThreshold = llclamp(f, FOLLOW_CAM_MIN_FOCUS_THRESHOLD,
							  FOLLOW_CAM_MAX_FOCUS_THRESHOLD);
}

void LLFollowCamParams::setPitch(F32 p)
{
	mPitch = llclamp(p, FOLLOW_CAM_MIN_PITCH, FOLLOW_CAM_MAX_PITCH);
}

void LLFollowCamParams::setBehindnessLag(F32 b)
{
	mBehindnessLag = llclamp(b, FOLLOW_CAM_MIN_BEHINDNESS_LAG,
							 FOLLOW_CAM_MAX_BEHINDNESS_LAG);
}

void LLFollowCamParams::setBehindnessAngle(F32 b)
{
	mBehindnessMaxAngle = llclamp(b, FOLLOW_CAM_MIN_BEHINDNESS_ANGLE,
								  FOLLOW_CAM_MAX_BEHINDNESS_ANGLE);
}

void LLFollowCamParams::setDistance(F32 d)
{
	mDistance = llclamp(d, FOLLOW_CAM_MIN_DISTANCE,
						mMaxCameraDistantFromSubject);
}

void LLFollowCamParams::setFocusOffset(const LLVector3& v)
{
	mFocusOffset = v;
	mFocusOffset.clamp(FOLLOW_CAM_MIN_FOCUS_OFFSET,
					   FOLLOW_CAM_MAX_FOCUS_OFFSET);
}

//-----------------------------------------------------------------------------
// LLFollowCam
//-----------------------------------------------------------------------------
LLFollowCam::LLFollowCam()
:	LLFollowCamParams(),
	mUpVector(LLVector3::z_axis),
	mSubjectPosition(LLVector3::zero),
	mSubjectRotation(LLQuaternion::DEFAULT),
	mZoomedToMinimumDistance(false),
	mPitchCos(0.f),
	mPitchSin(0.f),
	mPitchSineAndCosineNeedToBeUpdated(true)
{
	mSimulatedDistance = mDistance;
}

void LLFollowCam::copyParams(LLFollowCamParams& params)
{
	setPositionLag(params.getPositionLag());
	setFocusLag(params.getFocusLag());
	setFocusThreshold(params.getFocusThreshold());
	setPositionThreshold(params.getPositionThreshold());
	setPitch(params.getPitch());
	setFocusOffset(params.getFocusOffset());
	setBehindnessAngle(params.getBehindnessAngle());
	setBehindnessLag(params.getBehindnessLag());

	setPositionLocked(params.getPositionLocked());
	setFocusLocked(params.getFocusLocked());

	setDistance(params.getDistance());
	if (params.getUsePosition())
	{
		setPosition(params.getPosition());
	}
	if (params.getUseFocus())
	{
		setFocus(params.getFocus());
	}
}

void LLFollowCam::update()
{
	// Update Focus

	LLVector3 offset_subject_pos = mSubjectPosition +
								   (mFocusOffset * mSubjectRotation);

	LLVector3 simulated_pos_agent =
		gAgent.getPosAgentFromGlobal(mSimulatedPositionGlobal);
	LLVector3 vec_to_subject = offset_subject_pos - simulated_pos_agent;
	F32 dist_to_subject = vec_to_subject.length();

	LLVector3 desired_focus = mFocus;
	LLVector3 focus_pt_agent =
		gAgent.getPosAgentFromGlobal(mSimulatedFocusGlobal);
	if (mFocusLocked)
	{
		// If focus is locked, only relative focus needs to be updated
		mRelativeFocus = (focus_pt_agent - mSubjectPosition) *
						 ~mSubjectRotation;
	}
	else
	{
		LLVector3 focus_offset = offset_subject_pos - focus_pt_agent;
		F32 focus_offset_dist = focus_offset.length();

		LLVector3 focus_offsetDirection = focus_offset / focus_offset_dist;
		desired_focus = focus_pt_agent +
						(focus_offsetDirection *
						 (focus_offset_dist - mFocusThreshold));
		if (focus_offset_dist > mFocusThreshold)
		{
			F32 lag_lerp = LLCriticalDamp::getInterpolant(mFocusLag);
			focus_pt_agent = lerp(focus_pt_agent, desired_focus, lag_lerp);
			mSimulatedFocusGlobal =
				gAgent.getPosGlobalFromAgent(focus_pt_agent);
		}
		mRelativeFocus = lerp(mRelativeFocus,
							  (focus_pt_agent - mSubjectPosition) *
							  ~mSubjectRotation,
							  LLCriticalDamp::getInterpolant(0.05f));
	}

	LLVector3 desired_cam_pos =
		gAgent.getPosAgentFromGlobal(mSimulatedPositionGlobal);
	if (mPositionLocked)
	{
		mRelativePos = (desired_cam_pos - mSubjectPosition) *
					   ~mSubjectRotation;
	}
	else
	{
		// Update Position

		//---------------------------------------------------------------------
		// Determine the horizontal vector from the camera to the subject
		//---------------------------------------------------------------------
		LLVector3 horiz_vector_to_subject = vec_to_subject;
		horiz_vector_to_subject.mV[VZ] = 0.f;

		//---------------------------------------------------------------------
		// Now I determine the horizontal distance
		//---------------------------------------------------------------------
		F32 horiz_distance_to_subject = horiz_vector_to_subject.length();

		//---------------------------------------------------------------------
		// Then I get the (normalized) horizontal direction...
		//---------------------------------------------------------------------
		LLVector3 horiz_dir_to_subject;
		if (horiz_distance_to_subject < DISTANCE_EPSILON)
		{
			// make sure we still have a normalized vector if distance is
			// really small (this case is rare and fleeting)
			horiz_dir_to_subject = LLVector3::z_axis;
		}
		else
		{
			// Not using the "normalize" method, because we can just divide
			// by horiz_distance_to_subject
			horiz_dir_to_subject = horiz_vector_to_subject /
								   horiz_distance_to_subject;
		}

		//---------------------------------------------------------------------
		// Here is where we determine an offset relative to subject position in
		// oder to set the ideal position.
		//---------------------------------------------------------------------
		if (mPitchSineAndCosineNeedToBeUpdated)
		{
			calculatePitchSineAndCosine();
			mPitchSineAndCosineNeedToBeUpdated = false;
		}

		LLVector3 offset_from_subject(horiz_dir_to_subject.mV[VX] *
									  mPitchCos,
									  horiz_dir_to_subject.mV[VY] *
									  mPitchCos, -mPitchSin);

		offset_from_subject *= mSimulatedDistance;

		//---------------------------------------------------------------------
		// Finally, ideal position is set by taking the subject position and
		// extending the offset_from_subject from that
		//---------------------------------------------------------------------
		LLVector3 ideal_cam_pos = offset_subject_pos - offset_from_subject;

		//---------------------------------------------------------------------
		// Now I prepare to move the current camera position towards its ideal
		// position...
		//---------------------------------------------------------------------
		LLVector3 vec_to_ideal_pos = ideal_cam_pos - simulated_pos_agent;
		F32 dist_to_ideal_pos = vec_to_ideal_pos.length();

		// Put this inside of the block ?
		LLVector3 normal_to_ideal_pos = vec_to_ideal_pos / dist_to_ideal_pos;

		desired_cam_pos = simulated_pos_agent +
						  normal_to_ideal_pos *
						  (dist_to_ideal_pos - mPositionThreshold);
		//---------------------------------------------------------------------
		// The following method takes the target camera position and resets it
		// so that it stays "behind" the subject, using behindness angle and
		// behindness force as parameters affecting the exact behavior
		//---------------------------------------------------------------------

		if (dist_to_ideal_pos > mPositionThreshold)
		{
			F32 pos_pull_lerp = LLCriticalDamp::getInterpolant(mPositionLag);
			simulated_pos_agent = lerp(simulated_pos_agent, desired_cam_pos,
									   pos_pull_lerp);
		}

		//--------------------------------------------------------------------
		// Do not let the camera get farther than its official max distance
		//--------------------------------------------------------------------
		if (dist_to_subject > mMaxCameraDistantFromSubject)
		{
			LLVector3 dir_to_subject = vec_to_subject / dist_to_subject;
			simulated_pos_agent = offset_subject_pos -
								  dir_to_subject *
								  mMaxCameraDistantFromSubject;
		}

		//---------------------------------------------------------------------
		// The following method takes mSimulatedPositionGlobal and resets it so
		// that it stays "behind" the subject, using behindness angle and
		// behindness force as parameters affecting the exact behavior
		//---------------------------------------------------------------------
		updateBehindnessConstraint(gAgent.getPosAgentFromGlobal(mSimulatedFocusGlobal),
								   simulated_pos_agent);
		mSimulatedPositionGlobal =
			gAgent.getPosGlobalFromAgent(simulated_pos_agent);

		mRelativePos = lerp(mRelativePos,
							(simulated_pos_agent - mSubjectPosition) *
							~mSubjectRotation,
							LLCriticalDamp::getInterpolant(0.05f));
	}

	// Update UpVector. This just points upward for now, but I anticipate
	// future effects requiring some rolling ("banking" effects for fun, swoopy
	// vehicles, etc)
	mUpVector = LLVector3::z_axis;
}

bool LLFollowCam::updateBehindnessConstraint(LLVector3 focus,
											 LLVector3& cam_position)
{
	bool constraint_active = false;
	// Only apply this stuff if the behindness angle is something other than
	// opened up all the way
	if (mBehindnessMaxAngle < FOLLOW_CAM_MAX_BEHINDNESS_ANGLE -
							  FOLLOW_CAM_BEHINDNESS_EPSILON)
	{
		//---------------------------------------------------------------------
		// Horizontalized vector from focus to camera
		//---------------------------------------------------------------------
		LLVector3 horiz_vec_to_cam;
		horiz_vec_to_cam.set(cam_position - focus);
		horiz_vec_to_cam.mV[VZ] = 0.f;
		F32 cameraZ = cam_position.mV[VZ];

		//---------------------------------------------------------------------
		// Distance of horizontalized vector
		//---------------------------------------------------------------------
		F32 horizontalDistance = horiz_vec_to_cam.length();

		//---------------------------------------------------------------------
		// Calculate horizontalized back vector of the subject and scale by
		// horizontalDistance
		//---------------------------------------------------------------------
		LLVector3 horiz_subject_back(-1.f, 0.f, 0.f);
		horiz_subject_back *= mSubjectRotation;
		horiz_subject_back.mV[VZ] = 0.f;
		// because horizontalizing might make it shorter than 1
		horiz_subject_back.normalize();
		horiz_subject_back *= horizontalDistance;

		//---------------------------------------------------------------------
		// Find the angle (in degrees) between these vectors
		//---------------------------------------------------------------------
		F32 cam_offset_angle = 0.f;
		LLQuaternion camera_offset_rot;
		camera_offset_rot.shortestArc(horiz_subject_back, horiz_vec_to_cam);
		LLVector3 cam_offset_rot_axis;
		camera_offset_rot.getAngleAxis(&cam_offset_angle, cam_offset_rot_axis);
		cam_offset_angle *= RAD_TO_DEG;

		if (cam_offset_angle > mBehindnessMaxAngle)
		{
			F32 fraction = ((cam_offset_angle - mBehindnessMaxAngle) /
							cam_offset_angle) *
						   LLCriticalDamp::getInterpolant(mBehindnessLag);
			cam_position = focus + horiz_subject_back *
						   (slerp(fraction, camera_offset_rot,
								  LLQuaternion::DEFAULT));
			// clamp z value back to what it was before we started messing with
			// it
			cam_position.mV[VZ] = cameraZ;
			constraint_active = true;
		}
	}
	return constraint_active;
}

void LLFollowCam::calculatePitchSineAndCosine()
{
	F32 radian = mPitch * DEG_TO_RAD;
	mPitchCos = cosf(radian);
	mPitchSin = sinf(radian);
}

void LLFollowCam::zoom(S32 z)
{
	F32 zoomAmount = z * mSimulatedDistance * FOLLOW_CAM_ZOOM_FACTOR;

	if (zoomAmount < FOLLOW_CAM_MIN_ZOOM_AMOUNT &&
		zoomAmount > -FOLLOW_CAM_MIN_ZOOM_AMOUNT)
	{
		if (zoomAmount < 0.f)
		{
			zoomAmount = -FOLLOW_CAM_MIN_ZOOM_AMOUNT;
		}
		else
		{
			zoomAmount = FOLLOW_CAM_MIN_ZOOM_AMOUNT;
		}
	}

	mSimulatedDistance += zoomAmount;

	mZoomedToMinimumDistance = false;
	if (mSimulatedDistance < FOLLOW_CAM_MIN_DISTANCE)
	{
		mSimulatedDistance = FOLLOW_CAM_MIN_DISTANCE;

		// If zoomAmount is negative (i.e. getting closer), then we hit the
		// minimum:
		if (zoomAmount < 0.f)
		{
			mZoomedToMinimumDistance = true;
		}
	}
	else if (mSimulatedDistance > mMaxCameraDistantFromSubject)
	{
		mSimulatedDistance = mMaxCameraDistantFromSubject;
	}
}

void LLFollowCam::reset(const LLVector3 p, const LLVector3 f,
						const LLVector3 u)
{
	setPosition(p);
	setFocus(f);
	mUpVector = u;
}

void LLFollowCam::setPitch(F32 p)
{
	LLFollowCamParams::setPitch(p);
	mPitchSineAndCosineNeedToBeUpdated = true; // important
}

void LLFollowCam::setDistance(F32 d)
{
	if (d != mDistance)
	{
		LLFollowCamParams::setDistance(d);
		mSimulatedDistance = d;
		mZoomedToMinimumDistance = false;
	}
}

void LLFollowCam::setPosition(const LLVector3& p)
{
	if (p != mPosition)
	{
		LLFollowCamParams::setPosition(p);
		mSimulatedPositionGlobal = gAgent.getPosGlobalFromAgent(mPosition);
		if (mPositionLocked)
		{
			mRelativePos = (mPosition - mSubjectPosition) * ~mSubjectRotation;
		}
	}
}

void LLFollowCam::setFocus(const LLVector3& f)
{
	if (f != mFocus)
	{
		LLFollowCamParams::setFocus(f);
		mSimulatedFocusGlobal = gAgent.getPosGlobalFromAgent(f);
		if (mFocusLocked)
		{
			mRelativeFocus = (mFocus - mSubjectPosition) * ~mSubjectRotation;
		}
	}
}

void LLFollowCam::setPositionLocked(bool locked)
{
	LLFollowCamParams::setPositionLocked(locked);
	if (locked)
	{
		// Propagate set position to relative position
		mRelativePos =
			(gAgent.getPosAgentFromGlobal(mSimulatedPositionGlobal) -
			 mSubjectPosition) * ~mSubjectRotation;
	}
}

void LLFollowCam::setFocusLocked(bool locked)
{
	LLFollowCamParams::setFocusLocked(locked);
	if (locked)
	{
		// Propagate set position to relative position
		mRelativeFocus = (gAgent.getPosAgentFromGlobal(mSimulatedFocusGlobal) -
						  mSubjectPosition) * ~mSubjectRotation;
	}
}

//-----------------------------------------------------------------------------
// LLFollowCamMgr class
//-----------------------------------------------------------------------------

//static
void LLFollowCamMgr::cleanupClass()
{
	for (auto it = sParamMap.begin(), end = sParamMap.end(); it != end; ++it)
	{
		delete it->second;
	}
	sParamMap.clear();

	sParamStack.clear();
}

//static
void LLFollowCamMgr::setPositionLag(const LLUUID& source, F32 lag)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setPositionLag(lag);
	}
}

//static
void LLFollowCamMgr::setFocusLag(const LLUUID& source, F32 lag)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setFocusLag(lag);
	}
}

//static
void LLFollowCamMgr::setFocusThreshold(const LLUUID& source, F32 threshold)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setFocusThreshold(threshold);
	}
}

//static
void LLFollowCamMgr::setPositionThreshold(const LLUUID& source, F32 threshold)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setPositionThreshold(threshold);
	}
}

//static
void LLFollowCamMgr::setDistance(const LLUUID& source, F32 distance)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setDistance(distance);
	}
}

//static
void LLFollowCamMgr::setPitch(const LLUUID& source, F32 pitch)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setPitch(pitch);
	}
}

//static
void LLFollowCamMgr::setFocusOffset(const LLUUID& source,
									const LLVector3& offset)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setFocusOffset(offset);
	}
}

//static
void LLFollowCamMgr::setBehindnessAngle(const LLUUID& source, F32 angle)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setBehindnessAngle(angle);
	}
}

//static
void LLFollowCamMgr::setBehindnessLag(const LLUUID& source, F32 force)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setBehindnessLag(force);
	}
}

//static
void LLFollowCamMgr::setPosition(const LLUUID& source,
								 const LLVector3 position)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setPosition(position);
	}
}

//static
void LLFollowCamMgr::setFocus(const LLUUID& source, const LLVector3 focus)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setFocus(focus);
	}
}

//static
void LLFollowCamMgr::setPositionLocked(const LLUUID& source, bool locked)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setPositionLocked(locked);
	}
}

//static
void LLFollowCamMgr::setFocusLocked(const LLUUID& source, bool locked)
{
	LLFollowCamParams* paramsp = getParamsForID(source);
	if (paramsp)
	{
		paramsp->setFocusLocked(locked);
	}
}

//static
LLFollowCamParams* LLFollowCamMgr::getParamsForID(const LLUUID& source)
{
	LLFollowCamParams* params = NULL;

	param_map_t::iterator found_it = sParamMap.find(source);
	if (found_it == sParamMap.end()) // didn't find it?
	{
		params = new LLFollowCamParams();
		sParamMap[source] = params;
	}
	else
	{
		params = found_it->second;
	}

	return params;
}

//static
void LLFollowCamMgr::setCameraActive(const LLUUID& source, bool active)
{
	LLFollowCamParams* params = getParamsForID(source);
	param_stack_t::iterator found_it = std::find(sParamStack.begin(),
												 sParamStack.end(), params);
	if (found_it != sParamStack.end())
	{
		sParamStack.erase(found_it);
	}
	// Put on top of stack
	if (active)
	{
		sParamStack.push_back(params);
	}
}

//static
void LLFollowCamMgr::removeFollowCamParams(const LLUUID& source)
{
	setCameraActive(source, false);
	LLFollowCamParams* params = getParamsForID(source);
	sParamMap.erase(source);
	delete params;
}

//static
void LLFollowCamMgr::dump()
{
	S32 param_count = 0;
	llinfos << "Scripted camera active stack" << llendl;
	for (S32 i = 0, count = sParamStack.size(); i < count; ++i)
	{
		LLFollowCamParams* params = sParamStack[i];
		llinfos << param_count++
				<< " - rot_limit: " << params->getBehindnessAngle()
				<< " - rot_lag: " << params->getBehindnessLag()
				<< " - distance: " << params->getDistance()
				<< " - focus: " << params->getFocus()
				<< " - foc_lag: " << params->getFocusLag()
				<< " - foc_lock: " << (params->getFocusLocked() ? "Y" : "N")
				<< " - foc_offset: " << params->getFocusOffset()
				<< " - foc_thresh: " << params->getFocusThreshold()
				<< " - pitch: " << params->getPitch()
				<< " - pos: " << params->getPosition()
				<< " - pos_lag: " << params->getPositionLag()
				<< " - pos_lock: " << (params->getPositionLocked() ? "Y" : "N")
				<< " - pos_thresh: " << params->getPositionThreshold()
				<< llendl;
	}
}
