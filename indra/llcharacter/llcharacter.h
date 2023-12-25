/**
 * @file llcharacter.h
 * @brief Implementation of LLCharacter class.
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

#ifndef LL_LLCHARACTER_H
#define LL_LLCHARACTER_H

#include <string>

#include "boost/container/flat_map.hpp"

#include "lljoint.h"
#include "llmotioncontroller.h"
#include "llpointer.h"
#include "llstringtable.h"
#include "llthread.h"
#include "llvisualparam.h"

class LLPolyMesh;

class LLPauseRequestHandle : public LLThreadSafeRefCount
{
public:
	LLPauseRequestHandle() = default;
};

typedef LLPointer<LLPauseRequestHandle> LLAnimPauseRequest;

class LLCharacter
{
protected:
	LOG_CLASS(LLCharacter);

public:
	LLCharacter();
	virtual ~LLCharacter();

	//-------------------------------------------------------------------------
	// LLCharacter Interface
	// These functions must be implemented by subclasses.
	//-------------------------------------------------------------------------

	// Gets the prefix to be used to lookup motion data files
	// from the viewer data directory
	virtual const char* getAnimationPrefix() = 0;

	// Gets the root joint of the character
	virtual LLJoint* getRootJoint() = 0;

	// Gets the specified joint. The default implementation does recursive
	// search, subclasses may optimize/cache results.
	virtual LLJoint* getJoint(U32 key);

	// Gets the position of the character
	virtual LLVector3 getCharacterPosition() = 0;

	// Gets the rotation of the character
	virtual LLQuaternion getCharacterRotation() = 0;

	// Gets the velocity of the character
	virtual LLVector3 getCharacterVelocity() = 0;

	// Gets the angular velocity of the character
	virtual LLVector3 getCharacterAngularVelocity() = 0;

	// Gets the height & normal of the ground under a point
	virtual void getGround(const LLVector3& inPos, LLVector3& out_pos,
						   LLVector3& out_norm) = 0;

	// Skeleton joint accessor to support joint subclasses
	virtual LLJoint* getCharacterJoint(U32 i) = 0;

	// Gets the physics time dilation for the simulator
	virtual F32 getTimeDilation() = 0;

	// Gets current pixel area of this character
	virtual F32 getPixelArea() const = 0;

	// Gets the head mesh of the character
	virtual LLPolyMesh*	getHeadMesh() = 0;

	// Gets the upper body mesh of the character
	virtual LLPolyMesh*	getUpperBodyMesh() = 0;

	// Gets 'which' mesh for the character
	virtual LLPolyMesh*	getMesh(S32 which) = 0;

	// Gets global coordinates from agent local coordinates
	virtual LLVector3d getPosGlobalFromAgent(const LLVector3& position) = 0;

	// Gets agent local coordinates from global coordinates
	virtual LLVector3 getPosAgentFromGlobal(const LLVector3d& position) = 0;

	// Updates all visual parameters for this character
	virtual void updateVisualParams();

	virtual void addDebugText(const std::string& text) = 0;

	virtual const LLUUID& getID() = 0;

	//-------------------------------------------------------------------------
	// End Interface
	//-------------------------------------------------------------------------

	// Registers a motion with the character. returns true if successful.
	LL_INLINE bool registerMotion(const LLUUID& id,
								  LLMotionConstructor create)	{ return mMotionController.registerMotion(id, create); }

	LL_INLINE void removeMotion(const LLUUID& id)				{ mMotionController.removeMotion(id); }

	// Returns an instance of a registered motion, creating one if necessary
	// NOTE: always assign the result to a LLPointer !
	LLMotion* createMotion(const LLUUID& id)					{ return mMotionController.createMotion(id); }

	// Returns an existing instance of a registered motion
	LL_INLINE LLMotion* findMotion(const LLUUID& id)			{ return mMotionController.findMotion(id); }

	// Start a motion. Returns true if successful, false if an error occurred.
	LL_INLINE virtual bool startMotion(const LLUUID& id,
									   F32 start_offset = 0.f)	{ return mMotionController.startMotion(id, start_offset); }

	// Stop a motion
	LL_INLINE virtual bool stopMotion(const LLUUID& id,
									  bool immediatly = false)	{ return mMotionController.stopMotionLocally(id, immediatly); }

	// Is this motion active ?
	bool isMotionActive(const LLUUID& id);

	// Event handler for motion deactivation. Called when a motion has
	// completely stopped and has been deactivated. Subclasses may optionally
	// override this.
	LL_INLINE virtual void requestStopMotion(LLMotion* motion)	{}

	// Periodic update function, steps the motion controller
	enum e_update_t { NORMAL_UPDATE, HIDDEN_UPDATE, FORCE_UPDATE };
	virtual void updateMotions(e_update_t update_type);

	LLAnimPauseRequest requestPause();
	LL_INLINE bool areAnimationsPaused() const					{ return mMotionController.isPaused(); }
	LL_INLINE void setAnimTimeFactorMultiplier(F32 factor)		{ mMotionController.mTimeFactorMultiplier = factor; }
	LL_INLINE F32 getAnimTimeFactorMultiplier()					{ return mMotionController.mTimeFactorMultiplier; }
	LL_INLINE void setTimeStep(F32 time_step)					{ mMotionController.setTimeStep(time_step); }

	LL_INLINE LLMotionController& getMotionController()			{ return mMotionController; }

	// Releases all motion instances which should result in no cached
	// references to character joint data. This is useful if a character
	// wants to rebuild its skeleton.
	LL_INLINE virtual void deactivateAllMotions()				{ mMotionController.deactivateAllMotions(); }

	// Flush only wipes active animations.
	LL_INLINE virtual void flushAllMotions()					{ mMotionController.flushAllMotions(); }

	// Dumps information for debugging
	virtual void dumpCharacter(LLJoint* joint = NULL);

	LL_INLINE virtual F32 getPreferredPelvisHeight()			{ return mPreferredPelvisHeight; }

	LL_INLINE virtual LLVector3 getVolumePos(S32 joint_index,
											 LLVector3& offset)	{ return LLVector3::zero; }

	LL_INLINE virtual LLJoint* findCollisionVolume(S32 vol_id)	{ return NULL; }

	LL_INLINE virtual S32 getCollisionVolumeID(std::string& n)	{ return -1; }

	LL_INLINE void setAnimationData(const std::string& name,
									void* data)					{ mAnimationData[name] = data; }

	void* getAnimationData(const std::string& name);

	LL_INLINE void removeAnimationData(const std::string& name)	{ mAnimationData.erase(name); }

	void addVisualParam(LLVisualParam* param);
	void addSharedVisualParam(LLVisualParam* param);

	virtual bool setVisualParamWeight(const LLVisualParam* which_param,
									  F32 weight, bool upload_bake = false);
	virtual bool setVisualParamWeight(const char* param_name, F32 weight,
									  bool upload_bake = false);
	virtual bool setVisualParamWeight(S32 index, F32 weight,
									  bool upload_bake = false);

	// Gets visual param weight by param or name
	F32 getVisualParamWeight(LLVisualParam* distortion);
	F32 getVisualParamWeight(const char* param_name);
	F32 getVisualParamWeight(S32 index);

	// set all morph weights to defaults
	void clearVisualParamWeights();

	// Visual parameter accessors

	LL_INLINE LLVisualParam* getFirstVisualParam()
	{
		mCurIterator = mVisualParamIndexMap.begin();
		return getNextVisualParam();
	}

	LL_INLINE LLVisualParam* getNextVisualParam()
	{
		return mCurIterator == mVisualParamIndexMap.end() ? NULL
														  : (mCurIterator++)->second;
	}

	LL_INLINE LLVisualParam* getVisualParam(S32 id) const
	{
		visual_param_index_map_t::const_iterator iter = mVisualParamIndexMap.find(id);
		return iter == mVisualParamIndexMap.end() ? NULL : iter->second;
	}

	S32 getVisualParamCountInGroup(const EVisualParamGroup group) const;
	S32 getVisualParamID(LLVisualParam* id);

	LL_INLINE S32 getVisualParamCount() const					{ return (S32)mVisualParamIndexMap.size(); }
	LLVisualParam* getVisualParam(const char* name);

	LL_INLINE ESex getSex() const								{ return mSex; }
	LL_INLINE void setSex(ESex sex)								{ mSex = sex; }

	LL_INLINE U32 getAppearanceSerialNum() const				{ return mAppearanceSerialNum; }
	LL_INLINE void setAppearanceSerialNum(U32 num)				{ mAppearanceSerialNum = num; }

	LL_INLINE U32 getSkeletonSerialNum() const					{ return mSkeletonSerialNum; }
	LL_INLINE void bumpSkeletonSerialNum()						{ ++mSkeletonSerialNum; }

	static void initClass();
	static void dumpStats();

	LL_INLINE virtual void setHoverOffset(const LLVector3& hover_offset,
										  bool send_update = true)
	{
		mHoverOffset = hover_offset;
	}

	LL_INLINE const LLVector3& getHoverOffset() const			{ return mHoverOffset; }

public:
	static std::vector<LLCharacter*> sInstances;

protected:
	LLMotionController			mMotionController;

	typedef std::map<std::string, void*> animation_data_map_t;
	animation_data_map_t		mAnimationData;

	LLAnimPauseRequest			mPauseRequest;

	ESex						mSex;
	U32							mAppearanceSerialNum;
	U32							mSkeletonSerialNum;
	F32							mPreferredPelvisHeight;

private:
	LLVector3					mHoverOffset;

	// Visual parameters stuff

	// NOTE: do not replace with flat_hmap: this would not work porperly since
	// the visual parameters must stay ordered in the map... HB
	typedef boost::container::flat_map<S32,
									   LLVisualParam*> visual_param_index_map_t;
	typedef visual_param_index_map_t::iterator visual_param_index_map_it_t;
	typedef boost::container::flat_map<char*,
									   LLVisualParam*> visual_param_name_map_t;

	visual_param_index_map_it_t mCurIterator;
	visual_param_index_map_t	mVisualParamIndexMap;
	visual_param_name_map_t		mVisualParamNameMap;

	// Used to store replaced visual parameters that may still be referenced by
	// worn wearables (see LLCharacter::addVisualParam()). HB
	std::vector<LLVisualParam*>	mDeferredDeletions;

	static LLStringTable		sVisualParamNames;
};

#endif // LL_LLCHARACTER_H
