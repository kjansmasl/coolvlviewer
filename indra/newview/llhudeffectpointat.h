/**
 * @file llhudeffectpointat.h
 * @brief LLHUDEffectPointAt class definition
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

#ifndef LL_LLHUDEFFECTPOINTAT_H
#define LL_LLHUDEFFECTPOINTAT_H

#include "llhudeffect.h"

class LLViewerObject;
class LLVOAvatar;

typedef enum e_pointat_type
{
	POINTAT_TARGET_NONE,
	POINTAT_TARGET_SELECT,
	POINTAT_TARGET_GRAB,
	POINTAT_TARGET_CLEAR,
	POINTAT_NUM_TARGETS
} EPointAtType;

class LLHUDEffectPointAt : public LLHUDEffect
{
protected:
	LOG_CLASS(LLHUDEffectPointAt);

public:
	friend class LLHUDObject;

	void markDead() override;
	void setSourceObject(LLViewerObject* objectp) override;

	bool setPointAt(EPointAtType target_type, LLViewerObject* object,
					LLVector3 position);
	void clearPointAtTarget();

	LL_INLINE EPointAtType getPointAtType()			{ return mTargetType; }
	LL_INLINE const LLVector3& getPointAtPosAgent()	{ return mTargetPos; }

	const LLVector3d getPointAtPosGlobal();

protected:
	LLHUDEffectPointAt(U8 type);
	~LLHUDEffectPointAt() override = default;

	void render() override;
	void packData(LLMessageSystem* mesgsys) override;
	void unpackData(LLMessageSystem* mesgsys, S32 blocknum) override;

	// Point-at behavior has either target position or target object with
	// offset
	void setTargetObjectAndOffset(LLViewerObject* objp,
								  const LLVector3d& offset);
	void setTargetPosGlobal(const LLVector3d& target_pos_global);
	bool calcTargetPosition();
	void update() override;

public:
	static bool		sDebugPointAt;

private:
	LLVector3d		mTargetOffsetGlobal;
	LLVector3		mLastSentOffsetGlobal;
	LLVector3		mTargetPos;
	LLFrameTimer	mTimer;
	EPointAtType	mTargetType;
	F32				mKillTime;
	F32				mLastSendTime;
};

#endif // LL_LLHUDEFFECTPOINTAT_H
