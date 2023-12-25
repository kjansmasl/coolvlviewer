/**
 * @file llvoavatar.h
 * @brief Declaration of LLVOAvatar class which is a derivation of
 * LLViewerObject
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

#ifndef LL_VOAVATAR_H
#define LL_VOAVATAR_H

#include <deque>

#include "boost/signals2.hpp"
#include "boost/unordered_map.hpp"

#include "llavatarappearancedefines.h"
#include "llcharacter.h"
#include "lldriverparam.h"
#include "llmaterialtable.h"
#include "llrendertarget.h"
#include "lltexglobalcolor.h"
#include "llwearabletype.h"

#include "llchat.h"
#include "llmutelist.h"
#include "llviewerjointattachment.h"
#include "llviewerjointmesh.h"
#include "llviewertexlayer.h"

class LLHUDEffectSpiral;
class LLHUDText;
class LLMeshSkinInfo;
class LLPuppetMotion;
class LLViewerWearable;
class LLVoiceVisualizer;
struct LLAppearanceMessageContents;

//------------------------------------------------------------------------
// LLVOAvatar
//------------------------------------------------------------------------
class LLVOAvatar : public LLAvatarAppearance, public LLViewerObject,
				   public LLMuteListObserver, public boost::signals2::trackable
{
protected:
	LOG_CLASS(LLVOAvatar);

public:
	friend class LLVOAvatarSelf;

/******************************************************************************
 **                                                                          **
 **                    INITIALIZATION
 **/

public:
	LL_INLINE void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		ll_aligned_free_16(ptr);
	}

	LLVOAvatar(const LLUUID& id, LLViewerRegion* regionp);

	void markDead() override;

	// Setups data that is only initialized once per class.
	static void initClass();

	// Cleanups data that is only initialized once per class.
	static void cleanupClass();

	// Called after construction to initialize the class.
	void initInstance() override;

protected:
	~LLVOAvatar() override;

/**                    Initialization
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    INHERITED
 **/

	// LLMuteListObserver interface
	void onChange() override;

	//--------------------------------------------------------------------
	// LLViewerObject interface and related
	//--------------------------------------------------------------------
public:
	void updateGL() override;

	LL_INLINE LLVOAvatar* asAvatar() override				{ return this; }

	U32 processUpdateMessage(LLMessageSystem* mesgsys, void** user_data,
							 U32 block_num, EObjectUpdateType upd_type,
							 LLDataPacker* dp) override;

	void idleUpdate(F64 time) override;

	bool updateLOD() override;
	bool updateJointLODs();
	void updateLODRiggedAttachments();

	// Whether this object needs to do an idleUpdate:
	LL_INLINE bool isActive() const override				{ return true; }
	void bakedTextureOriginCounts(S32& sb_count, S32& host_count,
								  S32& both_count, S32& neither_count);
	void updateTextures() override;
	LLViewerFetchedTexture* getBakedTextureImage(U8 te, const LLUUID& id);
	LLViewerTexture* getBakedTexture(U8 te);
	// If setting a baked texture, need to request it from a non-local sim:
	S32 setTETexture(U8 te, const LLUUID& uuid) override;
	void onShift(const LLVector4a& shift_vector) override;
	U32 getPartitionType() const override;
	const LLVector3 getRenderPosition() const override;
	void updateDrawable(bool force_damped) override;
	LLDrawable* createDrawable() override;
	LL_INLINE bool updateGeometry(LLDrawable*) override		{ return true; }
	void setPixelAreaAndAngle() override;
	void updateRegion(LLViewerRegion*) override				{}
	void updateSpatialExtents(LLVector4a& min, LLVector4a& max) override;
	void calculateSpatialExtents(LLVector4a& new_min, LLVector4a& new_max);

	// 'face' is the face to check, -1 = ALL_SIDES
	// 'face_hit' is the face that got hit
	// 'intersection' returns the intersection point
	// 'tex_coord' returns the texture coordinates of the intersection point
	// 'normal' returns the surface normal at the intersection point
	// 'tangent'returns the surface tangent at the intersection point
	bool lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
							  S32 face = -1, bool pick_transparent = false,
							  bool pick_rigged = false, S32* face_hit = NULL,
							  LLVector4a* intersection = NULL,
							  LLVector2* tex_coord = NULL,
							  LLVector4a* normal = NULL,
							  LLVector4a* tangent = NULL) override;

	virtual LLViewerObject*	lineSegmentIntersectRiggedAttachments(
								const LLVector4a& start,
								const LLVector4a& end,
								S32 face = -1,
								bool pick_transparent = false,
								bool pick_rigged = false,
								S32* face_hit = NULL,
								LLVector4a* intersection = NULL,
								LLVector2* tex_coord = NULL,
								LLVector4a* normal = NULL,
								LLVector4a* tangent = NULL);
protected:
	bool allLocalTexturesCompletelyDownloaded() const;
	bool allBakedTexturesCompletelyDownloaded() const;
	bool allTexturesCompletelyDownloaded(uuid_list_t& ids) const;
	void collectLocalTextureUUIDs(uuid_list_t& ids) const;
	void collectBakedTextureUUIDs(uuid_list_t& ids) const;
	void collectTextureUUIDs(uuid_list_t& ids);
	void releaseOldTextures();

	//--------------------------------------------------------------------
	// LLCharacter interface and related
	//--------------------------------------------------------------------
public:
	LLVector3 getCharacterPosition() override;
	LLQuaternion getCharacterRotation() override;
	LLVector3 getCharacterVelocity() override;
	LLVector3 getCharacterAngularVelocity() override;

	LLUUID remapMotionID(const LLUUID& id);
	bool startMotion(const LLUUID& id, F32 time_offset = 0.f) override;
	bool stopMotion(const LLUUID& id, bool stop_immediate = false) override;

	LL_INLINE virtual bool hasMotionFromSource(const LLUUID& source_id)
	{
		return false;
	}

	LL_INLINE virtual void stopMotionFromSource(const LLUUID& source_id)
	{
	}

	void requestStopMotion(LLMotion* motion) override;
	LLMotion* findMotion(const LLUUID& id) const;
	void startDefaultMotions();

	LLJoint* getJoint(U32 key) override;

	void rebuildAttachmentOverrides();
	void updateAttachmentOverrides();
	void addAttachmentOverridesForObject(LLViewerObject* vo,
										 uuid_list_t* meshes_seen = NULL,
										 bool recursive = true);
	void removeAttachmentOverridesForObject(LLViewerObject* vo);
	void removeAttachmentOverridesForObject(const LLUUID& mesh_id);
	bool jointIsRiggedTo(U32 joint_key);
	void clearAttachmentOverrides();

	LL_INLINE const LLUUID&	getID() override			{ return mID; }

	void addDebugText(const std::string& text) override;
	F32 getTimeDilation() override;
	void getGround(const LLVector3& in_pos, LLVector3& out_pos,
				   LLVector3& out_norm) override;
	F32 getPixelArea() const override;
	LLVector3d getPosGlobalFromAgent(const LLVector3& position) override;
	LLVector3 getPosAgentFromGlobal(const LLVector3d& position) override;
	void updateVisualParams() override;

	void updateRiggingInfo() override;

private:
	uuid_list_t					mActiveOverrideMeshes;

	// Used by updateRiggingInfo() only, to detect rigging changes in meshes
	// or their LOD. Implemented with two vectors replacing mLastRiggingInfoKey
	// to avoid costly maps reallocations and comparisons. HB

	uuid_vec_t					mLastRiggingInfoIDs;
	std::vector<S32>			mLastRiggingInfoLODs;

	// Replaces a stack-allocated vector that would end up being reallocated
	// and resized in a very costly way at each call. HB
	std::vector<LLVOVolume*>	mTempVolumes;

/**                    Inherited
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    STATE
 **/

public:
	bool isValid() const override;
	// True if this avatar is for this viewer's agent
	LL_INLINE bool isSelf() const override			{ return false; }
	// True if this avatar is for UI preview floaters (no associated user)
	virtual LL_INLINE bool isUIAvatar() const		{ return false; }
	// True if this avatar is a puppet (no associated user)
	virtual LL_INLINE bool isPuppetAvatar() const	{ return false; }

	virtual LL_INLINE LLVOAvatar* getAttachedAvatar()
	{
		return NULL;
	}

	virtual LL_INLINE LLVOAvatar* getAttachedAvatar() const
	{
		return NULL;
	}

private: // Aligned members
	alignas(16) LLVector4a mImpostorExtents[2];

	//--------------------------------------------------------------------
	// Updates
	//--------------------------------------------------------------------
private:
	void updateFootstepSounds();
	void computeUpdatePeriod(bool& visible);
	void updateTimeStep();
	void updateRootPositionAndRotation(F32 speed, bool sat_on_ground);
	void accountRenderComplexityForObject(LLViewerObject* object, U32& cost);

public:
	virtual bool updateCharacter();
	void idleUpdateVoiceVisualizer(bool voice_enabled);
	void idleUpdateMisc(bool detailed_update);
	virtual void idleUpdateAppearanceAnimation();
	void idleUpdateLipSync();
	void idleUpdateLoadingEffect();
	void idleUpdateWindEffect();
	void idleUpdateNameTag(const LLVector3& root_pos_last);
	void clearNameTag();
	void deleteNameTag();
	static void invalidateNameTag(const LLUUID& agent_id);
	// Forces all name tags to rebuild, useful when display names turned on/off
	static void invalidateNameTags();

	void idleUpdateRenderComplexity();
	void calculateUpdateRenderComplexity();

	LL_INLINE void updateVisualComplexity()		{ mVisualComplexityStale = true; }
	LL_INLINE U32 getVisualComplexity()			{ return mVisualComplexity; }
	LL_INLINE F32 getAttachmentSurfaceArea()	{ return mAttachmentSurfaceArea; }

	LL_INLINE void addAttachmentArea(F32 delta)	{ mAttachmentSurfaceArea += delta; }

	LL_INLINE void subtractAttachmentArea(F32 delta)
	{
		if (mAttachmentSurfaceArea <= delta)
		{
			mAttachmentSurfaceArea = 0.f;
		}
		else
		{
			mAttachmentSurfaceArea -= delta;
		}
	}

	LL_INLINE U32 getAttachmentSurfaceBytes()	{ return mAttachmentGeometryBytes; }

	LL_INLINE void addAttachmentBytes(U32 dlt)	{ mAttachmentGeometryBytes += dlt; }

	LL_INLINE void subtractAttachmentBytes(U32 delta)
	{
		if (mAttachmentGeometryBytes < delta)
		{
			mAttachmentGeometryBytes = 0;
		}
		else
		{
			mAttachmentGeometryBytes -= delta;
		}
	}

	LL_INLINE LLColor4& getMutedAVColor()		{ return mMutedAVColor; }

	LL_INLINE virtual bool useImpostors()		{ return sUseImpostors; }
	LL_INLINE virtual U32 getMaxNonImpostors()	{ return sMaxNonImpostors; }

	void idleUpdateBelowWater();

	static void updateSettings();

	//--------------------------------------------------------------------
	// Static preferences (controlled by user settings/menus)
	//--------------------------------------------------------------------
public:
	static S32		sRenderName;
	static S32		sNumLODChangesThisFrame;
	static S32		sNumVisibleChatBubbles;
	// Distance at which avatars will render:
	static F32		sRenderDistance;
	static F32		sLODFactor;				// User-settable LOD factor
	static F32		sPhysicsLODFactor;		// User-settable physics LOD factor
	static bool		sRenderGroupTitles;
	static bool		sShowAnimationDebug;	// Show animation debug info
	static bool		sUseImpostors;			// Use impostors for far avatars
	static bool		sUsePuppetImpostors;	// Use impostors for avatar puppets
	static U32		sMaxNonImpostors;
	static U32		sMaxNonImpostorsPuppets;
	static bool		sVisibleInFirstPerson;
	static bool		sDebugInvisible;
	static bool		sShowAttachmentPoints;
	static bool		sAvatarPhysics;			// true to enable avatar physics
	// Output total number of joints being touched for each avatar:
	static bool		sJointDebug;

	static std::string sAgentAppearanceServiceURL;

	//--------------------------------------------------------------------
	// Region state
	//--------------------------------------------------------------------
public:
	const LLHost& getObjectHost() const;

	//--------------------------------------------------------------------
	// Loading state
	//--------------------------------------------------------------------
public:
	bool isFullyLoaded(bool truly = false) const;
	virtual bool isTooComplex() const;
	bool visualParamWeightsAreDefault();
	bool sendAvatarTexturesRequest(bool force = false);

protected:
	virtual bool getIsCloud();
	bool updateIsFullyLoaded();
	bool processFullyLoadedChange(bool loading);
	void updateRuthTimer(bool loading);
	F32 calcMorphAmount();

private:
	bool			mFullyLoaded;
	bool			mPreviousFullyLoaded;
	bool			mFullyLoadedInitialized;
	S32				mFullyLoadedFrameCounter;
	LLFrameTimer	mFullyLoadedTimer;
	LLFrameTimer	mRuthTimer;

protected:
	LLFrameTimer    mInvisibleTimer;

/**                    State
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    SKELETON
 **/

protected:
	// Returns LLViewerJoint
	LLAvatarJoint* createAvatarJoint() override;

	// Returns LLViewerJointMesh
	LLAvatarJointMesh* createAvatarJointMesh() override;

public:
	void updateHeadOffset();
	void postPelvisSetRecalc();

	bool loadSkeletonNode() override;
	void buildCharacter() override;

	void initAttachmentPoints(bool ignore_hud_joints = false);
	void resetVisualParams();

public:
	U32				mLastSkeletonSerialNum;

/**                    Skeleton
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    RENDERING
 **/

public:
	enum VisualMuteSettings
	{
		AV_RENDER_NORMALLY	= 0,
		AV_DO_NOT_RENDER	= 1,
		AV_ALWAYS_RENDER	= 2
	};

	void setVisualMuteSettings(VisualMuteSettings value);

	LL_INLINE VisualMuteSettings getVisualMuteSettings()
	{
		return sUseImpostors? mVisuallyMuteSetting : AV_RENDER_NORMALLY;
	}

	virtual bool isVisuallyMuted();

	LL_INLINE bool isInMuteList()					{ return mCachedMute; }
//MK
	LL_INLINE bool isRLVMuted()						{ return mCachedRLVMute; }
//mk

	U32 renderImpostor(LLColor4U color = LLColor4U(255, 255, 255, 255),
					   S32 diffuse_channel = 0);
	U32 renderRigid();
	U32 renderSkinned();
	LL_INLINE F32 getLastSkinTime()					{ return mLastSkinTime; }
	U32 renderTransparent(bool first_pass);
	void renderCollisionVolumes();
	void renderBones(const std::string& selected_joint = LLStringUtil::null);
	void renderJoints();

	void setMinimapColor(const LLColor4& color);

	LL_INLINE const LLColor4& getMinimapColor() const
	{
		return mMinimapColor;
	}

	static const LLColor4& getMinimapColor(const LLUUID& id);

	void setNameTagColor(const LLColor4& color);

	LL_INLINE void setRadarColor(const LLColor4& color)
	{
		mRadarColor = color;
	}

	LL_INLINE const LLColor4& getRadarColor() const
	{
		return mRadarColor;
	}

	static void	deleteCachedImages(bool clearAll = true);
	static void	destroyGL();
	static void	restoreGL();

private:
	bool shouldAlphaMask();

public:
	S32					mSpecialRenderMode;		// Special lighting

	// Estimated surface area of attachments:
	F32					mAttachmentSurfaceArea;
	// Estimated bytes used by attachments
	U32					mAttachmentGeometryBytes;

private:
	// value of gFrameTimeSeconds at last skin update
	F32					mLastSkinTime;

	S32					mUpdatePeriod;
	// number of faces generated when creating the avatar drawable, does not
	// include splitted faces due to long vertex buffer.
	S32					mNumInitFaces;

	// Time to update mCachedVisualMute
	F32					mCachedVisualMuteUpdateTime;

	// Cached mute flags and description for this avatar
	S32					mCachedMuteFlags;
	std::string			mCachedMuteDesc;

	LLColor4			mMutedAVColor;
	LLColor4			mMinimapColor;
	LLColor4			mNameTagColor;
	LLColor4			mRadarColor;

	VisualMuteSettings	mVisuallyMuteSetting;

	// avatar has been animated and verts have not been updated
	bool				mNeedsSkin;

	// Cached return values for mutes checking functions
	bool				mCachedVisualMute;
	bool				mCachedMute;
//MK
	bool				mCachedRLVMute;
//mk

	mutable bool		mVisualComplexityStale;
	mutable U32			mVisualComplexity;
	F32					mComplexityUpdateTime;

	typedef fast_hmap<LLUUID, LLColor4> colors_map_t;
	static colors_map_t	sMinimapColorsMap;

	//--------------------------------------------------------------------
	// Morph masks
	//--------------------------------------------------------------------
public:
	void applyMorphMask(U8* tex_data, S32 width, S32 height, S32 num_comp,
						LLAvatarAppearanceDefines::EBakedTextureIndex index =
							LLAvatarAppearanceDefines::BAKED_NUM_INDICES) override;
#if 0
	bool morphMaskNeedsUpdate(LLAvatarAppearanceDefines::EBakedTextureIndex index =
								LLAvatarAppearanceDefines::BAKED_NUM_INDICES);
#endif

	//--------------------------------------------------------------------
	// Global colors
	//--------------------------------------------------------------------
public:
	void onGlobalColorChanged(const LLTexGlobalColor* global_color,
							  bool upload_bake) override;

	//--------------------------------------------------------------------
	// Visibility
	//--------------------------------------------------------------------
//MK
	LL_INLINE bool getVisible()					{ return mVisible; }
//mk

protected:
	void updateVisibility();

private:
	U32			mVisibilityRank;
	bool		mVisible;

	//--------------------------------------------------------------------
	// Impostors
	//--------------------------------------------------------------------
public:
	virtual bool isImpostor();

	LL_INLINE bool needsImpostorUpdate() const	{ return mNeedsImpostorUpdate; }

	LL_INLINE const LLVector3& getImpostorOffset() const
	{
		return mImpostorOffset;
	}

	LL_INLINE void setImpostorDim(const LLVector2& dim)
	{
		mImpostorDim = dim;
	}

	LL_INLINE const LLVector2& getImpostorDim() const
	{
		return mImpostorDim;
	}

	void getImpostorValues(LLVector4a* extents, LLVector3& angle,
						   F32& distance) const;
	void cacheImpostorValues();

	LL_INLINE const LLVector3* getLastAnimExtents() const
	{
		return mLastAnimExtents;
	}

	static void resetImpostors();
	static void updateImpostors();

public:
	LLRenderTarget		mImpostor;
	bool				mNeedsImpostorUpdate;

private:
	LLVector3			mImpostorOffset;
	LLVector2			mImpostorDim;
	bool				mNeedsAnimUpdate;
	bool				mNeedsExtentUpdate;
	S32					mNextFrameForExtentUpdate;
	LLVector3			mImpostorAngle;
	F32					mImpostorDistance;
	F32					mImpostorPixelArea;
	LLVector3			mLastAnimExtents[2];
	LLVector3			mLastAnimBasePos;

	//--------------------------------------------------------------------
	// Wind rippling in clothes
	//--------------------------------------------------------------------
public:
	LLVector4		mWindVec;
	F32				mRipplePhase;
	bool			mBelowWater;

private:
	F32				mWindFreq;
	LLFrameTimer	mRippleTimer;
	F32				mRippleTimeLast;
	LLVector3		mRippleAccel;
	LLVector3		mLastVel;

	//--------------------------------------------------------------------
	// Culling
	//--------------------------------------------------------------------
public:
	static void cullAvatarsByPixelArea();
	LL_INLINE bool isCulled() const				{ return mCulled; }

	LL_INLINE static void setAvatarCullingDirty()
	{
		sAvatarCullingDirty = true;
	}

	LL_INLINE static bool avatarCullingDirty()	{ return sAvatarCullingDirty; }

private:
	bool			mCulled;
	static bool		sAvatarCullingDirty;

/**                    Rendering
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    TEXTURES
 **/

	//--------------------------------------------------------------------
	// Loading status
	//--------------------------------------------------------------------
public:
	bool isTextureDefined(LLAvatarAppearanceDefines::ETextureIndex type,
						  U32 index = 0) const override;
	virtual bool isTextureVisible(LLAvatarAppearanceDefines::ETextureIndex type,
								  U32 index = 0) const;
	virtual bool isTextureVisible(LLAvatarAppearanceDefines::ETextureIndex type,
								  LLViewerWearable* wearable) const;

protected:
	bool isFullyBaked();

	//--------------------------------------------------------------------
	// Baked textures
	//--------------------------------------------------------------------
public:
	// Return LLViewerTexLayerSet
	LLTexLayerSet* createTexLayerSet() override;
	// ! BACKWARDS COMPATIBILITY !
	void releaseComponentTextures();

protected:
	static void onBakedTextureMasksLoaded(bool success,
										  LLViewerFetchedTexture* src_vi,
										  LLImageRaw* src,
										  LLImageRaw* aux_src,
										  S32 discard_level,
										  bool is_final,
										  void* userdata);
	static void onInitialBakedTextureLoaded(bool success,
											LLViewerFetchedTexture* src_vi,
											LLImageRaw* src,
											LLImageRaw* aux_src,
											S32 discard_level,
											bool is_final,
											void* userdata);
	static void onBakedTextureLoaded(bool success,
									 LLViewerFetchedTexture* src_vi,
									 LLImageRaw* src,
									 LLImageRaw* aux_src,
									 S32 discard_level,
									 bool is_final,
									 void* userdata);

	LL_INLINE virtual void removeMissingBakedTextures()		{}

	void useBakedTexture(const LLUUID& id);

	LL_INLINE LLViewerTexLayerSet* getTexLayerSet(U32 index)
	{
		LLTexLayerSet* layer = mBakedTextureDatas[index].mTexLayerSet;
		return layer ? layer->asViewerTexLayerSet() : NULL;
	}

	uuid_list_t	mTextureIDs;
	uuid_list_t	mCallbackTextureList;
	bool		mLoadedCallbacksPaused;

	//--------------------------------------------------------------------
	// Local Textures
	//--------------------------------------------------------------------
protected:
	virtual void setLocalTexture(LLAvatarAppearanceDefines::ETextureIndex type,
								 LLViewerTexture* tex,
								 bool baked_version_exists, U32 index = 0);
	virtual void addLocalTextureStats(LLAvatarAppearanceDefines::ETextureIndex type,
									  LLViewerFetchedTexture* imagep,
									  F32 texel_area_ratio, bool rendered,
									  bool covered_by_baked);
	// MULTI-WEARABLE: make self-only ?
	virtual void setBakedReady(LLAvatarAppearanceDefines::ETextureIndex type,
							   bool baked_version_exists, U32 index = 0);

	//--------------------------------------------------------------------
	// Texture accessors
	//--------------------------------------------------------------------
private:
	virtual	void setImage(U8 te, LLViewerTexture* imagep, U32 index);
	virtual LLViewerTexture* getImage(U8 te, U32 index) const;
	std::string getImageURL(U8 te, const LLUUID& uuid);

	void checkTextureLoading();

	//--------------------------------------------------------------------
	// Layers
	//--------------------------------------------------------------------
protected:
	void deleteLayerSetCaches(bool clear_all = true);
	void addBakedTextureStats(LLViewerFetchedTexture* imagep, F32 pixel_area,
							  F32 texel_area_ratio);

	//--------------------------------------------------------------------
	// Composites
	//--------------------------------------------------------------------
public:
	LL_INLINE void invalidateComposite(LLTexLayerSet*, bool) override
	{
	}

	LL_INLINE virtual void invalidateAll()					{}
	LL_INLINE virtual void setCompositeUpdatesEnabled(bool)	{}
	LL_INLINE virtual void setCompositeUpdatesEnabled(U32, bool)	{}
	LL_INLINE virtual bool isCompositeUpdateEnabled(U32)	{ return false; }

	//--------------------------------------------------------------------
	// Static texture/mesh/baked dictionary
	//--------------------------------------------------------------------
public:
	static bool isIndexLocalTexture(LLAvatarAppearanceDefines::ETextureIndex i);
	static bool isIndexBakedTexture(LLAvatarAppearanceDefines::ETextureIndex i);

private:
	LL_INLINE static const LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary* getDictionary()
	{
		return sAvatarDictionary;
	}

private:
	static LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary* sAvatarDictionary;

	//--------------------------------------------------------------------
	// Messaging
	//--------------------------------------------------------------------
public:
	void		onFirstTEMessageReceived();

private:
	bool		mFirstTEMessageReceived;
	bool		mFirstAppearanceMessageReceived;

/**                    Textures
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    MESHES
 **/
public:
	void debugColorizeSubMeshes(U32 i, const LLColor4& color);
	void updateMeshTextures() override;
	void updateMeshVisibility();
	void updateSexDependentLayerSets(bool upload_bake);
	void dirtyMesh() override;				// Dirties the avatar mesh
	void resetSkeleton();
	void updateMeshData();

	const F32* getRiggedMatrix(const LLMeshSkinInfo* skin, U32& count);
	const LLMatrix4a* getRiggedMatrix4a(const LLMeshSkinInfo* skin,
										U32& count);

protected:
	void refreshAttachmentBakes();
	void releaseMeshData();
	virtual void restoreMeshData();

private:
	// This ref-counted sub-class is used to cache rigged meshes matrix for use
	// by the multiple render passes during one frame. The cache is valid only
	// for the current frame, thus the reason for the mFrameNumber stamp. HB
	class alignas(16) RiggedMatrix final : public LLRefCount
	{
	public:
		LL_INLINE void* operator new(size_t size)
		{
			return ll_aligned_malloc_16(size);
		}

		LL_INLINE void* operator new[](size_t size)
		{
			return ll_aligned_malloc_16(size);
		}

		LL_INLINE void operator delete(void* ptr)
		{
			ll_aligned_free_16(ptr);
		}

		LL_INLINE void operator delete[](void* ptr)
		{
			ll_aligned_free_16(ptr);
		}

	public:
		// Note: before these variables, we find the 32 bits counter from
		// LLRefCount... Since mMatrix4a will be 16-bytes aligned, try and
		// reduce the alignment gap in the cache line by filling it up with
		// other member variables. HB
		U32						mFrameNumber;
		U32						mCount;
		alignas(16) LLMatrix4a	mMatrix4a[LL_MAX_JOINTS_PER_MESH_OBJECT];
		F32						mMatrix[LL_MAX_JOINTS_PER_MESH_OBJECT * 12];
	};
	typedef fast_hmap<LLUUID, LLPointer<RiggedMatrix> > rig_tf_cache_t;
	typedef rig_tf_cache_t::iterator rtf_cache_it_t;
	rtf_cache_it_t initRiggedMatrixCache(const LLMeshSkinInfo* skin,
										 U32& count);

	// Dirties the avatar mesh with priority:
	void dirtyMesh(S32 priority) override;
	LLViewerJoint* getViewerJoint(S32 idx);

private:
	rig_tf_cache_t	mRiggedMatrixDataCache;

	S32				mDirtyMesh; // 0 = not dirty, 1 = morphed, 2 = LOD
	bool			mMeshTexturesDirty;

	//--------------------------------------------------------------------
	// Destroy invisible mesh
	//--------------------------------------------------------------------
protected:
	bool			mMeshValid;
	LLFrameTimer	mMeshInvisibleTime;

/**                    Meshes
 **                                                                          **
 *****************************************************************************/


/******************************************************************************
 **                                                                          **
 **                    APPEARANCE
 **/
public:
	void parseAppearanceMessage(LLMessageSystem* mesgsys,
								LLAppearanceMessageContents& msg);
	void processAvatarAppearance(LLMessageSystem* mesgsys);
	void applyParsedAppearanceMessage(LLAppearanceMessageContents& contents,
									  bool slam_params = false);
	void hideHair();
	void hideSkirt();
	void startAppearanceAnimation();
	void bodySizeChanged() override;

	//--------------------------------------------------------------------
	// Appearance morphing
	//--------------------------------------------------------------------
	LL_INLINE bool getIsAppearanceAnimating() const
	{
		return mAppearanceAnimating;
	}

	// True if we are computing our appearance via local compositing instead of
	// baked textures, as for example during wearable editing or when waiting
	// for a subsequent server rebake.
	// *FIXME: review isUsingLocalAppearance uses, some should be isEditing
	// instead.
	LL_INLINE bool isUsingLocalAppearance() const override
	{
		return mUseLocalAppearance;
	}

	// True if this avatar should fetch its baked textures via the new
	// appearance mechanism.
	bool isUsingServerBakes() const override;
	void setIsUsingServerBakes(bool newval);

	// True if we are currently in appearance editing mode. Often but not
	// always the same as isUsingLocalAppearance().
	LL_INLINE bool isEditingAppearance() const override
	{
		return mIsEditingAppearance;
	}

private:
	LLPointer<LLAppearanceMessageContents> mLastProcessedAppearance;

	LLFrameTimer	mAppearanceMorphTimer;
	F32				mLastAppearanceBlendTime;

	bool			mAppearanceAnimating;
	// Flag for if we are actively in appearance editing mode
	bool			mIsEditingAppearance;
	// Flag for if we are using a local composite
	bool			mUseLocalAppearance;
	// Flag for if baked textures should be fetched from baking service (false
	// if they are temporary uploads)
	bool			mUseServerBakes;

	//--------------------------------------------------------------------
	// Visibility
	//--------------------------------------------------------------------
public:
	bool isVisible() const;

	virtual LL_INLINE bool shouldRenderRigged() const		{ return true; }

public:
	static S32 sNumVisibleAvatars; // Number of instances of this class

/**                    Appearance
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    WEARABLES
 **/

	//--------------------------------------------------------------------
	// Attachments
	//--------------------------------------------------------------------
public:
	void clampAttachmentPositions();
	virtual const LLViewerJointAttachment* attachObject(LLViewerObject* vobj);
	virtual bool detachObject(LLViewerObject* vobj);
#if 0	// Not used/needed
	void cleanupAttachedMesh(LLViewerObject* vobj);
#endif
	static bool getRiggedMeshID(LLViewerObject* vobj, LLUUID& mesh_id);
	static LLVOAvatar* findAvatarFromAttachment(LLViewerObject* vobj);
	bool isWearingWearableType(LLWearableType::EType type) const override;
	LLViewerJointAttachment* getTargetAttachmentPoint(LLViewerObject* vobj);

	U32 getNumAttachments() const;
	U32 getNumAnimatedObjectAttachments() const; // O(N), not O(1)

protected:
	void lazyAttach();
	void rebuildRiggedAttachments();

	//--------------------------------------------------------------------
	// Map of attachment points, by ID
	//--------------------------------------------------------------------

public:
	// Map of attachment points, by ID
	typedef flat_hmap<S32, LLViewerJointAttachment*> attachment_map_t;
	attachment_map_t								mAttachmentPoints;
	std::vector<LLPointer<LLViewerObject> >			mPendingAttachment;
	typedef std::vector<std::pair<LLViewerObject*,
								  LLViewerJointAttachment*> > attachments_vec_t;
	attachments_vec_t								mAttachedObjectsVector;

public:
	//--------------------------------------------------------------------
	// HUD functions
	//--------------------------------------------------------------------

	bool hasHUDAttachment() const;
	LLBBox getHUDBBox() const;

	virtual S32 getMaxAnimatedObjectAttachments() const;
	bool canAttachMoreAnimatedObjects(U32 n = 1) const;

/**                    Wearables
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    ACTIONS
 **/

	//--------------------------------------------------------------------
	// Animations
	//--------------------------------------------------------------------
public:
	void resetAnimations();
	bool isAnyAnimationSignaled(const LLUUID* anim_array, S32 num_anims) const;
	void processAnimationStateChanges();

protected:
	bool processSingleAnimationStateChange(const LLUUID& anim_id, bool start);

private:
	LLTimer		mAnimTimer;
	F32			mTimeLast;

	//--------------------------------------------------------------------
	// Animation state data
	//--------------------------------------------------------------------
public:
	// NOTE: DO NOT convert to safe_hmap: it would crash when using anything
	// else than std or boost containers... HB
	typedef boost::unordered_map<LLUUID, S32> anim_map_t;
	typedef anim_map_t::iterator anim_it_t;

	// Requested state of Animation name/value
	anim_map_t								mSignaledAnimations;
	// Current state of Animation name/value
	anim_map_t								mPlayingAnimations;

	typedef std::multimap<LLUUID, LLUUID> anim_src_map_t;
	typedef anim_src_map_t::iterator anim_src_map_it_t;
	// Object ids that triggered anim ids
	anim_src_map_t							mAnimationSources;

	LLPuppetMotion* getPuppetMotion();

	//--------------------------------------------------------------------
	// Chat
	//--------------------------------------------------------------------
public:
	void addChat(const LLChat& chat);
	void clearChat();
	LL_INLINE void startTyping()	{ mTyping = true; mTypingTimer.reset(); }
	LL_INLINE void stopTyping()		{ mTyping = false; }

private:
	bool			mVisibleChat;

	//--------------------------------------------------------------------
	// Lip synch morphs
	//--------------------------------------------------------------------
private:
	bool			mLipSyncActive;	// We are morphing for lip sync
	LLVisualParam*	mOohMorph;		// Cached pointers morphs for lip sync
	LLVisualParam*	mAahMorph;		// Cached pointers morphs for lip sync

	//--------------------------------------------------------------------
	// Flight
	//--------------------------------------------------------------------
public:
	LLFrameTimer	mTimeInAir;
	bool			mInAir;

/**                    Actions
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    PHYSICS
 **/

private:
	bool		mTurning;		// Controls hysteresis on avatar rotation
	F32			mSpeedAccum;	// Measures speed (for diagnostics mostly).
	F32			mSpeed;			// Misc. animation repeated state

	//--------------------------------------------------------------------
	// Dimensions
	//--------------------------------------------------------------------
public:
	void resolveHeightGlobal(const LLVector3d& in_pos, LLVector3d& out_pos,
							 LLVector3& out_norm);
	bool distanceToGround(const LLVector3d& start_pt, LLVector3d& collision_pt,
						  F32 dist_to_intersection_along_ray);
	void resolveHeightAgent(const LLVector3& in_pos, LLVector3& out_pos,
							LLVector3& out_norm);
	void resolveRayCollisionAgent(const LLVector3d start_pt,
								  const LLVector3d end_pt, LLVector3d& out_pos,
								  LLVector3& out_norm);

	// Slams position to transmitted position (for teleport);
	void slamPosition();

	//--------------------------------------------------------------------
	// Material being stepped on
	//--------------------------------------------------------------------
private:
	bool		mStepOnLand;
	U8			mStepMaterial;
	LLVector3	mStepObjectVelocity;

/**                    Physics
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    HIERARCHY
 **/

public:
	bool setParent(LLViewerObject* parent) override;
	void addChild(LLViewerObject* childp) override;
	void removeChild(LLViewerObject* childp) override;

	//--------------------------------------------------------------------
	// Sitting
	//--------------------------------------------------------------------
public:
	void sitOnObject(LLViewerObject* sit_object);
	void getOffObject();

public:
	bool 			mIsSitting;

/**                    Hierarchy
 **                                                                          **
 *****************************************************************************/

/*****************************************************************************
 **                                                                         **
 **                    NAME
 **/

public:
	// Returns "FirstName LastName". Always omit " Resident" when passed true.
	std::string getFullname(bool omit_resident = false);

protected:
	static void getAnimLabels(std::vector<std::string>* labels);
	static void getAnimNames(std::vector<std::string>* names);

private:
	LLWString				mNameString;		// UTF-8 title + name + status
	std::string				mTitle;
	std::string				mCompleteName;
	std::string				mLegacyName;
	S32						mNameMute;
	bool					mNewResident;		// Is last name "Resident" ?
	bool					mNameAway;
	bool					mNameBusy;
	bool					mNameTyping;
	bool					mNameAppearance;
	bool					mRenderGroupTitles;

	//--------------------------------------------------------------------
	// Display the name (then optionally fade it out)
	//--------------------------------------------------------------------
public:
	LLFrameTimer			mChatTimer;
	LLPointer<LLHUDText>	mNameText;

private:
	LLFrameTimer			mTimeVisible;
	LLFrameTimer			mTypingTimer;
	std::deque<LLChat>		mChats;
	bool					mTyping;

/**                    Name
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    SOUNDS
 **/

private:
	// Responsible for detecting the user's voice signal (and when the user
	// speaks, it puts a voice symbol over the avatar's head) and
	// gesticulations
	LLVoiceVisualizer*  mVoiceVisualizer;
	S32					mCurrentGesticulationLevel;

	//--------------------------------------------------------------------
	// Step sound
	//--------------------------------------------------------------------
protected:
	const LLUUID& getStepSound() const;

private:
	// Global table of sound ids per material, and the ground
	const static LLUUID	sStepSounds[LL_MCODE_END];
	const static LLUUID	sStepSoundOnLand;

	//--------------------------------------------------------------------
	// Foot step state (for generating sounds)
	//--------------------------------------------------------------------
public:
	LL_INLINE void setFootPlane(const LLVector4& plane)	{ mFootPlane = plane; }

public:
	LLVector4		mFootPlane;

private:
	bool			mWasOnGroundLeft;
	bool			mWasOnGroundRight;

/**                    Sounds
 **                                                                          **
 *****************************************************************************/

/******************************************************************************
 **                                                                          **
 **                    DIAGNOSTICS
 **/

	//--------------------------------------------------------------------
	// General
	//--------------------------------------------------------------------
public:
	void getSortedJointNames(S32 joint_type,
							 std::vector<std::string>& result) const;

	void dumpArchetypeXML(const std::string& filename);

	static void dumpBakedStatus();
	const std::string getBakedStatusForPrintout() const;
	void dumpAvatarTEs(const std::string& context) const;

public:
	// Total seconds with >=1 unbaked avatars:
	static F32 			sUnbakedTime;
	// Last time stats were updated (to prevent multiple updates per frame):
	static F32 			sUnbakedUpdateTime;
	// Total seconds with >=1 grey avatars:
	static F32 			sGreyTime;
	// Last time stats were updated (to prevent multiple updates per frame):
	static F32 			sGreyUpdateTime;

protected:
	S32 getUnbakedPixelAreaRank();

protected:
	bool				mHasGrey;

	bool				mEnableDefaultMotions;

private:
	F32					mMinPixelArea;
	F32					mMaxPixelArea;
	F32					mAdjustedPixelArea;
	std::string			mDebugText;

	//--------------------------------------------------------------------
	// COF monitoring
	//--------------------------------------------------------------------
public:

	// COF version of last viewer-initiated appearance update request. For
	// non-self avatars, this will remain at default.
	S32 mLastUpdateRequestCOFVersion;

	// COF version of last appearance message received for this avatar.
	S32 mLastUpdateReceivedCOFVersion;

/**                    Diagnostics
 **                                                                          **
 *****************************************************************************/
};

class LLVOAvatarUI final : public LLVOAvatar
{
protected:
	LOG_CLASS(LLVOAvatarUI);

public:
	LLVOAvatarUI(const LLUUID& id, LLViewerRegion* regionp);

	void initInstance() override;

	LL_INLINE bool isUIAvatar() const override	{ return true; }

	LL_INLINE bool isVisuallyMuted() override	{ return false; }
};

// Baked textures prioty
constexpr F32 SELF_ADDITIONAL_PRI = 0.75f;
constexpr F32 ADDITIONAL_PRI = 0.5f;

constexpr F32 MAX_HOVER_Z = 2.f;
constexpr F32 MIN_HOVER_Z = -2.f;

#endif // LL_VO_AVATAR_H
