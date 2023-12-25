/**
 * @file llfollowcam.h
 * @author Jeffrey Ventrella
 * @brief LLFollowCam class definition
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

//-----------------------------------------------------------------------------
// FollowCam
//
// The FollowCam controls three dynamic variables which determine a camera
// orientation and position for a "loose" third-person view (orientation being
// derived from a combination of focus and up vector). It is good for fast
// moving vehicles that change acceleration a lot, but it can also be general
// purpose, like for avatar navigation. It has a handful of parameters allowing
// it to be tweaked to assume different styles of tracking objects.
//-----------------------------------------------------------------------------

#ifndef LL_FOLLOWCAM_H
#define LL_FOLLOWCAM_H

#include <vector>

#include "indra_constants.h"

#include "llcoordframe.h"
#include "llcriticaldamp.h"
#include "hbfastmap.h"
#include "llmath.h"
#include "llquaternion.h"
#include "lltimer.h"

class LLFollowCamParams
{
public:
	LLFollowCamParams();
	virtual ~LLFollowCamParams() = default;

	LL_INLINE virtual void setPosition(const LLVector3& pos)
	{
		mUsePosition = true;
		mPosition = pos;
	}

	LL_INLINE virtual void setFocus(const LLVector3& focus)
	{
		mUseFocus = true;
		mFocus = focus;
	}

	LL_INLINE virtual void setPositionLocked(bool b)			{ mPositionLocked = b; }
	LL_INLINE virtual void setFocusLocked(bool b)				{ mFocusLocked = b; }

	virtual void setPositionLag(F32);
	virtual void setFocusLag(F32);
	virtual void setFocusThreshold(F32);
	virtual void setPositionThreshold(F32);
	virtual void setDistance(F32);
	virtual void setPitch(F32);
	virtual void setFocusOffset(const LLVector3&);
	virtual void setBehindnessAngle(F32);
	virtual void setBehindnessLag(F32);

	LL_INLINE virtual F32		getPositionLag() const			{ return mPositionLag; }
	LL_INLINE virtual F32		getFocusLag() const				{ return mFocusLag; }
	LL_INLINE virtual F32		getPositionThreshold() const	{ return mPositionThreshold; }
	LL_INLINE virtual F32		getFocusThreshold() const		{ return mFocusThreshold; }
	LL_INLINE virtual F32		getDistance() const				{ return mDistance; }
	LL_INLINE virtual F32		getPitch() const				{ return mPitch; }
	LL_INLINE virtual LLVector3	getFocusOffset() const			{ return mFocusOffset; }
	LL_INLINE virtual F32		getBehindnessAngle() const		{ return mBehindnessMaxAngle; }
	LL_INLINE virtual F32		getBehindnessLag() const		{ return mBehindnessLag; }
	LL_INLINE virtual LLVector3	getPosition() const				{ return mPosition; }
	LL_INLINE virtual LLVector3	getFocus() const				{ return mFocus; }
	LL_INLINE virtual bool		getFocusLocked() const			{ return mFocusLocked; }
	LL_INLINE virtual bool		getPositionLocked() const		{ return mPositionLocked; }
	LL_INLINE virtual bool		getUseFocus() const				{ return mUseFocus; }
	LL_INLINE virtual bool		getUsePosition() const			{ return mUsePosition; }

protected:
	F32			mPositionLag;
	F32			mFocusLag;
	F32			mFocusThreshold;
	F32			mPositionThreshold;
	F32			mDistance;
	F32			mPitch;
	F32			mBehindnessMaxAngle;
	F32			mBehindnessLag;
	F32			mMaxCameraDistantFromSubject;

	LLVector3	mPosition;		// where the camera is (in world-space)
	LLVector3	mFocus;			// what the camera is aimed at (in world-space)
	LLVector3	mFocusOffset;

	bool		mPositionLocked;
	bool		mFocusLocked;
	bool		mUsePosition;	// specific camera point specified by script
	bool		mUseFocus;		// specific focus point specified by script
};

class LLFollowCam : public LLFollowCamParams
{
protected:
	LOG_CLASS(LLFollowCam);

public:
	LLFollowCam();

	//-------------------------------------------------------------------------
	// The following methods must be called every time step. However, if you
	// know for sure that your subject matter (what the camera is looking at)
	// is not moving, then you can get away with not calling "update" But keep
	// in mind that "update" may still be needed after the subject matter has
	// stopped moving because the camera may still need to animate itself
	// catching up to its ideal resting place.
	//-------------------------------------------------------------------------
	LL_INLINE void setSubjectPositionAndRotation(const LLVector3 p,
												 const LLQuaternion r)
	{
		mSubjectPosition = p;
		mSubjectRotation = r;
	}

	void update();

	// Initializes from another instance of LLFollowCamParams
	void copyParams(LLFollowCamParams& params);

	//-------------------------------------------------------------------------
	// This is how to bang the followCam into a specific configuration. Keep in
	// mind that it will immediately try to adjust these values according to
	// its attributes.
	//-------------------------------------------------------------------------
	void reset(const LLVector3 position, const LLVector3 focus,
			   const LLVector3 upVector);

	// This should be determined by llAgent
	LL_INLINE void setMaxCameraDistantFromSubject(F32 m)		{ mMaxCameraDistantFromSubject = m; }

	LL_INLINE bool isZoomedToMinimumDistance()					{ return mZoomedToMinimumDistance; }
	LL_INLINE LLVector3 getUpVector()							{ return mUpVector; }

	void zoom(S32);

	// Overrides for setters and getters

	void setPosition(const LLVector3& pos) override;
	void setFocus(const LLVector3& focus) override;
	void setPositionLocked(bool) override;
	void setFocusLocked(bool) override;
	void setPitch(F32) override;
	void setDistance(F32) override;

	// Returns simulated position
	LL_INLINE LLVector3 getSimulatedPosition() const
	{
		return mSubjectPosition + mRelativePos * mSubjectRotation;
	}

	// Returns simulated focus point
	LL_INLINE LLVector3 getSimulatedFocus() const
	{
		return mSubjectPosition + mRelativeFocus * mSubjectRotation;
	}

protected:
	void calculatePitchSineAndCosine();
	bool updateBehindnessConstraint(LLVector3 focus, LLVector3& cam_position);

protected:
	F32				mPitchCos;	// Derived from mPitch
	F32				mPitchSin;	// Derived from mPitch

	// Where the camera is (global coordinates), simulated
	LLGlobalVec		mSimulatedPositionGlobal;

	// What the camera is aimed at (global coordinates), simulated
	LLGlobalVec		mSimulatedFocusGlobal;
	F32				mSimulatedDistance;

	// This is the position we are looking at
	LLVector3		mSubjectPosition;

	// This is the rotation we are looking at
	LLQuaternion	mSubjectRotation;

	// The camera up vector in world-space (determines roll)
	LLVector3		mUpVector;

	LLVector3		mRelativeFocus;
	LLVector3		mRelativePos;

	LLFrameTimer	mTimer;

	bool			mZoomedToMinimumDistance;
	bool			mPitchSineAndCosineNeedToBeUpdated;
};

class LLFollowCamMgr
{
protected:
	LOG_CLASS(LLFollowCamMgr);

public:
	// WARNING: should this method get modified to do anything else than
	// removing all follow-camera constraints data, it would be necessary
	// to make a new method for calling it from llviewermenu.cpp (for the
	// "Release camera" action).
	static void cleanupClass();

	static void setPositionLag(const LLUUID& source, F32 lag);
	static void setFocusLag(const LLUUID& source, F32 lag);
	static void setFocusThreshold(const LLUUID& source, F32 threshold);
	static void setPositionThreshold(const LLUUID& source, F32 threshold);
	static void setDistance(const LLUUID& source, F32 distance);
	static void setPitch(const LLUUID& source, F32 pitch);
	static void setFocusOffset(const LLUUID& source, const LLVector3& offset);
	static void setBehindnessAngle(const LLUUID& source, F32 angle);
	static void setBehindnessLag(const LLUUID& source, F32 lag);
	static void setPosition(const LLUUID& source, const LLVector3 position);
	static void setFocus(const LLUUID& source, const LLVector3 focus);
	static void setPositionLocked(const LLUUID& source, bool locked);
	static void setFocusLocked(const LLUUID& source, bool locked);

	static void setCameraActive(const LLUUID& source, bool active);

	LL_INLINE static LLFollowCamParams* getActiveFollowCamParams()
	{
		return sParamStack.empty() ? NULL : sParamStack.back();
	}

	static LLFollowCamParams* getParamsForID(const LLUUID& source);
	static void removeFollowCamParams(const LLUUID& source);

	LL_INLINE static bool isScriptedCameraSource(const LLUUID& source)
	{
		return sParamMap.count(source) != 0;
	}

	static void dump();

protected:
	typedef fast_hmap<LLUUID, LLFollowCamParams*> param_map_t;
	static param_map_t sParamMap;

	typedef std::vector<LLFollowCamParams*> param_stack_t;
	static param_stack_t sParamStack;
};

// Script-related constants
enum EFollowCamAttributes {
	FOLLOWCAM_PITCH = 0,
	FOLLOWCAM_FOCUS_OFFSET,
	// This HAS to come after FOLLOWCAM_FOCUS_OFFSET in this list:
	FOLLOWCAM_FOCUS_OFFSET_X,
	FOLLOWCAM_FOCUS_OFFSET_Y,
	FOLLOWCAM_FOCUS_OFFSET_Z,
	FOLLOWCAM_POSITION_LAG,
	FOLLOWCAM_FOCUS_LAG,
	FOLLOWCAM_DISTANCE,
	FOLLOWCAM_BEHINDNESS_ANGLE,
	FOLLOWCAM_BEHINDNESS_LAG,
	FOLLOWCAM_POSITION_THRESHOLD,
	FOLLOWCAM_FOCUS_THRESHOLD,
	FOLLOWCAM_ACTIVE,
	FOLLOWCAM_POSITION,
	// This HAS to come after FOLLOWCAM_POSITION in this list:
	FOLLOWCAM_POSITION_X,
	FOLLOWCAM_POSITION_Y,
	FOLLOWCAM_POSITION_Z,
	FOLLOWCAM_FOCUS,
	// This HAS to come after FOLLOWCAM_FOCUS in this list:
	FOLLOWCAM_FOCUS_X,
	FOLLOWCAM_FOCUS_Y,
	FOLLOWCAM_FOCUS_Z,
	FOLLOWCAM_POSITION_LOCKED,
	FOLLOWCAM_FOCUS_LOCKED,
	NUM_FOLLOWCAM_ATTRIBUTES
};

#endif //LL_FOLLOWCAM_H
