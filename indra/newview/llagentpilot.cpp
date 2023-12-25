/**
 * @file llagentpilot.cpp
 * @brief LLAgentPilot class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llagentpilot.h"

#include "lldir.h"
#include "llnotifications.h"
#if 0					// For renderAutoPilotTarget(), which is not used
#include "llrender.h"
#endif

#include "llagent.h"
#include "llappviewer.h"
#include "llstartup.h"
#include "hbviewerautomation.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llvoavatarself.h"
#include "llworld.h"

// Autopilot constants
constexpr F32 AUTOPILOT_HEIGHT_ADJUST_DISTANCE = 8.f;			// meters
constexpr F32 AUTOPILOT_MIN_TARGET_HEIGHT_OFF_GROUND = 1.f;		// meters
constexpr F32 AUTOPILOT_MAX_TIME_NO_PROGRESS_WALK = 1.5f;		// seconds
constexpr F32 AUTOPILOT_MAX_TIME_NO_PROGRESS_FLY = 2.5f;		// seconds

LLAgentPilot gAgentPilot;

bool LLAgentPilot::sLoop = true;
bool LLAgentPilot::sAllowFlying = false;

LLAgentPilot::LLAgentPilot()
:	mAutoPilotFinishedCallback(NULL),
	mAutoPilotCallbackData(NULL),
	mAutoPilotStopDistance(1.f),
	mAutoPilotTargetDist(0.f),
	mAutoPilotRotationThreshold(0.f),
	mAutoPilotNoProgressFrameCount(0),
	mAutoPilot(false),
	mAutoPilotUseRotation(false),
	mAutoPilotAllowFlying(false),
	mAutoPilotFlyOnStop(false),
	mNumRuns(-1),
	mCurrentAction(0),
	mLastRecordTime(0.f),
	mRecording(false),
	mStarted(false),
	mPlaying(false),
	mAllowFlying(false)
{
}

void LLAgentPilot::startAutoPilotGlobal(const LLVector3d& target_global,
										const std::string& behavior_name,
										const LLQuaternion* target_rotation,
										void (*finish_callback)(bool, void*),
										void* callback_data, F32 stop_distance,
										F32 rot_threshold, bool allow_flying)
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	if (target_global.isExactlyZero())
	{
		llwarns << "Cancelling attempt to start autopilot towards invalid position"
				<< llendl;
		return;
	}

	mAutoPilotFinishedCallback = finish_callback;
	mAutoPilotCallbackData = callback_data;
	mAutoPilotRotationThreshold = rot_threshold;
	mAutoPilotBehaviorName = behavior_name;
	mAutoPilotAllowFlying = allow_flying;

	LLVector3d delta_pos(target_global);
	delta_pos -= gAgent.getPositionGlobal();
	F64 distance = delta_pos.length();

	if (stop_distance > 0.f)
	{
		mAutoPilotStopDistance = stop_distance;
	}
	else
	{
		// Guess at a reasonable stop distance.
		mAutoPilotStopDistance = sqrtf(distance);
		if (mAutoPilotStopDistance < 0.5f)
		{
			mAutoPilotStopDistance = 0.5f;
		}
	}

	LLVector3d trace_target = target_global;
	trace_target.mdV[VZ] -= 10.f;
	LLVector3d intersection;
	LLVector3 normal;
	LLViewerObject* hit_obj;
	F32 height_delta = gWorld.resolveStepHeightGlobal(NULL, target_global,
													 trace_target,
													 intersection,
													 normal, &hit_obj);

	mAutoPilotFlyOnStop = mAutoPilotAllowFlying && gAgent.getFlying();

	if (distance > 30.f && mAutoPilotAllowFlying)
	{
		gAgent.setFlying(true);
	}

	if (distance > 2.f && mAutoPilotAllowFlying &&
		height_delta > sqrtf(mAutoPilotStopDistance) + 2.f)
	{
		gAgent.setFlying(true);
		// Do not force flying for "Sit" behaviour to prevent flying after
		// pressing "Stand" from an object.
		if (mAutoPilotBehaviorName != "Sit")
		{
			mAutoPilotFlyOnStop = true;
		}
	}

	mAutoPilot = true;
	mAutoPilotTargetGlobal = target_global;

	// Trace ray down to find height of destination from ground
	LLVector3d trace_end_pt = target_global;
	trace_end_pt.mdV[VZ] -= 20.f;
	LLVector3d target_on_gnd;
	LLVector3 gnd_norm;
	LLViewerObject* obj;
	gWorld.resolveStepHeightGlobal(NULL, target_global, trace_end_pt,
								  target_on_gnd, gnd_norm, &obj);
	F64 target_height = llmax((F64)gAgentAvatarp->getPelvisToFoot(),
							  target_global.mdV[VZ] - target_on_gnd.mdV[VZ]);

	// Clamp z value of target to minimum height above ground
	mAutoPilotTargetGlobal.mdV[VZ] = target_on_gnd.mdV[VZ] + target_height;
	mAutoPilotTargetDist = (F32)dist_vec(gAgent.getPositionGlobal(),
										 mAutoPilotTargetGlobal);
	if (target_rotation)
	{
		mAutoPilotUseRotation = true;
		mAutoPilotTargetFacing = LLVector3::x_axis * *target_rotation;
		mAutoPilotTargetFacing.mV[VZ] = 0.f;
		mAutoPilotTargetFacing.normalize();
	}
	else
	{
		mAutoPilotUseRotation = false;
	}

	mAutoPilotNoProgressFrameCount = 0;
}

bool LLAgentPilot::startFollowPilot(const LLUUID& leader_id, bool allow_flying,
									F32 stop_distance)
{
	if (mAutoPilot || leader_id.isNull() || !isAgentAvatarValid())
	{
		return false;
	}

	LLViewerObject* object = gObjectList.findObject(leader_id);
	if (!object)
	{
		mLeaderID.setNull();
		return false;
	}

	mLeaderID = leader_id;
	startAutoPilotGlobal(object->getPositionGlobal(), mLeaderID.asString(),
						 NULL, NULL, NULL, stop_distance, 0.03f,
						 allow_flying);
	return true;
}

void LLAgentPilot::stopAutoPilot(bool user_cancel)
{
	if (!mAutoPilot || !isAgentAvatarValid())
	{
		return;
	}

	mAutoPilot = false;
	if (mAutoPilotUseRotation && !user_cancel)
	{
		gAgent.resetAxes(mAutoPilotTargetFacing);
	}
	// Note: auto pilot can terminate for a reason other than reaching the
	// destination
	bool reached = dist_vec(gAgent.getPositionGlobal(),
							mAutoPilotTargetGlobal) < mAutoPilotStopDistance;
	if (gAutomationp)
	{
		gAutomationp->onAutoPilotFinished(mAutoPilotBehaviorName, user_cancel,
										  reached);
	}
	if (mAutoPilotFinishedCallback)
	{
		mAutoPilotFinishedCallback(!user_cancel && reached,
								   mAutoPilotCallbackData);
	}
	mLeaderID.setNull();

	// If the user cancelled, do not change the fly state
	if (!user_cancel)
	{
		gAgent.setFlying(mAutoPilotFlyOnStop);
	}
	gAgent.setControlFlags(AGENT_CONTROL_STOP);

	if (user_cancel && mAutoPilotBehaviorName == "Attach")
	{
		gNotifications.add("CancelledAttach");
	}
	else if (!mAutoPilotBehaviorName.empty())
	{
		llinfos << "Auto-pilot "<< mAutoPilotBehaviorName
				<< " was canceled by user action." << llendl;
	}
	else
	{
		LL_DEBUGS("AutoPilot") << "Auto-pilot was canceled by user action."
							   << LL_ENDL;
	}
}

// Returns necessary agent pitch and yaw changes, radians.
void LLAgentPilot::autoPilot(F32* delta_yaw)
{
	if (!mAutoPilot || !isAgentAvatarValid())
	{
		return;
	}

	if (mLeaderID.notNull())
	{
		LLViewerObject* object = gObjectList.findObject(mLeaderID);
		if (!object)
		{
			stopAutoPilot();
			return;
		}
		mAutoPilotTargetGlobal = object->getPositionGlobal();
	}

	if (mAutoPilotAllowFlying && gAgentAvatarp->mInAir)
	{
		gAgent.setFlying(true);
	}

	LLVector3 at = gAgent.getAtAxis();
	LLVector3 agent_tgt = gAgent.getPosAgentFromGlobal(mAutoPilotTargetGlobal);
	LLVector3 direction = agent_tgt - gAgent.getPositionAgent();

	F32 target_dist = direction.length();
	if (target_dist >= mAutoPilotTargetDist)
	{
		++mAutoPilotNoProgressFrameCount;
	}

	bool flying = gAgent.getFlying();

	S32 frame_delta;
	if (flying)
	{
		frame_delta = AUTOPILOT_MAX_TIME_NO_PROGRESS_FLY * gFPSClamped;
	}
	else
	{
		frame_delta = AUTOPILOT_MAX_TIME_NO_PROGRESS_WALK * gFPSClamped;
	}
	if (mAutoPilotNoProgressFrameCount > frame_delta)
	{
		stopAutoPilot();
		return;
	}

	mAutoPilotTargetDist = target_dist;

	// Make this a two-dimensional solution

	at.mV[VZ] = 0.f;
	at.normalize();

	direction.mV[VZ] = 0.f;
	F32 xy_distance = direction.normalize();

	F32 yaw = 0.f;
	if (mAutoPilotTargetDist > mAutoPilotStopDistance)
	{
		yaw = angle_between(gAgent.getAtAxis(), direction);
	}
	else if (mAutoPilotUseRotation)
	{
		// We are close now just aim at target facing
		yaw = angle_between(at, mAutoPilotTargetFacing);
		direction = mAutoPilotTargetFacing;
	}

	yaw = 4.f * yaw / gFPSClamped;

	// Figure out which direction to turn
	LLVector3 scratch(at % direction);

	if (scratch.mV[VZ] > 0.f)
	{
		gAgent.setControlFlags(AGENT_CONTROL_YAW_POS);
	}
	else
	{
		yaw = -yaw;
		gAgent.setControlFlags(AGENT_CONTROL_YAW_NEG);
	}

	*delta_yaw = yaw;

	// Compute when to start slowing down and when to stop
	F32 slow_distance;
	if (flying)
	{
		slow_distance = llmax(8.f, mAutoPilotStopDistance + 5.f);
	}
	else
	{
		slow_distance = llmax(3.f, mAutoPilotStopDistance + 2.f);
	}

	// If we are flying, handle autopilot points above or below you.
	if (flying && xy_distance < AUTOPILOT_HEIGHT_ADJUST_DISTANCE)
	{
		F64 curr_height = gAgentAvatarp->getPositionGlobal().mdV[VZ];
		F32 delta_z = (F32)(mAutoPilotTargetGlobal.mdV[VZ] - curr_height);
		F32 slope = delta_z / xy_distance;
		if (slope > 0.45f && delta_z > 6.f)
		{
			gAgent.setControlFlags(AGENT_CONTROL_FAST_UP |
								   AGENT_CONTROL_UP_POS);
		}
		else if (slope > 0.002f && delta_z > 0.5f)
		{
			gAgent.setControlFlags(AGENT_CONTROL_UP_POS);
		}
		else if (slope < -0.45f && delta_z < -6.f &&
				 curr_height > AUTOPILOT_MIN_TARGET_HEIGHT_OFF_GROUND)
		{
			gAgent.setControlFlags(AGENT_CONTROL_FAST_UP |
								   AGENT_CONTROL_UP_NEG);
		}
		else if (slope < -0.002f && delta_z < -0.5f &&
				 curr_height > AUTOPILOT_MIN_TARGET_HEIGHT_OFF_GROUND)
		{
			gAgent.setControlFlags(AGENT_CONTROL_UP_NEG);
		}
	}

	// Calculate delta rotation to target heading
	F32 delta_target_heading = angle_between(gAgent.getAtAxis(),
											 mAutoPilotTargetFacing);

	if (xy_distance > slow_distance && yaw < F_PI / 10.f)
	{
		// Walking/flying fast
		gAgent.setControlFlags(AGENT_CONTROL_FAST_AT | AGENT_CONTROL_AT_POS);
	}
	else if (mAutoPilotTargetDist > mAutoPilotStopDistance)
	{
		U32 movement_flag = 0;
		// Walking/flying slow
		if (at * direction > 0.9f)
		{
			movement_flag = AGENT_CONTROL_AT_POS;
		}
		else if (at * direction < -0.9f)
		{
			movement_flag = AGENT_CONTROL_AT_NEG;
		}
		if (flying)
		{
			// Flying is too fast and has high inertia, artificially slow it
			// down. Do not update flags too often: server might not react.
			static U64 last_time_microsec = 0;
			U64 time_microsec = LLTimer::totalTime();
			U64 delta = time_microsec - last_time_microsec;
			// Fly during ~0-40 ms, stop during ~40-250 ms
			if (delta > 250000) // 250ms
			{
				// Reset even if !movement_flag
				last_time_microsec = time_microsec;
			}
			else if (delta > 40000) // 40 ms
			{
				gAgent.clearControlFlags(AGENT_CONTROL_AT_POS |
										 AGENT_CONTROL_AT_POS);
				movement_flag = 0;
			}
		}
		if (movement_flag)
		{
			gAgent.setControlFlags(movement_flag);
		}
	}

	// Check to see if we need to keep rotating to target orientation
	if (mAutoPilotTargetDist < mAutoPilotStopDistance)
	{
		gAgent.setControlFlags(AGENT_CONTROL_STOP);
		if (!mAutoPilotUseRotation ||
			delta_target_heading < mAutoPilotRotationThreshold)
		{
			stopAutoPilot();
		}
	}
}

#if 0	// Not used
// Draw a representation of current autopilot target
void LLAgentPilot::renderAutoPilotTarget()
{
	if (mAutoPilot)
	{
		F32 height_meters;
		LLVector3d target_global;

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();

		// Not textured
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		// Lovely green
		gGL.color4f(0.f, 1.f, 1.f, 1.f);

		target_global = mAutoPilotTargetGlobal;

		gGL.translatef((F32)(target_global.mdV[VX]),
					   (F32)(target_global.mdV[VY]),
					   (F32)(target_global.mdV[VZ]));

		height_meters = 1.f;

		gGL.scalef(height_meters, height_meters, height_meters);

		gSphere.render();

		gGL.popMatrix();
	}
}
#endif

bool LLAgentPilot::load(const std::string& filename)
{
	if (filename.empty() || !LLStartUp::isLoggedIn())
	{
		return false;
	}

	std::string fullpath = filename;
	if (gDirUtilp->getExtension(filename) != "plt")
	{
		fullpath += ".plt";
	}

	fullpath = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT, fullpath);
	if (!LLFile::exists(fullpath))
	{
		return false;
	}

	llifstream file(fullpath.c_str());
	if (!file.is_open())
	{
		llwarns << "Could not open: " << fullpath << ". Aborted." << llendl;
		return false;
	}

	llinfos << "Loading pilot file: " << fullpath << llendl;

	S32 num_actions;
	file >> num_actions;

	mActions.reserve(num_actions);
	for (S32 i = 0; i < num_actions; ++i)
	{
		S32 action_type;
		Action new_action;
		file >> new_action.mTime >> action_type;
		file >> new_action.mTarget.mdV[VX] >> new_action.mTarget.mdV[VY]
			 >> new_action.mTarget.mdV[VZ];
		new_action.mType = (EActionType)action_type;
		mActions.push_back(new_action);
	}

	file.close();

	return true;
}

bool LLAgentPilot::save(const std::string& filename)
{
	S32 count = mActions.size();
	if (filename.empty() || !count || !LLStartUp::isLoggedIn())
	{
		return false;
	}

	std::string fullpath = filename;
	if (gDirUtilp->getExtension(filename) != "plt")
	{
		fullpath += ".plt";
	}

	fullpath = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT, fullpath);
	llofstream file(fullpath.c_str());
	if (!file.is_open())
	{
		llwarns << "Could not open: " << fullpath << " Aborted." << llendl;
		return false;
	}

	llinfos << "Saving to pilot file: " << fullpath << llendl;
	file << count << '\n';

	for (S32 i = 0; i < count; ++i)
	{
		file << mActions[i].mTime << "\t" << mActions[i].mType << "\t";
		file << std::setprecision(32) << mActions[i].mTarget.mdV[VX] << "\t"
			 << mActions[i].mTarget.mdV[VY] << "\t"
			 << mActions[i].mTarget.mdV[VZ] << '\n';
	}

	file.close();

	return true;
}

//static
void LLAgentPilot::remove(const std::string& filename)
{
	if (filename.empty() || !LLStartUp::isLoggedIn())
	{
		return;
	}

	std::string fullpath = filename;
	if (gDirUtilp->getExtension(filename) != "plt")
	{
		fullpath += ".plt";
	}

	fullpath = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT, fullpath);
	if (LLFile::exists(fullpath))
	{
		llinfos << "Deleting pilot file: " << fullpath << llendl;
		LLFile::remove(fullpath);
	}
}

bool LLAgentPilot::startRecord()
{
	if (mRecording || mPlaying || !isAgentAvatarValid())
	{
		return false;
	}
	mActions.clear();
	mTimer.reset();
	addAction(STRAIGHT);
	mRecording = true;
	return true;
}

bool LLAgentPilot::stopRecord()
{
	if (!mRecording || !isAgentAvatarValid())
	{
		return false;
	}
	addAction(STRAIGHT);
	mRecording = false;
	return true;
}

void LLAgentPilot::addAction(enum EActionType action_type)
{
	if (!isAgentAvatarValid())
	{
		return;
	}
	LL_DEBUGS("AutoPilot") << "Adding waypoint: " << gAgent.getPositionGlobal()
						   << LL_ENDL;
	Action action;
	action.mType = action_type;
	action.mTarget = gAgent.getPositionGlobal();
	action.mTime = mTimer.getElapsedTimeF32();
	mLastRecordTime = (F32)action.mTime;
	mActions.push_back(action);
}

bool LLAgentPilot::startPlayback(S32 num_runs, bool allow_flying)
{
	if (mPlaying || mRecording || mActions.empty() || !isAgentAvatarValid())
	{
		return false;
	}

	mNumRuns = num_runs;
	mAllowFlying = allow_flying;
	mPlaying = true;
	mCurrentAction = 0;
	mTimer.reset();
	llinfos << "Starting playback, moving to waypoint 0." << llendl;
	if (!allow_flying)
	{
		gAgent.setFlying(false);
	}
	startAutoPilotGlobal(mActions[0].mTarget, "Playback", NULL, NULL, NULL,
						 0.5f, 0.03f, allow_flying);
	mStarted = false;
	return true;
}

bool LLAgentPilot::stopPlayback()
{
	if (!mPlaying)
	{
		return false;
	}

	mPlaying = false;
	mCurrentAction = 0;
	mTimer.reset();
	stopAutoPilot();
	return true;
}

void LLAgentPilot::updateTarget()
{
	if (mPlaying)
	{
		if (mCurrentAction < (S32)mActions.size())
		{
			if (mCurrentAction == 0)
			{
				if (mAutoPilot)
				{
					// Wait until we get to the first location before starting.
					return;
				}
				else if (!mStarted)
				{
					llinfos << "At start, beginning playback" << llendl;
					mTimer.reset();
					mStarted = true;
				}
			}
			if (mTimer.getElapsedTimeF32() > mActions[mCurrentAction].mTime)
			{
#if 0
				stopAutoPilot();
#endif
				if (++mCurrentAction < (S32)mActions.size())
				{
					startAutoPilotGlobal(mActions[mCurrentAction].mTarget,
										 "Playback", NULL, NULL, NULL, 0.5f,
										 0.03f, mAllowFlying);
				}
				else
				{
					stopPlayback();
					if (--mNumRuns != 0)
					{
						llinfos << "Looping playback." << llendl;
						startPlayback(mNumRuns, mAllowFlying);
					}
					else
					{
						llinfos << "Done with all runs, disabling pilot."
								<< llendl;
					}
				}
			}
		}
		else
		{
			stopPlayback();
		}
	}
	else if (mRecording && mTimer.getElapsedTimeF32() - mLastRecordTime > 1.f)
	{
		addAction(STRAIGHT);
	}
}

//static
void LLAgentPilot::beginRecord(void*)
{
	gAgentPilot.startRecord();
}

//static
void LLAgentPilot::endRecord(void*)
{
	if (gAgentPilot.stopRecord())
	{
		gAgentPilot.save(gSavedSettings.getString("AutoPilotFile"));
	}
}

//static
void LLAgentPilot::forgetRecord(void*)
{
	if (gAgentPilot.mRecording || gAgentPilot.mPlaying)
	{
		llwarns << "Cannot forget a record while recording or playing it."
				<< llendl;
		return;
	}
	gAgentPilot.mActions.clear();
	remove(gSavedSettings.getString("AutoPilotFile"));
}

//static
void LLAgentPilot::startPlayback(void*)
{
	gAgentPilot.startPlayback(sLoop ? -1 : 1, sAllowFlying);
}

//static
void LLAgentPilot::stopPlayback(void*)
{
	gAgentPilot.stopPlayback();
}
