/**
 * @file llvisualparamhint.h
 * @brief A dynamic texture class for displaying avatar visual params effects
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

// Note: probably because of obscure pre-historical reasons, this file is
// named "lltoolmorph.h" in LL's viewer sources. I renamed it based on the
// class it declares instead. HB

#ifndef LL_LLVISUALPARAMHINT_H
#define LL_LLVISUALPARAMHINT_H

#include "llui.h"
#include "llviewervisualparam.h"

#include "lldynamictexture.h"

class LLJoint;
class LLPolyMesh;
class LLViewerJointMesh;
class LLViewerObject;

class LLVisualParamHint final : public LLViewerDynamicTexture
{
protected:
	LOG_CLASS(LLVisualParamHint);

protected:
	~LLVisualParamHint() override;

public:
	LLVisualParamHint(S32 pos_x,  S32 pos_y, S32 width, S32 height,
					  LLViewerJointMesh* mesh, LLViewerVisualParam* param,
					  LLWearable* wearable, F32 param_weight,
					  LLJoint* jointp);

	S8 getType() const override;
	bool needsRender() override;
	void preRender(bool clear_depth) override;
	bool render() override;

	void setWearable(LLWearable* wearable, LLViewerVisualParam* param);

	LL_INLINE void requestUpdate(S32 delay_frames)
	{
		mNeedsUpdate = true;
		mDelayFrames = delay_frames;
	}

	LL_INLINE void setUpdateDelayFrames(S32 delay)	{ mDelayFrames = delay; }

	void draw();

	LL_INLINE LLViewerVisualParam* getVisualParam()	{ return mVisualParam; }
	LL_INLINE F32 getVisualParamWeight()			{ return mVisualParamWeight; }
	LL_INLINE bool getVisible()						{ return mIsVisible; }

	LL_INLINE void setAllowsUpdates(bool b)			{ mAllowsUpdates = b; }

	LL_INLINE const LLRect& getRect()				{ return mRect; }

	// Requests updates for all instances (excluding two possible exceptions)
	//  Grungy but efficient.
	static void requestHintUpdates(LLVisualParamHint* exception1 = NULL,
								   LLVisualParamHint* exception2 = NULL);

protected:
	LLViewerJointMesh*		mJointMesh;			// mesh that this distortion applies to
	LLViewerVisualParam*	mVisualParam;		// visual param applied by this hint
	LLWearable*				mWearablePtr;		// wearable we're editing
	LLJoint*				mCamTargetJoint;	// joint to target with preview camera
	LLUIImagePtr			mBackgroundp;
	LLRect					mRect;
	S32						mDelayFrames;		// updates are blocked for this many frames
	F32						mVisualParamWeight;	// weight for this visual parameter
	F32						mLastParamWeight;

	bool					mNeedsUpdate;		// does this texture need to be re-rendered?
	bool					mAllowsUpdates;		// updates are blocked unless this is true
	bool					mIsVisible;			// is this distortion hint visible ?

	typedef std::set<LLVisualParamHint*> instance_list_t;
	static instance_list_t	sInstances;
};

// This class resets avatar data at the end of an update cycle
class LLVisualParamReset final : public LLViewerDynamicTexture
{
protected:
	~LLVisualParamReset() override	{}

public:
	LLVisualParamReset();

	bool render() override;
	S8 getType() const override;

	static bool sDirty;
};

#endif // LL_LLVISUALPARAMHINT_H
