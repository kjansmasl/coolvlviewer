/**
 * @file llviewervisualparam.h
 * @brief viewer side visual params (with data file parsing)
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

#ifndef LL_LLVIEWERVISUALPARAM_H
#define LL_LLVIEWERVISUALPARAM_H

#include "llvisualparam.h"

class LLWearable;

//-----------------------------------------------------------------------------
// LLViewerVisualParamInfo
//-----------------------------------------------------------------------------
class LLViewerVisualParamInfo : public LLVisualParamInfo
{
	friend class LLViewerVisualParam;

public:
	LLViewerVisualParamInfo();

	bool parseXml(LLXmlTreeNode* node) override;

	void toStream(std::ostream& out) override;

protected:
	S32			mWearableType;
	F32			mCamDist;
	F32			mCamAngle;		// degrees
	F32			mCamElevation;
	F32			mEditGroupDisplayOrder;
	// When in simple UI, apply this minimum, range 0.f to 100.f
	F32			mSimpleMin;
	// When in simple UI, apply this maximum, range 0.f to 100.f
	F32			mSimpleMax;
	std::string	mEditGroup;
	bool		mCrossWearable;
};

//-----------------------------------------------------------------------------
// LLViewerVisualParam - Virtual class
// A viewer side interface class for a generalized parametric modification of
// the avatar mesh
//-----------------------------------------------------------------------------
class alignas(16) LLViewerVisualParam : public LLVisualParam
{
protected:
	LLViewerVisualParam(const LLViewerVisualParam& other);

public:
	LLViewerVisualParam();

	LL_INLINE LLViewerVisualParam* asViewerVisualParam() override
	{
		return this;
	}

	// Special: These functions are overridden by child classes

	LL_INLINE LLViewerVisualParamInfo* getInfo() const
	{
		return (LLViewerVisualParamInfo*)mInfo;
	}

	// This sets mInfo and calls initialization functions
	bool setInfo(LLViewerVisualParamInfo* info);

	virtual LLViewerVisualParam* cloneParam(LLWearable* wearable) const = 0;

#if 0	// Unused methods
	// New Virtual functions
	virtual F32 getTotalDistortion() = 0;
	virtual const LLVector4a& getAvgDistortion() = 0;
	virtual F32 getMaxDistortion() = 0;
	virtual LLVector4a getVertexDistortion(S32 index, LLPolyMesh* mesh) = 0;
	virtual const LLVector4a* getFirstDistortion(U32* idx, LLPolyMesh** m) = 0;
	virtual const LLVector4a* getNextDistortion(U32* idx, LLPolyMesh** m) = 0;
#endif

	// Interface methods
	LL_INLINE F32 getDisplayOrder() const		{ return getInfo()->mEditGroupDisplayOrder; }
	LL_INLINE S32 getWearableType() const		{ return getInfo()->mWearableType; }

	LL_INLINE const std::string& getEditGroup() const
	{
		return getInfo()->mEditGroup;
	}

	LL_INLINE F32 getCameraDistance()	const	{ return getInfo()->mCamDist; }
	LL_INLINE F32 getCameraAngle() const		{ return getInfo()->mCamAngle; }
	LL_INLINE F32 getCameraElevation() const	{ return getInfo()->mCamElevation; }

	LL_INLINE F32 getSimpleMin() const			{ return getInfo()->mSimpleMin; }
	LL_INLINE F32 getSimpleMax() const			{ return getInfo()->mSimpleMax; }

	LL_INLINE bool getCrossWearable() const		{ return getInfo()->mCrossWearable; }
};

#endif // LL_LLVIEWERVISUALPARAM_H
