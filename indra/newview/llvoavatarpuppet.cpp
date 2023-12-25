/**
 * @file llvoavatarpuppet.cpp
 * @brief Implementation for special dummy avatar used to drive rigged meshes.
 *
 * $LicenseInfo:firstyear=2017&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2017, Linden Research, Inc.
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

#include "llvoavatarpuppet.h"

#include "llanimationstates.h"

#include "llagent.h"
#include "llmeshrepository.h"
#include "llpipeline.h"
#include "llskinningutil.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwearable.h"
#include "llvoavatarself.h"

constexpr F32 MAX_LEGAL_OFFSET = 3.f;
constexpr F32 MAX_LEGAL_SIZE = 64.f;

// Static members
boost::signals2::connection	LLVOAvatarPuppet::sRegionChangedSlot;
LLVOAvatarPuppet::signaled_anim_map_t LLVOAvatarPuppet::sSignaledAnimMap;

LLVOAvatarPuppet::LLVOAvatarPuppet(const LLUUID& id, LLViewerRegion* regionp)
:	LLVOAvatar(id, regionp),
	mRootVolp(NULL),
	mGlobalScale(1.f),
	mScaleConstraintFixup(1.f),
#if LL_ANIMESH_VPARAMS
	mBodySizeHeightFix(0.f),
#endif
	mPlaying(false),
	mMarkedForDeath(false),
	mRegionChanged(false)
{
	mIsDummy = true;
	mEnableDefaultMotions = false;
}

//virtual
void LLVOAvatarPuppet::initInstance()
{
	LLVOAvatar::initInstance();

	createDrawable();
	updateJointLODs();
	updateGeometry(mDrawable);
	hideSkirt();

#if LL_ANIMESH_VPARAMS
	computeBodySize();
	LL_DEBUGS("Puppets") << "Initial body size Z is:  " << mBodySize[2]
						 << LL_ENDL;
#endif
}

LLVOAvatarPuppet* LLVOAvatarPuppet::createAvatarPuppet(LLVOVolume* obj)
{
	LLVOAvatarPuppet* puppet =
		(LLVOAvatarPuppet*)gObjectList.createObjectViewer(LL_PCODE_LEGACY_AVATAR,
														  gAgent.getRegion(),
														  CO_FLAG_PUPPET_AVATAR);
	if (puppet)
	{
		puppet->mRootVolp = obj;
		// Sync up position/rotation with object
		puppet->matchVolumeTransform();
	}
	return puppet;
}

//virtual
U32 LLVOAvatarPuppet::getPartitionType() const
{
	return LLViewerRegion::PARTITION_PUPPET;
}

void LLVOAvatarPuppet::getNewConstraintFixups(LLVector3& new_pos_fixup,
											  F32& new_scale_fixup) const
{
	static LLCachedControl<F32> max_offset(gSavedSettings,
										   "AnimatedObjectsMaxLegalOffset");
	F32 max_legal_offset = max_offset >= 0.f ? max_offset : MAX_LEGAL_OFFSET;

	static LLCachedControl<F32> max_size(gSavedSettings,
										 "AnimatedObjectsMaxLegalSize");
	F32 max_legal_size = max_size >= 1.f ? max_size : MAX_LEGAL_SIZE;

	new_pos_fixup.clear();
	new_scale_fixup = 1.f;

	if (!LLVector3::boxValidAndNonZero(getLastAnimExtents()))
	{
		return;
	}

	// The goal here is to ensure that the extent of the avatar's bounding box
	// does not wander too far from the official position of the corresponding
	// volume. We do this by tracking the distance and applying a correction to
	// the puppet avatar position if needed.
	const LLVector3* extents = getLastAnimExtents();
	LLVector3 unshift_extents[2];
	unshift_extents[0] = extents[0] - mPositionConstraintFixup;
	unshift_extents[1] = extents[1] - mPositionConstraintFixup;
	LLVector3 box_dims = extents[1] - extents[0];
	F32 box_size = llmax(box_dims[0], box_dims[1], box_dims[2]);
	if (!mRootVolp->isAttachment())
	{
		LLVector3 pos_box_offset =
			LLVector3::pointToBoxOffset(mRootVolp->getRenderPosition(),
										unshift_extents);
		F32 offset_dist = pos_box_offset.length();
		F32 target_dist = offset_dist - max_legal_offset;
		if (target_dist > 0.f && offset_dist > 0.f)
		{
			new_pos_fixup = (target_dist / offset_dist) * pos_box_offset;
		}
	}
	if (box_size / mScaleConstraintFixup > max_legal_size)
	{
		new_scale_fixup = mScaleConstraintFixup * max_legal_size / box_size;
	}
}

void LLVOAvatarPuppet::matchVolumeTransform()
{
	if (!mRootVolp || isDead())
	{
		return;
	}

	if (mRegionChanged)
	{
		mRegionChanged = false;
	}
	else
	{
		LLVector3 new_pos_fixup;
		F32 new_scale_fixup;
		getNewConstraintFixups(new_pos_fixup, new_scale_fixup);
		mPositionConstraintFixup = new_pos_fixup;
		mScaleConstraintFixup = new_scale_fixup;
	}

#if LL_ANIMESH_VPARAMS
	// This needs to be validated against constraint logic
	LLVector3 hover_param_offset =
		getVisualParamWeight(LLAvatarAppearanceDefines::AVATAR_HOVER) *
		LLVector3::z_axis;
	LLVector3 body_size_offset = mBodySizeHeightFix * LLVector3::z_axis;
#endif

	if (mRootVolp->isAttachment())
	{
		LLVOAvatar* avatarp = mRootVolp->getAvatarAncestor();
		if (!avatarp)
		{
			llwarns_once << "Cannot find attached avatar for puppet: "
						 << std::hex << (uintptr_t)this << std::dec << llendl;
			return;
		}

		LLViewerJointAttachment* attachp =
			avatarp->getTargetAttachmentPoint(mRootVolp);
		if (attachp)	// Paranoia
		{
			if (getRegion())
			{
				setPositionAgent(mRootVolp->getRenderPosition());
			}
			attachp->updateWorldPRSParent();
			LLVector3 joint_pos = attachp->getWorldPosition();
			LLQuaternion joint_rot = attachp->getWorldRotation();
			LLDrawable* drawblep = mRootVolp->mDrawable;
			LLVector3 obj_pos;
			LLQuaternion obj_rot;
			if (drawblep)
			{
				obj_pos = drawblep->getPosition();
				obj_rot = drawblep->getRotation();
			}
			else
			{
				obj_pos = mRootVolp->getPosition();
				obj_rot = mRootVolp->getRotation();
			}
			obj_pos.rotVec(joint_rot);
			mRoot->setWorldPosition(obj_pos + joint_pos);
			mRoot->setWorldRotation(obj_rot * joint_rot);
			setRotation(mRoot->getRotation());
			setGlobalScale(mScaleConstraintFixup);
		}
		return;
	}

	LLVector3 vol_pos = mRootVolp->getRenderPosition();

	LLQuaternion obj_rot;
	LLDrawable* drawable = mRootVolp->mDrawable;
	if (drawable)
	{
		obj_rot = drawable->getRotation();
	}
	else
	{
		obj_rot = mRootVolp->getRotation();
	}
#if 1
	const LLMeshSkinInfo* skin_info = mRootVolp->getSkinInfo();
	if (skin_info)
	{
		LLQuaternion bind_rot;
		bind_rot =
			LLSkinningUtil::getUnscaledQuaternion(skin_info->mBindShapeMatrix);
		setRotation(bind_rot * obj_rot);
		mRoot->setWorldRotation(bind_rot * obj_rot);
	}
	else
#endif
	{
		setRotation(obj_rot);
		mRoot->setWorldRotation(obj_rot);
	}
	if (getRegion())
	{
		setPositionAgent(vol_pos);
	}
#if LL_ANIMESH_VPARAMS
	mRoot->setPosition(vol_pos + mPositionConstraintFixup + body_size_offset +
					   hover_param_offset);
#else
	mRoot->setPosition(vol_pos + mPositionConstraintFixup);
#endif

	setGlobalScale(mScaleConstraintFixup);
}

void LLVOAvatarPuppet::setGlobalScale(F32 scale)
{
	if (scale <= 0.f)
	{
		llwarns << "invalid global scale " << scale << llendl;
		return;
	}

	if (mGlobalScale == 0.f)	// This should never happen
	{
		mGlobalScale = 1.f;
	}

	if (scale != mGlobalScale)
	{
		F32 adjust_scale = scale / mGlobalScale;
		LL_DEBUGS("Puppets") << "scale " << scale << " adjustment "
							 << adjust_scale << LL_ENDL;
		// AXON: should we be scaling from the pelvis or the root ?
		recursiveScaleJoint(mPelvisp, adjust_scale);
		mGlobalScale = scale;
	}
}

void LLVOAvatarPuppet::recursiveScaleJoint(LLJoint* joint, F32 factor)
{
	if (!joint) return;

	joint->setScale(factor * joint->getScale());

	for (S32 i = 0, count = joint->mChildren.size(); i < count; ++i)
	{
		LLJoint* child = joint->mChildren[i];
		if (child)	// Paranoia
		{
			recursiveScaleJoint(child, factor);
		}
	}
}

// Based on LLViewerJointAttachment::setupDrawable(), without the attaching
// part.
void LLVOAvatarPuppet::updateVolumeGeom()
{
	if (!mRootVolp)
	{
		return;
	}

	LLDrawable* drawable = mRootVolp->mDrawable;
 	if (!drawable)
	{
		return;
	}

	if (drawable->isActive())
	{
		drawable->makeStatic(false);
	}
	drawable->makeActive();
	gPipeline.markMoved(drawable);

	// Face may need to change draw pool to/from POOL_HUD
	gPipeline.markTextured(drawable);

	LLViewerObject::const_child_list_t& child_list = mRootVolp->getChildren();
	for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin(),
													  end = child_list.end();
		 iter != end; ++iter)
	{
		LLViewerObject* childp = *iter;
		if (!childp) continue;

		LLDrawable* drawable = childp->mDrawable;
		if (drawable)
		{
			// Face may need to change draw pool to/from POOL_HUD
			gPipeline.markTextured(drawable);
			gPipeline.markMoved(drawable);
		}
	}

	gPipeline.markRebuild(drawable);
	mRootVolp->markForUpdate();

	// Note that attachment overrides are not needed here as they have already
	// been applied at the time the puppet avatar was created in llvovolume.cpp

	matchVolumeTransform();
}

//virtual
void LLVOAvatarPuppet::markDead()
{
	// Normally mRootVolp has already been cleared in unlinkPuppetAvatar(),
	// unless there's some bulk object cleanup happening e.g. on region
	// destruction. In that case the puppet avatar may be killed first.
	if (mRootVolp)
	{
		mRootVolp->mPuppetAvatar = NULL;
		mRootVolp = NULL;
	}
	LLVOAvatar::markDead();
}

//virtual
void LLVOAvatarPuppet::idleUpdate(F64 time)
{
	if (mMarkedForDeath)
	{
		if (!isDead())
		{
			markDead();
		}
	}
	else
	{
		LLVOAvatar::idleUpdate(time);
	}
}

void LLVOAvatarPuppet::getAnimatedVolumes(std::vector<LLVOVolume*>& volumes)
{
	if (!mRootVolp)
	{
		return;
	}

	volumes.push_back(mRootVolp);

	LLViewerObject::const_child_list_t& child_list = mRootVolp->getChildren();
	for (LLViewerObject::const_child_list_t::const_iterator
			iter = child_list.begin(), end = child_list.end();
		 iter != end; ++iter)
	{
		LLViewerObject* childp = *iter;
		if (!childp) continue;	// Paranoia

		LLVOVolume* child_volp = childp->asVolume();
		if (child_volp && child_volp->isAnimatedObject())
		{
			volumes.push_back(child_volp);
		}
	}
}

// This is called after an associated object receives an animation message.
// Combine the signaled animations for all associated objects and process any
// resulting state changes.
void LLVOAvatarPuppet::updateAnimations()
{
	if (!mRootVolp)
	{
		llwarns_once << "No root volume for puppet " << std::hex << this
					 << std::dec << ". Aborted." << llendl;
		return;
	}

	std::vector<LLVOVolume*> volumes;
	getAnimatedVolumes(volumes);

	// Rebuild mSignaledAnimations from the associated volumes.
	LLVOAvatar::anim_map_t anims;
	for (S32 i = 0, count = volumes.size(); i < count; ++i)
	{
		LLVOVolume* volp = volumes[i];
		LLVOAvatar::anim_map_t& sig_anims = sSignaledAnimMap[volp->getID()];
		for (LLVOAvatar::anim_it_t anim_it = sig_anims.begin(),
								   anim_end = sig_anims.end();
			 anim_it != anim_end; ++anim_it)
		{
			LLVOAvatar::anim_it_t found_it = anims.find(anim_it->first);
			if (found_it != anim_end)
			{
				// Animation already present, use the larger sequence id
				anims[anim_it->first] = llmax(found_it->second,
											  anim_it->second);
			}
			else
			{
				// Animation not already present, use this sequence id.
				anims.emplace(anim_it->first, anim_it->second);
			}
		}
	}
	if (!mPlaying)
	{
		mPlaying = true;
#if 0
		if (!mRootVolp->isAnySelected())
#endif
		{
			updateVolumeGeom();
			mRootVolp->recursiveMarkForUpdate();
		}
	}

	mSignaledAnimations.swap(anims);
	processAnimationStateChanges();
}

#if LL_ANIMESH_VPARAMS
//virtual
void LLVOAvatarPuppet::updateVisualParams()
{
	// *FIXME: Axon: should look for changes to *reference* body size (that is,
	// the body size as it would be computed by appearance service/simulator,
	// without considering effects from animations). Currently using overall
	// body size which includes everything.
	if (mBodySize == LLVector3::zero)
	{
		// Set initial value. No offset to update. This should have been set
		// in initInstance()
		llwarns << "Unitialized mBodySize for puppet: " << std::hex
				<< (uintptr_t)this << std::dec << llendl;
		computeBodySize();
		LL_DEBUGS("Puppets") << "Initial body size Z is:  " << mBodySize[2]
							 << LL_ENDL;
		LLVOAvatar::updateVisualParams();
		return;
	}
	F32 orig_pelvis_to_foot = mPelvisToFoot;
	LLVOAvatar::updateVisualParams();
	mBodySizeHeightFix += mPelvisToFoot - orig_pelvis_to_foot;
	LL_DEBUGS("Puppets") << "mBodySizeHeightFix = " << mBodySizeHeightFix
						 << LL_ENDL;
}
#endif

//virtual
LLViewerObject* LLVOAvatarPuppet::lineSegmentIntersectRiggedAttachments(const LLVector4a& start,
																		const LLVector4a& end,
																		S32 face,
																		bool pick_transparent,
																		bool pick_rigged,
																		S32* face_hit,
																		LLVector4a* intersection,
																		LLVector2* tex_coord,
																		LLVector4a* normal,
																		LLVector4a* tangent)
{
	if (!mRootVolp || !lineSegmentBoundingBox(start, end))
	{
		return NULL;
	}

	LLVector4a local_end = end;
	LLVector4a local_intersection;
	if (mRootVolp->lineSegmentIntersect(start, local_end, face,
										pick_transparent, pick_rigged,
										face_hit, &local_intersection,
										tex_coord, normal, tangent))
	{
		local_end = local_intersection;
		if (intersection)
		{
			*intersection = local_intersection;
		}
		return (LLViewerObject*)mRootVolp;
	}

	std::vector<LLVOVolume*> volumes;
	getAnimatedVolumes(volumes);
	for (S32 i = 0, count = volumes.size(); i < count; ++i)
	{
		LLVOVolume* volp = volumes[i];
		if (volp != mRootVolp &&
			volp->lineSegmentIntersect(start, local_end, face,
									   pick_transparent, pick_rigged,
									   face_hit, &local_intersection,
									   tex_coord, normal, tangent))
		{
			local_end = local_intersection;
			if (intersection)
			{
				*intersection = local_intersection;
			}
			return (LLViewerObject*)volp;
		}
	}

	return NULL;
}

//virtual
LLVOAvatar* LLVOAvatarPuppet::getAttachedAvatar()
{
	if (mRootVolp && mRootVolp->isAttachment())
	{
		return mRootVolp->getAvatarAncestor();
	}
	return NULL;
}

//virtual
LLVOAvatar* LLVOAvatarPuppet::getAttachedAvatar() const
{
	if (mRootVolp && mRootVolp->isAttachment())
	{
		return mRootVolp->getAvatarAncestor();
	}
	return NULL;
}

//virtual
bool LLVOAvatarPuppet::shouldRenderRigged() const
{
	if (mRootVolp && mRootVolp->isAttachment())
	{
		LLVOAvatar* avatarp = mRootVolp->getAvatarAncestor();
		if (avatarp)
		{
			return avatarp->shouldRenderRigged();
		}
	}
	return true;
}

//virtual
bool LLVOAvatarPuppet::isImpostor()
{
	if (mMarkedForDeath) return false;

	if (mRootVolp && mRootVolp->isAttachment())
	{
		// Attached animated objects should match state of their attached av.
		LLVOAvatar* avatarp = mRootVolp->getAvatarAncestor();
		if (avatarp)
		{
			return avatarp->isImpostor();
		}
	}
	return LLVOAvatar::isImpostor();
}

//virtual
bool LLVOAvatarPuppet::isTooComplex() const
{
#if 0	// Avatar puppets "jelly-dollifying" does not work anyway...
	if (mMarkedForDeath) return false;

	if (mRootVolp && mRootVolp->isAttachment())
	{
		// NOTE: an attached animesh complexity is already accounted for as any
		// other avatar attachment: should it prove to be too complex, then the
		// avatar it is attached to gets jelly-dollified, and none of its
		// attachments are rendered at all, this animesh included. We therefore
		// do not need to check for attached animesh complexity and can always
		// return false here. HB
		return false;
	}

	// This is a standalone animesh: use complexity...
	return LLVOAvatar::isTooComplex();
#else
	return false;
#endif
}

#if 0	// *TODO ?
//virtual
void LLVOAvatarPuppet::updateDebugText()
{
}
#endif

//static
void LLVOAvatarPuppet::onRegionChanged()
{
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatarPuppet* puppet =
			dynamic_cast<LLVOAvatarPuppet*>(LLCharacter::sInstances[i]);
		if (puppet)
		{
			puppet->mRegionChanged = true;
		}
	}
}
