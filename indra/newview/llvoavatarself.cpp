/**
 * @file llvoavatarself.cpp
 * @brief Implementation of LLVOAvatarSelf class
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

#include "llviewerprecompiledheaders.h"

#include "llvoavatarself.h"

#include "imageids.h"
#include "llcallbacklist.h"
#include "llcorehttputil.h"
#include "lleconomy.h"
#include "lltrans.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llapp.h"
#include "llappearancemgr.h"
#include "llappviewer.h"			// For gFrameTimeSeconds
#include "lldrawpoolalpha.h"
#include "lleconomy.h"
#include "llfasttimer.h"
#include "llhudeffectspiral.h"
#include "llhudmanager.h"
#include "llinventorybridge.h"
#include "llmeshrepository.h"
#include "hbobjectbackup.h"			// For HBObjectBackup::validateAssetPerms()
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltoolgrab.h"				// For needsRenderBeam
#include "lltoolmgr.h"				// For needsRenderBeam
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerstats.h"
#include "llviewerregion.h"
#include "llviewertexlayer.h"
#include "llviewerwearable.h"
#include "llvisualparamhint.h"
#include "llvoavatar.h"
#include "llvovolume.h"

constexpr F32 Z_OFFSET_THROTTLE_DELAY = 1.f;	// In seconds

// Globals
LLPointer<LLVOAvatarSelf> gAgentAvatarp = NULL;
LLFrameTimer gAttachmentsTimer;
bool gAttachmentsListDirty = true;
LLPieMenu* gAttachBodyPartPieMenusp[8];
LLPieMenu* gDetachBodyPartPieMenusp[8];
U32 gMaxSelfAttachments = 0;

using namespace LLAvatarAppearanceDefines;

///////////////////////////////////////////////////////////////////////////////
// Support classes
///////////////////////////////////////////////////////////////////////////////

struct LocalTextureData
{
	LocalTextureData()
	:	mIsBakedReady(false),
		mDiscard(MAX_DISCARD_LEVEL + 1),
		mImage(NULL),
		mWearableID(IMG_DEFAULT_AVATAR),
		mTexEntry(NULL)
	{
	}

	LLPointer<LLViewerFetchedTexture>	mImage;
	bool								mIsBakedReady;
	S32									mDiscard;
	// UUID of the wearable that this texture belongs to, not of the image
	// itself
	LLUUID								mWearableID;
	LLTextureEntry*						mTexEntry;
};

///////////////////////////////////////////////////////////////////////////////
// LLVOAvatarSelf class
///////////////////////////////////////////////////////////////////////////////

LLVOAvatarSelf::LLVOAvatarSelf(const LLUUID& id, LLViewerRegion* regionp)
:	LLVOAvatar(id, regionp),
	mInitialBakesLoaded(false),
	mScreenp(NULL),
	mLastRegionHandle(0),
	mRegionCrossingCount(0),
	// Value outside legal range, so will always be a mismatch the first time
	// through.
	mLastHoverOffsetSent(LLVector3(0.f, 0.f, -999.f)),
	mAttachmentUpdateEnabled(true),
	mAttachmentUpdateExpiry(0.f)
{
	LL_DEBUGS("Avatar") << "Marking avatar as self " << id << LL_ENDL;

	gAgent.setAvatarObject(this);
	gAgentWearables.setAvatarObject(this);
	gAttachmentsTimer.reset();
#if 0
	mMotionController.mIsSelf = true;
#endif

	mOffsetUpdateDelay.stop();

	gMaxSelfAttachments =
		(U32)llmax(MAX_AGENT_ATTACHMENTS,
				   LLEconomy::getInstance()->getAttachmentLimit());
	mAttachedObjectsVector.reserve(gMaxSelfAttachments);
	llinfos << "Maximum number of attachments set to: " << gMaxSelfAttachments
			<< llendl;

	setAttachmentUpdatePeriod(DEFAULT_ATTACHMENT_UPDATE_PERIOD);
}

void LLVOAvatarSelf::initInstance()
{
	bool status = true;
	// Creates hud joint(mScreen) among other things:
	status &= loadAvatarSelf();

	// Adds attachment points to mScreen among other things:
	LLVOAvatar::initInstance();

	for (U32 i = 0; i < BAKED_NUM_INDICES; ++i)
	{
		mInitialBakeIDs[i].setNull();
	}

	status &= buildMenus();
	if (!status)
	{
		llerrs << "Unable to load user's avatar" << llendl;
	}

	mTeleportFinishedSlot =
		gViewerParcelMgr.setTPFinishedCallback(boost::bind(&LLVOAvatarSelf::handleTeleportFinished,
														   this));

	scheduleHoverUpdate();
}

//static
bool LLVOAvatarSelf::useAvatarHoverHeight()
{
	LLViewerRegion* region = NULL;
	if (isAgentAvatarValid())
	{
		region = gAgentAvatarp->getRegion();
	}
	return region && region->avatarHoverHeightEnabled();
}

void LLVOAvatarSelf::scheduleHoverUpdate()
{
	mOffsetUpdateDelay.start();
	mOffsetUpdateDelay.setTimerExpirySec(Z_OFFSET_THROTTLE_DELAY);
}

void LLVOAvatarSelf::setHoverIfRegionEnabled()
{
	LLViewerRegion* region = getRegion();
	if (!region || region != gAgent.getRegion())
	{
		mLastHoverOffsetSent = LLVector3(0.f, 0.f, -999.f);
		scheduleHoverUpdate();	// retry later...
		return;
	}

	if (region->getFeaturesReceived())
	{
		if (useAvatarHoverHeight())
		{
			// Transform avatar Z size offset into altitude (Z) offset
			F32 ptf = getPelvisToFoot();
			F32 ahh_dividor = ptf > 0.f ? mBodySize.mV[VZ] / ptf : 1.f;
			F32 hover =
				llclamp(gSavedSettings.getF32("AvatarOffsetZ") / ahh_dividor,
						MIN_HOVER_Z, MAX_HOVER_Z);
			setHoverOffset(LLVector3(0.f, 0.f, hover));
			llinfos << "Set hover height for self from debug setting: "
					<< hover << "m" << llendl;
		}
		else
		{
			setHoverOffset(LLVector3::zero);
			if (canUseServerBaking())
			{
				llwarns_once << "Cannot set Z offset by lack of capability"
							 << llendl;
			}
			else
			{
				llinfos_once << "Avatar Hover Offset disabled or not supported. Falling back to legacy method."
							 << llendl;
				gAgent.sendAgentSetAppearance();
			}
		}
		return;
	}

	llinfos << "Region or simulator features not yet known, delaying change to hover for self"
			<< llendl;
	if (region)
	{
		region->setFeaturesReceivedCB(boost::bind(&LLVOAvatarSelf::onSimulatorFeaturesReceived,
												  this, _1));
	}
}

//virtual
void LLVOAvatarSelf::markDead()
{
	mBeam = NULL;
	LLVOAvatar::markDead();
}

//virtual
bool LLVOAvatarSelf::loadAvatar()
{
	bool success = LLVOAvatar::loadAvatar();

	// Set all parameters stored directly in the avatar to have the isSelfParam
	// to be true: this is used to prevent them from being animated or trigger
	// accidental rebakes when we copy params from the wearable to the base
	// avatar.
	for (LLViewerVisualParam* param =
			(LLViewerVisualParam*)getFirstVisualParam();
		 param; param = (LLViewerVisualParam*)getNextVisualParam())
	{
		if (param->getWearableType() != LLWearableType::WT_INVALID)
		{
			param->setIsDummy(true);
		}
	}

	return success;
}

bool LLVOAvatarSelf::loadAvatarSelf()
{
	// avatar_skeleton.xml
	if (!buildSkeletonSelf(sAvatarSkeletonInfo))
	{
		llwarns << "Avatar file: buildSkeleton() failed" << llendl;
		return false;
	}

	return true;
}

bool LLVOAvatarSelf::buildSkeletonSelf(const LLAvatarSkeletonInfo* info)
{
	// Add special-purpose "screen" joint
	mScreenp = new LLViewerJoint("mScreen", NULL);
	// For now, put screen at origin, as it is only used during special HUD
	// rendering mode
	F32 aspect = gViewerCamera.getAspect();
	LLVector3 scale(1.f, aspect, 1.f);
	mScreenp->setScale(scale);
	mScreenp->setWorldPosition(LLVector3::zero);
	return true;
}

bool LLVOAvatarSelf::buildMenus()
{
	//-------------------------------------------------------------------------
	// Build the attach and detach menus
	//-------------------------------------------------------------------------

	// *TODO: Translate
	gAttachBodyPartPieMenusp[0] = new LLPieMenu("More limbs>");
	gAttachBodyPartPieMenusp[1] = new LLPieMenu("Right Arm >");
	gAttachBodyPartPieMenusp[2] = new LLPieMenu("Head >");
	gAttachBodyPartPieMenusp[3] = new LLPieMenu("Left Arm >");
	gAttachBodyPartPieMenusp[4] = new LLPieMenu("Head extras>");
	gAttachBodyPartPieMenusp[5] = new LLPieMenu("Left Leg >");
	gAttachBodyPartPieMenusp[6] = new LLPieMenu("Torso >");
	gAttachBodyPartPieMenusp[7] = new LLPieMenu("Right Leg >");

	gDetachBodyPartPieMenusp[0] = new LLPieMenu("More limbs>");
	gDetachBodyPartPieMenusp[1] = new LLPieMenu("Right Arm >");
	gDetachBodyPartPieMenusp[2] = new LLPieMenu("Head >");
	gDetachBodyPartPieMenusp[3] = new LLPieMenu("Left Arm >");
	gDetachBodyPartPieMenusp[4] = new LLPieMenu("Head extras>");
	gDetachBodyPartPieMenusp[5] = new LLPieMenu("Left Leg >");
	gDetachBodyPartPieMenusp[6] = new LLPieMenu("Torso >");
	gDetachBodyPartPieMenusp[7] = new LLPieMenu("Right Leg >");

	LLMenuItemCallGL* item;
	for (S32 i = 0; i < 8; ++i)
	{
		if (gAttachBodyPartPieMenusp[i])
		{
			gAttachPieMenup->appendPieMenu(gAttachBodyPartPieMenusp[i]);
		}
		else
		{
			bool attachment_found = false;
			for (attachment_map_t::iterator iter = mAttachmentPoints.begin();
				 iter != mAttachmentPoints.end(); )
			{
				attachment_map_t::iterator curiter = iter++;
				LLViewerJointAttachment* attachment = curiter->second;
				if (attachment && attachment->getGroup() == i)
				{
					item = new LLMenuItemCallGL(LLTrans::getString(attachment->getName()),
												NULL,
												object_selected_and_point_valid);
					item->addListener(gMenuHolderp->getListenerByName("Object.AttachToAvatar"),
									  "on_click", curiter->first);
					gAttachPieMenup->append(item);
					attachment_found = true;

					break;
				}
			}

			if (!attachment_found)
			{
				gAttachPieMenup->appendSeparator();
			}
		}

		if (gDetachBodyPartPieMenusp[i])
		{
			gDetachPieMenup->appendPieMenu(gDetachBodyPartPieMenusp[i]);
		}
		else
		{
			bool attachment_found = false;
			for (attachment_map_t::iterator iter = mAttachmentPoints.begin();
				 iter != mAttachmentPoints.end(); )
			{
				attachment_map_t::iterator curiter = iter++;
				LLViewerJointAttachment* attachment = curiter->second;
				if (attachment && attachment->getGroup() == i)
				{
					item = new LLMenuItemCallGL(LLTrans::getString(attachment->getName()),
												&handle_detach_from_avatar,
												object_attached,
												attachment);
					gDetachPieMenup->append(item);

					attachment_found = true;
					break;
				}
			}

			if (!attachment_found)
			{
				gDetachPieMenup->appendSeparator();
			}
		}
	}

	// Add screen attachments
	for (attachment_map_t::iterator iter = mAttachmentPoints.begin();
		 iter != mAttachmentPoints.end(); )
	{
		attachment_map_t::iterator curiter = iter++;
		LLViewerJointAttachment* attachment = curiter->second;
		if (attachment && attachment->getGroup() == 8)
		{
			std::string pt_name = LLTrans::getString(attachment->getName());
			item = new LLMenuItemCallGL(pt_name, NULL,
										object_selected_and_point_valid);
			item->addListener(gMenuHolderp->getListenerByName("Object.AttachToAvatar"),
							  "on_click", curiter->first);
			gAttachScreenPieMenup->append(item);

			item = new LLMenuItemCallGL(pt_name, &handle_detach_from_avatar,
										object_attached, attachment);
			gDetachScreenPieMenup->append(item);
		}
	}

	for (S32 pass = 0; pass < 2; ++pass)
	{
		for (attachment_map_t::iterator iter = mAttachmentPoints.begin();
			 iter != mAttachmentPoints.end(); )
		{
			attachment_map_t::iterator curiter = iter++;
			LLViewerJointAttachment* attachment = curiter->second;
			if (attachment && attachment->getIsHUDAttachment() != (pass == 1))
			{
				continue;
			}
			std::string pt_name = LLTrans::getString(attachment->getName());
			item = new LLMenuItemCallGL(pt_name, NULL,
										&object_selected_and_point_valid,
										&attach_label, attachment);
			item->addListener(gMenuHolderp->getListenerByName("Object.AttachToAvatar"),
							  "on_click", curiter->first);
			gAttachSubMenup->append(item);

			item = new LLMenuItemCallGL(pt_name,
										&handle_detach_from_avatar,
										object_attached, &detach_label,
										attachment);
			gDetachSubMenup->append(item);
		}
		if (pass == 0)
		{
			// put separator between non-hud and hud attachments
			gAttachSubMenup->appendSeparator();
			gDetachSubMenup->appendSeparator();
		}
	}

	for (S32 group = 0; group < 8; ++group)
	{
		// skip over groups that don't have sub menus
		if (!gAttachBodyPartPieMenusp[group] ||
			!gDetachBodyPartPieMenusp[group])
		{
			continue;
		}

		std::multimap<S32, S32> attachment_pie_menu_map;

		// Gather up all attachment points assigned to this group, and throw
		// into map sorted by pie slice number
		for (attachment_map_t::iterator iter = mAttachmentPoints.begin();
			 iter != mAttachmentPoints.end(); )
		{
			attachment_map_t::iterator curiter = iter++;
			LLViewerJointAttachment* attachment = curiter->second;
			if (attachment && attachment->getGroup() == group)
			{
				// Use multimap to provide a partial order off of the pie slice
				// key
				S32 pie_index = attachment->getPieSlice();
				attachment_pie_menu_map.emplace(pie_index, curiter->first);
			}
		}

		// Add in requested order to pie menu, inserting separators as
		// necessary
		S32 cur_pie_slice = 0;
		for (std::multimap<S32, S32>::iterator attach_it =
				attachment_pie_menu_map.begin();
			 attach_it != attachment_pie_menu_map.end(); ++attach_it)
		{
			S32 requested_pie_slice = attach_it->first;
			S32 attach_index = attach_it->second;
			while (cur_pie_slice < requested_pie_slice)
			{
				gAttachBodyPartPieMenusp[group]->appendSeparator();
				gDetachBodyPartPieMenusp[group]->appendSeparator();
				++cur_pie_slice;
			}

			LLViewerJointAttachment* attachment =
				get_ptr_in_map(mAttachmentPoints, attach_index);
			if (attachment)
			{
				std::string pt_name =
					LLTrans::getString(attachment->getName());
				item = new LLMenuItemCallGL(pt_name, NULL,
											object_selected_and_point_valid);
				gAttachBodyPartPieMenusp[group]->append(item);
				item->addListener(gMenuHolderp->getListenerByName("Object.AttachToAvatar"),
								  "on_click", attach_index);

				item = new LLMenuItemCallGL(pt_name,
											&handle_detach_from_avatar,
											object_attached, attachment);
				gDetachBodyPartPieMenusp[group]->append(item);
				++cur_pie_slice;
			}
		}
	}

	return true;
}

LLVOAvatarSelf::~LLVOAvatarSelf()
{
	markDead();
 	delete mScreenp;
 	mScreenp = NULL;
	mRegionp = NULL;
}

//virtual
bool LLVOAvatarSelf::updateCharacter()
{
	// Update screen joint size
	if (mScreenp)
	{
		F32 aspect = gViewerCamera.getAspect();
		LLVector3 scale(1.f, aspect, 1.f);
		mScreenp->setScale(scale);
		mScreenp->updateWorldMatrixChildren();
		resetHUDAttachments();
	}

	return LLVOAvatar::updateCharacter();
}

//virtual
bool LLVOAvatarSelf::isValid() const
{
	return getRegion() && !isDead();
}

//virtual
void LLVOAvatarSelf::idleUpdate(F64 time)
{
	if (isValid())
	{
		LLVOAvatar::idleUpdate(time);
		{
			LL_FAST_TIMER(FTM_AVATAR_UPDATE);

			idleUpdateTractorBeam();
			gAppearanceMgr.checkOutfit();

			if (mOffsetUpdateDelay.getStarted() &&
				mOffsetUpdateDelay.hasExpired())
			{
				mOffsetUpdateDelay.stop();
				setHoverIfRegionEnabled();
			}
		}
	}
}

//virtual
LLJoint* LLVOAvatarSelf::getJoint(U32 key)
{
	LLJoint* jointp = LLVOAvatar::getJoint(key);
	if (!jointp && mScreenp)
	{
		jointp = mScreenp->findJoint(key);
		if (jointp)
		{
			mJointMap[key] = jointp;
		}
	}
	return jointp;
}

//virtual
bool LLVOAvatarSelf::setVisualParamWeight(const LLVisualParam* which_param,
										  F32 weight, bool upload_bake)
{
	if (!which_param)
	{
		return false;
	}
	LLViewerVisualParam* param =
		(LLViewerVisualParam*)LLCharacter::getVisualParam(which_param->getID());
	return setParamWeight(param, weight, upload_bake);
}

//virtual
bool LLVOAvatarSelf::setVisualParamWeight(const char* param_name, F32 weight,
										  bool upload_bake)
{
	if (!param_name)
	{
		return false;
	}
	LLViewerVisualParam* param =
		(LLViewerVisualParam*)LLCharacter::getVisualParam(param_name);
	return setParamWeight(param, weight, upload_bake);
}

//virtual
bool LLVOAvatarSelf::setVisualParamWeight(S32 index, F32 weight,
										  bool upload_bake)
{
	LLViewerVisualParam* param =
		(LLViewerVisualParam*)LLCharacter::getVisualParam(index);
	return setParamWeight(param, weight, upload_bake);
}

bool LLVOAvatarSelf::setParamWeight(const LLViewerVisualParam* param,
									F32 weight, bool upload_bake)
{
	if (!param)
	{
		return false;
	}

#if 0
	// *FIXME: kludgy way to avoid overwriting avatar state from wearables.
	if (isUsingServerBakes() && !isUsingLocalAppearance())
	{
		return false;
	}
#endif

	if (param->getCrossWearable())
	{
		LLWearableType::EType type =
			(LLWearableType::EType)param->getWearableType();
		U32 size = gAgentWearables.getWearableCount(type);
		for (U32 count = 0; count < size; ++count)
		{
			LLViewerWearable* wearable =
				gAgentWearables.getViewerWearable(type, count);
			if (wearable)
			{
				wearable->setVisualParamWeight(param->getID(), weight,
											   upload_bake);
			}
		}
	}

	return LLCharacter::setVisualParamWeight(param, weight, upload_bake);
}

void LLVOAvatarSelf::writeWearablesToAvatar()
{
	for (U32 type = 0; type < LLWearableType::WT_COUNT; ++type)
	{
		LLWearable* wearable =
			gAgentWearables.getTopWearable((LLWearableType::EType)type);
		if (wearable)
		{
			wearable->writeToAvatar(this);
		}
	}
}

//virtual
void LLVOAvatarSelf::idleUpdateAppearanceAnimation()
{
	// Animate all top-level wearable visual parameters
	gAgentWearables.animateAllWearableParams(calcMorphAmount(), false);

	// Apply wearable visual params to avatar
	writeWearablesToAvatar();

	// Allow avatar to process updates
	LLVOAvatar::idleUpdateAppearanceAnimation();
}

//virtual
void LLVOAvatarSelf::requestStopMotion(LLMotion* motion)
{
	// Only agent avatars should handle the stop motion notifications.

	// Notify agent that motion has stopped
	gAgent.requestStopMotion(motion);
}

//virtual
bool LLVOAvatarSelf::hasMotionFromSource(const LLUUID& source_id)
{
	return mAnimationSources.find(source_id) != mAnimationSources.end();
}

//virtual
void LLVOAvatarSelf::stopMotionFromSource(const LLUUID& source_id)
{
	anim_src_map_it_t it = mAnimationSources.find(source_id);
	while (it != mAnimationSources.end())
	{
		gAgent.sendAnimationRequest(it->second, ANIM_REQUEST_STOP);
		mAnimationSources.erase(it);
		// We must find() after each erase() to deal with potential
		// iterator invalidation; this also ensures that we do not go past the
		// end of this source's animations into those of another source.
		it = mAnimationSources.find(source_id);
	}

	LLViewerObject* object = gObjectList.findObject(source_id);
	if (object)
	{
		object->setFlagsWithoutUpdate(FLAGS_ANIM_SOURCE, false);
	}
}

//virtual
U32  LLVOAvatarSelf::processUpdateMessage(LLMessageSystem* mesgsys,
										  void** user_data, U32 block_num,
										  EObjectUpdateType update_type,
										  LLDataPacker* dp)
{
	U32 retval = LLVOAvatar::processUpdateMessage(mesgsys, user_data,
												  block_num, update_type, dp);

#if 1	// It is not clear this does anything useful. If we wait until an
		// appearance message has been received, we already have the texture
		// IDs. If we do not wait, we do not yet know where to look for baked
		// textures, because we have not received the appearance version data
		// from the appearance message. This looks like an old optimization
		// that is incompatible with server-side texture baking.

	// *FIXME: skipping in the case of !mFirstAppearanceMessageReceived
	// prevents us from trying to load textures before we know where they come
	// from (ie, from baking service or not); unknown impact on performance.
	if (!mInitialBakesLoaded && retval == 0x0 &&
		mFirstAppearanceMessageReceived)
	{
		// Call update textures to force the images to be created
		updateMeshTextures();

		// Unpack the texture UUIDs to the texture slots
		if (mesgsys)
		{
			retval = unpackTEMessage(mesgsys, _PREHASH_ObjectData,
									 (S32)block_num);
		}
		else
		{
			retval = false;
		}

		// Need to trigger a few operations to get the avatar to use the new
		// bakes
		for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
		{
			const ETextureIndex te = mBakedTextureDatas[i].mTextureIndex;
			LLUUID texture_id = getTEImage(te)->getID();
			setNewBakedTexture(te, texture_id);
			mInitialBakeIDs[i] = texture_id;
		}

		onFirstTEMessageReceived();

		mInitialBakesLoaded = true;
	}
#endif

	return retval;
}

void LLVOAvatarSelf::setLocalTextureTE(U8 te, LLViewerTexture* texp, U32 index)
{
	if (te >= TEX_NUM_INDICES)
	{
		llassert(false);
		return;
	}
	LLViewerTexture* te_texp = getTEImage(te);
	if (!te_texp || te_texp->getID() == texp->getID())
	{
		return;
	}
	if (isIndexBakedTexture((ETextureIndex)te))
	{
		llassert(false);
		return;
	}

	setTEImage(te, texp);
}

//virtual
void LLVOAvatarSelf::removeMissingBakedTextures()
{
	bool removed = false;
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		const S32 te = mBakedTextureDatas[i].mTextureIndex;
		const LLViewerTexture* tex = getTEImage(te);

		// Replace with default if we cannot find the asset, assuming the
		// default is actually valid (which it should be unless something is
		// seriously wrong).
		if (!tex || tex->isMissingAsset())
		{
			LLViewerTexture* imagep =
				LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR);
			if (imagep && imagep != tex)
			{
				setTEImage(te, imagep);
				removed = true;
			}
		}
	}

	if (removed)
	{
		for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
		{
			LLViewerTexLayerSet* layerset = getTexLayerSet(i);
			if (layerset)
			{
				layerset->setUpdatesEnabled(true);
				invalidateComposite(layerset, false);
			}
		}

		updateMeshTextures();	// may call back into this function

		requestLayerSetUploads();
	}
}

void LLVOAvatarSelf::onSimulatorFeaturesReceived(const LLUUID&)
{
	llinfos << "Simulator features received, setting hover based on region props"
			<< llendl;
	scheduleHoverUpdate();
}

//virtual
void LLVOAvatarSelf::updateRegion(LLViewerRegion* regionp)
{
	// Save the global position
	LLVector3d global_pos_from_old_region = getPositionGlobal();

	// Change the region
	setRegion(regionp);

	if (regionp)
	{
		// Set correct region-relative position from global coordinates
		setPositionGlobal(global_pos_from_old_region);

		// Update hover height
		scheduleHoverUpdate();
	}

	if (!regionp || regionp->getHandle() != mLastRegionHandle)
	{
		if (mLastRegionHandle != 0)
		{
			F64 delta = (F64)mRegionCrossingTimer.getElapsedTimeF32();
			F64 avg = 0.0;
			F64 max = 0.0;
			if (++mRegionCrossingCount > 1)
			{
				avg = gViewerStats.getStat(LLViewerStats::ST_CROSSING_AVG);
				max = gViewerStats.getStat(LLViewerStats::ST_CROSSING_MAX);
			}
			F64 delta_avg = (delta + avg * (mRegionCrossingCount - 1)) /
							mRegionCrossingCount;
			gViewerStats.setStat(LLViewerStats::ST_CROSSING_AVG, delta_avg);
			max = llmax(delta, max);
			gViewerStats.setStat(LLViewerStats::ST_CROSSING_MAX, max);

			// Diagnostics
			llinfos << "Region crossing took " << (F32)delta * 1000.0f
					<< " ms " << llendl;
		}
		if (regionp)
		{
			mLastRegionHandle = regionp->getHandle();
		}
	}
	mRegionCrossingTimer.reset();

#if 0	// No operation
	LLViewerObject::updateRegion(regionp);
#endif
}

//--------------------------------------------------------------------
// Draws the tractor beam when editing objects
//--------------------------------------------------------------------
//virtual
void LLVOAvatarSelf::idleUpdateTractorBeam()
{
	// This is only done for yourself (maybe it should be moved to LLAgent ?)
	if (!needsRenderBeam() || !isBuilt())
	{
		mBeam = NULL;
		return;
	}

	if (mBeam.isNull() || mBeam->isDead())
	{
		// VEFFECT: Tractor Beam
		mBeam =
			(LLHUDEffectSpiral*)LLHUDManager::createEffect(LLHUDObject::LL_HUD_EFFECT_BEAM);
		mBeam->setColor(LLColor4U(gAgent.getEffectColor()));
		mBeam->setSourceObject(this);
		mBeamTimer.reset();
	}

	LLObjectSelectionHandle selection = gSelectMgr.getSelection();

	if (gAgent.mPointAt.notNull())
	{
		// Get point from pointat effect
		mBeam->setPositionGlobal(gAgent.mPointAt->getPointAtPosGlobal());
		mBeam->triggerLocal();
	}
	else if (selection->getFirstRootObject() &&
			 selection->getSelectType() != SELECT_TYPE_HUD)
	{
		LLViewerObject* objectp = selection->getFirstRootObject();
		mBeam->setTargetObject(objectp);
	}
	else
	{
		mBeam->setTargetObject(NULL);
		LLTool* toolp = gToolMgr.getCurrentTool();
		if (toolp && toolp->isEditing())
		{
			if (toolp->getEditingObject())
			{
				mBeam->setTargetObject(toolp->getEditingObject());
			}
			else
			{
				mBeam->setPositionGlobal(toolp->getEditingPointGlobal());
			}
		}
		else
		{
			const LLPickInfo& pick = gViewerWindowp->getLastPick();
			mBeam->setPositionGlobal(pick.mPosGlobal);
		}
	}

	if (mBeamTimer.getElapsedTimeF32() > 0.25f)
	{
		mBeam->setColor(LLColor4U(gAgent.getEffectColor()));
		mBeam->setNeedsSendToSim(true);
		mBeamTimer.reset();
	}
}

//virtual
void LLVOAvatarSelf::restoreMeshData()
{
	mMeshValid = true;
	updateJointLODs();
	updateAttachmentVisibility(gAgent.getCameraMode());

	// Force mesh update as LOD might not have changed to trigger this
	gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_GEOMETRY);
}

void LLVOAvatarSelf::updateAttachmentVisibility(U32 camera_mode)
{
	for (attachment_map_t::iterator iter = mAttachmentPoints.begin(),
									end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (!attachment) continue;	// Paranoia

		if (camera_mode != CAMERA_MODE_MOUSELOOK ||
			attachment->getIsHUDAttachment())
		{
			attachment->setAttachmentVisibility(true);
		}
		else if (LLVOAvatar::sVisibleInFirstPerson &&
				 attachment->getVisibleInFirstPerson())
		{
			attachment->setAttachmentVisibility(true);
		}
		else
		{
			attachment->setAttachmentVisibility(false);
		}
	}
}

// Forces an update to any baked textures relevant to type. Will force an
// upload of the resulting bake if the second parameter is true.
void LLVOAvatarSelf::wearableUpdated(LLWearableType::EType type,
									 bool upload_result)
{
	for (LLAvatarAppearanceDictionary::BakedTextures::const_iterator
			baked_iter = gAvatarAppDictp->getBakedTextures().begin(),
			end = gAvatarAppDictp->getBakedTextures().end();
		 baked_iter != end; ++baked_iter)
	{
		const LLAvatarAppearanceDictionary::BakedEntry* baked_dictp =
			baked_iter->second;
		if (!baked_dictp)
		{
			continue;
		}
		const EBakedTextureIndex index = baked_iter->first;
		for (wearables_vec_t::const_iterator
				type_iter = baked_dictp->mWearables.begin(),
				type_end = baked_dictp->mWearables.end();
			 type_iter != type_end; ++type_iter)
		{
			const LLWearableType::EType comp_type = *type_iter;
			if (comp_type == type)
			{
				LLViewerTexLayerSet* layersetp = getLayerSet(index);
				if (layersetp)
				{
					layersetp->setUpdatesEnabled(true);
					invalidateComposite(layersetp, upload_result);
				}
				break;
			}
		}
	}

	// Physics type has no associated baked textures, but change of params
	// needs to be sent to other avatars.
	if (type == LLWearableType::WT_PHYSICS)
	{
		gAgent.sendAgentSetAppearance();
	}
}

bool LLVOAvatarSelf::isWearingAttachment(const LLUUID& inv_item_id) const
{
	const LLUUID& base_inv_id = gInventory.getLinkedItemID(inv_item_id);
	for (attachment_map_t::const_iterator iter = mAttachmentPoints.begin(),
										  end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		const LLViewerJointAttachment* attachp = iter->second;
		if (attachp && attachp->getAttachedObject(base_inv_id))
		{
			return true;
		}
	}
	return false;
}

LLViewerObject* LLVOAvatarSelf::getWornAttachment(const LLUUID& inv_item_id)
{
	const LLUUID& base_inv_id = gInventory.getLinkedItemID(inv_item_id);
	for (attachment_map_t::iterator iter = mAttachmentPoints.begin(),
									end = mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* attachp = iter->second;
		if (!attachp) continue;	// Paranoia

		LLViewerObject* objectp = attachp->getAttachedObject(base_inv_id);
 		if (objectp)
		{
			return objectp;
		}
	}
	return NULL;
}

const std::string LLVOAvatarSelf::getAttachedPointName(const LLUUID& inv_item_id,
													   bool translate) const
{
	if (inv_item_id.notNull())
	{
		const LLUUID& base_inv_id = gInventory.getLinkedItemID(inv_item_id);
		if (base_inv_id.notNull())
		{
			for (attachment_map_t::const_iterator
					iter = mAttachmentPoints.begin(),
					end = mAttachmentPoints.end();
				 iter != end; ++iter)
			{
				const LLViewerJointAttachment* attachp = iter->second;
				if (attachp && attachp->getAttachedObject(base_inv_id))
				{
					const std::string name = attachp->getName();
					if (translate)
					{
						return LLTrans::getString(name);
					}
					if (name == "Avatar Center")
					{
						return "Root";
					}
					return name;
				}
			}
		}
	}

	return LLStringUtil::null;
}


//virtual
S32 LLVOAvatarSelf::getMaxAnimatedObjectAttachments() const
{
	S32 limit = LLEconomy::getInstance()->getAnimatedObjectLimit();
	return limit >= 0 ? limit : LLVOAvatar::getMaxAnimatedObjectAttachments();
}

//virtual
const LLViewerJointAttachment* LLVOAvatarSelf::attachObject(LLViewerObject* objp)
{
	const LLViewerJointAttachment* attachp = LLVOAvatar::attachObject(objp);
	if (!attachp)
	{
		return NULL;
	}

//MK
	if (gRLenabled)
	{
		// If the corresponding inventory item is under #RLV and does not
		// contain any attachment info in its name, rename it (or its parent
		// category) for later use by RestrainedLove.
		gRLInterface.addAttachmentPointName(objp);
	}
//mk

	updateAttachmentVisibility(gAgent.getCameraMode());

	// Then make sure the inventory is in sync with the avatar.
	gInventory.addChangedMask(LLInventoryObserver::LABEL,
							  objp->getAttachmentItemID());
	gInventory.notifyObservers();

	updateLODRiggedAttachments();

	return attachp;
}

//virtual
bool LLVOAvatarSelf::detachObject(LLViewerObject* objp)
{
	const LLUUID attachment_id = objp->getAttachmentItemID();
	if (LLVOAvatar::detachObject(objp))
	{
		// The simulator should automatically handle permission revocation
		stopMotionFromSource(attachment_id);

		LLFollowCamMgr::setCameraActive(objp->getID(), false);

		LLViewerObject::const_child_list_t& child_list = objp->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				iter = child_list.begin(), end = child_list.end();
			 iter != end; ++iter)
		{
			LLViewerObject* childp = *iter;
			if (childp)	// Paranoia
			{
				// The simulator should automatically handle permissions
				// revocation
				stopMotionFromSource(childp->getID());
				LLFollowCamMgr::setCameraActive(childp->getID(), false);
			}
		}

		// Make sure the inventory is in sync with the avatar.
		gInventory.addChangedMask(LLInventoryObserver::LABEL, attachment_id);
		gInventory.notifyObservers();

		return true;
	}
	return false;
}

//static
bool LLVOAvatarSelf::detachAttachmentIntoInventory(const LLUUID& item_id)
{
	LLInventoryItem* itemp =
		gInventory.getItem(gInventory.getLinkedItemID(item_id));
	if (!itemp  || !gAgent.getRegion())
	{
		return false;
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_DetachAttachmentIntoInv);
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_ItemID, item_id);
	msg->sendReliable(gAgent.getRegionHost());

	// This object might have been selected, so let the selection manager know
	// it is gone now.
	LLViewerObject* objp = gObjectList.findObject(item_id);
	if (objp)
	{
		gSelectMgr.remove(objp);
	}

	return true;
}

U32 LLVOAvatarSelf::getNumWearables(ETextureIndex i) const
{
	LLWearableType::EType type = gAvatarAppDictp->getTEWearableType(i);
	return gAgentWearables.getWearableCount(type);
}

//virtual
void LLVOAvatarSelf::localTextureLoaded(bool success,
										LLViewerFetchedTexture* texp,
										LLImageRaw* src_raw,
										LLImageRaw* aux_src,
										S32 discard_level,
										bool is_final, void* userdata)
{
	LLAvatarTexData* datap = (LLAvatarTexData*)userdata;
	ETextureIndex index = datap->mIndex;
	if (!isIndexLocalTexture(index))
	{
		return;
	}

	LLLocalTextureObject* ltop = getLocalTextureObject(index, 0);
	// Fix for EXT-268. Preventing using of NULL pointer
	if (!ltop)
	{
		llwarns << "There is no local texture object with index: " << index
				<< " - is_final: " << is_final << llendl;
		return;
	}
	const LLUUID& src_id = texp->getID();
	if (success)
	{
		if (!ltop->getBakedReady() && ltop->getImage() &&
			ltop->getID() == src_id && discard_level < ltop->getDiscard())
		{
			ltop->setDiscard(discard_level);
			requestLayerSetUpdate(index);
			if (isEditingAppearance())
			{
				LLVisualParamHint::requestHintUpdates();
			}
			updateMeshTextures();
		}
	}
	// Failed: asset is missing
	else if (is_final && !ltop->getBakedReady() && ltop->getImage() &&
			 ltop->getImage()->getID() == src_id)
	{
		ltop->setDiscard(0);
		requestLayerSetUpdate(index);
		updateMeshTextures();
	}
}

LLViewerFetchedTexture* LLVOAvatarSelf::getLocalTextureGL(ETextureIndex type,
														  U32 index) const
{
	if (!isIndexLocalTexture(type))
	{
		return NULL;
	}

	const LLLocalTextureObject* ltop = getLocalTextureObject(type, index);
	if (!ltop)
	{
		return NULL;
	}

	if (ltop->getID() == IMG_DEFAULT_AVATAR)
	{
		return LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR);
	}

	LLGLTexture* gltexp = ltop->getImage();
	return gltexp ? gltexp->asFetched() : NULL;
}

const LLUUID& LLVOAvatarSelf::getLocalTextureID(ETextureIndex type,
												U32 index) const
{
	if (!isIndexLocalTexture(type)) return IMG_DEFAULT_AVATAR;

	const LLLocalTextureObject* ltop = getLocalTextureObject(type, index);
	if (ltop && ltop->getImage())
	{
		return ltop->getImage()->getID();
	}
	return IMG_DEFAULT_AVATAR;
}

// Returns true if at least the lowest quality discard level exists for every
// texture in the layerset.
bool LLVOAvatarSelf::isLocalTextureDataAvailable(LLViewerTexLayerSet* layersetp)
{
	for (LLAvatarAppearanceDictionary::BakedTextures::const_iterator
			it = gAvatarAppDictp->getBakedTextures().begin(),
			end = gAvatarAppDictp->getBakedTextures().end();
		 it != end; ++it)
	{
		EBakedTextureIndex baked_index = it->first;
		if (layersetp == mBakedTextureDatas[baked_index].mTexLayerSet)
		{
			LLAvatarAppearanceDictionary::BakedEntry* baked_dictp =
				it->second;
			texture_vec_t& textures = baked_dictp->mLocalTextures;
			for (U32 i = 0, count = textures.size(); i < count; ++i)
			{
				ETextureIndex tex_index = textures[i];
				LLWearableType::EType wearable_type =
					LLAvatarAppearanceDictionary::getTEWearableType(tex_index);
				U32 wcount = gAgentWearables.getWearableCount(wearable_type);
				for (U32 w = 0; w < wcount; ++w)
				{
					if (getLocalDiscardLevel(tex_index, w) < 0)
					{
						return false;
					}
				}
			}
			return true;
		}
	}

	llassert(false);
	return false;
}

// Returns true if the highest quality discard level exists for every texture
// in the layerset.
bool LLVOAvatarSelf::isLocalTextureDataFinal(LLViewerTexLayerSet* layersetp)
{
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		if (layersetp == mBakedTextureDatas[i].mTexLayerSet)
		{
			const LLAvatarAppearanceDictionary::BakedEntry* baked_dictp =
					gAvatarAppDictp->getBakedTexture((EBakedTextureIndex)i);
			const texture_vec_t& textures = baked_dictp->mLocalTextures;
			for (U32 j = 0, count2 = textures.size(); j < count2; ++j)
			{
				ETextureIndex tex_index = textures[j];
				LLWearableType::EType wearable_type =
					LLAvatarAppearanceDictionary::getTEWearableType(tex_index);
				U32 wcount = gAgentWearables.getWearableCount(wearable_type);
				for (U32 w = 0; w < wcount; ++w)
				{
					if (getLocalDiscardLevel(tex_index, w) != 0)
					{
						return false;
					}
				}
			}
			return true;
		}
	}

	llassert(false);
	return false;
}

bool LLVOAvatarSelf::isBakedTextureFinal(EBakedTextureIndex index)
{
	LLViewerTexLayerSet* layersetp = getLayerSet(index);
	if (!layersetp) return false;

	LLViewerTexLayerSetBuffer* bufferp = layersetp->getViewerComposite();
	return bufferp && !bufferp->uploadNeeded();
}

bool LLVOAvatarSelf::isTextureDefined(ETextureIndex type, U32 index) const
{
	LLUUID id;
	bool is_defined = true;
	if (isIndexLocalTexture(type))
	{
		const LLWearableType::EType wearable_type =
			LLAvatarAppearanceDictionary::getTEWearableType(type);
		U32 count = gAgentWearables.getWearableCount(wearable_type);
		if (index >= count)
		{
			// Invalid index passed in. check all textures of a given type
			for (U32 i = 0; i < count; ++i)
			{
				id = getLocalTextureID(type, i);
				is_defined &= id != IMG_DEFAULT_AVATAR && id != IMG_DEFAULT;
			}
		}
		else
		{
			id = getLocalTextureID(type, index);
			is_defined &= id != IMG_DEFAULT_AVATAR && id != IMG_DEFAULT;
		}
	}
	else
	{
		id = getTEImage(type)->getID();
		is_defined &= id != IMG_DEFAULT_AVATAR && id != IMG_DEFAULT;
	}

	return is_defined;
}

//virtual
bool LLVOAvatarSelf::isTextureVisible(ETextureIndex type, U32 index) const
{
	if (isIndexBakedTexture(type))
	{
		return LLVOAvatar::isTextureVisible(type, 0U);
	}

	return LLDrawPoolAlpha::sShowDebugAlpha ||
		   getLocalTextureID(type, index) != IMG_INVISIBLE;
}

//virtual
bool LLVOAvatarSelf::isTextureVisible(ETextureIndex type,
									  LLViewerWearable* wearablep) const
{
	if (isIndexBakedTexture(type))
	{
		return LLVOAvatar::isTextureVisible(type);
	}

	U32 index;
	if (gAgentWearables.getWearableIndex(wearablep, index))
	{
		return isTextureVisible(type, index);
	}

	LL_DEBUGS("Avatar") << "Wearable not found on avatar" << LL_ENDL;
	return false;
}

void LLVOAvatarSelf::requestLayerSetUploads()
{
	if (!canUseServerBaking())
	{
		gAppearanceMgr.setRebaking();

		for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
		{
			requestLayerSetUpload((EBakedTextureIndex)i);
		}
	}
}

void LLVOAvatarSelf::requestLayerSetUpload(EBakedTextureIndex i)
{
	ETextureIndex tex_idx = mBakedTextureDatas[i].mTextureIndex;
	if (!isTextureDefined(tex_idx, gAgentWearables.getWearableCount(tex_idx)))
	{
		LLViewerTexLayerSet* layerset = getLayerSet(i);
		if (layerset)
		{
			layerset->requestUpload();
		}
	}
}

bool LLVOAvatarSelf::areTexturesCurrent()
{
	return !hasPendingBakedUploads() && gAgentWearables.areWearablesLoaded();
}

bool LLVOAvatarSelf::hasPendingBakedUploads()
{
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		LLViewerTexLayerSet* layerset = getTexLayerSet(i);
		if (layerset && layerset->getViewerComposite() &&
			layerset->getViewerComposite()->uploadPending())
		{
			gAppearanceMgr.setRebaking();
			return true;
		}
	}

	gAppearanceMgr.setRebaking(false);
	return false;
}

//virtual
void LLVOAvatarSelf::invalidateComposite(LLTexLayerSet* texlayersetp,
										 bool upload_result)
{
	if (!texlayersetp) return;

	LLViewerTexLayerSet* vtexlayersetp = texlayersetp->asViewerTexLayerSet();
	if (!vtexlayersetp || !vtexlayersetp->getUpdatesEnabled())
	{
		return;
	}

	vtexlayersetp->requestUpdate();
	vtexlayersetp->invalidateMorphMasks();

	if (upload_result && !canUseServerBaking())
	{
		ETextureIndex baked_te = getBakedTE(vtexlayersetp);
		setTEImage(baked_te,
				   LLViewerTextureManager::getFetchedTexture(IMG_DEFAULT_AVATAR));
		vtexlayersetp->requestUpload();
		updateMeshTextures();
	}
}

//virtual
void LLVOAvatarSelf::invalidateAll()
{
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		LLViewerTexLayerSet* layersetp = getTexLayerSet(i);
		invalidateComposite(layersetp, true);
	}
}

//virtual
void LLVOAvatarSelf::setCompositeUpdatesEnabled(bool b)
{
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		setCompositeUpdatesEnabled(i, b);
	}
}

//virtual
void LLVOAvatarSelf::setCompositeUpdatesEnabled(U32 index, bool b)
{
	LLViewerTexLayerSet* layersetp = getTexLayerSet(index);
	if (layersetp)
	{
		layersetp->setUpdatesEnabled(b);
	}
}

//virtual
bool LLVOAvatarSelf::isCompositeUpdateEnabled(U32 index)
{
	LLViewerTexLayerSet* layersetp = getTexLayerSet(index);
	return layersetp ? layersetp->getUpdatesEnabled() : false;
}

void LLVOAvatarSelf::setupComposites()
{
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		LLViewerTexLayerSet* layersetp = getTexLayerSet(i);
		if (layersetp)
		{
			ETextureIndex idx = mBakedTextureDatas[i].mTextureIndex;
			bool layer_baked =
				isTextureDefined(idx, gAgentWearables.getWearableCount(idx));
			layersetp->setUpdatesEnabled(!layer_baked);
		}
	}
}

void LLVOAvatarSelf::updateComposites()
{
	bool wearing_skirt = isWearingWearableType(LLWearableType::WT_SKIRT);
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		if (i != BAKED_SKIRT || wearing_skirt)
		{
			LLViewerTexLayerSet* layersetp = getTexLayerSet(i);
			if (layersetp)
			{
				layersetp->updateComposite();
			}
		}
	}
}

S32 LLVOAvatarSelf::getLocalDiscardLevel(ETextureIndex type, U32 wearable_idx)
{
	if (type < 0 || !isIndexLocalTexture(type)) return 0;

	LLLocalTextureObject* ltop = getLocalTextureObject(type, wearable_idx);
	if (ltop)
	{
		LLGLTexture* gltexp = ltop->getImage();
		if (gltexp)
		{
			LLViewerFetchedTexture* texp = gltexp->asFetched();
			if (texp && !texp->isMissingAsset() &&
				ltop->getID() != IMG_DEFAULT_AVATAR)
			{
				return texp->getDiscardLevel();
			}
		}
	}

	// We do not care about this (no image associated with the layer) treat as
	// fully loaded.
	return 0;
}

// Counts the memory footprint of local textures.
void LLVOAvatarSelf::getLocalTextureByteCount(S32* gl_bytes)
{
	*gl_bytes = 0;
	for (S32 type = 0; type < TEX_NUM_INDICES; ++type)
	{
		if (!isIndexLocalTexture((ETextureIndex)type))
		{
			continue;
		}

		U32 max_tex = getNumWearables((ETextureIndex)type);
		for (U32 num = 0; num < max_tex; ++num)
		{
			LLLocalTextureObject* ltop =
				getLocalTextureObject((ETextureIndex)type, num);
			if (!ltop) continue;

			LLGLTexture* gltexp = ltop->getImage();
			if (!gltexp) continue;

			LLViewerFetchedTexture* texp = gltexp->asFetched();
			if (texp && texp->hasGLTexture())
			{
				*gl_bytes += texp->getWidth() * texp->getHeight() *
							 texp->getComponents();
			}
		}
	}
}

//virtual
void LLVOAvatarSelf::setLocalTexture(ETextureIndex type,
									 LLViewerTexture* src_texp,
									 bool baked_version_ready, U32 index)
{
	if (type >= TEX_NUM_INDICES || !isIndexLocalTexture(type)) return;

	LLViewerFetchedTexture* texp = LLViewerTextureManager::staticCast(src_texp,
																	  true);
	if (!texp)
	{
		return;
	}

	LLLocalTextureObject* ltop = getLocalTextureObject(type, index);
	if (!ltop)
	{
		LLWearableType::EType wearable_type =
			gAvatarAppDictp->getTEWearableType(type);
		ltop = gAgentWearables.addLocalTextureObject(wearable_type, type,
													 index);
		if (!ltop)
		{
			// Wearable not loaded, could not set the texture.
			return;
		}

		LLViewerTexLayerSet* layersetp = getLayerSet(type);
		if (layersetp)
		{
			layersetp->cloneTemplates(ltop, type,
									  gAgentWearables.getViewerWearable(wearable_type,
																		index));
		}
	}
	if (!baked_version_ready)
	{
		if (texp != ltop->getImage() || ltop->getBakedReady())
		{
			ltop->setDiscard(MAX_DISCARD_LEVEL + 1);
		}
		if (texp->getID() != IMG_DEFAULT_AVATAR)
		{
			if (ltop->getDiscard() > 0)
			{
				S32 tex_discard = texp->getDiscardLevel();
				if (tex_discard == 0)
				{
					ltop->setDiscard(tex_discard);
					if (isSelf())
					{
						requestLayerSetUpdate(type);
						if (isEditingAppearance())
						{
							LLVisualParamHint::requestHintUpdates();
						}
					}
				}
				else
				{
					texp->setLoadedCallback(onLocalTextureLoaded, 0, true,
											false,
											new LLAvatarTexData(getID(), type),
											NULL);
				}
			}
			texp->setMinDiscardLevel(0);
		}
	}
	ltop->setImage(texp);
	ltop->setID(texp->getID());
	setBakedReady(type, baked_version_ready, index);
}

//virtual
void LLVOAvatarSelf::setBakedReady(ETextureIndex type, bool has_baked,
								   U32 index)
{
	if (!isIndexLocalTexture(type))
	{
		return;
	}
	LLLocalTextureObject* ltop = getLocalTextureObject(type, index);
	if (ltop)
	{
		ltop->setBakedReady(has_baked);
	}
}

void LLVOAvatarSelf::dumpLocalTextures()
{
	llinfos << "Local textures:" << llendl;

	bool is_god = gAgent.isGodlikeWithoutAdminMenuFakery();

	for (LLAvatarAppearanceDictionary::Textures::const_iterator
			iter = gAvatarAppDictp->getTextures().begin(),
			end = gAvatarAppDictp->getTextures().end();
		 iter != end; ++iter)
	{
		LLAvatarAppearanceDictionary::TextureEntry* t_dict = iter->second;
		if (!t_dict->mIsLocalTexture || !t_dict->mIsUsedByBakedTexture)
		{
			continue;
		}

		EBakedTextureIndex baked_index = t_dict->mBakedTextureIndex;
		ETextureIndex baked_equiv =
			gAvatarAppDictp->getBakedTexture(baked_index)->mTextureIndex;

		const std::string& name = t_dict->mName;
		// Index is baked texture - index is not relevant. putting in 0 as
		// placeholder
		if (isTextureDefined(baked_equiv, 0))
		{
			llinfos << "LocTex " << name << ": baked";
			if (is_god)
			{
				llcont << " - Id: " << getTEImage(baked_equiv)->getID();
			}
			llcont << llendl;
			continue;
		}

		LLViewerFetchedTexture* texp = NULL;
		LLLocalTextureObject* ltop = getLocalTextureObject(iter->first, 0);
		if (ltop && ltop->getImage())
		{
			texp = ltop->getImage()->asFetched();
		}
		if (!texp)
		{
			llinfos << "LocTex " << name << ": no LLViewerTexture" << llendl;
			continue;
		}

		if (ltop->getImage()->getID() == IMG_DEFAULT_AVATAR)
		{
			llinfos << "LocTex " << name << ": none" << llendl;
			continue;
		}

		llinfos << "LocTex " << name;
		if (is_god)
		{
			llcont << " - Id: " << texp->getID();
		}
		llcont << " - Size: " << texp->getWidth() << "x" << texp->getHeight()
			   << " - Discard level: " << texp->getDiscardLevel()
			   << " - Priority: " << texp->getDecodePriority() << llendl;
	}
}

//static
void LLVOAvatarSelf::onLocalTextureLoaded(bool success,
										  LLViewerFetchedTexture* texp,
										  LLImageRaw* src_raw_imagep,
										  LLImageRaw* src_aux_imagep,
										  S32 discard_level,
										  bool is_final,
										  void* userdata)
{
	LLAvatarTexData* datap = (LLAvatarTexData*)userdata;
	LLVOAvatarSelf* self =
		(LLVOAvatarSelf*)gObjectList.findAvatar(datap->mAvatarID);
	if (self)
	{
		self->localTextureLoaded(success, texp, src_raw_imagep, src_aux_imagep,
								 discard_level, is_final, userdata);
	}
	// Ensure data is cleaned up
	if (is_final || !success)
	{
		delete datap;
	}
}

//virtual
void LLVOAvatarSelf::setImage(U8 te, LLViewerTexture* imagep, U32 index)
{
	if (isIndexLocalTexture((ETextureIndex)te))
	{
		setLocalTexture((ETextureIndex)te, imagep, false, index);
	}
	else
	{
		setTEImage(te, imagep);
	}
}

//virtual
LLViewerTexture* LLVOAvatarSelf::getImage(U8 te, U32 index) const
{
	if (isIndexLocalTexture((ETextureIndex)te))
	{
		return getLocalTextureGL((ETextureIndex)te, index);
	}

	return getTEImage(te);
}

//static
void LLVOAvatarSelf::dumpTotalLocalTextureByteCount()
{
	S32 gl_bytes = 0;
	gAgentAvatarp->getLocalTextureByteCount(&gl_bytes);
	llinfos << "Total Avatar LocTex GL:" << gl_bytes / 1024 << "KB" << llendl;
}

bool LLVOAvatarSelf::getIsCloud()
{
	// Do we have our body parts ?
	if (gAgentWearables.getWearableCount(LLWearableType::WT_SHAPE) == 0 ||
		gAgentWearables.getWearableCount(LLWearableType::WT_HAIR) == 0 ||
		gAgentWearables.getWearableCount(LLWearableType::WT_EYES) == 0 ||
		gAgentWearables.getWearableCount(LLWearableType::WT_SKIN) == 0)
	{
		LL_DEBUGS("Avatar") << "Missing body part" << LL_ENDL;
		return true;
	}

	if (!isTextureDefined(TEX_HAIR, 0))
	{
		LL_DEBUGS("Avatar") << "No hair texture" << LL_ENDL;
		return true;
	}

	if (!mPreviousFullyLoaded)
	{
		if (!isLocalTextureDataAvailable(getLayerSet(BAKED_LOWER)) &&
			!isTextureDefined(TEX_LOWER_BAKED, 0))
		{
			LL_DEBUGS("Avatar") << "Lower textures not baked" << LL_ENDL;
			return true;
		}

		if (!isLocalTextureDataAvailable(getLayerSet(BAKED_UPPER)) &&
			!isTextureDefined(TEX_UPPER_BAKED, 0))
		{
			LL_DEBUGS("Avatar") << "Upper textures not baked" << LL_ENDL;
			return true;
		}

		bool wearing_skirt = isWearingWearableType(LLWearableType::WT_SKIRT);
		for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
		{
			if (i == BAKED_SKIRT && !wearing_skirt)
			{
				continue;
			}

			const BakedTextureData& texture_data = mBakedTextureDatas[i];
			if (!isTextureDefined(texture_data.mTextureIndex, 0))
			{
				continue;
			}

			// Check for the case that texture is defined but not sufficiently
			// loaded to display anything.
			const LLViewerTexture* baked_img =
				getImage(texture_data.mTextureIndex, 0);
			if (!baked_img || !baked_img->hasGLTexture())
			{
				LL_DEBUGS("Avatar") << "Texture at index " << i
									<< " (texture index is "
									<< texture_data.mTextureIndex
									<< ") is not loaded" << LL_ENDL;
				return true;
			}
		}

		LL_DEBUGS("Avatar") << "Avatar de-clouded" << LL_ENDL;
	}

	return false;
}

const LLUUID& LLVOAvatarSelf::grabBakedTexture(EBakedTextureIndex baked_index) const
{
	if (!canGrabBakedTexture(baked_index))
	{
		return LLUUID::null;
	}
	ETextureIndex tex_index =
		LLAvatarAppearanceDictionary::bakedToLocalTextureIndex(baked_index);
	if (tex_index == TEX_NUM_INDICES)
	{
		return LLUUID::null;
	}
	return getTEImage(tex_index)->getID();
}

bool LLVOAvatarSelf::canGrabBakedTexture(EBakedTextureIndex baked_index) const
{
	ETextureIndex tex_index =
		LLAvatarAppearanceDictionary::bakedToLocalTextureIndex(baked_index);
	if (tex_index == TEX_NUM_INDICES)
	{
		return false;
	}
	// Check if the texture has not been baked yet.
	if (!isTextureDefined(tex_index, 0))
	{
		LL_DEBUGS("Avatar") << "getTEImage( " << (U32) tex_index
							<< " )->getID() == IMG_DEFAULT_AVATAR" << LL_ENDL;
		return false;
	}

	if (gAgent.isGodlikeWithoutAdminMenuFakery())
	{
		return true;
	}

	// Check permissions of textures that show up in the baked texture. We
	// do not want people copying people's work via baked textures.
	const LLAvatarAppearanceDictionary::BakedEntry* baked_dict =
		gAvatarAppDictp->getBakedTexture(baked_index);
	const texture_vec_t& textures = baked_dict->mLocalTextures;
	for (U32 t = 0, tcount = textures.size(); t < tcount; ++t)
	{
		const ETextureIndex t_index = textures[t];
		LLWearableType::EType wearable_type =
			LLAvatarAppearanceDictionary::getTEWearableType(t_index);
		U32 wcount = gAgentWearables.getWearableCount(wearable_type);
		LL_DEBUGS("Avatar") << "Checking index " << (U32)t_index << " count: "
							<< wcount << LL_ENDL;

		for (U32 w = 0; w < wcount; ++w)
		{
			LLViewerWearable* wearablep =
				gAgentWearables.getViewerWearable(wearable_type, w);
			if (!wearablep) continue;

			const LLLocalTextureObject* ltop =
				wearablep->getLocalTextureObject((S32)t_index);
			const LLUUID& texture_id = ltop->getID();
			if (texture_id != IMG_DEFAULT_AVATAR &&
				texture_id != IMG_INVISIBLE &&
				!HBObjectBackup::validateAssetPerms(texture_id, true))
			{
				return false;
			}
		}
	}

	return true;
}

void LLVOAvatarSelf::addLocalTextureStats(ETextureIndex type,
										  LLViewerFetchedTexture* imagep,
										  F32 texel_area_ratio,
										  bool render_avatar,
										  bool covered_by_baked)
{
	if (covered_by_baked || !imagep || !isIndexLocalTexture(type))
	{
		return;
	}

	if (imagep->getID() != IMG_DEFAULT_AVATAR)
	{
		if (imagep->getDiscardLevel() != 0)
		{
			// Note: used to be 512x512, but increased to take into account
			// larger (1024x1024) new bakes. HB
			constexpr F32 MAX_AREA = 1024.f * 1024.f;
			F32 desired_pixels = llmin(mPixelArea, MAX_AREA);

			imagep->setBoostLevel(LLGLTexture::BOOST_AVATAR_SELF);
#if !LL_IMPLICIT_SETNODELETE
			imagep->setNoDelete();
#endif
			imagep->setAdditionalDecodePriority(SELF_ADDITIONAL_PRI);
			imagep->resetTextureStats();
			imagep->setMaxVirtualSizeResetInterval(S32_MAX);
			imagep->addTextureStats(desired_pixels / texel_area_ratio);
			imagep->forceUpdateBindStats();
			if (imagep->getDiscardLevel() < 0)
			{
				mHasGrey = true; // For statistics gathering
			}
		}
	}
	else
	{
		// Texture asset is missing
		mHasGrey = true; // for statistics gathering
	}
}

LLLocalTextureObject* LLVOAvatarSelf::getLocalTextureObject(ETextureIndex i,
															U32 wearable_index) const
{
	LLWearableType::EType type = gAvatarAppDictp->getTEWearableType(i);
	LLViewerWearable* wearable =
		gAgentWearables.getViewerWearable(type, wearable_index);
	return wearable ? wearable->getLocalTextureObject(i) : NULL;
}

// Used by the LayerSet (layer sets do not in general know what textures depend
// on them).
ETextureIndex LLVOAvatarSelf::getBakedTE(const LLViewerTexLayerSet* layerset) const
{
	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		if (layerset == mBakedTextureDatas[i].mTexLayerSet)
		{
			return mBakedTextureDatas[i].mTextureIndex;
		}
	}
	llassert(false);
	return TEX_HEAD_BAKED;
}

// A new baked texture has been successfully uploaded and we can start using it
// now.
void LLVOAvatarSelf::setNewBakedTexture(EBakedTextureIndex i, const LLUUID& id)
{
	ETextureIndex index =
		LLAvatarAppearanceDictionary::bakedToLocalTextureIndex(i);
	setNewBakedTexture(index, id);
}

// A new baked texture has been successfully uploaded and we can start using it
// now.
void LLVOAvatarSelf::setNewBakedTexture(ETextureIndex te, const LLUUID& uuid)
{
	// Baked textures live on other sims.
	LLHost target_host = getObjectHost();
	setTEImage(te,
			   LLViewerTextureManager::getFetchedTextureFromHost(uuid,
																 FTT_HOST_BAKE,
																 target_host));
	updateMeshTextures();
	dirtyMesh();
	refreshAttachmentBakes();
	setAvatarCullingDirty();

	const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
		gAvatarAppDictp->getTexture(te);
	if (t_dict && t_dict->mIsBakedTexture)
	{
		llinfos << "New baked texture: " << t_dict->mName << " UUID: "
				<< uuid <<llendl;
	}
	else
	{
		llwarns << "New baked texture: unknown te " << te << llendl;
	}

	// RN: throttle uploads
	if (!hasPendingBakedUploads())
	{
		gAgent.sendAgentSetAppearance();
	}
}

// A baked texture id was received from a cache query, make it active
void LLVOAvatarSelf::setCachedBakedTexture(ETextureIndex te,
										   const LLUUID& uuid)
{
	setTETexture(te, uuid);

	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		LLViewerTexLayerSet* layerset = getTexLayerSet(i);
		if (layerset && mBakedTextureDatas[i].mTextureIndex == te)
		{
			if (mInitialBakeIDs[i].notNull())
			{
				if (mInitialBakeIDs[i] == uuid)
				{
					llinfos << "baked texture #" << i
							<< " correctly loaded at login: " << llendl;
				}
				else
				{
					llwarns << "baked texture #" << i
							<< " does not match id loaded at login" << llendl;
				}
				mInitialBakeIDs[i].setNull();
			}
			layerset->cancelUpload();
		}
	}
}

//static
void LLVOAvatarSelf::processRebakeAvatarTextures(LLMessageSystem* msg, void**)
{
	LLUUID texture_id;
	msg->getUUID("TextureData", "TextureID", texture_id);
	if (!isAgentAvatarValid()) return;

	// If this is a texture corresponding to one of our baked entries, just
	// rebake that layer set.
	bool found = false;
	for (LLAvatarAppearanceDictionary::Textures::const_iterator
			iter = gAvatarAppDictp->getTextures().begin(),
			end = gAvatarAppDictp->getTextures().end();
		 iter != end; ++iter)
	{
		const ETextureIndex index = iter->first;
		const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
			iter->second;
		if (t_dict->mIsBakedTexture &&
			texture_id == gAgentAvatarp->getTEImage(index)->getID())
		{
			LLViewerTexLayerSet* layer_set = gAgentAvatarp->getLayerSet(index);
			if (layer_set)
			{
				llinfos << "TAT: rebake - matched entry " << (S32)index
						<< llendl;
				gAgentAvatarp->invalidateComposite(layer_set, true);
				found = true;
				gViewerStats.incStat(LLViewerStats::ST_TEX_REBAKES);
			}
			break;
		}
	}

	if (found)
	{
		// Not sure if this is necessary, but forceBakeAllTextures() does it.
		gAgentAvatarp->updateMeshTextures();
	}
	else
	{
		// If texture not found, rebake all entries.
		gAgentAvatarp->forceBakeAllTextures();
	}
}

void LLVOAvatarSelf::forceBakeAllTextures(bool slam_for_debug)
{
	llinfos << "TAT: forced full rebake. " << llendl;

	if (mIsEditingAppearance)
	{
		slam_for_debug = false;
	}

	for (U32 i = 0, count = mBakedTextureDatas.size(); i < count; ++i)
	{
		ETextureIndex baked_index = mBakedTextureDatas[i].mTextureIndex;
		LLViewerTexLayerSet* layer_set = getLayerSet(baked_index);
		if (layer_set)
		{
			if (slam_for_debug)
			{
				layer_set->setUpdatesEnabled(true);
				layer_set->cancelUpload();
			}

			invalidateComposite(layer_set, true);
			gViewerStats.incStat(LLViewerStats::ST_TEX_REBAKES);
		}
		else
		{
			llwarns << "TAT: NO LAYER SET FOR " << (S32)baked_index << llendl;
		}
	}

	// Is this needed really ?
	updateMeshTextures();

	static LLCachedControl<bool> aggressive_rebake(gSavedSettings,
												   "AvatarAggressiveRebake");
	if (slam_for_debug && aggressive_rebake)
	{
		// This is equivalent to entering and exiting the Edit Appearance mode
		// and should slam all baked textures for good, ensuring they all get
		// rebaked at next frame.
		gAgentQueryManager.resetPendingQueries();
		mIsEditingAppearance = true;
		mUseLocalAppearance = true;
		updateTextures();
		invalidateAll();
		updateMeshTextures();
		gAgent.sendAgentSetAppearance();
		mUseLocalAppearance = false;
		mIsEditingAppearance = false;
	}
}

void LLVOAvatarSelf::requestLayerSetUpdate(ETextureIndex index)
{
	const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
		gAvatarAppDictp->getTexture(index);
	if (!t_dict || !t_dict->mIsLocalTexture || !t_dict->mIsUsedByBakedTexture)
	{
		return;
	}
	EBakedTextureIndex baked_index = t_dict->mBakedTextureIndex;
	if (mBakedTextureDatas[baked_index].mTexLayerSet)
	{
		mBakedTextureDatas[baked_index].mTexLayerSet->requestUpdate();
	}
}

LLViewerTexLayerSet* LLVOAvatarSelf::getLayerSet(ETextureIndex index)
{
	const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
		gAvatarAppDictp->getTexture(index);
	if (!t_dict || !t_dict->mIsUsedByBakedTexture)
	{
		return NULL;
	}
	EBakedTextureIndex baked_index = t_dict->mBakedTextureIndex;
	return getLayerSet(baked_index);
}

LLViewerTexLayerSet* LLVOAvatarSelf::getLayerSet(EBakedTextureIndex baked_index)
{
	if (baked_index < 0 || baked_index >= BAKED_NUM_INDICES)
	{
		return NULL;
	}
	return getTexLayerSet(baked_index);
}

//virtual
void LLVOAvatarSelf::updateMotions(e_update_t update_type)
{
	LLCharacter::updateMotions(update_type);

	static LLCachedControl<bool> send_data(gSavedSettings,
										   "PuppetrySendAttachmentsData");
	if (!send_data)
	{
		return;
	}

	// Post motion update
	if (!mAttachmentUpdateEnabled ||
		gFrameTimeSeconds < mAttachmentUpdateExpiry)
	{
		return;
	}
	mAttachmentUpdateExpiry = gFrameTimeSeconds + mAttachmentUpdatePeriod;

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp ||
		!regionp->getRegionFlag(REGION_FLAGS_ENABLE_ANIMATION_TRACKING))
	{
		return;
	}

	LLVector3 pos;
	LLQuaternion rot;
	const LLVector3& agent_pos = getPositionRegion();
	LLQuaternion agent_inv_rot(~getWorldRotation());

	LLMessageSystem* msg = gMessageSystemp;
	bool start_new_message = true;
	for (attachment_map_t::iterator it = mAttachmentPoints.begin(),
									end = mAttachmentPoints.end();
		 it != end; ++it)
	{
		LLViewerJointAttachment* attachp = it->second;
		if (!attachp || attachp->getIsHUDAttachment()) continue;

		pos = attachp->getWorldPosition() - agent_pos;
		rot = attachp->getWorldRotation() * agent_inv_rot;
		if (!attachp->hasChanged(pos, rot))
		{
			continue;
		}
		attachp->setLastTracked(pos, rot);

		if (start_new_message)
		{
			start_new_message = false;
			msg->newMessageFast(_PREHASH_AgentAnimationTracking);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		}
		msg->nextBlockFast(_PREHASH_AttachmentPointUpdate);
		msg->addU8Fast(_PREHASH_AttachmentPoint, (U8)it->first);
		msg->addVector3Fast(_PREHASH_Position, pos);
		msg->addQuatFast(_PREHASH_Rotation, rot);
		msg->addF32Fast(_PREHASH_Radius, 0.f);
		if (msg->isSendFull())
		{
			gAgent.sendMessage();
			start_new_message = true;
		}
	}
	if (!start_new_message)
	{
		gAgent.sendMessage();
	}
}

void LLVOAvatarSelf::setAttachmentUpdatePeriod(F32 period_sec)
{
	constexpr F32 MIN_PERIOD = 0.01f;
	constexpr F32 MAX_PERIOD = 2.f;
	mAttachmentUpdatePeriod = llclamp(period_sec, MIN_PERIOD, MAX_PERIOD);
}

//static
bool LLVOAvatarSelf::canUseServerBaking()
{
	return isAgentAvatarValid() && gAgentAvatarp->getRegion() &&
		   gAgentAvatarp->getRegion()->getCentralBakeVersion();
}

//static
void LLVOAvatarSelf::onCustomizeStart()
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->mIsEditingAppearance = true;
		gAgentAvatarp->mUseLocalAppearance = true;
		gAgentAvatarp->invalidateAll();
		gAgentAvatarp->updateMeshTextures();
		gAgentAvatarp->updateTextures();
	}
}

//static
void LLVOAvatarSelf::onCustomizeEnd()
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->mIsEditingAppearance = false;
		if (!canUseServerBaking())
		{
			// *FIXME: move to sendAgentSetAppearance, make conditional on
			// upload complete.
			gAgentAvatarp->mUseLocalAppearance = false;
		}
		else
		{
			gAppearanceMgr.incrementCofVersion();
		}
		gAgentAvatarp->invalidateAll();
	}
}

#if 0	// Removed from LL's viewer, at some point... Prevented some small mesh
		// avatars from getting leveled with the ground using the shape Hover
		// visual parameter.
//virtual
void LLVOAvatarSelf::computeBodySize(bool force)
{
	LLAvatarAppearance::computeBodySize();

	// Certain configurations of avatars can force the overall height (with
	// offset) to go negative. Enforce a constraint to make sure we don't go
	// below 1.1 meters (server-enforced limit). Camera positioning and other
	// things start to break down when your avatar is "walking" while being
	// fully underground
	if (getWearableData() && !LLApp::isExiting() &&
		// Do not force a hover parameter change while we have pending
		// attachments, which may be mesh-based with joint offsets.
		mPendingAttachment.size() == 0)
	{
		LLViewerWearable* shape =
			(LLViewerWearable*)getWearableData()->getWearable(LLWearableType::WT_SHAPE,
															  0);
		bool loaded = true;
		for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
		{
			const LLViewerObject* object = mAttachedObjectsVector[i].first;
			if (object)
			{
				loaded &= !object->isDrawableState(LLDrawable::REBUILD_ALL);
				if (!loaded && shape && !shape->getVolatile())
				{
					llwarns << "Caught unloaded attachment at point: "
							<< mAttachedObjectsVector[i].second->getName()
							<< " - Object: "
							<< object->getAttachmentItemID()
							<< ". Skipping enforcement." << llendl;
					break;
				}
				LL_DEBUGS("Avatar") << "Attachment at point: "
									<< mAttachedObjectsVector[i].second->getName()
									<< " object: "
									<< object->getAttachmentItemID()
									<< LL_ENDL;
			}
		}
		if ((loaded || force) && shape && !shape->getVolatile())
		{
			F32 hover_value = shape->getVisualParamWeight(AVATAR_HOVER);
			if (hover_value < 0.0f && mBodySize.mV[VZ] + hover_value < 1.1f)
			{
				// Avoid floating point rounding making the above check
				// continue to fail.
				hover_value = -mBodySize.mV[VZ] + 1.1f;
				llassert(mBodySize.mV[VZ] + hover_value >= 1.1f);

				// Do not force the hover value to be greater than 0.
				hover_value = llmin(hover_value, 0.f);

				LL_DEBUGS("Avatar") << "Changed hover value to: "
									<< hover_value << " from: "
									<< mAvatarOffset.mV[VZ] << LL_ENDL;

				mAvatarOffset.mV[VZ] = hover_value;
				shape->setVisualParamWeight(AVATAR_HOVER, hover_value, false);
				if (canUseServerBaking() && !useAvatarHoverHeight())
				{
					F32 offset = gSavedSettings.getF32("AvatarOffsetZ");
#if 1
					F32 factor = gSavedSettings.getF32("HoverToZOffsetFactor");
					if (factor > 1.f)
					{
						// Hover is wrongly accounted twice in LL's viewer...
						hover_value *= factor;
					}
#endif
					if (hover_value != offset)
					{
						gSavedSettings.setF32("AvatarOffsetZ", hover_value);
					}
				}
			}
		}
	}
}
#endif

//virtual
bool LLVOAvatarSelf::shouldRenderRigged() const
{
	return gAgent.needsRenderAvatar();
}

// HACK: this will null out the avatar's local texture IDs before the TE message
//		 is sent to ensure local texture IDs are not sent to other clients in
//		 the area. This is a short-term solution. The long term solution will be
//		 to not set the texture IDs in the avatar object, and keep them only in
//		 the wearable. This will involve further refactoring that is too risky
//		 for the initial release of 2.0.
void LLVOAvatarSelf::sendAppearanceMessage(LLMessageSystem* mesgsys) const
{
	LLUUID texture_id[TEX_NUM_INDICES];

	// Pack away current TEs to make sure we don't send them out
	for (LLAvatarAppearanceDictionary::Textures::const_iterator
			iter = gAvatarAppDictp->getTextures().begin(),
			end = gAvatarAppDictp->getTextures().end();
		 iter != end; ++iter)
	{
		const ETextureIndex index = iter->first;
		const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
			iter->second;
		if (t_dict && !t_dict->mIsBakedTexture)
		{
			LLTextureEntry* entry = getTE((U8)index);
			if (entry)
			{
				texture_id[index] = entry->getID();
				entry->setID(IMG_DEFAULT_AVATAR);
			}
		}
	}

	packTEMessage(mesgsys);

	// Unpack TEs to make sure we don't re-trigger a bake
	for (LLAvatarAppearanceDictionary::Textures::const_iterator
			iter = gAvatarAppDictp->getTextures().begin(),
			end = gAvatarAppDictp->getTextures().end();
		 iter != end; ++iter)
	{
		const ETextureIndex index = iter->first;
		const LLAvatarAppearanceDictionary::TextureEntry* t_dict =
			iter->second;
		if (t_dict && !t_dict->mIsBakedTexture)
		{
			LLTextureEntry* entry = getTE((U8)index);
			if (entry)
			{
				entry->setID(texture_id[index]);
			}
		}
	}
}

void LLVOAvatarSelf::sendHoverHeight() const
{
	const std::string& url = gAgent.getRegionCapability("AgentPreferences");
	if (url.empty())
	{
		return;
	}

	LLSD update = LLSD::emptyMap();
	const LLVector3& hover_offset = getHoverOffset();
	update["hover_height"] = hover_offset.mV[VZ];
	mLastHoverOffsetSent = hover_offset;

	LL_DEBUGS("Avatar") << "Sending hover height value for self: "
						<< hover_offset.mV[VZ] << "m" << LL_ENDL;
	LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(url, update,
														  "Hover height sent to sim",
														  "Failed to send hover height to sim");
}

void LLVOAvatarSelf::setHoverOffset(const LLVector3& hover_offset,
									bool send_update)
{
	if (getHoverOffset() != hover_offset)
	{
		llinfos << "Setting hover value for self due to change: "
				<< hover_offset[2] << llendl;
		LLVOAvatar::setHoverOffset(hover_offset, send_update);
	}
	if (send_update && hover_offset != mLastHoverOffsetSent)
	{
		llinfos << "Sending hover value for self due to change: "
				<< hover_offset[2] << LL_ENDL;
		sendHoverHeight();
	}
}

bool LLVOAvatarSelf::needsRenderBeam()
{
	// No beam for selected far objects when PrivateLookAt is true
	static LLCachedControl<bool> private_point_at(gSavedSettings,
												  "PrivatePointAt");
	static LLCachedControl<U32> point_at_limit(gSavedSettings,
											   "PrivatePointAtLimit");
	LLObjectSelectionHandle selection;
	selection = gSelectMgr.getSelection();
	LLViewerObject* objp = selection->getFirstObject();
	LLVector3d agent_pos = gAgent.getPositionGlobal();
	if (objp && private_point_at &&
		(objp->getPositionGlobal() - agent_pos).length() > point_at_limit)
	{
		return false;
	}

	bool is_touching_or_grabbing = gToolMgr.isCurrentTool(&gToolGrab) &&
								   gToolGrab.isEditing();
	if (is_touching_or_grabbing)
	{
		objp = gToolGrab.getEditingObject();
		if (objp && !objp->isAttachment() &&
			(objp->getPositionGlobal() - agent_pos).length() <= point_at_limit)
		{
			return true;
		}
		// Do not render selection beam on hud objects, or on far objects when
		// PrivateLookAt is true
		is_touching_or_grabbing = false;
	}
	return is_touching_or_grabbing ||
		   ((mAttachmentState & AGENT_STATE_EDITING) != 0 &&
			gSelectMgr.shouldShowSelection());
}

void LLVOAvatarSelf::resetHUDAttachments()
{
	for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
	{
		const LLViewerObject* object = mAttachedObjectsVector[i].first;
		if (object && object->isHUDAttachment() && object->mDrawable.notNull())
		{
			gPipeline.markMoved(object->mDrawable);
		}
	}
}

void LLVOAvatarSelf::refreshAttachments()
{
	for (S32 i = 0, count = mAttachedObjectsVector.size(); i < count; ++i)
	{
		const LLViewerObject* object = mAttachedObjectsVector[i].first;
		if (object && object->isAttachment())
		{
			if (object->mDrawable.notNull())
			{
				gPipeline.markMoved(object->mDrawable);
			}
			object->dirtySpatialGroup();
		}
	}
}

void LLVOAvatarSelf::handleTeleportFinished()
{
	doAfterInterval(boost::bind(&LLVOAvatarSelf::refreshAttachments, this), 2);
}
