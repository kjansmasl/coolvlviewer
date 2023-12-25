/**
 * @file llhudeffectspiral.cpp
 * @brief LLHUDEffectSpiral class implementation
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

#include "llviewerprecompiledheaders.h"

#include <utility>

#include "llhudeffectspiral.h"

#include "llimagegl.h"
#include "llmessage.h"

#include "llagent.h"
#include "lldrawable.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llhudmanager.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerpartsim.h"
#include "llviewerpartsource.h"
#include "llviewertexturelist.h"
#include "llvoavatarself.h"
#include "llworld.h"

LLHUDEffectSpiral::LLHUDEffectSpiral(U8 type)
:	LLHUDEffect(type),
	mKillTime(10.f),
	mVMag(1.f),
	mVOffset(0.f),
	mInitialRadius(1.f),
	mFinalRadius(1.f),
	mSpinRate(10.f),
	mFlickerRate(50.f),
	mScaleBase(0.1f),
	mScaleVar(0.f)
{
	mFadeInterp.setStartTime(0.f);
	mFadeInterp.setEndTime(mKillTime);
	mFadeInterp.setStartVal(1.f);
	mFadeInterp.setEndVal(1.f);
}

void LLHUDEffectSpiral::markDead()
{
	if (mPartSourcep)
	{
		mPartSourcep->setDead();
		mPartSourcep = NULL;
	}
	LLHUDEffect::markDead();
}

void LLHUDEffectSpiral::packData(LLMessageSystem* mesgsys)
{
	LLHUDEffect::packData(mesgsys);

	U8 packed_data[56];
	memset(packed_data, 0, 56);

	if (mSourceObject)
	{
		htonmemcpy(packed_data, mSourceObject->mID.mData, MVT_LLUUID, 16);
	}
	if (mTargetObject)
	{
		htonmemcpy(packed_data + 16, mTargetObject->mID.mData, MVT_LLUUID, 16);
	}
	if (!mPositionGlobal.isExactlyZero())
	{
		htonmemcpy(packed_data + 32, mPositionGlobal.mdV, MVT_LLVector3d, 24);
	}
	mesgsys->addBinaryDataFast(_PREHASH_TypeData, packed_data, 56);
}

void LLHUDEffectSpiral::unpackData(LLMessageSystem* mesgsys, S32 blocknum)
{
	constexpr S32 EFFECT_SIZE = 56;
	U8 packed_data[EFFECT_SIZE];

	LLHUDEffect::unpackData(mesgsys, blocknum);
	LLUUID object_id, target_object_id;
	S32 size = mesgsys->getSizeFast(_PREHASH_Effect, blocknum,
									_PREHASH_TypeData);
	if (size != EFFECT_SIZE)
	{
		llwarns << "Spiral effect with bad size " << size << llendl;
		return;
	}
	mesgsys->getBinaryDataFast(_PREHASH_Effect, _PREHASH_TypeData,
							   packed_data, EFFECT_SIZE, blocknum,
							   EFFECT_SIZE);

	htonmemcpy(object_id.mData, packed_data, MVT_LLUUID, 16);
	htonmemcpy(target_object_id.mData, packed_data + 16, MVT_LLUUID, 16);
	htonmemcpy(mPositionGlobal.mdV, packed_data + 32, MVT_LLVector3d, 24);

	if (object_id.isNull())
	{
		setSourceObject(NULL);
	}
	else
	{
		LLViewerObject* objp = gObjectList.findObject(object_id);
		if (objp)
		{
			setSourceObject(objp);
		}
		else
		{
			// We do not have this object, kill this effect
			markDead();
			return;
		}
	}

	if (target_object_id.isNull())
	{
		setTargetObject(NULL);
	}
	else
	{
		LLViewerObject* objp = gObjectList.findObject(target_object_id);
		if (objp)
		{
			setTargetObject(objp);
		}
		else
		{
			// We do not have this object, kill this effect
			markDead();
			return;
		}
	}

	triggerLocal();
}

void LLHUDEffectSpiral::triggerLocal()
{
//MK
	if (gRLenabled && gRLInterface.mVisionRestricted &&
		gRLInterface.mCamDistDrawAlphaMax >= 0.25f)
	{
		return;
	}
//mk

	mKillTime = mTimer.getElapsedTimeF32() + mDuration;

	static LLCachedControl<bool> show_beam(gSavedSettings,
										   "ShowSelectionBeam");
	LLColor4 color;
	color.set(mColor);

	if (!mPartSourcep)
	{
		if (mTargetObject.notNull() && mSourceObject.notNull())
		{
			if (show_beam)
			{
				mPartSourcep = new LLViewerPartSourceBeam;
				LLViewerPartSourceBeam* psb =
					(LLViewerPartSourceBeam*)mPartSourcep.get();
				psb->setColor(color);
				psb->setSourceObject(mSourceObject);
				psb->setTargetObject(mTargetObject);
				psb->setOwnerUUID(gAgentID);
				gViewerPartSim.addPartSource(mPartSourcep);
			}
		}
		else if (mSourceObject.notNull() && !mPositionGlobal.isExactlyZero())
		{
			if (show_beam)
			{
				mPartSourcep = new LLViewerPartSourceBeam;
				LLViewerPartSourceBeam* psb =
					(LLViewerPartSourceBeam*)mPartSourcep.get();
				psb->setSourceObject(mSourceObject);
				psb->setTargetObject(NULL);
				psb->setColor(color);
				psb->mLKGTargetPosGlobal = mPositionGlobal;
				psb->setOwnerUUID(gAgentID);
				gViewerPartSim.addPartSource(mPartSourcep);
			}
		}
		else
		{
			LLVector3 pos;
			if (mSourceObject)
			{
				pos = mSourceObject->getPositionAgent();
			}
			else
			{
				pos = gAgent.getPosAgentFromGlobal(mPositionGlobal);
			}
			mPartSourcep = new LLViewerPartSourceSpiral(pos);
			LLViewerPartSourceSpiral* pss =
				(LLViewerPartSourceSpiral*)mPartSourcep.get();
			if (mSourceObject.notNull())
			{
				pss->setSourceObject(mSourceObject);
			}
			pss->setColor(color);
			pss->setOwnerUUID(gAgentID);
			gViewerPartSim.addPartSource(mPartSourcep);
		}
	}
	else if (mPartSourcep->getType() == LLViewerPartSource::LL_PART_SOURCE_BEAM)
	{
		LLViewerPartSourceBeam* psb =
			(LLViewerPartSourceBeam*)mPartSourcep.get();
		psb->setSourceObject(mSourceObject);
		psb->setTargetObject(mTargetObject);
		psb->setColor(color);
		if (mTargetObject.isNull())
		{
			psb->mLKGTargetPosGlobal = mPositionGlobal;
		}
	}
	else
	{
		LLViewerPartSourceSpiral* pss =
			(LLViewerPartSourceSpiral*)mPartSourcep.get();
		pss->setSourceObject(mSourceObject);
	}
}

void LLHUDEffectSpiral::setTargetObject(LLViewerObject* objp)
{
	if (objp == mTargetObject)
	{
		return;
	}

	mTargetObject = objp;
}

void LLHUDEffectSpiral::update()
{
	F32 time = mTimer.getElapsedTimeF32();
	static LLCachedControl<bool> show_selection_beam(gSavedSettings,
													 "ShowSelectionBeam");
	if (mKillTime < time ||
		(mSourceObject.notNull() && mSourceObject->isDead()) ||
	    (mTargetObject.notNull() && mTargetObject->isDead()) ||
	    (mPartSourcep.notNull() && !show_selection_beam))
	{
		markDead();
	}
}

//static
void LLHUDEffectSpiral::agentBeamToObject(LLViewerObject* objectp)
{
	if (!isAgentAvatarValid() || !objectp) return;

	LLHUDEffectSpiral* self =
		(LLHUDEffectSpiral*)LLHUDManager::createEffect(LL_HUD_EFFECT_BEAM);
	self->setSourceObject(gAgentAvatarp);
	self->setTargetObject(objectp);
	self->setDuration(LL_HUD_DUR_SHORT);
	self->setColor(LLColor4U(gAgent.getEffectColor()));
}

//static
void LLHUDEffectSpiral::agentBeamToPosition(const LLVector3d& pos)
{
	if (!isAgentAvatarValid()) return;

	LLHUDEffectSpiral* self =
		(LLHUDEffectSpiral*)LLHUDManager::createEffect(LL_HUD_EFFECT_BEAM);
	self->setSourceObject(gAgentAvatarp);
	self->setPositionGlobal(pos);
	self->setDuration(LL_HUD_DUR_SHORT);
	self->setColor(LLColor4U(gAgent.getEffectColor()));
}

//static
void LLHUDEffectSpiral::swirlAtPosition(const LLVector3d& pos, F32 duration,
										bool send_now)
{
	LLHUDEffectSpiral* self =
		(LLHUDEffectSpiral*)LLHUDManager::createEffect(LL_HUD_EFFECT_POINT);
	self->setPositionGlobal(pos);
	self->setColor(LLColor4U(gAgent.getEffectColor()));
	if (duration > 0.f)
	{
		self->setDuration(duration);
	}
	if (send_now)
	{
		LLHUDManager::sendEffects();
	}
	if (duration == 0.f)
	{
		self->markDead();	// Remove it.
	}
}

//static
void LLHUDEffectSpiral::sphereAtPosition(const LLVector3d& pos)
{
	LLHUDEffectSpiral* self =
		(LLHUDEffectSpiral*)LLHUDManager::createEffect(LL_HUD_EFFECT_SPHERE);
	self->setPositionGlobal(pos);
	self->setColor(LLColor4U(gAgent.getEffectColor()));
	self->setDuration(0.25f);
}
