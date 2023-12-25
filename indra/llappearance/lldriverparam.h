/**
 * @file lldriverparam.h
 * @brief A visual parameter that drives (controls) other visual parameters.
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

#ifndef LL_LLDRIVERPARAM_H
#define LL_LLDRIVERPARAM_H

#include <deque>

#include "llmemory.h"
#include "llviewervisualparam.h"
#include "llwearabletype.h"

class LLAvatarAppearance;
class LLDriverParam;
class LLWearable;

struct LLDrivenEntryInfo
{
	LLDrivenEntryInfo(S32 id, F32 min1, F32 max1, F32 max2, F32 min2)
	:	mDrivenID(id),
		mMin1(min1),
		mMax1(max1),
		mMax2(max2),
		mMin2(min2)
	{
	}

	S32 mDrivenID;
	F32 mMin1;
	F32 mMax1;
	F32 mMax2;
	F32 mMin2;
};

struct LLDrivenEntry
{
	LLDrivenEntry(LLViewerVisualParam* param, LLDrivenEntryInfo* info)
	:	mParam(param),
		mInfo(info)
	{
	}

	LLViewerVisualParam*	mParam;
	LLDrivenEntryInfo*		mInfo;
};

class LLDriverParamInfo final : public LLViewerVisualParamInfo
{
	friend class LLDriverParam;

protected:
	LOG_CLASS(LLDriverParamInfo);

public:
	LLDriverParamInfo();

	bool parseXml(LLXmlTreeNode* node) override;

	void toStream(std::ostream& out) override;

protected:
	typedef std::deque<LLDrivenEntryInfo> entry_info_list_t;
	entry_info_list_t	mDrivenInfoList;

	LLDriverParam*		mDriverParam; // backpointer
};

class alignas(16) LLDriverParam final : public LLViewerVisualParam
{
public:
	// No default constructor. Force construction with LLAvatarAppearance.
	LLDriverParam() = delete;

	LLDriverParam(LLAvatarAppearance* appearance, LLWearable* wearable = NULL);

	LL_INLINE LLDriverParam* asDriverParam() override	{ return this; }

	// Special: These functions are overridden by child classes

	LL_INLINE LLDriverParamInfo* getInfo() const		{ return (LLDriverParamInfo*)mInfo; }

	// This sets mInfo and calls initialization functions
	bool setInfo(LLDriverParamInfo* info);

	LL_INLINE LLAvatarAppearance* getAvatarAppearance()	{ return mAvatarAppearance; }

	LL_INLINE const LLAvatarAppearance* getAvatarAppearance() const
	{
		return mAvatarAppearance;
	}

	void updateCrossDrivenParams(LLWearableType::EType driven_type);

	LLViewerVisualParam* cloneParam(LLWearable* wearable) const override;

	// LLVisualParam Virtual functions

	// Apply is called separately for each driven param:
	LL_INLINE void apply(ESex sex) override				{}
	void setWeight(F32 weight, bool upload_bake) override;
	void setAnimationTarget(F32 target_value, bool upload_bake) override;
	void stopAnimating(bool upload_bake) override;
	bool linkDrivenParams(visual_param_mapper mapper,
						  bool only_cross_params) override;
	void resetDrivenParams() override;

	// LLViewerVisualParam Virtual functions

#if 0	// Unused methods
	F32 getTotalDistortion() override;
	const LLVector4a& getAvgDistortion() override;
	F32 getMaxDistortion() override;
	LLVector4a getVertexDistortion(S32 index, LLPolyMesh* poly_mesh) override;
	const LLVector4a* getFirstDistortion(U32* idx, LLPolyMesh** mesh) override;
	const LLVector4a* getNextDistortion(U32* idx, LLPolyMesh** pmesh) override;
#endif

	S32 getDrivenParamsCount() const;
	const LLViewerVisualParam* getDrivenParam(S32 index) const;

	typedef std::vector<LLDrivenEntry> entry_list_t;
	LL_INLINE entry_list_t& getDrivenList()					{ return mDriven; }
	LL_INLINE void  setDrivenList(entry_list_t& list)		{ mDriven = list; }

protected:
	LLDriverParam(const LLDriverParam& other);

	F32 getDrivenWeight(const LLDrivenEntry* driven, F32 input_weight);
	void setDrivenWeight(LLDrivenEntry* driven, F32 driven_weight,
						 bool upload_bake);

protected:
	LLVector4a				mDefaultVec;	// Temp holder

	entry_list_t			mDriven;

	LLViewerVisualParam*	mCurrentDistortionParam;
	LLWearable*				mWearablep;

	// Backlink only; don't make this an LLPointer.
	LLAvatarAppearance*		mAvatarAppearance;
};

#endif  // LL_LLDRIVERPARAM_H
