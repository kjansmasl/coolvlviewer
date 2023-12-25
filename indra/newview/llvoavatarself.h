/**
 * @file llvoavatarself.h
 * @brief Declaration of LLVOAvatarSelf class
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLVOAVATARSELF_H
#define LL_LLVOAVATARSELF_H

#include "llvoavatar.h"

struct LocalTextureData;

// Globals (see also gAgentAvatarp at the end of this file)
extern bool			gAttachmentsListDirty;
extern LLFrameTimer	gAttachmentsTimer;
// Must be declared early since it is used in inlined methods below
extern U32			gMaxSelfAttachments;

constexpr F32 DEFAULT_ATTACHMENT_UPDATE_PERIOD = 0.1f;

class LLVOAvatarSelf final : public LLVOAvatar
{
protected:
	LOG_CLASS(LLVOAvatarSelf);

/******************************************************************************
 **                    INITIALIZATION
 **/

public:
	LLVOAvatarSelf(const LLUUID& id, LLViewerRegion* regionp);

	~LLVOAvatarSelf() override;
	void markDead() override;
	// Called after construction to initialize the class.
	void initInstance() override;

protected:
	bool loadAvatar() override;
	bool loadAvatarSelf();
	bool buildSkeletonSelf(const LLAvatarSkeletonInfo* info);
	bool buildMenus();

/**                    Initialization
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                    INHERITED
 **/

	//-------------------------------------------------------------------------
	// LLViewerObject interface and related
	//-------------------------------------------------------------------------
public:
	void onSimulatorFeaturesReceived(const LLUUID&);
	void updateRegion(LLViewerRegion* regionp) override;
	void idleUpdate(F64 time) override;

	//-------------------------------------------------------------------------
	// LLCharacter interface and related
	//-------------------------------------------------------------------------
	bool hasMotionFromSource(const LLUUID& source_id) override;
	void stopMotionFromSource(const LLUUID& source_id) override;
	void requestStopMotion(LLMotion* motion) override;
	LLJoint* getJoint(U32 key) override;

	bool setVisualParamWeight(const LLVisualParam* which_param, F32 weight,
							  bool upload_bake = false) override;
	bool setVisualParamWeight(const char* param_name, F32 weight,
							  bool upload_bake = false) override;
	bool setVisualParamWeight(S32 index, F32 weight,
							  bool upload_bake = false) override;
	void writeWearablesToAvatar();
	void idleUpdateAppearanceAnimation() override;

	U32 processUpdateMessage(LLMessageSystem* mesgsys, void** user_data,
							 U32 block_num, EObjectUpdateType upd_type,
							 LLDataPacker* dp) override;

#if 0
	void computeBodySize(bool force = false) override;
#endif

private:
	// helper function. Passed in param is assumed to be in avatar's parameter
	// list.
	bool setParamWeight(const LLViewerVisualParam* param, F32 weight,
						bool upload_bake = false);

/*****************************************************************************/

	LLUUID mInitialBakeIDs[LLAvatarAppearanceDefines::BAKED_NUM_INDICES];
	bool mInitialBakesLoaded;

/******************************************************************************
 **                    STATE
 **/

public:
	LL_INLINE bool isSelf() const override		{ return true; }
	bool isValid() const override;

	//-------------------------------------------------------------------------
	// Updates
	//-------------------------------------------------------------------------
	bool updateCharacter() override;
	void idleUpdateTractorBeam();

	//-------------------------------------------------------------------------
	// Loading state
	//-------------------------------------------------------------------------
	bool getIsCloud() override;

	//-------------------------------------------------------------------------
	// Region state
	//-------------------------------------------------------------------------
	LL_INLINE void resetRegionCrossingTimer()	{ mRegionCrossingTimer.reset();	}

private:
	U64				mLastRegionHandle;
	LLFrameTimer	mRegionCrossingTimer;
	S32				mRegionCrossingCount;

/**                    State
 *****************************************************************************/

/******************************************************************************
 **                    RENDERING
 **/

	//-------------------------------------------------------------------------
	// Render beam
	//-------------------------------------------------------------------------
protected:
	bool needsRenderBeam();

private:
	LLPointer<LLHUDEffectSpiral>	mBeam;
	LLFrameTimer					mBeamTimer;
	boost::signals2::connection		mTeleportFinishedSlot;

public:
	void resetHUDAttachments();
	void refreshAttachments();
	void handleTeleportFinished();
#if 0	// Empty
	LL_INLINE void rebuildHUD()					{}
#endif

	LL_INLINE bool isVisuallyMuted() override
	{
		return false;
	}

	LL_INLINE bool isImpostor() override		{ return false; }

/**                    Rendering
 *****************************************************************************/

/******************************************************************************
 **                    TEXTURES
 **/

	//-------------------------------------------------------------------------
	// Loading status
	//-------------------------------------------------------------------------
	bool hasPendingBakedUploads();
	S32 getLocalDiscardLevel(LLAvatarAppearanceDefines::ETextureIndex type,
							 U32 wearable_idx);
	bool areTexturesCurrent();
	bool isLocalTextureDataAvailable(LLViewerTexLayerSet* layersetp);
	bool isLocalTextureDataFinal(LLViewerTexLayerSet* layersetp);
	bool isBakedTextureFinal(LLAvatarAppearanceDefines::EBakedTextureIndex i);
	// If you want to check all textures of a given type, pass
	// gAgentWearables.getWearableCount() for index
	bool isTextureDefined(LLAvatarAppearanceDefines::ETextureIndex type,
						  U32 index) const override;
	bool isTextureVisible(LLAvatarAppearanceDefines::ETextureIndex type,
						  U32 index = 0) const override;
	bool isTextureVisible(LLAvatarAppearanceDefines::ETextureIndex type,
						  LLViewerWearable* wearable) const override;

	//-------------------------------------------------------------------------
	// Local Textures
	//-------------------------------------------------------------------------
	LLViewerFetchedTexture* getLocalTextureGL(LLAvatarAppearanceDefines::ETextureIndex type,
											  U32 index) const;
	const LLUUID& getLocalTextureID(LLAvatarAppearanceDefines::ETextureIndex type,
									U32 index) const;
	void setLocalTextureTE(U8 te, LLViewerTexture* texp, U32 index);
	void setLocalTexture(LLAvatarAppearanceDefines::ETextureIndex type,
						 LLViewerTexture* texp, bool baked_version_exits,
						 U32 index) override;

protected:
	void setBakedReady(LLAvatarAppearanceDefines::ETextureIndex type,
					   bool baked_version_exists, U32 index) override;
	void localTextureLoaded(bool succcess, LLViewerFetchedTexture* texp,
							LLImageRaw* src_imagep, LLImageRaw* aux_src_imagep,
							S32 discard_level, bool is_final, void* userdata);
	void getLocalTextureByteCount(S32* gl_byte_count);
	void addLocalTextureStats(LLAvatarAppearanceDefines::ETextureIndex i,
							  LLViewerFetchedTexture* imagep,
							  F32 texel_area_ratio, bool rendered,
							  bool covered_by_baked) override;
	LLLocalTextureObject* getLocalTextureObject(LLAvatarAppearanceDefines::ETextureIndex i,
												U32 index) const;

private:
	static void onLocalTextureLoaded(bool succcess,
									 LLViewerFetchedTexture* texp,
									 LLImageRaw* src_imagep,
									 LLImageRaw* aux_src_imagep,
									 S32 discard_level, bool is_final,
									 void* userdata);

	void setImage(U8 te, LLViewerTexture* imagep, U32 index) override;
	LLViewerTexture* getImage(U8 te, U32 index) const override;

	//-------------------------------------------------------------------------
	// Baked textures
	//-------------------------------------------------------------------------
public:
	LLAvatarAppearanceDefines::ETextureIndex getBakedTE(const LLViewerTexLayerSet* layerset) const;
	void setNewBakedTexture(LLAvatarAppearanceDefines::EBakedTextureIndex i,
							const LLUUID& uuid);
	void setNewBakedTexture(LLAvatarAppearanceDefines::ETextureIndex i,
							const LLUUID& uuid);
	void setCachedBakedTexture(LLAvatarAppearanceDefines::ETextureIndex i,
							   const LLUUID& uuid);
	void forceBakeAllTextures(bool slam_for_debug = false);
	static void processRebakeAvatarTextures(LLMessageSystem* msg, void**);

protected:
	void removeMissingBakedTextures() override;

	//-------------------------------------------------------------------------
	// Layers
	//-------------------------------------------------------------------------
public:
	void requestLayerSetUploads();
	void requestLayerSetUpload(LLAvatarAppearanceDefines::EBakedTextureIndex i);
	void requestLayerSetUpdate(LLAvatarAppearanceDefines::ETextureIndex i);

	LLViewerTexLayerSet* getLayerSet(LLAvatarAppearanceDefines::EBakedTextureIndex baked_index);
	LLViewerTexLayerSet* getLayerSet(LLAvatarAppearanceDefines::ETextureIndex index);

	//-------------------------------------------------------------------------
	// Composites
	//-------------------------------------------------------------------------
	void invalidateComposite(LLTexLayerSet* layerset, bool upload) override;
	void invalidateAll() override;
	void setCompositeUpdatesEnabled(bool b) override; // Only works for self
	void setCompositeUpdatesEnabled(U32 index, bool b) override;
	bool isCompositeUpdateEnabled(U32 index) override;
	void setupComposites();
	void updateComposites();

	const LLUUID& grabBakedTexture(LLAvatarAppearanceDefines::EBakedTextureIndex baked_index) const;
	bool canGrabBakedTexture(LLAvatarAppearanceDefines::EBakedTextureIndex baked_index) const;

/**                    Textures
 *****************************************************************************/

protected:
	void restoreMeshData() override;

/******************************************************************************
 **                    WEARABLES
 **/

public:
	void wearableUpdated(LLWearableType::EType type, bool upload_result);

protected:
	U32 getNumWearables(LLAvatarAppearanceDefines::ETextureIndex i) const;

	//-------------------------------------------------------------------------
	// Attachments
	//-------------------------------------------------------------------------
public:
	void updateAttachmentVisibility(U32 camera_mode);

	bool isWearingAttachment(const LLUUID& inv_item_id) const;

	LLViewerObject* getWornAttachment(const LLUUID& inv_item_id);
	const std::string getAttachedPointName(const LLUUID& inv_item_id,
										   bool translate = false) const;

	LL_INLINE bool canAttachMoreObjects() const
	{
		return getNumAttachments() < gMaxSelfAttachments;
	}

	LL_INLINE bool canAttachMoreObjects(U32 n) const
	{
		return getNumAttachments() + n <= gMaxSelfAttachments;
	}

	S32 getMaxAnimatedObjectAttachments() const override;

	const LLViewerJointAttachment* attachObject(LLViewerObject* vobj) override;
	bool detachObject(LLViewerObject* vobj) override;

	static bool detachAttachmentIntoInventory(const LLUUID& item_id);

private:
	LLViewerJoint* 		mScreenp; // Special purpose joint for HUD attachments

/**                    WEARABLES
 *****************************************************************************/

/******************************************************************************
 **                    ANIMATIONS
 **/
public:
	void updateMotions(e_update_t update_type) override;
	void setAttachmentUpdatePeriod(F32 period_sec);

	LL_INLINE void setAttachmentUpdateEnabled(bool b)
	{
		mAttachmentUpdateEnabled = b;
	}

	LL_INLINE bool getAttachmentUpdateEnabled() const
	{
		return mAttachmentUpdateEnabled;
	}

private:
	F32					mAttachmentUpdatePeriod; // In seconds
	F32					mAttachmentUpdateExpiry; // In seconds
	bool				mAttachmentUpdateEnabled;

/**                    ANIMATIONS
 *****************************************************************************/

/******************************************************************************
 **                    APPEARANCE
 **/

public:
	static bool canUseServerBaking();

	static void onCustomizeStart();
	static void onCustomizeEnd();

	//-------------------------------------------------------------------------
	// Visibility
	//-------------------------------------------------------------------------
public:
	void sendAppearanceMessage(LLMessageSystem* mesgsys) const;

	// Care and feeding of hover height.
	static bool useAvatarHoverHeight();
	void scheduleHoverUpdate();
	void setHoverOffset(const LLVector3& hover_offset,
						bool send_update = true) override;

	bool shouldRenderRigged() const override;

public:
	LLTimer				mOffsetUpdateDelay;

private:
	void setHoverIfRegionEnabled();
	void sendHoverHeight() const;

private:
	mutable LLVector3	mLastHoverOffsetSent;

/**                    Appearance
 *****************************************************************************/

/******************************************************************************
 **                    DIAGNOSTICS
 **/

public:
	static void dumpTotalLocalTextureByteCount();
	void dumpLocalTextures();

	struct LLAvatarTexData
	{
		LLAvatarTexData(const LLUUID& id,
						LLAvatarAppearanceDefines::ETextureIndex index)
		: 	mAvatarID(id),
			mIndex(index)
		{
		}

		LLUUID										mAvatarID;
		LLAvatarAppearanceDefines::ETextureIndex	mIndex;
	};
};

// Another global
extern LLPointer<LLVOAvatarSelf> gAgentAvatarp;

LL_INLINE bool isAgentAvatarValid()
{
	return gAgentAvatarp.notNull() && !gAgentAvatarp->isDead() &&
		   gAgentAvatarp->getRegion();
}

#endif // LL_VO_AVATARSELF_H
