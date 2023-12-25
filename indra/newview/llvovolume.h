/**
 * @file llvovolume.h
 * @brief LLVOVolume class header file
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

#ifndef LL_LLVOVOLUME_H
#define LL_LLVOVOLUME_H

#include "hbfastmap.h"
#include "llframetimer.h"
#include "llmatrix3.h"
#include "llmatrix4.h"
#include "llpointer.h"

#include "llmeshrepository.h"
#include "llviewermedia.h"
#include "llviewerobject.h"
#include "llviewertexture.h"

class LLDrawPool;
class LLMaterialID;
class LLMeshSkinInfo;
class LLObjectMediaDataClient;
class LLObjectMediaNavigateClient;
class LLSelectNode;
class LLViewerTextureAnim;
class LLVOAvatar;

typedef std::vector<viewer_media_t> media_list_t;

enum LLVolumeInterfaceType
{
	INTERFACE_FLEXIBLE = 1,
};

class LLRiggedVolume : public LLVolume
{
public:
	LLRiggedVolume(const LLVolumeParams& params)
	:	LLVolume(params, 0.f)
	{
	}

	enum
	{
		UPDATE_ALL_FACES = -1,
		DO_NOT_UPDATE_FACES = -2
	};

	void update(const LLMeshSkinInfo* skin, LLVOAvatar* avatar,
				const LLVolume* src_volume, S32 face_index = UPDATE_ALL_FACES,
				bool rebuild_face_octrees = true);
};

// Base class for implementations of the volume - Primitive, Flexible Object,
// etc.
class LLVolumeInterface
{
public:
	virtual ~LLVolumeInterface() = default;

	virtual LLVolumeInterfaceType getInterfaceType() const = 0;

	virtual void doIdleUpdate() = 0;

	virtual bool doUpdateGeometry(LLDrawable* drawable) = 0;

	virtual LLVector3 getPivotPosition() const = 0;

	virtual void onSetVolume(const LLVolumeParams& volume_params,
							 S32 detail) = 0;

	virtual void onSetScale(const LLVector3& scale, bool damped) = 0;

	virtual void onParameterChanged(U16 param_type, LLNetworkData* data,
									bool in_use, bool local_origin) = 0;

	virtual void onShift(const LLVector4a& shift_vector) = 0;

	 // Do we need a unique LLVolume instance ?
	virtual bool isVolumeUnique() const = 0;
	// Are we in global space ?
	virtual bool isVolumeGlobal() const = 0;
	// Is this object currently active ?
	virtual bool isActive() const = 0;

	virtual const LLMatrix4& getWorldMatrix(LLXformMatrix* xform) const = 0;

	virtual void updateRelativeXform(bool force_identity = false) = 0;

	virtual U32 getID() const = 0;

	virtual void preRebuild() = 0;
};

// Class which embodies all Volume objects (with pcode LL_PCODE_VOLUME)
class LLVOVolume : public LLViewerObject
{
	friend class LLDrawable;
	friend class LLFace;
	friend class LLVolumeImplFlexible;

protected:
	LOG_CLASS(LLVOVolume);

	~LLVOVolume() override;

public:
	static void initClass();
	static void updateSettings();
	static void initSharedMedia();
	static void cleanupClass();
	LL_INLINE static void preUpdateGeom()				{ sNumLODChanges = 0; }

	enum
	{
		VERTEX_DATA_MASK =	(1 << LLVertexBuffer::TYPE_VERTEX) |
							(1 << LLVertexBuffer::TYPE_NORMAL) |
							(1 << LLVertexBuffer::TYPE_TEXCOORD0) |
							(1 << LLVertexBuffer::TYPE_TEXCOORD1) |
							(1 << LLVertexBuffer::TYPE_COLOR)
	};

	LLVOVolume(const LLUUID& id, LLViewerRegion* regionp);

	LL_INLINE LLVOVolume* asVolume() override	 		{ return this; }

	// Override (and call through to parent) to clean up media references
	void markDead() override;

	LLDrawable* createDrawable() override;

	void deleteFaces();

	void animateTextures();

	bool isVisible() const;
	LL_INLINE bool isActive() const override			{ return !mStatic; }

	bool isAttachment() const override;
	bool isHUDAttachment() const override;

	// Overridden for sake of attachments treating themselves as a root object
	LL_INLINE bool isRootEdit() const override			{ return !mParent || ((LLViewerObject*)mParent)->isAvatar(); }

	void generateSilhouette(LLSelectNode* nodep, const LLVector3& view_point);

	bool setParent(LLViewerObject* parent) override;

	LL_INLINE S32 getLOD() const override				{ return mLOD; }

	LL_INLINE const LLVector3 getPivotPositionAgent() const override;

	LL_INLINE const LLMatrix4& getRelativeXform() const	{ return mRelativeXform; }

	LL_INLINE const LLMatrix3& getRelativeXformInvTrans() const
	{
		return mRelativeXformInvTrans;
	}

	const LLMatrix4& getRenderMatrix() const override;

	typedef fast_hmap<LLUUID, S32> texture_cost_t;
	U32 getRenderCost(texture_cost_t& textures) const;
	F32 getEstTrianglesMax() const override;
	F32 getEstTrianglesStreamingCost() const override;
	F32 getStreamingCost(S32* bytes = NULL, S32* visible_bytes = NULL,
						 F32* unscaled_value = NULL) const override;
	U32 getTriangleCount(S32* vcount = NULL) const override;
	U32 getHighLODTriangleCount() override;

	LLMeshCostData* getCostData() const;

	bool lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
							  // Which face to check (-1=ALL_SIDES)
							  S32 face = -1,
							  bool pick_transparent = false,
							  bool pick_rigged = false,
							  // Which face was hit
							  S32* face_hit = NULL,
							  // Intersection point
							  LLVector4a* intersection = NULL,
							  // Tex coord of the intersection
							  LLVector2* tex_coord = NULL,
							  // Surface normal at intersection
							  LLVector4a* normal = NULL,
							  // Surface tangent at intersection
							  LLVector4a* tangent = NULL) override;

	LLVector3 agentPositionToVolume(const LLVector3& pos) const;
	LLVector3 agentDirectionToVolume(const LLVector3& dir) const;
	LLVector3 volumePositionToAgent(const LLVector3& dir) const;
	LLVector3 volumeDirectionToAgent(const LLVector3& dir) const;

	LL_INLINE bool getVolumeChanged() const				{ return mVolumeChanged; }

	LL_INLINE F32 getRadius() const						{ return mVObjRadius; }

	const LLMatrix4& getWorldMatrix(LLXformMatrix* xform) const override;

	void markForUpdate(bool rebuild_all = false) override;

	LL_INLINE void faceMappingChanged()	override		{ mFaceMappingChanged = true; }

	// Called when the drawable shifts
	void onShift(const LLVector4a& shift_vector) override;

	void parameterChanged(U16 param_type, bool local_origin) override;
	void parameterChanged(U16 param_type, LLNetworkData* data, bool in_use,
						  bool local_origin) override;

	void updateReflectionProbePtr();

	U32 processUpdateMessage(LLMessageSystem* mesgsys, void** user_data,
							 U32 block_num, EObjectUpdateType upd_type,
							 LLDataPacker* dp) override;

	void setSelected(bool sel) override;
	bool setDrawableParent(LLDrawable* parentp) override;

	void setScale(const LLVector3& scale, bool damped) override;

	void setNumTEs(U8 num_tes) override;
	void setTEImage(U8 te, LLViewerTexture* imagep) override;
	S32 setTETexture(U8 te, const LLUUID& uuid) override;
	S32 setTEColor(U8 te, const LLColor3& color) override;
	S32 setTEColor(U8 te, const LLColor4& color) override;
	S32 setTEBumpmap(U8 te, U8 bump) override;
	S32 setTEShiny(U8 te, U8 shiny) override;
	S32 setTEFullbright(U8 te, U8 fullbright) override;
	S32 setTEBumpShinyFullbright(U8 te, U8 bump) override;
	S32 setTEMediaFlags(U8 te, U8 media_flags) override;
	S32 setTEGlow(U8 te, F32 glow) override;
	S32 setTEMaterialID(U8 te, const LLMaterialID& matidp) override;
	static void setTEMaterialParamsCallbackTE(const LLUUID& objid,
											  const LLMaterialID& matidp,
											  const LLMaterialPtr paramsp,
											  U32 te);
	S32 setTEMaterialParams(U8 te, const LLMaterialPtr paramsp) override;
	S32 setTEGLTFMaterialOverride(U8 te, LLGLTFMaterial* mat) override;
	S32 setTEScale(U8 te, F32 s, F32 t) override;
	S32 setTEScaleS(U8 te, F32 s) override;
	S32 setTEScaleT(U8 te, F32 t) override;
	S32 setTETexGen(U8 te, U8 texgen) override;
	S32 setTEMediaTexGen(U8 te, U8 media) override;

	void setTexture(S32 face);
	LL_INLINE S32 getIndexInTex(U32 ch) const			{ return mIndexInTex[ch]; }
	LL_INLINE void setIndexInTex(U32 ch, S32 index)		{ mIndexInTex[ch] = index; }

#if LL_FIX_MAT_TRANSPARENCY
	bool notifyAboutCreatingTexture(LLViewerTexture* texture);
	bool notifyAboutMissingAsset(LLViewerTexture* texture);
#endif

	bool setVolume(const LLVolumeParams& vol_params, S32 detail,
				   bool unique_volume = false) override;
	void updateSculptTexture();
	void sculpt();
	void updateRelativeXform(bool force_identity = false);
	bool updateGeometry(LLDrawable* drawable) override;
	void updateFaceSize(S32 idx) override;
	bool updateLOD() override;
	// LOD set with this method will only last till next updateLOD() call:
	void tempSetLOD(S32 lod);
	// With this method, you may force an object to render always at full LOD
	LL_INLINE void setMaxLOD(bool b = true)				{ mLockMaxLOD = b; mLODChanged = true; }
	LL_INLINE bool getMaxLOD() const					{ return mLockMaxLOD; }

	void updateRadius() override;
	void updateTextures() override;
	void updateTextureVirtualSize(bool forced = false);

	void updateFaceFlags();
	void regenFaces();
	bool genBBoxes(bool force_global, bool update_bounds = true);
	void preRebuild();
	void updateSpatialExtents(LLVector4a&, LLVector4a&) override;
	F32 getBinRadius() override;

	U32 getPartitionType() const override;

	// For lights
	void setIsLight(bool is_light);
	void setLightLinearColor(const LLColor3& color);
	void setLightIntensity(F32 intensity);
	void setLightRadius(F32 radius);
	void setLightFalloff(F32 falloff);
	void setLightCutoff(F32 cutoff);
	void setLightTextureID(const LLUUID& id);
	void setSpotLightParams(const LLVector3& params);
	LL_INLINE void setLightSRGBColor(const LLColor3& c)	{ setLightLinearColor(linearColor3(c)); }
	bool getIsLight() const;
	LLColor3 getLightLinearBaseColor() const;	// Not scaled by intensity
	LLColor3 getLightLinearColor() const;		// Scaled by intensity
	const LLUUID& getLightTextureID() const;
	bool isLightSpotlight() const;
	LLVector3 getSpotLightParams() const;
	void updateSpotLightPriority();
	LL_INLINE F32 getSpotLightPriority() const			{ return mSpotLightPriority; }
	// Gets the light color in sRGB color space not scaled by intensity.
	LL_INLINE LLColor3 getLightSRGBBaseColor() const	{ return srgbColor3(getLightLinearBaseColor()); }
	// Gets the light color in sRGB color space scaled by intensity.
	LLColor3 getLightSRGBColor() const;

	LLViewerTexture* getLightTexture();
	F32 getLightIntensity() const;
	F32 getLightRadius() const;
	F32 getLightFalloff(F32 fudge_factor = 1.f) const;
	F32 getLightCutoff() const;

	// Reflection Probes
	bool setIsReflectionProbe(bool is_probe);
	bool setReflectionProbeAmbiance(F32 ambiance);
	bool setReflectionProbeNearClip(F32 near_clip);
	bool setReflectionProbeIsBox(bool is_box);
	bool setReflectionProbeIsDynamic(bool is_dynamic);

	bool isReflectionProbe() const override;
	F32 getReflectionProbeAmbiance() const;
	F32 getReflectionProbeNearClip() const;
	bool getReflectionProbeIsBox() const;
	bool getReflectionProbeIsDynamic() const;

	U32 getVolumeInterfaceID() const;

	bool isVolumeGlobal() const;
	bool isSculpted() const override;
	bool isMesh() const override;
	bool isRiggedMesh() const override;
	bool hasLightTexture() const override;

	// Flexible Objects
	bool isFlexible() const override;
	bool canBeFlexible() const;
	bool setIsFlexible(bool is_flexible);

	const LLMeshSkinInfo* getSkinInfo() const;

	// Extended mesh properties
	U32 getExtendedMeshFlags() const;
	void setExtendedMeshFlags(U32 flags);
	bool canBeAnimatedObject() const;
	bool isAnimatedObject() const override;
	void onReparent(LLViewerObject* old_parent,
					LLViewerObject* new_parent) override;
	void afterReparent() override;

	void updateRiggingInfo() override;

	// Functions that deal with media, or media navigation

	// Update this object's media data with the given media data array
	// (typically this is only called upon a response from a server request)
	void updateObjectMediaData(const LLSD& media_data_array,
							   const std::string& media_version);

	// Bounce back media at the given index to its current URL (or home URL, if
	// current URL is empty)
	void mediaNavigateBounceBack(U8 texture_index);

	// Returns whether or not this object has permission to navigate or control
	// the given media entry
	enum MediaPermType
	{
		MEDIA_PERM_INTERACT, MEDIA_PERM_CONTROL
	};

	bool hasMediaPermission(const LLMediaEntry* media_entry,
							MediaPermType perm_type);

	void mediaNavigated(LLViewerMediaImpl* impl, LLPluginClassMedia* plugin,
						std::string new_location);
	void mediaEvent(LLViewerMediaImpl* impl, LLPluginClassMedia* plugin,
					LLViewerMediaObserver::EMediaEvent event);

	// Sync the given media data with the impl and the given te
	void syncMediaData(S32 te, const LLSD& media_data, bool merge,
					   bool ignore_agent);

	// Send media data update to the simulator.
	void sendMediaDataUpdate();

	viewer_media_t getMediaImpl(U8 face_id) const;
	S32 getFaceIndexWithMediaImpl(const LLViewerMediaImpl* media_impl,
								  S32 start_face_id);
	F64 getTotalMediaInterest() const;

	bool hasMedia() const;

	LLVector3 getApproximateFaceNormal(U8 face_id);

	// Returns 'true' iff the media data for this object is in flight
	bool isMediaDataBeingFetched() const;

	// Returns the "last fetched" media version, or -1 if not fetched yet
	LL_INLINE S32 getLastFetchedMediaVersion() const	{ return mLastFetchedMediaVersion; }

	LL_INLINE void addMDCImpl()							{ ++mMDCImplCount; }
	LL_INLINE void removeMDCImpl()						{ --mMDCImplCount; }

	// Flags any corresponding avatar as needing update
	void updateVisualComplexity();

	void notifyMeshLoaded();
	void notifySkinInfoLoaded(LLMeshSkinInfo* skin);
	void notifySkinInfoUnavailable();

	// Rigged volume update (for raycasting)
	// By default, this updates the bounding boxes of all the faces and builds
	// an octree for precise per-triangle raycasting.
	void updateRiggedVolume(bool force_treat_as_rigged,
							S32 face_index = LLRiggedVolume::UPDATE_ALL_FACES,
							bool rebuild_face_octrees = true);
	LL_INLINE LLRiggedVolume* getRiggedVolume()			{ return mRiggedVolume.get(); }

	// Returns true if volume should be treated as a rigged volume, i.e. if:
	// - object is selected
	// - object is an attachment
	// - object is rendered as rigged
	bool treatAsRigged();

	// Clear out rigged volume and revert back to non-rigged state for
	// picking/LOD/distance updates
	void clearRiggedVolume();

	// Used by the mesh repository. HB
	LL_INLINE void setInMeshCache()						{ mInMeshCache = true; }
	LL_INLINE void setInSkinCache()						{ mInSkinCache = true; }

protected:
	void onDrawableUpdateFromServer();

	S32	computeLODDetail(F32 distance, F32 radius, F32 lod_factor);
	bool calcLOD();
	LLFace* addFace(S32 face_index);

	void requestMediaDataUpdate(bool isNew);
	void cleanUpMediaImpls();
	void addMediaImpl(LLViewerMediaImpl* media_impl, S32 texture_index);
	void removeMediaImpl(S32 texture_index);

private:
	bool lodOrSculptChanged(LLDrawable* drawable, bool& update_bounds);
	void onSetExtendedMeshFlags(U32 flags);

private:
	LLMatrix4							mRelativeXform;
	LLMatrix3							mRelativeXformInvTrans;

	LLVolumeInterface*					mVolumeImpl;

	LLPointer<LLViewerFetchedTexture>	mSculptTexture;
	LLPointer<LLViewerFetchedTexture>	mLightTexture;

	LLPointer<LLRiggedVolume>			mRiggedVolume;
	LLPointer<LLMeshSkinInfo>			mSkinInfo;

	mutable LLPointer<LLMeshCostData>	mCostData;

	F32									mSpotLightPriority;
	F32									mVObjRadius;
	F32									mLastDistance;
	S32									mLOD;
	U32									mServerDrawableUpdateCount;
	F32									mLastServerDrawableUpdate;
	S32									mIndexInTex[LLRender::NUM_VOLUME_TEXTURE_CHANNELS];
	S32									mMDCImplCount;
	// As fetched from server, starts as -1:
	S32									mLastFetchedMediaVersion;

	bool								mLockMaxLOD;

	bool								mLODChanged;
	bool								mSculptChanged;
	bool								mColorChanged;
	bool								mFaceMappingChanged;
	bool								mVolumeChanged;
	bool								mSkinInfoFailed;
	// NOTE: these booleans mark this volume address as registered in the mesh
	// repository; they are *never* reset to false, and when set, must cause
	// markDead() to call the mesh repository unregisterVolume() method. HB
	bool								mInMeshCache;
	bool								mInSkinCache;

	LLFrameTimer						mTextureUpdateTimer;

	media_list_t						mMediaImplList;

#if LL_FIX_MAT_TRANSPARENCY
	struct material_info
	{
		LLRender::eTexIndex	map;
		U8					te;

		material_info(LLRender::eTexIndex map_, U8 te_)
		:	map(map_),
			te(te_)
		{
		}
	};
	typedef std::multimap<LLUUID, material_info> mmap_uuid_map_t;
	mmap_uuid_map_t						mWaitingTextureInfo;
#endif

protected:
	static S32							sNumLODChanges;

public:
	LLViewerTextureAnim*				mTextureAnimp;
	U8									mTexAnimMode;

	S32									mLastRiggingInfoLOD;

	// Also used in llvowlsky.cpp:
	static U32							sRenderMaxVBOSize;
	// LOD scale factor:
	static F32							sLODFactor;
	// LOD distance factor:
	static F32							sDistanceFactor;

	static bool							sAnimateTextures;

	static LLPointer<LLObjectMediaDataClient>		sObjectMediaClient;
	static LLPointer<LLObjectMediaNavigateClient>	sObjectMediaNavigateClient;
};

#endif // LL_LLVOVOLUME_H
