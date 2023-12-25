/**
 * @file llvoavatarpuppet.h
 * @brief Special dummy avatar used to drive rigged meshes.
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

#ifndef LL_VOAVATARPUPPET_H
#define LL_VOAVATARPUPPET_H

#include "boost/signals2.hpp"

#include "llvoavatar.h"
#include "llvovolume.h"

class LLVOAvatarPuppet final : public LLVOAvatar
{
	LOG_CLASS(LLVOAvatarPuppet);

public:
	LLVOAvatarPuppet(const LLUUID& id, LLViewerRegion* regionp);

	void initInstance() override;

	bool isImpostor() override;

	void matchVolumeTransform();
	void updateVolumeGeom();

	void setGlobalScale(F32 scale);
	void recursiveScaleJoint(LLJoint* joint, F32 factor);
	static LLVOAvatarPuppet* createAvatarPuppet(LLVOVolume* obj);

	// Delayed kill so we do not make graphics pipeline unhappy calling
	// markDead() inside other graphics pipeline operations.
	LL_INLINE void markForDeath()
	{
		mMarkedForDeath = true;
		mRootVolp = NULL;
	}

	void markDead() override;

	bool isPuppetAvatar() const override			{ return true; } 

	LLVOAvatar* getAttachedAvatar() override;
	LLVOAvatar* getAttachedAvatar() const override;

	void idleUpdate(F64 time) override;

	U32 getPartitionType() const override;

	LL_INLINE bool useImpostors() override			{ return sUsePuppetImpostors; }
	LL_INLINE U32 getMaxNonImpostors() override		{ return sMaxNonImpostorsPuppets; }

	bool isTooComplex() const override;

#if LL_ANIMESH_VPARAMS
	void updateVisualParams() override;
#endif

	void getAnimatedVolumes(std::vector<LLVOVolume*>& volumes);
	void updateAnimations();

	LLViewerObject*	lineSegmentIntersectRiggedAttachments(
									const LLVector4a& start,
									const LLVector4a& end,
									S32 face = -1,
									bool pick_transparent = false,
									bool pick_rigged = false,
									S32* face_hit = NULL,
									LLVector4a* intersection = NULL,
									LLVector2* tex_coord = NULL,
									LLVector4a* normal = NULL,
									LLVector4a* tangent = NULL) override;
#if 0	// *TODO ?
	void updateDebugText() override;
#endif

	bool shouldRenderRigged() const override;

	static void onRegionChanged();

	// NOTE: DO NOT convert to safe_hmap: it would crash when using anything
	// else than std or boost containers... HB
	typedef boost::unordered_map<LLUUID,
								 LLVOAvatar::anim_map_t> signaled_anim_map_t;

	LL_INLINE static signaled_anim_map_t& getSignaledAnimMap()
	{
		return sSignaledAnimMap;
	}

private:
	void getNewConstraintFixups(LLVector3& pos_fixup, F32& scale_fixup) const;

public:
	LLVOVolume*							mRootVolp;
	bool								mPlaying;

	static boost::signals2::connection	sRegionChangedSlot;

private:
	bool								mMarkedForDeath;
	bool								mRegionChanged;
	F32									mGlobalScale;
	F32									mScaleConstraintFixup;
#if LL_ANIMESH_VPARAMS
	F32									mBodySizeHeightFix;
#endif
	LLVector3							mPositionConstraintFixup;

	// Stores information about previously requested animations, by object Id.
	// Pointlessly implemented as a LLSingleton (LLObjectSignaledAnimationMap)
	// in LL's viewer; the motto at LL is "why making things simple and fast,
	// when you can make them complicated and slow ?"... HB
	static signaled_anim_map_t			sSignaledAnimMap;
};

#endif //LL_VOAVATARPUPPET_H
