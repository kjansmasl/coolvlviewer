/**
 * @file lltoolgrab.h
 * @brief LLToolGrab class header file
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

#ifndef LL_TOOLGRAB_H
#define LL_TOOLGRAB_H

#include "llquaternion.h"
#include "lluuid.h"
#include "llvector3.h"

#include "lltool.h"
#include "llviewerwindow.h"		// For LLPickInfo

class LLView;
class LLTextBox;
class LLViewerObject;
class LLPickInfo;

class LLToolGrabBase : public LLTool
{
protected:
	LOG_CLASS(LLToolGrabBase);

public:
	LLToolGrabBase(LLToolComposite* composite = NULL);

	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;

	LL_INLINE void render() override			{}		// 3D elements
	LL_INLINE void draw() override				{}		// 2D elements

	void handleSelect() override;
	void handleDeselect() override;

	LLViewerObject* getEditingObject() override;
	LLVector3d getEditingPointGlobal() override;
	bool isEditing() override;
	void stopEditing() override;

	void onMouseCaptureLost() override;

	// Capture the mouse and start grabbing.
	bool handleObjectHit(const LLPickInfo& info);

	static void pickCallback(const LLPickInfo& pick_info);

private:
	LLVector3d getGrabPointGlobal();
	void startGrab();
	void stopGrab();

	void startSpin();
	void stopSpin();

	void handleHoverSpin(S32 x, S32 y, MASK mask);
	void handleHoverActive(S32 x, S32 y, MASK mask);
	void handleHoverNonPhysical(S32 x, S32 y, MASK mask);
	void handleHoverInactive(S32 x, S32 y, MASK mask);
	void handleHoverFailed(S32 x, S32 y, MASK mask);

private:
	enum EGrabMode { GRAB_INACTIVE, GRAB_ACTIVE_CENTER, GRAB_NONPHYSICAL,
					 GRAB_LOCKED, GRAB_NOOBJECT };

	EGrabMode		mMode;

	// Send simulator time between hover movements
	LLTimer			mGrabTimer;

	// Meters from CG of object
	LLVector3		mGrabOffsetFromCenterInitial;
	// In cursor hidden drag, how far is grab offset from camera
	LLVector3d		mGrabHiddenOffsetFromCamera;

	LLVector3d		mDragStartPointGlobal;	// Projected into world
	LLVector3d		mDragStartFromCamera;	// Drag start relative to camera

	LLPickInfo		mGrabPick;

	S32				mLastMouseX;
	S32				mLastMouseY;
	// Since cursor hidden, how far have you moved ?
	S32				mAccumDeltaX;
	S32				mAccumDeltaY;

	S32             mLastFace;
	LLVector2       mLastUVCoords;
	LLVector2       mLastSTCoords;
	LLVector3       mLastIntersection;
	LLVector3       mLastNormal;
	LLVector3       mLastBinormal;
	LLVector3       mLastGrabPos;

	LLQuaternion	mSpinRotation;

	// Has mouse moved off center at all ?
	bool			mHasMoved;
	// Nas mouse moved outside center 5 pixels ?
	bool			mOutsideSlop;

	bool			mVerticalDragging;
	bool			mSpinGrabbing;
	bool			mClickedInMouselook;
};

class LLToolGrab final : public LLToolGrabBase
{
protected:
	LOG_CLASS(LLToolGrab);

public:
	LLToolGrab() = default;
};

extern LLToolGrab gToolGrab;
extern LLTool* gGrabTransientTool;
extern bool gGrabBtnVertical;
extern bool gGrabBtnSpin;

#endif  // LL_TOOLGRAB_H
