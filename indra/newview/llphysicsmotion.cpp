/**
 * @file llphysicsmotion.cpp
 * @brief Implementation of LLPhysicsMotion class.
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 *
 * Copyright (C) 2011, Linden Research, Inc.
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

#include "llphysicsmotion.h"

#include "llcharacter.h"
#include "lldriverparam.h"
#include "llfasttimer.h"
#include "llviewervisualparam.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llvoavatarself.h"

typedef std::map<std::string, std::string> controller_map_t;
typedef std::map<std::string, F32> default_controller_map_t;

#define MIN_REQUIRED_PIXEL_AREA_AVATAR_PHYSICS_MOTION 0.f
// We use TIME_ITERATION_STEP_MAX in division operation, make sure this is not
// an irrational value so that division won't end with repeated/recurring tail
// like 1.333(3)
#define TIME_ITERATION_STEP_MAX 0.05f

LL_INLINE F32 llsgn(F32 a)
{
	return a >= 0.f ? 1.f : -1.f;
}

/*
	At a high level, this works by setting temporary parameters that are not
	stored in the avatar's list of params, and are not conveyed to other users.
	We accomplish this by creating some new temporary driven params inside
	avatar_lad that are then driven by the actual params that the user sees and
	sets. For example, in the old system, the user sets a param called breast
	bouyancy, which controls the Z value of the breasts.
	In our new system, the user still sets the breast bouyancy, but that param
	is redefined as a driver param so that affects a new temporary driven param
	that the bounce is applied to.
*/

class LLPhysicsMotion
{
public:
	typedef enum
	{
		SMOOTHING = 0,
		MASS,
		GRAVITY,
		SPRING,
		GAIN,
		DAMPING,
		DRAG,
		MAX_EFFECT,
		NUM_PARAMS
	} eParamName;

	/*
		param_driver_name: The param that controls the params that are being
		affected by the physics.
		joint_name: The joint that the body part is attached to. The joint is
		used to determine the orientation (rotation) of the body part.

		character: The avatar that this physics affects.

		motion_direction_vec: The direction (in world coordinates) that
		determines the motion. For example, (0, 0, 1) is up-down, and means
		that up-down motion is what determines how this joint moves.

		controllers: The various settings (e.g. spring force, mass) that
		determine how the body part behaves.
	*/
	LLPhysicsMotion(const std::string& param_driver_name, U32 joint_key,
					LLCharacter* character,
					const LLVector3& motion_direction_vec,
					const controller_map_t& controllers)
	:	mParamDriverName(param_driver_name),
		mJointKey(joint_key),
		mMotionDirectionVec(motion_direction_vec),
		mParamDriver(NULL),
		mParamControllers(controllers),
		mCharacter(character),
		mLastTime(0),
		mPosition_local(0.f),
		mVelocityJoint_local(0.f),
		mPositionLastUpdate_local(0.f),
		mAccelerationJoint_local(0.f)
	{
		mJointState = new LLJointState;

		for (U32 i = 0; i < NUM_PARAMS; ++i)
		{
			mParamCache[i] = NULL;
		}
	}

	bool initialize();

	~LLPhysicsMotion() {}

	bool onUpdate(F32 time);

	LLPointer<LLJointState> getJointState()
	{
		return mJointState;
	}

protected:
	F32 getParamValue(eParamName param)
	{
		static std::string controller_key[] =
		{
			"Smoothing",
			"Mass",
			"Gravity",
			"Spring",
			"Gain",
			"Damping",
			"Drag",
			"MaxEffect"
		};

		if (!mParamCache[param])
		{
			const controller_map_t::const_iterator& entry =
				mParamControllers.find(controller_key[param]);
			if (entry == mParamControllers.end())
			{
				return sDefaultController[controller_key[param]];
			}
			const std::string& param_name = entry->second.c_str();
			mParamCache[param] = mCharacter->getVisualParam(param_name.c_str());
		}

		if (mParamCache[param])
		{
			return mParamCache[param]->getWeight();
		}
		else
		{
			return sDefaultController[controller_key[param]];
		}
	}

	void setParamValue(const LLViewerVisualParam* param,
					   F32 new_value_local, F32 behavior_maxeffect);

	F32 toLocal(const LLVector3& world);
	F32 calculateVelocity_local(F32 time_delta);
	F32 calculateAcceleration_local(F32 velocity_local, F32 time_delta);

private:
	const std::string mParamDriverName;
	const std::string mParamControllerName;
	const LLVector3 mMotionDirectionVec;

	U32 mJointKey;

	F32 mPosition_local;
	F32 mVelocityJoint_local;		// How fast the joint is moving
	F32 mAccelerationJoint_local;	// Acceleration on the joint

	F32 mVelocity_local;			// How fast the param is moving
	F32 mPositionLastUpdate_local;
	LLVector3 mPosition_world;

	LLViewerVisualParam *mParamDriver;
	const controller_map_t mParamControllers;

	LLPointer<LLJointState> mJointState;
	LLCharacter *mCharacter;

	F32 mLastTime;

	LLVisualParam* mParamCache[NUM_PARAMS];

	static default_controller_map_t sDefaultController;
};

default_controller_map_t initDefaultController()
{
	default_controller_map_t controller;
	controller["Mass"] = 0.2f;
	controller["Gravity"] = 0.f;
	controller["Damping"] = .05f;
	controller["Drag"] = 0.15f;
	controller["MaxEffect"] = 0.1f;
	controller["Spring"] = 0.1f;
	controller["Gain"] = 10.f;
	return controller;
}

default_controller_map_t LLPhysicsMotion::sDefaultController = initDefaultController();

bool LLPhysicsMotion::initialize()
{
	if (!mJointState->setJoint(mCharacter->getJoint(mJointKey)))
	{
		return false;
	}
	mJointState->setUsage(LLJointState::ROT);

	mParamDriver = (LLViewerVisualParam*)mCharacter->getVisualParam(mParamDriverName.c_str());
	if (mParamDriver == NULL)
	{
		llinfos << "Failure reading in  [ " << mParamDriverName << " ]"
				<< llendl;
		return false;
	}

	return true;
}

LLPhysicsMotionController::LLPhysicsMotionController(const LLUUID& id)
:	LLMotion(id),
	mCharacter(NULL)
{
	mName = "breast_motion";
}

LLPhysicsMotionController::~LLPhysicsMotionController()
{
	for (motion_vec_t::iterator iter = mMotions.begin();
		 iter != mMotions.end(); ++iter)
	{
		delete (*iter);
	}
}

LLMotion::LLMotionInitStatus LLPhysicsMotionController::onInitialize(LLCharacter* character)
{
	LL_FAST_TIMER(FTM_AVATAR_UPDATE);
	{
		LL_FAST_TIMER(FTM_PHYSICS_UPDATE);

		mCharacter = character;

		mMotions.clear();

		// Breast Cleavage
		{
			controller_map_t controller;
			controller["Mass"] = "Breast_Physics_Mass";
			controller["Gravity"] = "Breast_Physics_Gravity";
			controller["Drag"] = "Breast_Physics_Drag";
			controller["Damping"] = "Breast_Physics_InOut_Damping";
			controller["MaxEffect"] = "Breast_Physics_InOut_Max_Effect";
			controller["Spring"] = "Breast_Physics_InOut_Spring";
			controller["Gain"] = "Breast_Physics_InOut_Gain";
			LLPhysicsMotion* motion =
				new LLPhysicsMotion("Breast_Physics_InOut_Controller",
									LL_JOINT_KEY_CHEST, character,
									LLVector3::x_axis_neg, controller);
			if (!motion->initialize())
			{
				llassert_always(false);
				return STATUS_FAILURE;
			}
			addMotion(motion);
		}

		// Breast Bounce
		{
			controller_map_t controller;
			controller["Mass"] = "Breast_Physics_Mass";
			controller["Gravity"] = "Breast_Physics_Gravity";
			controller["Drag"] = "Breast_Physics_Drag";
			controller["Damping"] = "Breast_Physics_UpDown_Damping";
			controller["MaxEffect"] = "Breast_Physics_UpDown_Max_Effect";
			controller["Spring"] = "Breast_Physics_UpDown_Spring";
			controller["Gain"] = "Breast_Physics_UpDown_Gain";
			LLPhysicsMotion* motion =
				new LLPhysicsMotion("Breast_Physics_UpDown_Controller",
									LL_JOINT_KEY_CHEST, character,
									LLVector3::z_axis, controller);
			if (!motion->initialize())
			{
				llassert_always(false);
				return STATUS_FAILURE;
			}
			addMotion(motion);
		}

		// Breast Sway
		{
			controller_map_t controller;
			controller["Mass"] = "Breast_Physics_Mass";
			controller["Gravity"] = "Breast_Physics_Gravity";
			controller["Drag"] = "Breast_Physics_Drag";
			controller["Damping"] = "Breast_Physics_LeftRight_Damping";
			controller["MaxEffect"] = "Breast_Physics_LeftRight_Max_Effect";
			controller["Spring"] = "Breast_Physics_LeftRight_Spring";
			controller["Gain"] = "Breast_Physics_LeftRight_Gain";
			LLPhysicsMotion* motion =
				new LLPhysicsMotion("Breast_Physics_LeftRight_Controller",
									LL_JOINT_KEY_CHEST, character,
									LLVector3::y_axis_neg, controller);
			if (!motion->initialize())
			{
				llassert_always(false);
				return STATUS_FAILURE;
			}
			addMotion(motion);
		}

		// Butt Bounce
		{
			controller_map_t controller;
			controller["Mass"] = "Butt_Physics_Mass";
			controller["Gravity"] = "Butt_Physics_Gravity";
			controller["Drag"] = "Butt_Physics_Drag";
			controller["Damping"] = "Butt_Physics_UpDown_Damping";
			controller["MaxEffect"] = "Butt_Physics_UpDown_Max_Effect";
			controller["Spring"] = "Butt_Physics_UpDown_Spring";
			controller["Gain"] = "Butt_Physics_UpDown_Gain";
			LLPhysicsMotion* motion =
				new LLPhysicsMotion("Butt_Physics_UpDown_Controller",
									LL_JOINT_KEY_PELVIS, character,
									LLVector3::z_axis_neg, controller);
			if (!motion->initialize())
			{
				llassert_always(false);
				return STATUS_FAILURE;
			}
			addMotion(motion);
		}

		// Butt LeftRight
		{
			controller_map_t controller;
			controller["Mass"] = "Butt_Physics_Mass";
			controller["Gravity"] = "Butt_Physics_Gravity";
			controller["Drag"] = "Butt_Physics_Drag";
			controller["Damping"] = "Butt_Physics_LeftRight_Damping";
			controller["MaxEffect"] = "Butt_Physics_LeftRight_Max_Effect";
			controller["Spring"] = "Butt_Physics_LeftRight_Spring";
			controller["Gain"] = "Butt_Physics_LeftRight_Gain";
			LLPhysicsMotion* motion =
				new LLPhysicsMotion("Butt_Physics_LeftRight_Controller",
									LL_JOINT_KEY_PELVIS, character,
									LLVector3::y_axis_neg, controller);
			if (!motion->initialize())
			{
				llassert_always(false);
				return STATUS_FAILURE;
			}
			addMotion(motion);
		}

		// Belly Bounce
		{
			controller_map_t controller;
			controller["Mass"] = "Belly_Physics_Mass";
			controller["Gravity"] = "Belly_Physics_Gravity";
			controller["Drag"] = "Belly_Physics_Drag";
			controller["Damping"] = "Belly_Physics_UpDown_Damping";
			controller["MaxEffect"] = "Belly_Physics_UpDown_Max_Effect";
			controller["Spring"] = "Belly_Physics_UpDown_Spring";
			controller["Gain"] = "Belly_Physics_UpDown_Gain";
			LLPhysicsMotion* motion =
				new LLPhysicsMotion("Belly_Physics_UpDown_Controller",
									LL_JOINT_KEY_PELVIS, character,
									LLVector3::z_axis_neg, controller);
			if (!motion->initialize())
			{
				llassert_always(false);
				return STATUS_FAILURE;
			}
			addMotion(motion);
		}
	}

	return STATUS_SUCCESS;
}

void LLPhysicsMotionController::addMotion(LLPhysicsMotion* motion)
{
	addJointState(motion->getJointState());
	mMotions.push_back(motion);
}

F32 LLPhysicsMotionController::getMinPixelArea()
{
	return MIN_REQUIRED_PIXEL_AREA_AVATAR_PHYSICS_MOTION;
}

// Local space means "parameter space".
F32 LLPhysicsMotion::toLocal(const LLVector3& world)
{
	LLJoint* joint = mJointState->getJoint();
	const LLQuaternion rotation_world = joint->getWorldRotation();

	LLVector3 dir_world = mMotionDirectionVec * rotation_world;
	dir_world.normalize();
	return world * dir_world;
}

F32 LLPhysicsMotion::calculateVelocity_local(F32 time_delta)
{
	if (time_delta <= 0.f) return 0.f;

	constexpr F32 world_to_model_scale = 100.f;
	LLJoint* joint = mJointState->getJoint();
	const LLVector3 position_world = joint->getWorldPosition();
	const LLVector3 last_position_world = mPosition_world;
	const LLVector3 positionchange_world = (position_world - last_position_world) *
										   world_to_model_scale;
	const F32 velocity_local = toLocal(positionchange_world) / time_delta;
	return velocity_local;
}

F32 LLPhysicsMotion::calculateAcceleration_local(F32 velocity_local,
												 F32 time_delta)
{
	if (time_delta <= 0.f) return 0.f;

#if 0	// Removed smoothing param since it is probably not necessary:
	const F32 smoothing = getParamValue("Smoothing");
#endif
	constexpr F32 smoothing = 3.f;
	constexpr F32 factor = (smoothing - 1.f) / smoothing;
	F32 accel_loc = (velocity_local - mVelocityJoint_local) / time_delta;
	F32 smoothed_accel = accel_loc / smoothing +
						 mAccelerationJoint_local * factor;

	return smoothed_accel;
}

bool LLPhysicsMotionController::onUpdate(F32 time, U8* joint_mask)
{
	LL_FAST_TIMER(FTM_AVATAR_UPDATE);

	// Skip if disabled globally.
	if (!LLVOAvatar::sAvatarPhysics)
	{
		return true;
	}

	{
		LL_FAST_TIMER(FTM_PHYSICS_UPDATE);

		bool update_visuals = false;
		for (motion_vec_t::iterator iter = mMotions.begin(),
									end = mMotions.end();
			 iter != end; ++iter)
		{
			LLPhysicsMotion* motion = *iter;
			if (motion)	// Paranoia
			{
				update_visuals |= motion->onUpdate(time);
			}
		}

		if (update_visuals)
		{
			mCharacter->updateVisualParams();
		}
	}

	return true;
}

// Returns true if character has to update visual params.
bool LLPhysicsMotion::onUpdate(F32 time)
{
	if (!mParamDriver)
	{
		return false;
	}

	if (!mLastTime || mLastTime >= time)
	{
		mLastTime = time;
		return false;
	}

	///////////////////////////////////////////////////////////////////////////
	// Get all parameters and settings

	const F32 time_delta = time - mLastTime;
#if 0
	// Do not update too frequently, to avoid precision errors from small time
	// slices.
	if (time_delta <= .01f)
	{
		return false;
	}
#endif
	// If less than 1FPS, we don't want to be spending time updating physics
	// at all.
	if (time_delta > 1.f)
	{
		mLastTime = time;
		return false;
	}

	// Higher LOD is better. This controls the granularity and frequency of
	// updates for the motions.
	const F32 lod_factor = LLVOAvatar::sPhysicsLODFactor;
	if (lod_factor == 0.f)
	{
		return true;
	}

	LLJoint* joint = mJointState->getJoint();

	const F32 behavior_mass = getParamValue(MASS);
	const F32 behavior_gravity = getParamValue(GRAVITY);
	const F32 behavior_spring = getParamValue(SPRING);
	const F32 behavior_gain = getParamValue(GAIN);
	const F32 behavior_damping = getParamValue(DAMPING);
	const F32 behavior_drag = getParamValue(DRAG);
	F32 behavior_maxeffect = getParamValue(MAX_EFFECT);

	// Normalize the param position to be from [0,1].
	// We have to use normalized values because there may be more than one
	// driven param, and each of these driven params may have its own range.
	// This means we'll do all our calculations in normalized [0,1] local
	// coordinates.
	const F32 position_user_local = (mParamDriver->getWeight() -
									 mParamDriver->getMinWeight()) /
									(mParamDriver->getMaxWeight() -
									 mParamDriver->getMinWeight());

	// End parameters and settings
	///////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////
	// Calculate velocity and acceleration in parameter space.

	const F32 joint_local_factor = 30.f;
	const F32 velocity_joint_local =
		calculateVelocity_local(time_delta * joint_local_factor);
	const F32 acceleration_joint_local =
		calculateAcceleration_local(velocity_joint_local,
									time_delta * joint_local_factor);

	// End velocity and acceleration
	///////////////////////////////////////////////////////////////////////////

	bool update_visuals = false;

	// Break up the physics into a bunch of iterations so that differing
	// framerates will show roughly the same behavior.
	U32 steps = (U32)(time_delta / TIME_ITERATION_STEP_MAX) + 1;
	// Note: minimal time_iteration_step ends up as 0.025
	F32 time_iteration_step = time_delta / (F32)steps;
	for (U32 i = 0; i < steps; ++i)
	{
		// mPositon_local should be in normalized 0,1 range already. Just
		// making sure...
		const F32 position_current_local = llclamp(mPosition_local, 0.f, 1.f);
		// If the effect is turned off then don't process unless we need one
		// more update to set the position to the default (i.e. user) position.
		if (behavior_maxeffect == 0 &&
			position_current_local == position_user_local)
		{
			return update_visuals;
		}

		///////////////////////////////////////////////////////////////////////
		// Calculate the total force

		// Spring force is a restoring force towards the original user-set
		// breast position. F = kx
		const F32 spring_length = position_current_local - position_user_local;
		const F32 force_spring = -spring_length * behavior_spring;

		// Acceleration is the force that comes from the change in velocity of
		// the torso. F = ma
		const F32 force_accel = behavior_gain * (acceleration_joint_local *
												 behavior_mass);

		// Gravity always points downward in world space. F = mg
		const LLVector3 gravity_world(0.f, 0.f, 1.f);
		const F32 force_gravity = toLocal(gravity_world) *
								   behavior_gravity * behavior_mass;

		// Damping is a restoring force that opposes the current velocity.
		// F = -kv
		const F32 force_damping = -behavior_damping * mVelocity_local;

		// Drag is a force imparted by velocity (intuitively it is similar to
		// wind resistance). F = .5kv^2
		const F32 force_drag = .5f * behavior_drag * velocity_joint_local *
							   velocity_joint_local *
							   llsgn(velocity_joint_local);

		const F32 force_net = force_accel + force_gravity + force_spring +
							  force_damping + force_drag;

		// End total force
		///////////////////////////////////////////////////////////////////////

		///////////////////////////////////////////////////////////////////////
		// Calculate new params

		// Calculate the new acceleration based on the net force. a = F/m

		const F32 acceleration_new_local = force_net / behavior_mass;
		// magic number, used to be customizable:
		constexpr F32 max_velocity = 100.f;
		F32 velocity_new_local = mVelocity_local +
								 acceleration_new_local * time_iteration_step;
		velocity_new_local = llclamp(velocity_new_local, -max_velocity,
									 max_velocity);

		// Calculate the new parameters, or remain unchanged if max speed is 0.
		F32 new_pos_local = position_current_local +
							velocity_new_local * time_iteration_step;
		if (behavior_maxeffect == 0)
		{
			new_pos_local = position_user_local;
		}

		// Zero out the velocity if the param is being pushed beyond its limits
		if ((new_pos_local < 0.f && velocity_new_local < 0.f) ||
		    (new_pos_local > 1.f && velocity_new_local > 0.f))
		{
			velocity_new_local = 0.f;
		}

		// Check for NaN values. A NaN value is detected if the variables does
		// not equal itself. If NaN, then reset everything.
		if (mPosition_local != mPosition_local ||
			mVelocity_local != mVelocity_local ||
		    new_pos_local != new_pos_local)
		{
			new_pos_local = mPosition_local = 0.f;
			mVelocity_local = mVelocityJoint_local = 0.f;
			mAccelerationJoint_local = 0.f;
			mPosition_world.clear();
		}

		const F32 new_pos_local_clamped = llclamp(new_pos_local, 0.f, 1.f);

		LLDriverParam* driver_param = mParamDriver->asDriverParam();
		if (!driver_param)
		{
			llerrs << "Not a driver param !" << llendl;
		}

		// If this is one of our "hidden" driver params, then make sure it is
		// the default value.
		if (driver_param->getGroup() != VISUAL_PARAM_GROUP_TWEAKABLE &&
		    driver_param->getGroup() != VISUAL_PARAM_GROUP_TWEAKABLE_NO_TRANSMIT)
		{
			mCharacter->setVisualParamWeight(driver_param, 0, false);
		}
		S32 num_driven = driver_param->getDrivenParamsCount();
		for (S32 i = 0; i < num_driven; ++i)
		{
			const LLViewerVisualParam* driven_param =
				driver_param->getDrivenParam(i);
			setParamValue(driven_param, new_pos_local_clamped,
						  behavior_maxeffect);
		}

		// End calculate new params
		///////////////////////////////////////////////////////////////////////

		///////////////////////////////////////////////////////////////////////
		// Conditionally update the visual params

		// Updating the visual params (i.e. what the user sees) is fairly
		// expensive, so only update if the params have changed enough, and
		// also take into account the graphics LOD settings.

		// For non-self, if the avatar is small enough visually, then don't update.
		constexpr F32 area_for_max_settings = 0.f;
		constexpr F32 area_for_min_settings = 1400.f;
		const F32 area_for_this_setting = area_for_max_settings +
										  (area_for_min_settings -
										  area_for_max_settings) *
										  (1.f - lod_factor);
		const F32 pixel_area = sqrtf(mCharacter->getPixelArea());

		// Note: the following static cast is only valid because the sole child
		// class of LLCharacter is LLAvatarAppearance which itself got for sole
		// child class LLVOAvatar. Should this change in the future, this cast
		// would become illegal.
		LLVOAvatar* avatarp = (LLVOAvatar*)((LLAvatarAppearance*)mCharacter);
		bool is_self = avatarp && avatarp->isSelf();
		if ((pixel_area > area_for_this_setting) || is_self)
		{
			const F32 position_diff_local = fabsf(mPositionLastUpdate_local -
												  new_pos_local_clamped);
			const F32 min_delta = (1.0001f - lod_factor) * 0.4f;
			if (fabsf(position_diff_local) > min_delta)
			{
				update_visuals = true;
				mPositionLastUpdate_local = new_pos_local;
			}
		}

		// End update visual params
		///////////////////////////////////////////////////////////////////////

		mVelocity_local = velocity_new_local;
		mAccelerationJoint_local = acceleration_joint_local;
		mPosition_local = new_pos_local;
	}
	mLastTime = time;
	mPosition_world = joint->getWorldPosition();
	mVelocityJoint_local = velocity_joint_local;

	return update_visuals;
}

// Range of new_value_local is assumed to be [0 , 1] normalized.
void LLPhysicsMotion::setParamValue(const LLViewerVisualParam* param,
									F32 new_value_normalized,
									F32 behavior_maxeffect)
{
	const F32 value_min_local = param->getMinWeight();
	const F32 value_max_local = param->getMaxWeight();
	const F32 min_val = (1.f - behavior_maxeffect) * 0.5f;
	const F32 max_val = (1.f + behavior_maxeffect) * 0.5f;

	// Scale from [0,1] to [min_val,max_val]
	const F32 new_value_rescaled = min_val +
								   (max_val - min_val) * new_value_normalized;

	// Scale from [0,1] to [value_min_local,value_max_local]
	const F32 new_value_local = value_min_local +
								(value_max_local - value_min_local) *
								 new_value_rescaled;

	mCharacter->setVisualParamWeight(param, new_value_local, false);
}
