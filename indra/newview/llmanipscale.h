/**
 * @file llmanipscale.h
 * @brief LLManipScale class definition
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

#ifndef LL_MANIPSCALE_H
#define LL_MANIPSCALE_H

#include "llbbox.h"
#include "llvector3.h"
#include "llvector4.h"

#include "llmanip.h"
#include "lltool.h"
#include "llviewerobject.h"

class LLToolComposite;
class LLColor4;

typedef	enum e_scale_manipulator_type
{
	SCALE_MANIP_CORNER,
	SCALE_MANIP_FACE
} EScaleManipulatorType;

// Treatead as a bitmask.
typedef	enum e_snap_regimes
{
	// The cursor is not in either of the snap regimes.
	SNAP_REGIME_NONE = 0x0,
	// The cursor is, non-exclusively, in the first of the snap regimes.
	SNAP_REGIME_UPPER = 0x1,
	// The cursor is, non-exclusively, in the second of the snap regimes.
	SNAP_REGIME_LOWER = 0x2
} ESnapRegimes;

class LLManipScale : public LLManip
{
protected:
	LOG_CLASS(LLManipScale);

public:
	class ManipulatorHandle
	{
	public:
		LLVector3				mPosition;
		EManipPart				mManipID;
		EScaleManipulatorType	mType;

		ManipulatorHandle(LLVector3 pos, EManipPart id,
						  EScaleManipulatorType type)
		:	mPosition(pos),
			mManipID(id),
			mType(type)
		{
		}
	};

	LLManipScale(LLToolComposite* composite);
	~LLManipScale() override;

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	void render() override;
	void handleSelect() override;

	bool handleMouseDownOnPart(S32 x, S32 y, MASK mask) override;

	// Decides which manipulator, if any, should be highlighted by mouse hover:
	void highlightManipulators(S32 x, S32 y) override;

	bool canAffectSelection() override;

	static F32 maxPrimScale(bool is_flora = false);
	static F32 minPrimScale(bool is_flora = false);

	static void setUniform(bool b);
	static bool getUniform();
	static void setStretchTextures(bool b);
	static bool getStretchTextures();
	static void setShowAxes(bool b);
	static bool getShowAxes();
	static bool getSnapEnabled();

private:
	void renderCorners(const LLBBox& local_bbox);
	void renderFaces(const LLBBox& local_bbox);
	void renderBoxHandle(F32 x, F32 y, F32 z);
	void renderAxisHandle(const LLVector3& start, const LLVector3& end);
	void renderGuidelinesPart(const LLBBox& local_bbox);
	void renderSnapGuides(const LLBBox& local_bbox);

	void revert();

	LL_INLINE void conditionalHighlight(U32 part,
										const LLColor4* highlight = NULL,
										const LLColor4* normal = NULL);

	void drag(S32 x, S32 y);
	void dragFace(S32 x, S32 y);
	void dragCorner(S32 x, S32 y);

	void sendUpdates(bool send_position_update, bool send_scale_update,
					 bool corner = false);

	LLVector3 faceToUnitVector(S32 part) const;
	LLVector3 cornerToUnitVector(S32 part) const;
	LLVector3 edgeToUnitVector(S32 part) const;
	LLVector3 partToUnitVector(S32 part) const;
	LLVector3 unitVectorToLocalBBoxExtent(const LLVector3& v,
												const LLBBox& bbox) const;
	F32 partToMaxScale(S32 part, const LLBBox& bbox) const;
	F32 partToMinScale(S32 part, const LLBBox& bbox) const;
	LLVector3 nearestAxis(const LLVector3& v) const;

	void stretchFace(const LLVector3& drag_start_agent,
					 const LLVector3& drag_delta_agent);

	// Adjusts texture coords based on mSavedScale and current scale, only
	// works for boxes
	void adjustTextureRepeats();

	void updateSnapGuides(const LLBBox& bbox);

private:
	struct compare_manipulators
	{
		LL_INLINE bool operator()(const ManipulatorHandle* const a,
								  const ManipulatorHandle* const b) const
		{
			if (a->mType != b->mType)
			{
				return a->mType < b->mType;
			}
			else if (a->mPosition.mV[VZ] != b->mPosition.mV[VZ])
			{
				return a->mPosition.mV[VZ] < b->mPosition.mV[VZ];
			}
			else
			{
				return a->mManipID < b->mManipID;
			}
		}
	};

private:
	// The size of the handles at the corners of the bounding box
	F32					mBoxHandleSize;

	// Handle size after scaling for selection feedback
	F32					mScaledBoxHandleSize;

	LLVector3d			mDragStartPointGlobal;

	// The center of the bounding box of all selected objects at time of drag
	// start
	LLVector3d			mDragStartCenterGlobal;

	LLVector3d			mDragPointGlobal;
	LLVector3d 			mDragFarHitGlobal;
	S32					mLastMouseX;
	S32					mLastMouseY;
	U32 				mLastUpdateFlags;

	typedef std::set<ManipulatorHandle*,
					 compare_manipulators> manipulator_list_t;
	manipulator_list_t	mProjectedManipulators;

	LLVector4			mManipulatorVertices[14];

	F32					mScaleSnapUnit1;  // size of snap multiples for axis 1
	F32					mScaleSnapUnit2;  // size of snap multiples for axis 2

	// Normal of plane in which scale occurs that most faces camera
	LLVector3			mScalePlaneNormal1;

	// Normal of plane in which scale occurs that most faces camera
	LLVector3			mScalePlaneNormal2;

	// The direction in which the upper snap guide tick marks face.
	LLVector3			mSnapGuideDir1;

	// The direction in which the lower snap guide tick marks face.
	LLVector3			mSnapGuideDir2;

	// The direction in which the upper snap guides face.
	LLVector3			mSnapDir1;

	// The direction in which the lower snap guides face.
	LLVector3			mSnapDir2;

	// How far off the scale axis centerline the mouse can be before it
	// exits/enters the snap regime.
	F32					mSnapRegimeOffset;

	// The pixel spacing between snap guide tick marks for the upper scale.
	F32					mTickPixelSpacing1;

	// The pixel spacing between snap guide tick marks for the lower scale.
	F32					mTickPixelSpacing2;

	F32					mSnapGuideLength;

	// The location of the origin of the scaling operation.
	LLVector3			mScaleCenter;

	// The direction of the scaling action. In face-dragging this is aligned
	// with one of the cardinal axis relative to the prim, but in
	// corner-dragging this is along the diagonal.
	LLVector3			mScaleDir;

	// The distance of the current position nearest the mouse location,
	// measured along mScaleDir. Is measured either from the center or from the
	// far face/corner depending upon whether uniform scaling is true or false
	// respectively.
	F32					mScaleSnappedValue;

	// Which, if any, snap regime the cursor is currently residing in:
	ESnapRegimes		mSnapRegime;

	F32*				mManipulatorScales;
};

#endif  // LL_MANIPSCALE_H
