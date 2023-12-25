/**
 * @file llhudeffectspiral.h
 * @brief LLHUDEffectSpiral class definition
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

#ifndef LL_LLHUDEFFECTSPIRAL_H
#define LL_LLHUDEFFECTSPIRAL_H

#include "llhudeffect.h"

#include "llframetimer.h"
#include "llinterp.h"

#include "llviewerpartsim.h"

class LLVector3d;
class LLViewerObject;

constexpr U32 NUM_TRAIL_POINTS = 40;

class LLHUDEffectSpiral : public LLHUDEffect
{
	friend class LLHUDObject;

protected:
	LOG_CLASS(LLHUDEffectSpiral);

public:
	void markDead() override;
	void setTargetObject(LLViewerObject* objectp) override;

	LL_INLINE void setVMag(F32 vmag)			{ mVMag = vmag; }
	LL_INLINE void setVOffset(F32 offset)		{ mVOffset = offset; }
	LL_INLINE void setInitialRadius(F32 radius)	{ mInitialRadius = radius; }
	LL_INLINE void setFinalRadius(F32 radius)	{ mFinalRadius = radius; }
	LL_INLINE void setScaleBase(F32 scale)		{ mScaleBase = scale; }
	LL_INLINE void setScaleVar(F32 scale)		{ mScaleVar = scale; }
	LL_INLINE void setSpinRate(F32 rate)		{ mSpinRate = rate; }
	LL_INLINE void setFlickerRate(F32 rate)		{ mFlickerRate = rate; }

	// Start the effect playing locally.
	void triggerLocal();

	// Factorized code to create the standard beam effect from the agent to an
	// object or a global position with the standard agent effect color. HB
	static void agentBeamToObject(LLViewerObject* objectp);
	static void agentBeamToPosition(const LLVector3d& pos);
	// Swirling particles at global position, with optional duration (0 to mark
	// dead once sent) and optional immediate sending to server. HB
	static void swirlAtPosition(const LLVector3d& pos, F32 duration = -1.f,
								bool send_now = false);
	// Sphere effect at global position, for 0.25s (used by LLToolPie only). HB
	static void sphereAtPosition(const LLVector3d& pos);

protected:
	LLHUDEffectSpiral(U8 type);
	~LLHUDEffectSpiral() override = default;

	void update() override;
	LL_INLINE void render() override			{}
	void packData(LLMessageSystem* mesgsys) override;
	void unpackData(LLMessageSystem* mesgsys, S32 blocknum) override;

private:
	LLPointer<LLViewerPartSource>	mPartSourcep;

	F32								mKillTime;
	F32								mVMag;
	F32								mVOffset;
	F32								mInitialRadius;
	F32								mFinalRadius;
	F32								mSpinRate;
	F32								mFlickerRate;
	F32								mScaleBase;
	F32								mScaleVar;
	LLFrameTimer					mTimer;
	LLInterpLinear					mFadeInterp;
};

#endif // LL_LLHUDEFFECTSPIRAL_H
