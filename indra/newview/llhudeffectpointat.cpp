/**
 * @file llhudeffectpointat.cpp
 * @brief LLHUDEffectPointAt class implementation
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

#include "llhudeffectpointat.h"

#include "llanimationstates.h"
#include "llgl.h"
#include "llrender.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawable.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewerobjectlist.h"
#include "llvoavatar.h"
#include "llmessage.h"

// packet layout
constexpr S32 SOURCE_AVATAR = 0;
constexpr S32 TARGET_OBJECT = 16;
constexpr S32 TARGET_POS = 32;
constexpr S32 POINTAT_TYPE = 56;
constexpr S32 PKT_SIZE = 57;

// Throttle
constexpr F32 DELAY_BETWEEN_SENDS = 0.25f; // seconds

constexpr F32 MIN_DELTAPOS_FOR_UPDATE = 0.05f;

// timeouts
// can't use actual F32_MAX, because we add this to the current frametime
constexpr F32 MAX_TIMEOUT = F32_MAX * 0.25f;

static const F32 POINTAT_TIMEOUTS[POINTAT_NUM_TARGETS] =
{
	MAX_TIMEOUT, //POINTAT_TARGET_NONE
	MAX_TIMEOUT, //POINTAT_TARGET_SELECT
	MAX_TIMEOUT, //POINTAT_TARGET_GRAB
	0.f, //POINTAT_TARGET_CLEAR
};

static const S32 POINTAT_PRIORITIES[POINTAT_NUM_TARGETS] =
{
	0, //POINTAT_TARGET_NONE
	1, //POINTAT_TARGET_SELECT
	2, //POINTAT_TARGET_GRAB
	3, //POINTAT_TARGET_CLEAR
};

// static
bool LLHUDEffectPointAt::sDebugPointAt = false;

LLHUDEffectPointAt::LLHUDEffectPointAt(U8 type)
:	LLHUDEffect(type),
	mKillTime(0.f),
	mLastSendTime(0.f)
{
	clearPointAtTarget();
}

void LLHUDEffectPointAt::packData(LLMessageSystem* mesgsys)
{
	// Pack the default data
	LLHUDEffect::packData(mesgsys);

	// Pack the type-specific data which uses a fun packed binary format.
	U8 packed_data[PKT_SIZE];
	memset(packed_data, 0, PKT_SIZE);
	if (mSourceObject)
	{
		htonmemcpy(&(packed_data[SOURCE_AVATAR]), mSourceObject->mID.mData,
				   MVT_LLUUID, 16);
	}
	else
	{
		htonmemcpy(&(packed_data[SOURCE_AVATAR]), LLUUID::null.mData,
				   MVT_LLUUID, 16);
	}

	// pack both target object and position
	// position interpreted as offset if target object is non-null
	if (mTargetObject)
	{
		htonmemcpy(&(packed_data[TARGET_OBJECT]), mTargetObject->mID.mData, MVT_LLUUID, 16);
	}
	else
	{
		htonmemcpy(&(packed_data[TARGET_OBJECT]), LLUUID::null.mData, MVT_LLUUID, 16);
	}

	htonmemcpy(&(packed_data[TARGET_POS]), mTargetOffsetGlobal.mdV, MVT_LLVector3d, 24);

	U8 pointAtTypePacked = (U8)mTargetType;
	htonmemcpy(&(packed_data[POINTAT_TYPE]), &pointAtTypePacked, MVT_U8, 1);

	mesgsys->addBinaryDataFast(_PREHASH_TypeData, packed_data, PKT_SIZE);

	mLastSendTime = mTimer.getElapsedTimeF32();
}

void LLHUDEffectPointAt::unpackData(LLMessageSystem* mesgsys, S32 blocknum)
{
	LLUUID data_id;
	mesgsys->getUUIDFast(_PREHASH_Effect, _PREHASH_ID, data_id, blocknum);
	// Ignore messages from ourselves
	if (gAgent.mPointAt.notNull() && data_id == gAgent.mPointAt->getID())
	{
		return;
	}

	LLHUDEffect::unpackData(mesgsys, blocknum);
	S32 size = mesgsys->getSizeFast(_PREHASH_Effect, blocknum,
									_PREHASH_TypeData);
	if (size != PKT_SIZE)
	{
		llwarns << "PointAt effect with bad size: " << size << " - skipped."
				<< llendl;
		return;
	}

	U8 packed_data[PKT_SIZE];
	mesgsys->getBinaryDataFast(_PREHASH_Effect, _PREHASH_TypeData, packed_data,
							   PKT_SIZE, blocknum);

	LLUUID source_id;
	htonmemcpy(source_id.mData, &(packed_data[SOURCE_AVATAR]), MVT_LLUUID, 16);

	LLVOAvatar* avatarp = gObjectList.findAvatar(source_id);
	if (avatarp)
	{
		setSourceObject(avatarp);
	}
	else
	{
#if 0
		llwarns << "Could not find source avatar for pointat effect" << llendl;
#endif
		return;
	}

	LLUUID target_id;
	htonmemcpy(target_id.mData, &(packed_data[TARGET_OBJECT]), MVT_LLUUID, 16);

	LLViewerObject* objp = gObjectList.findObject(target_id);

	LLVector3d new_target;
	htonmemcpy(new_target.mdV, &(packed_data[TARGET_POS]), MVT_LLVector3d, 24);

	if (objp)
	{
		setTargetObjectAndOffset(objp, new_target);
	}
	else if (target_id.isNull())
	{
		setTargetPosGlobal(new_target);
	}

	U8 unpacked_type = 0;
	htonmemcpy(&unpacked_type, &(packed_data[POINTAT_TYPE]), MVT_U8, 1);
	mTargetType = (EPointAtType)unpacked_type;

#if 0
	mKillTime = mTimer.getElapsedTimeF32() + mDuration;
#endif
}

void LLHUDEffectPointAt::setTargetObjectAndOffset(LLViewerObject* objp,
												  const LLVector3d& offset)
{
	mTargetObject = objp;
	mTargetOffsetGlobal = offset;
}

void LLHUDEffectPointAt::setTargetPosGlobal(const LLVector3d& target_pos_global)
{
	mTargetObject = NULL;
	mTargetOffsetGlobal = target_pos_global;
}

bool LLHUDEffectPointAt::setPointAt(EPointAtType target_type,
									LLViewerObject* object, LLVector3 position)
{
	if (!mSourceObject)
	{
		return false;
	}

	if (target_type >= POINTAT_NUM_TARGETS)
	{
		llwarns << "Bad target_type " << (int)target_type << " - ignoring." << llendl;
		return false;
	}

	// Must be same or higher priority than existing effect
	if (POINTAT_PRIORITIES[target_type] < POINTAT_PRIORITIES[mTargetType])
	{
		return false;
	}

	F32 current_time  = mTimer.getElapsedTimeF32();

	// If type of point-at behavior or target object has changed or moved
	if (target_type != mTargetType || object != mTargetObject ||
		(current_time - mLastSendTime > DELAY_BETWEEN_SENDS &&
		 dist_vec(position, mLastSentOffsetGlobal) > MIN_DELTAPOS_FOR_UPDATE))
	{
		mLastSentOffsetGlobal = position;
		setDuration(POINTAT_TIMEOUTS[target_type]);
		setNeedsSendToSim(true);
	}

	if (target_type == POINTAT_TARGET_CLEAR)
	{
		clearPointAtTarget();
	}
	else
	{
		mTargetType = target_type;
		mTargetObject = object;
		if (object)
		{
			mTargetOffsetGlobal.set(position);
		}
		else
		{
			mTargetOffsetGlobal = gAgent.getPosGlobalFromAgent(position);
		}

		mKillTime = mTimer.getElapsedTimeF32() + mDuration;
	}

	return true;
}

void LLHUDEffectPointAt::clearPointAtTarget()
{
	mTargetObject = NULL;
	mTargetOffsetGlobal.clear();
	mTargetType = POINTAT_TARGET_NONE;
}

void LLHUDEffectPointAt::markDead()
{
	if (mSourceObject.notNull() && mSourceObject->isAvatar())
	{
		LLVOAvatar* av = (LLVOAvatar*)mSourceObject.get();
		av->removeAnimationData("PointAtPoint");
	}

	clearPointAtTarget();
	LLHUDEffect::markDead();
}

void LLHUDEffectPointAt::setSourceObject(LLViewerObject* objectp)
{
	// Restrict source objects to avatars
	if (objectp && objectp->isAvatar())
	{
		LLHUDEffect::setSourceObject(objectp);
	}
}

void LLHUDEffectPointAt::render()
{
//MK
	if (gRLenabled && gRLInterface.mVisionRestricted)
	{
		return;
	}
//mk

	if (sDebugPointAt && mTargetType != POINTAT_TARGET_NONE)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		LLVector3 target = mTargetPos + mSourceObject->getRenderPosition();
		gGL.pushMatrix();
		gGL.translatef(target.mV[VX], target.mV[VY], target.mV[VZ]);
		gGL.scalef(0.3f, 0.3f, 0.3f);
		gGL.begin(LLRender::LINES);
		{
			gGL.color3f(1.f, 0.f, 0.f);
			gGL.vertex3f(-1.f, 0.f, 0.f);
			gGL.vertex3f(1.f, 0.f, 0.f);

			gGL.vertex3f(0.f, -1.f, 0.f);
			gGL.vertex3f(0.f, 1.f, 0.f);

			gGL.vertex3f(0.f, 0.f, -1.f);
			gGL.vertex3f(0.f, 0.f, 1.f);
		}
		gGL.end();
		gGL.popMatrix();
	}
}

// Called every frame from LLHUDManager::updateEffects()
//virtual
void LLHUDEffectPointAt::update()
{
	// If the target object is dead, set the target object to NULL
	if (mTargetObject.notNull() && mTargetObject->isDead())
	{
		clearPointAtTarget();
	}

	if (mSourceObject.isNull() || mSourceObject->isDead())
	{
		markDead();
		return;
	}

	F32 time = mTimer.getElapsedTimeF32();

	// Clear out the effect if time is up
	if (mKillTime != 0.f && time > mKillTime)
	{
		mTargetType = POINTAT_TARGET_NONE;
	}

	if (mSourceObject->isAvatar())
	{
		LLVOAvatar* av = (LLVOAvatar*)mSourceObject.get();
		if (mTargetType == POINTAT_TARGET_NONE)
		{
			av->removeAnimationData("PointAtPoint");
		}
		else if (calcTargetPosition())
		{
			av->startMotion(ANIM_AGENT_EDITING);
		}
	}
}

// Returns whether we successfully calculated a finite target position.
bool LLHUDEffectPointAt::calcTargetPosition()
{
	LLViewerObject* objectp = mTargetObject.get();

	LLVector3 local_offset;
	if (objectp)
	{
		local_offset.set(mTargetOffsetGlobal);
	}
	else
	{
		local_offset = gAgent.getPosAgentFromGlobal(mTargetOffsetGlobal);
	}

	if (objectp && objectp->mDrawable.notNull())
	{
		LLQuaternion rot;
		if (objectp->isAvatar())
		{
			LLVOAvatar* avatarp = (LLVOAvatar*)objectp;
			mTargetPos = avatarp->mHeadp->getWorldPosition();
			rot = avatarp->mPelvisp->getWorldRotation();
		}
		else if (objectp->mDrawable->getGeneration() == -1)
		{
			mTargetPos = objectp->getPositionAgent();
			rot = objectp->getWorldRotation();
		}
		else
		{
			mTargetPos = objectp->getRenderPosition();
			rot = objectp->getRenderRotation();
		}

		mTargetPos += local_offset * rot;
	}
	else
	{
		mTargetPos = local_offset;
	}

	mTargetPos -= mSourceObject->getRenderPosition();

	if (!llfinite(mTargetPos.lengthSquared()))
	{
		return false;
	}

	if (mSourceObject->isAvatar())
	{
		LLVOAvatar* av = (LLVOAvatar*)mSourceObject.get();
		av->setAnimationData("PointAtPoint", (void*)&mTargetPos);
	}

	return true;
}

const LLVector3d LLHUDEffectPointAt::getPointAtPosGlobal()
{
	LLVector3d global_pos(mTargetPos);
	if (mSourceObject.notNull())
	{
		global_pos += mSourceObject->getPositionGlobal();
	}
	return global_pos;
}
