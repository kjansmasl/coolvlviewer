/**
 * @file llmotioncontroller.cpp
 * @brief Implementation of LLMotionController class.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include <algorithm>

#include "llmotioncontroller.h"

#include "llanimationstates.h"
#include "llfasttimer.h"
#include "llframetimer.h"
#include "llkeyframemotion.h"
#include "llmath.h"
#include "llstl.h"
#include "lltimer.h"

// This is why LL_CHARACTER_MAX_ANIMATED_JOINTS needs to be a multiple of 4:
constexpr S32 NUM_JOINT_SIGNATURE_STRIDES = LL_CHARACTER_MAX_ANIMATED_JOINTS / 4;
constexpr U32 MAX_MOTION_INSTANCES = 32;

uuid_vec_t LLMotionController::sMotionsToKill;
F32 LLMotionController::sTimeFactorMultiplier = 1.f;
LLMotionRegistry LLMotionController::sRegistry;

//-----------------------------------------------------------------------------
// LLMotionRegistry class
//-----------------------------------------------------------------------------

LLMotionRegistry::~LLMotionRegistry()
{
	mMotionTable.clear();
}

bool LLMotionRegistry::registerMotion(const LLUUID& id,
									  LLMotionConstructor constructor)
{
	if (mMotionTable.count(id))
	{
		return false;
	}
	mMotionTable.emplace(id, constructor);
	return true;
}

void LLMotionRegistry::markBad(const LLUUID& id)
{
	mMotionTable[id] = LLMotionConstructor(NULL);
}

LLMotion* LLMotionRegistry::createMotion(const LLUUID& id)
{
	LLMotionConstructor constructor = get_if_there(mMotionTable, id,
												   LLMotionConstructor(NULL));
	if (constructor)
	{
		return constructor(id);
	}

	// *FIX: need to replace with a better default scheme. RN
	return LLKeyframeMotion::create(id);
}

//-----------------------------------------------------------------------------
// LLMotionController class
//-----------------------------------------------------------------------------

//static
void LLMotionController::initClass()
{
	// Let's avoid memory fragmentation over time...
	sMotionsToKill.reserve(MAX_MOTION_INSTANCES * 2);
}

//static
void LLMotionController::dumpStats()
{
	llinfos << "sMotionsToKill capacity reached: " << sMotionsToKill.capacity()
			<< llendl;
}

LLMotionController::LLMotionController()
:	mTimeFactor(1.f),
	mTimeFactorMultiplier(sTimeFactorMultiplier),
	mCharacter(NULL),
	mAnimTime(0.f),
	mPrevTimerElapsed(0.f),
	mLastTime(0.0f),
	mHasRunOnce(false),
	mPaused(false),
	mPausedFrame(0),
	mTimeStep(0.f),
	mTimeStepCount(0),
	mLastInterp(0.f)
{
}

LLMotionController::~LLMotionController()
{
	deleteAllMotions();
}

void LLMotionController::incMotionCounts(S32& num_motions,
										 S32& num_loading_motions,
										 S32& num_loaded_motions,
										 S32& num_active_motions,
										 S32& num_deprecated_motions)
{
	num_motions += mAllMotions.size();
	num_loading_motions += mLoadingMotions.size();
	num_loaded_motions += mLoadedMotions.size();
	num_active_motions += mActiveMotions.size();
	num_deprecated_motions += mDeprecatedMotions.size();
}

void LLMotionController::deleteAllMotions()
{
	mLoadingMotions.clear();
	mLoadedMotions.clear();
	mActiveMotions.clear();

	for (motion_map_t::iterator it = mAllMotions.begin(),
								end = mAllMotions.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	mAllMotions.clear();

	for (motion_set_t::iterator it = mDeprecatedMotions.begin(),
								end = mDeprecatedMotions.end();
		 it != end; ++it)
	{
		delete *it;
	}
	mDeprecatedMotions.clear();
}

void LLMotionController::purgeExcessMotions()
{
	if (mLoadedMotions.size() > MAX_MOTION_INSTANCES)
	{
		// Clean up deprecated motions
		for (motion_set_t::iterator iter = mDeprecatedMotions.begin();
			 iter != mDeprecatedMotions.end(); )
		{
			motion_set_t::iterator cur_iter = iter++;
			LLMotion* cur_motionp = *cur_iter;
			if (!isMotionActive(cur_motionp))
			{
				// Motion is deprecated so we know it is not cannonical, we can
				// safely remove the instance
				removeMotionInstance(cur_motionp);
				mDeprecatedMotions.erase(cur_iter);
			}
		}
	}

	S32 excess_motions = mLoadedMotions.size() - MAX_MOTION_INSTANCES;
	if (excess_motions > 0)
	{
		// Too many motions active this frame, kill all blenders
		mPoseBlender.clearBlenders();

		for (motion_set_t::iterator iter = mLoadedMotions.begin(),
									end = mLoadedMotions.end();
			 iter != end; ++iter)
		{
			LLMotion* motionp = *iter;
			// Motion is not playing, mark for delete
			if (motionp && !isMotionActive(motionp))
			{
				sMotionsToKill.emplace_back(motionp->getID());
			}
		}
	}

	// Clean up all inactive, loaded motions
	excess_motions = sMotionsToKill.size();
	if (excess_motions > 0)
	{
		for (S32 i = 0; i < excess_motions; ++i)
		{
			// Look up the motion again by ID to get canonical instance and
			// kill it only if that one is inactive
			const LLUUID& motion_id = sMotionsToKill[i];
			LLMotion* motionp = findMotion(motion_id);
			if (motionp && !isMotionActive(motionp))
			{
				removeMotion(motion_id);
			}
		}
		sMotionsToKill.clear();
	}

	if (mLoadedMotions.size() > 2 * MAX_MOTION_INSTANCES)
	{
		LL_DEBUGS("Motion") << "> " << 2 * MAX_MOTION_INSTANCES
							<< " Loaded Motions" << LL_ENDL;
	}
}

void LLMotionController::deactivateStoppedMotions()
{
	// Since we are hidden, deactivate any stopped motions.
	for (motion_list_t::iterator iter = mActiveMotions.begin();
		 iter != mActiveMotions.end(); )
	{
		LLMotion* motionp = *iter++;
		if (motionp && motionp->isStopped())
		{
			deactivateMotionInstance(motionp);
		}
	}
}

void LLMotionController::setTimeStep(F32 step)
{
	mTimeStep = step;

	if (step != 0.f)
	{
		// Make sure timestamps conform to new quantum
		for (motion_list_t::iterator iter = mActiveMotions.begin(),
									 end = mActiveMotions.end();
			 iter != end; ++iter)
		{
			LLMotion* motionp = *iter;
			if (!motionp) continue;

			F32 activation_time = motionp->mActivationTimestamp;
			motionp->mActivationTimestamp =
				(F32)(llfloor(activation_time / step)) * step;
			bool stopped = motionp->isStopped();
			motionp->setStopTime((F32)(llfloor(motionp->getStopTime() /
											   step)) * step);
			motionp->setStopped(stopped);
			motionp->mSendStopTimestamp =
				(F32)llfloor(motionp->mSendStopTimestamp / step) * step;
		}
	}
}

bool LLMotionController::registerMotion(const LLUUID& id,
										LLMotionConstructor constructor)
{
	return sRegistry.registerMotion(id, constructor);
}

void LLMotionController::removeMotion(const LLUUID& id)
{
	mAllMotions.erase(id);
	removeMotionInstance(findMotion(id));
}

// Removes instance of a motion from all runtime structures, but does not erase
// entry by Id as this could be a duplicate instance; use removeMotion(id) to
// remove all references to a given motion by Id.
void LLMotionController::removeMotionInstance(LLMotion* motionp)
{
	if (motionp)
	{
		llassert(findMotion(motionp->getID()) != motionp);
		if (motionp->isActive())
		{
			motionp->deactivate();
		}
		mLoadingMotions.erase(motionp);
		mLoadedMotions.erase(motionp);
		mActiveMotions.remove(motionp);
		delete motionp;
	}
}

LLMotion* LLMotionController::createMotion(const LLUUID& id)
{
	if (id.isNull())
	{
		// Happens for hand animations (Bento mesh avatar with hand joints and
		// no hands anim defined ?). Just abort.
		return NULL;
	}

	// Do we have an instance of this motion for this character ?
	LLMotion* motion = findMotion(id);

	// If not, we need to create one
	if (!motion)
	{
		// Look up constructor and create it
		motion = sRegistry.createMotion(id);
		if (!motion)
		{
			return NULL;
		}

		// Look up name for default motions
		const char* motion_name = gAnimLibrary.animStateToString(id);
		if (motion_name)
		{
			motion->setName(motion_name);
		}

		// Initialize the new instance
		LLMotion::LLMotionInitStatus stat = motion->onInitialize(mCharacter);
		switch (stat)
		{
			case LLMotion::STATUS_FAILURE:
				llinfos << "Motion " << id << " init failed." << llendl;
				sRegistry.markBad(id);
				delete motion;
				return NULL;

			case LLMotion::STATUS_HOLD:
				mLoadingMotions.insert(motion);
				break;

			case LLMotion::STATUS_SUCCESS:
		    	// Add motion to our list
		    	mLoadedMotions.insert(motion);
				break;

			default:
				llerrs << "Invalid initialization status" << llendl;
		}

		mAllMotions.emplace(id, motion);
	}
	return motion;
}

bool LLMotionController::startMotion(const LLUUID& id, F32 start_offset)
{
	// Do we have an instance of this motion for this character ?
	LLMotion* motion = findMotion(id);

	// Motion that is stopping will be allowed to stop but replaced by a new
	// instance of that motion
	if (motion && !mPaused && motion->canDeprecate() &&
		// Not LOD-ed out:
		motion->getFadeWeight() > 0.01f &&
		(motion->isBlending() || motion->getStopTime() != 0.f))
	{
		deprecateMotionInstance(motion);
		// Force creation of new instance
		motion = NULL;
	}

	// Create new motion instance
	if (!motion)
	{
		motion = createMotion(id);
	}

	if (!motion)
	{
		return false;
	}
	if (motion->canDeprecate() && isMotionActive(motion))
	{
		// If the motion is already active and allows deprecation, then let it
		// keep/ playing
		return true;
	}

	return activateMotionInstance(motion, mAnimTime - start_offset);
}

// If motion is already inactive, returns false
bool LLMotionController::stopMotionLocally(const LLUUID& id, bool stop_now)
{
	LLMotion* motion = findMotion(id);
	return stopMotionInstance(motion, stop_now || mPaused);
}

bool LLMotionController::stopMotionInstance(LLMotion* motion, bool stop_now)
{
	if (!motion)
	{
		return false;
	}

	// If on active list, stop it
	if (isMotionActive(motion) && !motion->isStopped())
	{
		motion->setStopTime(mAnimTime);
		if (stop_now)
		{
			deactivateMotionInstance(motion);
		}
		return true;
	}

	if (isMotionLoading(motion))
	{
		motion->setStopped(true);
		return true;
	}

	return false;
}

void LLMotionController::updateRegularMotions()
{
	updateMotionsByType(LLMotion::NORMAL_BLEND);
}

void LLMotionController::updateAdditiveMotions()
{
	updateMotionsByType(LLMotion::ADDITIVE_BLEND);
}

void LLMotionController::resetJointSignatures()
{
	const size_t bytes = sizeof(U8) * LL_CHARACTER_MAX_ANIMATED_JOINTS;
	memset(&mJointSignature[0][0], 0, bytes);
	memset(&mJointSignature[1][0], 0, bytes);
}

// minimal updates for active motions
void LLMotionController::updateIdleMotion(LLMotion* motionp)
{
	if (!motionp)
	{
		return;
	}

	if (motionp->isStopped() &&
		mAnimTime > motionp->getStopTime() + motionp->getEaseOutDuration())
	{
		deactivateMotionInstance(motionp);
	}
	else if (motionp->isStopped() && mAnimTime > motionp->getStopTime())
	{
		// is this the first iteration in the ease out phase?
		if (mLastTime <= motionp->getStopTime())
		{
			// store residual weight for this motion
			motionp->mResidualWeight = motionp->getPose()->getWeight();
		}
	}
	else if (mAnimTime > motionp->mSendStopTimestamp)
	{
		// Notify character of timed stop event on first iteration past
		// sendstoptimestamp. This will only be called when an animation stops
		// itself (runs out of time)
		if (mLastTime <= motionp->mSendStopTimestamp)
		{
			mCharacter->requestStopMotion(motionp);
			stopMotionInstance(motionp, false);
		}
	}
	else if (mAnimTime >= motionp->mActivationTimestamp)
	{
		if (mLastTime < motionp->mActivationTimestamp)
		{
			motionp->mResidualWeight = motionp->getPose()->getWeight();
		}
	}
}

// Call this instead of updateMotionsByType for hidden avatars
void LLMotionController::updateIdleActiveMotions()
{
	for (motion_list_t::iterator iter = mActiveMotions.begin();
		 iter != mActiveMotions.end(); )
	{
		LLMotion* motionp = *iter++;
		if (motionp)
		{
			updateIdleMotion(motionp);
		}
	}
}

void LLMotionController::updateMotionsByType(LLMotion::LLMotionBlendType anim_type)
{
	bool update_result = true;
	U8 last_joint_signature[LL_CHARACTER_MAX_ANIMATED_JOINTS];

	memset(&last_joint_signature, 0,
		   sizeof(U8) * LL_CHARACTER_MAX_ANIMATED_JOINTS);

	// Iterate through active motions in chronological order
	for (motion_list_t::iterator iter = mActiveMotions.begin();
		 iter != mActiveMotions.end(); )
	{
		LLMotion* motionp = *iter++;
		if (!motionp || motionp->getBlendType() != anim_type)
		{
			continue;
		}

		if (!motionp->needsUpdate())
		{
			// As far as the motion knows: it does not need an update but we
			// still update it if its "joint signature" causes a change to the
			// accumulated signature stored in 2D arraymJointSignature[][]
			// (whatever that means).
			bool update_motion = false;
			for (S32 i = 0; i < NUM_JOINT_SIGNATURE_STRIDES; ++i)
			{
				// First indice is 0 for positions.
		 		U32* current_signature = (U32*)&(mJointSignature[0][i * 4]);
				U32 test_signature =
					*(U32*)&(motionp->mJointSignature[0][i * 4]);
				if ((*current_signature | test_signature) > *current_signature)
				{
					*current_signature |= test_signature;
					update_motion = true;
				}

				// First indice is 1 for rotations.
				*((U32*)&last_joint_signature[i * 4]) =
					*(U32*)&(mJointSignature[1][i * 4]);
				current_signature = (U32*)&(mJointSignature[1][i * 4]);
				test_signature = *(U32*)&(motionp->mJointSignature[1][i * 4]);
				if ((*current_signature | test_signature) > *current_signature)
				{
					*current_signature |= test_signature;
					update_motion = true;
				}
			}
			if (!update_motion)
			{
				updateIdleMotion(motionp);
				continue;
			}
		}

		LLPose* posep = motionp->getPose();
		if (!posep)
		{
			llwarns_sparse << "NULL pose !" << llendl;
			continue;
		}

		// Only filter by LOD after running every animation at least once (to
		// prime the avatar state)
		if (mHasRunOnce &&
			motionp->getMinPixelArea() > mCharacter->getPixelArea())
		{
			motionp->fadeOut();

			// Should we notify the simulator that this motion should be
			// stopped (check even if skipped by LOD logic) ?
			if (mAnimTime > motionp->mSendStopTimestamp)
			{
				// Notify character of timed stop event on first iteration past
				// sendstoptimestamp. This will only be called when an
				// animation stops itself (runs out of time)
				if (mLastTime <= motionp->mSendStopTimestamp)
				{
					mCharacter->requestStopMotion(motionp);
					stopMotionInstance(motionp, false);
				}
			}

			if (motionp->getFadeWeight() < 0.01f)
			{
				if (motionp->isStopped() &&
					mAnimTime > motionp->getStopTime() + motionp->getEaseOutDuration())
				{
					posep->setWeight(0.f);
					deactivateMotionInstance(motionp);
				}
				continue;
			}
		}
		else
		{
			motionp->fadeIn();
		}

		//**********************
		// MOTION INACTIVE
		//**********************
		if (motionp->isStopped() &&
			mAnimTime > motionp->getStopTime() + motionp->getEaseOutDuration())
		{
			// This motion has gone on too long, deactivate it did we have a
			// chance to stop it ?
			if (mLastTime <= motionp->getStopTime())
			{
				// If not, let's stop it this time through and deactivate it the next

				posep->setWeight(motionp->getFadeWeight());
				motionp->onUpdate(motionp->getStopTime() - motionp->mActivationTimestamp,
								  last_joint_signature);
			}
			else
			{
				posep->setWeight(0.f);
				deactivateMotionInstance(motionp);
				continue;
			}
		}
		//**********************
		// MOTION EASE OUT
		//**********************
		else if (motionp->isStopped() && mAnimTime > motionp->getStopTime())
		{
			// Is this the first iteration in the ease out phase?
			if (mLastTime <= motionp->getStopTime())
			{
				// store residual weight for this motion
				motionp->mResidualWeight = motionp->getPose()->getWeight();
			}

			if (motionp->getEaseOutDuration() == 0.f)
			{
				posep->setWeight(0.f);
			}
			else
			{
				posep->setWeight(motionp->getFadeWeight() *
								 motionp->mResidualWeight *
								 cubic_step(1.f -
											(mAnimTime -
											 motionp->getStopTime()) /
											motionp->getEaseOutDuration()));
			}

			// Perform motion update
			{
				LL_FAST_TIMER(FTM_MOTION_ON_UPDATE);
				update_result = motionp->onUpdate(mAnimTime - motionp->mActivationTimestamp,
												  last_joint_signature);
			}
		}
		//**********************
		// MOTION ACTIVE
		//**********************
		else if (mAnimTime >
					motionp->mActivationTimestamp + motionp->getEaseInDuration())
		{
			posep->setWeight(motionp->getFadeWeight());

			// Should we notify the simulator that this motion should be
			// stopped ?
			if (mAnimTime > motionp->mSendStopTimestamp)
			{
				// Notify character of timed stop event on first iteration past
				// sendstoptimestamp. This will only be called when an
				// animation stops itself (runs out of time)
				if (mLastTime <= motionp->mSendStopTimestamp)
				{
					mCharacter->requestStopMotion(motionp);
					stopMotionInstance(motionp, false);
				}
			}

			// perform motion update
			update_result =
				motionp->onUpdate(mAnimTime - motionp->mActivationTimestamp,
								  last_joint_signature);
		}
		//**********************
		// MOTION EASE IN
		//**********************
		else if (mAnimTime >= motionp->mActivationTimestamp)
		{
			if (mLastTime < motionp->mActivationTimestamp)
			{
				motionp->mResidualWeight = motionp->getPose()->getWeight();
			}
			if (motionp->getEaseInDuration() == 0.f)
			{
				posep->setWeight(motionp->getFadeWeight());
			}
			else
			{
				// Perform motion update
				posep->setWeight(motionp->getFadeWeight() *
								 motionp->mResidualWeight +
								 (1.f - motionp->mResidualWeight) *
								 cubic_step((mAnimTime -
											 motionp->mActivationTimestamp) /
											motionp->getEaseInDuration()));
			}
			// perform motion update
			update_result = motionp->onUpdate(mAnimTime -
											  motionp->mActivationTimestamp,
											  last_joint_signature);
		}
		else
		{
			posep->setWeight(0.f);
			update_result = motionp->onUpdate(0.f, last_joint_signature);
		}

		// Allow motions to deactivate themselves
		if (!update_result &&
			(!motionp->isStopped() || motionp->getStopTime() > mAnimTime))
		{
			// Animation has stopped itself due to internal logic propagate
			// this to the network as not all viewers are guaranteed to have
			// access to the same logic.
			mCharacter->requestStopMotion(motionp);
			stopMotionInstance(motionp, false);
		}

		// Even if onupdate returns false, add this motion in to the blend one
		// last time
		mPoseBlender.addMotion(motionp);
	}
}

void LLMotionController::updateLoadingMotions()
{
	// Query pending motions for completion
	for (motion_set_t::iterator iter = mLoadingMotions.begin();
		 iter != mLoadingMotions.end(); )
	{
		LLMotion* motionp = *iter;
		if (!motionp)	// Maybe it should not happen but I have seen it - MG
		{
			llwarns << "NULL motion loading found. Removing from list."
					<< llendl;
			iter = mLoadingMotions.erase(iter);
			continue;
		}

		LLMotion::LLMotionInitStatus s = motionp->onInitialize(mCharacter);
		if (s == LLMotion::STATUS_SUCCESS)
		{
			// Add motion to our loaded motion list
			mLoadedMotions.insert(motionp);
			// Erase from loading motion list
			iter = mLoadingMotions.erase(iter);
			// This motion should be playing
			if (!motionp->isStopped())
			{
				activateMotionInstance(motionp, mAnimTime);
			}
		}
		else if (s == LLMotion::STATUS_FAILURE)
		{
			llwarns << "Motion " << motionp->getID() << " init failed."
					<< llendl;
			sRegistry.markBad(motionp->getID());
			iter = mLoadingMotions.erase(iter);
			motion_set_t::iterator found_it = mDeprecatedMotions.find(motionp);
			if (found_it != mDeprecatedMotions.end())
			{
				mDeprecatedMotions.erase(found_it);
			}
			mAllMotions.erase(motionp->getID());
			delete motionp;
		}
		else	// STATUS_HOLD
		{
			++iter;
		}
	}
}

void LLMotionController::updateMotions(bool force_update)
{
	bool use_quantum = mTimeStep != 0.f;

	// Always update mPrevTimerElapsed
	F32 cur_time = mTimer.getElapsedTimeF32();
	F32 delta_time = cur_time - mPrevTimerElapsed;
	if (delta_time < 0.f)
	{
		llwarns_sparse << "Negative time passed; zeroed." << llendl;
		delta_time = 0.f;
	}
	mPrevTimerElapsed = cur_time;
	mLastTime = mAnimTime;

	// Always cap the number of loaded motions
	purgeExcessMotions();

	// Update timing info for this time step.
	if (!mPaused)
	{
		F32 update_time = mAnimTime +
						  delta_time * mTimeFactor * mTimeFactorMultiplier;
		if (use_quantum)
		{
			F32 time_interval = fmodf(update_time, mTimeStep);

			// always animate *ahead* of actual time
#if 0
			S32 quantum_count = llmax(0,
									  llfloor((update_time - time_interval) /
											  mTimeStep)) + 1;
#else
			S32 quantum_count = llmax(ll_roundp(update_time / mTimeStep),
									  llceil(mAnimTime / mTimeStep));
#endif
			if (quantum_count == mTimeStepCount)
			{
				// We are still in same time quantum as before, so just
				// interpolate and exit
				if (!mPaused)
				{
					F32 interp = time_interval / mTimeStep;
					mPoseBlender.interpolate(interp - mLastInterp);
					mLastInterp = interp;
				}

				updateLoadingMotions();
				return;
			}

			// Is calculating a new keyframe pose, make sure the last one gets
			// applied
			mPoseBlender.interpolate(1.f);
			mPoseBlender.clearBlenders();

			mTimeStepCount = quantum_count;
			mAnimTime = (F32)quantum_count * mTimeStep;
			mLastInterp = 0.f;
		}
		else
		{
			mAnimTime = update_time;
		}
	}

	updateLoadingMotions();

	resetJointSignatures();

	if (mPaused && !force_update)
	{
		updateIdleActiveMotions();
	}
	else
	{
		// Update additive motions
		updateAdditiveMotions();
		resetJointSignatures();

		// Update all regular motions
		updateRegularMotions();

		if (use_quantum)
		{
			mPoseBlender.blendAndCache(true);
		}
		else
		{
			mPoseBlender.blendAndApply();
		}
	}

	mHasRunOnce = true;
}

// minimal update (e.g. while hidden)
void LLMotionController::updateMotionsMinimal()
{
	// Always update mPrevTimerElapsed
	mPrevTimerElapsed = mTimer.getElapsedTimeF32();

	purgeExcessMotions();
	updateLoadingMotions();
	resetJointSignatures();

	deactivateStoppedMotions();

	mHasRunOnce = true;
}

bool LLMotionController::activateMotionInstance(LLMotion* motion, F32 time)
{
	// It is not clear why the getWeight() line seems to be crashing this, but
	// hopefully this fixes it.
	if (!motion || !motion->getPose())
	{
		return false;
	}

	if (mLoadingMotions.count(motion) != 0)
	{
		// We want to start this motion, but we cannot yet, so flag it as
		// started
		motion->setStopped(false);
		// Report pending animations as activated
		return true;
	}

	motion->mResidualWeight = motion->getPose()->getWeight();

	// Set stop time based on given duration and ease out time
	if (motion->getDuration() != 0.f && !motion->getLoop())
	{
		F32 ease_out_time;
		F32 motion_duration;

		// Should we stop at the end of motion duration, or a bit earlier to
		// allow it to ease out while moving ?
		ease_out_time = motion->getEaseOutDuration();

		// Is the clock running when the motion is easing in ?  If not
		// (POSTURE_EASE) then we need to wait that much longer before
		// triggering the stop.
		motion_duration = llmax(motion->getDuration() - ease_out_time, 0.f);
		motion->mSendStopTimestamp = time + motion_duration;
	}
	else
	{
		motion->mSendStopTimestamp = F32_MAX;
	}

	if (motion->isActive())
	{
		mActiveMotions.remove(motion);
	}
	mActiveMotions.push_front(motion);

	motion->activate(time);
	motion->onUpdate(0.f, mJointSignature[1]);

	if (mAnimTime >= motion->mSendStopTimestamp)
	{
		motion->setStopTime(motion->mSendStopTimestamp);
		if (motion->mResidualWeight == 0.0f)
		{
			// Bit of a hack; if newly activating a motion while easing out,
			// weight should be 1.
			motion->mResidualWeight = 1.f;
		}
	}

	return true;
}

bool LLMotionController::deactivateMotionInstance(LLMotion* motion)
{
	if (!motion)
	{
		llwarns << "Attempted to deactivate a NULL motion (ignored) !"
				<< llendl;
		return true;
	}

	motion->deactivate();

	motion_set_t::iterator found_it = mDeprecatedMotions.find(motion);
	if (found_it != mDeprecatedMotions.end())
	{
		// Deprecated motions need to be completely excised
		removeMotionInstance(motion);
		mDeprecatedMotions.erase(found_it);
	}
	else
	{
		// For motions that we are keeping, simply remove from active queue
		mActiveMotions.remove(motion);
	}

	return true;
}

void LLMotionController::deprecateMotionInstance(LLMotion* motion)
{
	if (!motion)
	{
		llwarns << "Attempted to deprecate a NULL motion (ignored) !"
				<< llendl;
		return;
	}

	mDeprecatedMotions.insert(motion);

	// Fade out deprecated motion
	stopMotionInstance(motion, false);
	// No longer canonical
	mAllMotions.erase(motion->getID());
}

LLMotion* LLMotionController::findMotion(const LLUUID& id) const
{
	motion_map_t::const_iterator iter = mAllMotions.find(id);
	return iter == mAllMotions.end() ? NULL : iter->second;
}

void LLMotionController::deactivateAllMotions()
{
	for (motion_map_t::iterator iter = mAllMotions.begin();
		 iter != mAllMotions.end(); )
	{
		LLMotion* motionp = (iter++)->second;
		if (motionp)
		{
			deactivateMotionInstance(motionp);
		}
	}
}

void LLMotionController::pauseAllMotions()
{
	if (!mPaused)
	{
		mPaused = true;
		mPausedFrame = LLFrameTimer::getFrameCount();
	}
}

bool LLMotionController::isReallyPaused() const
{
	return mPaused && LLFrameTimer::getFrameCount() - mPausedFrame > 1;
}

void LLMotionController::flushAllMotions()
{
	std::vector<std::pair<LLUUID, F32> > active_motions;
	active_motions.reserve(mActiveMotions.size());
	for (motion_list_t::iterator iter = mActiveMotions.begin(),
								 end = mActiveMotions.end();
		 iter != end; ++iter)
	{
		LLMotion* motionp = *iter;
		if (motionp)
		{
			F32 dtime = mAnimTime - motionp->mActivationTimestamp;
			active_motions.push_back(std::make_pair(motionp->getID(), dtime));
			// Do not call deactivateMotionInstance() because we are going to
			// reactivate it
			motionp->deactivate();
		}
	}
 	mActiveMotions.clear();

	// Delete all motion instances
	deleteAllMotions();

	// Kill current hand pose that was previously called out by keyframe motion
	mCharacter->removeAnimationData("Hand Pose");

	// Restart motions
	for (std::vector<std::pair<LLUUID, F32> >::iterator
			iter = active_motions.begin(), end = active_motions.end();
		 iter != end; ++iter)
	{
		startMotion(iter->first, iter->second);
	}
}
