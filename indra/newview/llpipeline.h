/**
 * @file llpipeline.h
 * @brief Rendering pipeline definitions
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

#ifndef LL_LLPIPELINE_H
#define LL_LLPIPELINE_H

#include <stack>

#include "llrendertarget.h"
#include "llstat.h"

#include "llreflectionmapmanager.h"
#include "llspatialpartition.h"

// Disabled for now (not yet ported to PBR). HB
#define HB_PBR_SMAA_AND_CAS 0

class LLAgent;
class LLCubeMap;
class LLCullResult;
class LLDisplayPrimitive;
class LLDrawPoolAlpha;
class LLEdge;
class LLFace;
class LLGLSLShader;
class LLRenderFunc;
class LLTextureEntry;
class LLViewerObject;
class LLViewerTexture;
class LLVOAvatar;
class LLVOPartGroup;

typedef enum e_avatar_skinning_method
{
	SKIN_METHOD_SOFTWARE,
	SKIN_METHOD_VERTEX_PROGRAM
} EAvatarSkinningMethod;

bool LLRayAABB(const LLVector3& center, const LLVector3& size,
			   const LLVector3& origin, const LLVector3& dir,
			   LLVector3& coord, F32 epsilon = 0);

class LLPipeline
{
protected:
	LOG_CLASS(LLPipeline);

public:
	LLPipeline();

	void destroyGL();
	void restoreGL();
	void resetVertexBuffers();
	void doResetVertexBuffers(bool forced = false);
	void resizeScreenTexture();
	void resizeShadowTexture();
	void releaseGLBuffers();
	void releaseLUTBuffers();
	void createGLBuffers();
	void createLUTBuffers();

	void allocateScreenBuffer(U32 res_x, U32 res_y);
	bool allocateScreenBuffer(U32 res_x, U32 res_y, U32 samples);
	bool allocateShadowBuffer(U32 res_x, U32 res_y);
	void allocatePhysicsBuffer();

	void resetVertexBuffers(LLDrawable* drawable);
	void generateImpostor(LLVOAvatar* avatar);
	void previewAvatar(LLVOAvatar* avatar);

	// Automatically calls renderFinalizePBR() when in PBR rendering mode. HB
	void renderFinalize();

	void init();
	void cleanup();
	void dumpStats();
	LL_INLINE bool isInit()						{ return mInitialized; };

	// Gets a draw pool from pool type (POOL_SIMPLE, POOL_MEDIA) and texture.
	// Returns the draw pool, or NULL if not found.
	LLDrawPool* findPool(U32 pool_type, LLViewerTexture* tex0 = NULL);

	// Gets a draw pool for faces of the appropriate type and texture. Creates
	// if necessary. Always returns a draw pool.
	LLDrawPool* getPool(U32 pool_type, LLViewerTexture* tex0 = NULL);

	// Figures out draw pool type from texture entry. Creates a new pool if
	// necessary.
	static LLDrawPool* getPoolFromTE(const LLTextureEntry* tep,
									 LLViewerTexture* imagep);
	static U32 getPoolTypeFromTE(const LLTextureEntry* tep,
								 LLViewerTexture* imagep);

	// Only to be used by LLDrawPool classes for splitting pools !
	void addPool(LLDrawPool* poolp);
	void removePool(LLDrawPool* poolp);

	void allocDrawable(LLViewerObject* objp);

	void unlinkDrawable(LLDrawable*);

	// Object related methods
	void markVisible(LLDrawable* drawablep, LLCamera& camera);
	void markOccluder(LLSpatialGroup* groupp);

	// Only used by the EE renderer
	void doOcclusion(LLCamera& camera, LLRenderTarget& source,
					 LLRenderTarget& dest, LLRenderTarget* scratch = NULL);

	void doOcclusion(LLCamera& camera);
	void markNotCulled(LLSpatialGroup* groupp, LLCamera& camera);
	void markMoved(LLDrawable* drawablep, bool damped_motion = false);
	void markShift(LLDrawable* drawablep);
	void markTextured(LLDrawable* drawablep);
	void markGLRebuild(LLGLUpdate* glup);
	void markRebuild(LLSpatialGroup* group);
	void markRebuild(LLDrawable* drawablep,
					 LLDrawable::EDrawableFlags flag = LLDrawable::REBUILD_ALL);
	void markPartitionMove(LLDrawable* drawablep);
	void markMeshDirty(LLSpatialGroup* groupp);

	// Rebuild all LLVOVolume render batches
	void rebuildDrawInfo();

	// Gets the object between start and end that is closest to start.
	LLViewerObject* lineSegmentIntersectInWorld(const LLVector4a& start,
												const LLVector4a& end,
												bool pick_transparent,
												bool pick_rigged,
												// Returns the face hit
												S32* face_hit,
												// Returns the intersection point
												LLVector4a* intersection = NULL,
												// Returns texture coordinates
												LLVector2* tex_coord = NULL,
												// Returns the surface normal
												LLVector4a* normal = NULL,
												// Return the surface tangent
												LLVector4a* tangent = NULL);

	// Gets the closest particle to start between start and end, returns the
	// LLVOPartGroup and particle index:
	LLVOPartGroup* lineSegmentIntersectParticle(const LLVector4a& start,
												const LLVector4a& end,
												LLVector4a* intersection,
												S32* face_hit);

	LLViewerObject* lineSegmentIntersectInHUD(const LLVector4a& start,
											  const LLVector4a& end,
											  bool pick_transparent,
												// Returns the face hit
											  S32* face_hit,
											  // Returns the intersection point
											  LLVector4a* intersection = NULL,
											  // Returns texture coordinates
											  LLVector2* tex_coord = NULL,
											  // Returns the surface normal
											  LLVector4a* normal = NULL,
											  // Return the surface tangent
											  LLVector4a* tangent = NULL);

	// Something about these textures has changed.  Dirty them.
	void dirtyPoolObjectTextures(const LLViewerTextureList::dirty_list_t& textures);

	void resetDrawOrders();

	U32 addObject(LLViewerObject* obj);

	LL_INLINE bool canUseShaders() const		{ return mVertexShadersLoaded != -1; }
	LL_INLINE bool shadersLoaded() const		{ return mVertexShadersLoaded == 1; }
	bool canUseWindLightShaders() const;

	// Phases
	void resetFrameStats();

	void updateMoveDampedAsync(LLDrawable* drawablep);
	void updateMoveNormalAsync(LLDrawable* drawablep);
	void updateMovedList(LLDrawable::draw_vec_t& move_list);
	void updateMove(bool balance_vo_cache);

	bool getVisibleExtents(LLCamera& camera, LLVector3& min, LLVector3& max);
	bool getVisiblePointCloud(LLCamera& camera, LLVector3& min, LLVector3& max,
							  std::vector<LLVector3>& fp,
							  LLVector3 light_dir = LLVector3::zero);

	// Note: for the PBR renderer, planp and hud_attachments are ignored.
	void updateCull(LLCamera& camera, LLCullResult& result,
					LLPlane* planep = NULL, bool hud_attachments = false);

	void createObjects(F32 max_dtime);
	void createObject(LLViewerObject* objp);
	void processPartitionQ();
	void updateGeom(F32 max_dtime);
	void updateGL();
	void rebuildPriorityGroups();
	void clearRebuildGroups();
	void clearRebuildDrawables();

	// Calculates pixel area of given box from vantage point of given camera
	static F32 calcPixelArea(LLVector3 center, LLVector3 size,
							 LLCamera& camera);
	static F32 calcPixelArea(const LLVector4a& center, const LLVector4a& size,
							 LLCamera& camera);

	void stateSort(LLCamera& camera, LLCullResult& result);
	void stateSort(LLSpatialGroup* group, LLCamera& camera);
	void stateSort(LLSpatialBridge* bridge, LLCamera& camera,
				   bool fov_changed = false);
	void stateSort(LLDrawable* drawablep, LLCamera& camera);

	// Updates stats for textures in given DrawInfo
	void touchTextures(LLDrawInfo* info);
	void touchTexture(LLViewerTexture* tex, F32 vsize);

	void postSort(LLCamera& camera);

	void forAllVisibleDrawables(void (*func)(LLDrawable*));

	void renderObjects(U32 type, U32 mask, bool texture = true,
					   bool batch_texture = false, bool rigged = false);
	// Used only by the PBR renderer
	void renderGLTFObjects(U32 type, bool texture = true, bool rigged = false);

	void renderAlphaObjects(bool rigged);
	void renderMaskedObjects(U32 type, U32 mask, bool texture = true,
							 bool batch_texture = false, bool rigged = false);
	void renderFullbrightMaskedObjects(U32 type, U32 mask, bool texture = true,
							 bool batch_texture = false, bool rigged = false);

	void renderGroups(LLRenderPass* pass, U32 type, U32 mask, bool texture);
	void renderRiggedGroups(LLRenderPass* pass, U32 type, U32 mask,
							bool texture);

	void grabReferences(LLCullResult& result);
	void clearReferences();

#if LL_DEBUG
	// Checks references and asserts that there are no references in sCull
	// results to the provided data
	void checkReferences(LLFace* facep);
	void checkReferences(LLDrawable* drawablep);
	void checkReferences(LLDrawInfo* draw_infop);
	void checkReferences(LLSpatialGroup* groupp);
#endif

	// For EE rendering only
	void renderGeom(LLCamera& camera);
	// For EE rendering only
	void renderGeomDeferred(LLCamera& camera);
	// For PBR rendering only
	void renderGeomDeferred(LLCamera& camera, bool do_occlusion);
	// Note: 'do_occlusion' is ignored (always false) for PBR rendering. HB
	void renderGeomPostDeferred(LLCamera& camera, bool do_occlusion = true);
	void renderGeomShadow(LLCamera& camera);

	void bindDeferredShader(LLGLSLShader& shader,
							LLRenderTarget* light_targetp = NULL);
	// Fast path for shaders that have already been bound once. Used only by
	// the PBR renderer, for now (but could likely be used by EE too). HB
	void bindDeferredShaderFast(LLGLSLShader& shader);
	void unbindDeferredShader(LLGLSLShader& shader);

	void setupSpotLight(LLGLSLShader& shader, LLDrawable* drawablep);

	void renderDeferredLighting();

	// For EE rendering only
	void generateWaterReflection();

	void generateSunShadow();
	void renderHighlight(const LLViewerObject* obj, F32 fade);

	// For PBE rendering only
	void renderShadow(const LLMatrix4a& view, const LLMatrix4a& proj,
					  LLCamera& shadow_cam, LLCullResult& result,
					  bool depth_clamp);
	// For EE rendering only
	void renderShadow(const LLMatrix4a& view, const LLMatrix4a& proj,
					  LLCamera& camera, LLCullResult& result, bool use_shader,
					  bool use_occlusion, U32 target_width);

	void renderHighlights();
	void renderDebug();
	void renderPhysicsDisplay();
	
	// Returns 0 when the object is not to be highlighted, 1 when it can be
	// both highlighted and marked with a beacon, and 2 when it may can be
	// highlighted only. HB
	static U32 highlightable(const LLViewerObject* objp);

	void rebuildPools(); // Rebuild pools

#if LL_DEBUG && 0
	// Debugging method.
	// Find the lists which have references to this object
	void findReferences(LLDrawable* drawablep);
#endif

	// Verify that all data in the pipeline is "correct":
	bool verify();

	// This must be called each time the sky is updated to cache the current
	// values which will be reused during the frame rendering. Called by
	// LLEnvironment::update(). HB
	void cacheEnvironment();

	LL_INLINE S32 getLightCount() const			{ return mLights.size(); }

	void calcNearbyLights(LLCamera& camera);
	void setupHWLights();
	void setupAvatarLights(bool for_edit = false);
	void enableLights(U32 mask);
	void enableLightsStatic();
	void enableLightsDynamic();
	void enableLightsAvatar();
	void enableLightsPreview();
	void enableLightsAvatarEdit();
	void enableLightsFullbright();
	void disableLights();

	void shiftObjects(const LLVector3& offset);

	void setLight(LLDrawable* drawablep, bool is_light);

#if 0	// Not currently used
	LL_INLINE bool hasRenderBatches(U32 type) const
	{
		return sCull && sCull->hasRenderMap(type);
	}
#endif

	LL_INLINE LLCullResult::drawinfo_list_t& getRenderMap(U32 type)
	{
		return sCull->getRenderMap(type);
	}

	LL_INLINE LLCullResult::sg_list_t& getAlphaGroups()
	{
		return sCull->getAlphaGroups();
	}

	LL_INLINE LLCullResult::sg_list_t& getRiggedAlphaGroups()
	{
		return sCull->getRiggedAlphaGroups();
	}

	void addTrianglesDrawn(U32 index_count);

	LL_INLINE bool hasRenderDebugFeatureMask(U32 mask) const
	{
		return (mRenderDebugFeatureMask & mask) != 0;
	}

	LL_INLINE bool hasRenderDebugMask(U32 mask) const
	{
		return (mRenderDebugMask & mask) != 0;
	}

	LL_INLINE void setRenderDebugMask(U32 mask)
	{
		mRenderDebugMask = mask;
	}

	LL_INLINE bool hasRenderType(U32 type) const
	{
    	// STORM-365: LLViewerJointAttachment::setAttachmentVisibility() is
		// setting type to 0 to actually mean "do not render". We then need to
		// test that value here and return false to prevent attachment to
		// render (in mouselook for instance).
		// *TODO: reintroduce RENDER_TYPE_NONE in LLRenderTypeMask and
		// initialize its mRenderTypeEnabled[RENDER_TYPE_NONE] to false
		// explicitely
		return type != 0 && mRenderTypeEnabled[type];
	}

	bool hasAnyRenderType(U32 type, ...) const;

	void setRenderTypeMask(U32 type, ...);
	void orRenderTypeMask(U32 type, ...);
	void andRenderTypeMask(U32 type, ...);
	void clearRenderTypeMask(U32 type, ...);
	void setAllRenderTypes();

	void pushRenderTypeMask();
	void popRenderTypeMask();

	static void toggleRenderType(U32 type);

	// For UI control of render features
	static bool hasRenderTypeControl(void* data);
	static void toggleRenderDebug(void* data);
	static void toggleRenderDebugFeature(void* data);
	static void toggleRenderTypeControl(void* data);
	static bool toggleRenderTypeControlNegated(void* data);
	static bool toggleRenderDebugControl(void* data);
	static bool toggleRenderDebugFeatureControl(void* data);
	static void setRenderDebugFeatureControl(U32 bit, bool value);
	// Sets which UV setup to display in highlight overlay
	static void setRenderHighlightTextureChannel(LLRender::eTexIndex channel)
	{
		sRenderHighlightTextureChannel = channel;
	}
 
	// Used by the PBR renderer only.
	static bool isWaterClip();

	// Use this instead of the RenderWaterReflectionType (EE renderer) or the
	// RenderTransparentWater (PBR renderer) variables, when determining what
	// should actually be renderered. For EE, beyond opaque or transparent
	// water, it accounts for the camera distance to the water, and avoids
	// renderingreflections that would not even be seen. HB
	static U32 waterReflectionType();

	static void updateRenderDeferred();
	static void refreshCachedSettings();

	static void throttleNewMemoryAllocation(bool disable);

	void addDebugBlip(const LLVector3& position, const LLColor4& color);

	LLSpatialPartition* getSpatialPartition(LLViewerObject* objp);

	// Used to toggle between EE and PBR renderers.
	void toggleRenderer();

	// Used only by the PBR renderer
	void setEnvMat(LLGLSLShader& shader);
	void bindReflectionProbes(LLGLSLShader& shader);
	void unbindReflectionProbes(LLGLSLShader& shader);

	struct RenderTargetPack;	// Must be declared public
private:
	void releasePackBuffers(RenderTargetPack* packp);

	void connectRefreshCachedSettingsSafe(const char* name);

	void unloadShaders();

	void createAuxVBs();

	void releaseScreenBuffers();
	void releaseShadowTargets();

	// For EE rendering only
	void releaseShadowTarget(U32 index);	

	// For PBR rendering only
	void releaseSunShadowTargets();
	void releaseSpotShadowTargets();

	void addToQuickLookup(LLDrawPool* new_poolp);
	void removeFromQuickLookup(LLDrawPool* poolp);

	bool updateDrawableGeom(LLDrawable* drawablep);

	// Downsample source to dest, taking the maximum depth value per pixel in
	// source and writing to dest. If source's depth buffer cannot be bound for
	// reading, a scratch space depth buffer must be provided. Only used by the
	// EE renderer.
	void downsampleDepthBuffer(LLRenderTarget& sourcep, LLRenderTarget& destp,
							   LLRenderTarget* scratchp = NULL);

	// PBR version of culling, called by updateCull(). HB
	void updateCullPBR(LLCamera& camera, LLCullResult& result);
	// PBR version of occluding, called by doOcclusion(). HB
	void doOcclusionPBR(LLCamera& camera);
	// PBR version, called by renderAlphaObjects(). HB
	void renderAlphaObjectsPBR(bool rigged);
	// PBR buffers visualization.
	void visualizeBuffers(LLRenderTarget* srcp, LLRenderTarget* dstp,
						  U32 buff_idx);

	void copyRenderTarget(LLRenderTarget* srcp, LLRenderTarget* dstp);

	// PBR samples and mipmaps generation.
	void generateLuminance(LLRenderTarget* srcp, LLRenderTarget* dstp);
	void generateExposure(LLRenderTarget* srcp, LLRenderTarget* dstp);
	void generateGlow(LLRenderTarget* srcp);
	void combineGlow(LLRenderTarget* srcp, LLRenderTarget* dstp);

	// Other PBR rendering methods
	void gammaCorrect(LLRenderTarget* srcp, LLRenderTarget* dstp);
	bool applyFXAA(LLRenderTarget* srcp, LLRenderTarget* dstp);
#if HB_PBR_SMAA_AND_CAS
	bool applySMAA(LLRenderTarget* srcp, LLRenderTarget* dstp);
	void applyCAS(LLRenderTarget* srcp, LLRenderTarget* dstp);
#endif
	bool renderDoF(LLRenderTarget* srcp, LLRenderTarget* dstp);
	void copyScreenSpaceReflections(LLRenderTarget* srcp,
									LLRenderTarget* dstp);
	void renderFinalizePBR();
	void renderDeferredLightingPBR();
	void generateSunShadowPBR();

	void bindLightFunc(LLGLSLShader& shader);
	void bindShadowMaps(LLGLSLShader& shader);

	// Applies atmospheric haze based on contents of color and depth buffer;
	// should be called just before rendering water when camera is under water
	// and just before rendering alpha when camera is above water.
	void doAtmospherics();
	// Applies water haze based on contents of color and depth buffer; should
	// be called just before rendering pre-water alpha objects.
	void doWaterHaze();

public:
	enum { GPU_CLASS_MAX = 3 };

	enum LLRenderTypeMask : U32
	{
		// Following are pool types (some are also object types)
		RENDER_TYPE_SKY							= LLDrawPool::POOL_SKY,
		RENDER_TYPE_WL_SKY						= LLDrawPool::POOL_WL_SKY,
		RENDER_TYPE_TERRAIN						= LLDrawPool::POOL_TERRAIN,
		RENDER_TYPE_SIMPLE						= LLDrawPool::POOL_SIMPLE,
		RENDER_TYPE_GRASS						= LLDrawPool::POOL_GRASS,
		RENDER_TYPE_ALPHA_MASK					= LLDrawPool::POOL_ALPHA_MASK,
		RENDER_TYPE_FULLBRIGHT_ALPHA_MASK		= LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK,
		RENDER_TYPE_FULLBRIGHT					= LLDrawPool::POOL_FULLBRIGHT,
		RENDER_TYPE_BUMP						= LLDrawPool::POOL_BUMP,
		RENDER_TYPE_MATERIALS					= LLDrawPool::POOL_MATERIALS,
		RENDER_TYPE_AVATAR						= LLDrawPool::POOL_AVATAR,
		RENDER_TYPE_PUPPET						= LLDrawPool::POOL_PUPPET,
		RENDER_TYPE_TREE						= LLDrawPool::POOL_TREE,
		// EE only
		RENDER_TYPE_INVISIBLE					= LLDrawPool::POOL_INVISIBLE,
		RENDER_TYPE_VOIDWATER					= LLDrawPool::POOL_VOIDWATER,
		RENDER_TYPE_WATER						= LLDrawPool::POOL_WATER,
		// PBR only
		RENDER_TYPE_MAT_PBR						= LLDrawPool::POOL_MAT_PBR,
		// PBR only
		RENDER_TYPE_MAT_PBR_ALPHA_MASK			= LLDrawPool::POOL_MAT_PBR_ALPHA_MASK,
 		RENDER_TYPE_ALPHA						= LLDrawPool::POOL_ALPHA,
		// PBR only
		RENDER_TYPE_ALPHA_PRE_WATER				= LLDrawPool::POOL_ALPHA_PRE_WATER,
		// PBR only
		RENDER_TYPE_ALPHA_POST_WATER			= LLDrawPool::POOL_ALPHA_POST_WATER,
		RENDER_TYPE_GLOW						= LLDrawPool::POOL_GLOW,
		RENDER_TYPE_PASS_SIMPLE 				= LLRenderPass::PASS_SIMPLE,
		RENDER_TYPE_PASS_SIMPLE_RIGGED			= LLRenderPass::PASS_SIMPLE_RIGGED,
		RENDER_TYPE_PASS_GRASS					= LLRenderPass::PASS_GRASS,
		RENDER_TYPE_PASS_FULLBRIGHT				= LLRenderPass::PASS_FULLBRIGHT,
		RENDER_TYPE_PASS_FULLBRIGHT_RIGGED		= LLRenderPass::PASS_FULLBRIGHT_RIGGED,
		RENDER_TYPE_PASS_INVISIBLE				= LLRenderPass::PASS_INVISIBLE,
		RENDER_TYPE_PASS_INVISIBLE_RIGGED		= LLRenderPass::PASS_INVISIBLE_RIGGED,
		RENDER_TYPE_PASS_INVISI_SHINY			= LLRenderPass::PASS_INVISI_SHINY,
		RENDER_TYPE_PASS_INVISI_SHINY_RIGGED	= LLRenderPass::PASS_INVISI_SHINY_RIGGED,
		RENDER_TYPE_PASS_FULLBRIGHT_SHINY		= LLRenderPass::PASS_FULLBRIGHT_SHINY,
		RENDER_TYPE_PASS_FULLBRIGHT_SHINY_RIGGED= LLRenderPass::PASS_FULLBRIGHT_SHINY_RIGGED,
		RENDER_TYPE_PASS_SHINY					= LLRenderPass::PASS_SHINY,
		RENDER_TYPE_PASS_SHINY_RIGGED			= LLRenderPass::PASS_SHINY_RIGGED,
		RENDER_TYPE_PASS_BUMP					= LLRenderPass::PASS_BUMP,
		RENDER_TYPE_PASS_BUMP_RIGGED			= LLRenderPass::PASS_BUMP_RIGGED,
		RENDER_TYPE_PASS_POST_BUMP				= LLRenderPass::PASS_POST_BUMP,
		RENDER_TYPE_PASS_POST_BUMP_RIGGED		= LLRenderPass::PASS_POST_BUMP_RIGGED,
		RENDER_TYPE_PASS_GLOW					= LLRenderPass::PASS_GLOW,
		RENDER_TYPE_PASS_GLOW_RIGGED			= LLRenderPass::PASS_GLOW_RIGGED,
		RENDER_TYPE_PASS_PBR_GLOW				= LLRenderPass::PASS_PBR_GLOW,
		RENDER_TYPE_PASS_PBR_GLOW_RIGGED		= LLRenderPass::PASS_PBR_GLOW_RIGGED,
		RENDER_TYPE_PASS_ALPHA					= LLRenderPass::PASS_ALPHA,
		RENDER_TYPE_PASS_ALPHA_MASK				= LLRenderPass::PASS_ALPHA_MASK,
		RENDER_TYPE_PASS_ALPHA_MASK_RIGGED		= LLRenderPass::PASS_ALPHA_MASK_RIGGED,
		RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK	= LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK,
		RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK_RIGGED= LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED,
		RENDER_TYPE_PASS_MATERIAL				= LLRenderPass::PASS_MATERIAL,
		RENDER_TYPE_PASS_MATERIAL_RIGGED		= LLRenderPass::PASS_MATERIAL_RIGGED,
		RENDER_TYPE_PASS_MATERIAL_ALPHA			= LLRenderPass::PASS_MATERIAL_ALPHA,
		RENDER_TYPE_PASS_MATERIAL_ALPHA_RIGGED	= LLRenderPass::PASS_MATERIAL_ALPHA_RIGGED,
		RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK	= LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
		RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK_RIGGED= LLRenderPass::PASS_MATERIAL_ALPHA_MASK_RIGGED,
		RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE= LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
		RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED= LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED,
		RENDER_TYPE_PASS_SPECMAP				= LLRenderPass::PASS_SPECMAP,
		RENDER_TYPE_PASS_SPECMAP_RIGGED			= LLRenderPass::PASS_SPECMAP_RIGGED,
		RENDER_TYPE_PASS_SPECMAP_BLEND			= LLRenderPass::PASS_SPECMAP_BLEND,
		RENDER_TYPE_PASS_SPECMAP_BLEND_RIGGED	= LLRenderPass::PASS_SPECMAP_BLEND_RIGGED,
		RENDER_TYPE_PASS_SPECMAP_MASK			= LLRenderPass::PASS_SPECMAP_MASK,
		RENDER_TYPE_PASS_SPECMAP_MASK_RIGGED	= LLRenderPass::PASS_SPECMAP_MASK_RIGGED,
		RENDER_TYPE_PASS_SPECMAP_EMISSIVE		= LLRenderPass::PASS_SPECMAP_EMISSIVE,
		RENDER_TYPE_PASS_SPECMAP_EMISSIVE_RIGGED= LLRenderPass::PASS_SPECMAP_EMISSIVE_RIGGED,
		RENDER_TYPE_PASS_NORMMAP				= LLRenderPass::PASS_NORMMAP,
		RENDER_TYPE_PASS_NORMMAP_RIGGED			= LLRenderPass::PASS_NORMMAP_RIGGED,
		RENDER_TYPE_PASS_NORMMAP_BLEND			= LLRenderPass::PASS_NORMMAP_BLEND,
		RENDER_TYPE_PASS_NORMMAP_BLEND_RIGGED	= LLRenderPass::PASS_NORMMAP_BLEND_RIGGED,
		RENDER_TYPE_PASS_NORMMAP_MASK			= LLRenderPass::PASS_NORMMAP_MASK,
		RENDER_TYPE_PASS_NORMMAP_MASK_RIGGED	= LLRenderPass::PASS_NORMMAP_MASK_RIGGED,
		RENDER_TYPE_PASS_NORMMAP_EMISSIVE		= LLRenderPass::PASS_NORMMAP_EMISSIVE,
		RENDER_TYPE_PASS_NORMMAP_EMISSIVE_RIGGED= LLRenderPass::PASS_NORMMAP_EMISSIVE_RIGGED,
		RENDER_TYPE_PASS_NORMSPEC				= LLRenderPass::PASS_NORMSPEC,
		RENDER_TYPE_PASS_NORMSPEC_RIGGED		= LLRenderPass::PASS_NORMSPEC_RIGGED,
		RENDER_TYPE_PASS_NORMSPEC_BLEND			= LLRenderPass::PASS_NORMSPEC_BLEND,
		RENDER_TYPE_PASS_NORMSPEC_BLEND_RIGGED	= LLRenderPass::PASS_NORMSPEC_BLEND_RIGGED,
		RENDER_TYPE_PASS_NORMSPEC_MASK			= LLRenderPass::PASS_NORMSPEC_MASK,
		RENDER_TYPE_PASS_NORMSPEC_MASK_RIGGED	= LLRenderPass::PASS_NORMSPEC_MASK_RIGGED,
		RENDER_TYPE_PASS_NORMSPEC_EMISSIVE		= LLRenderPass::PASS_NORMSPEC_EMISSIVE,
		RENDER_TYPE_PASS_NORMSPEC_EMISSIVE_RIGGED= LLRenderPass::PASS_NORMSPEC_EMISSIVE_RIGGED,
		RENDER_TYPE_PASS_MAT_PBR				= LLRenderPass::PASS_MAT_PBR,
		RENDER_TYPE_PASS_MAT_PBR_RIGGED			= LLRenderPass::PASS_MAT_PBR_RIGGED,
		RENDER_TYPE_PASS_MAT_ALPHA_MASK_PBR		= LLRenderPass::PASS_MAT_PBR_ALPHA_MASK,
		RENDER_TYPE_PASS_MAT_ALPHA_MASK_PBR_RIGGED= LLRenderPass::PASS_MAT_PBR_ALPHA_MASK_RIGGED,
		// Following are object types (only used in drawable mRenderType)
		RENDER_TYPE_HUD							= LLRenderPass::NUM_RENDER_TYPES,
		RENDER_TYPE_VOLUME,
		RENDER_TYPE_PARTICLES,
		RENDER_TYPE_CLOUDS,
		RENDER_TYPE_HUD_PARTICLES,
		NUM_RENDER_TYPES,
		END_RENDER_TYPES						= NUM_RENDER_TYPES
	};

	enum LLRenderDebugFeatureMask
	{
		RENDER_DEBUG_FEATURE_UI					= 0x0001,
		RENDER_DEBUG_FEATURE_SELECTED			= 0x0002,
		RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES	= 0x0008,
		RENDER_DEBUG_FEATURE_FLEXIBLE			= 0x0010,
		RENDER_DEBUG_FEATURE_FOG				= 0x0020,
	};

	enum LLRenderDebugMask : U32
	{
		RENDER_DEBUG_COMPOSITION		= 0x00000001,
		RENDER_DEBUG_VERIFY				= 0x00000002,
		RENDER_DEBUG_BBOXES				= 0x00000004,
		RENDER_DEBUG_OCTREE				= 0x00000008,
		RENDER_DEBUG_WIND_VECTORS		= 0x00000010,
		RENDER_DEBUG_OCCLUSION			= 0x00000020,
		RENDER_DEBUG_POINTS				= 0x00000040,
		RENDER_DEBUG_TEXTURE_PRIORITY	= 0x00000080,
		RENDER_DEBUG_TEXTURE_AREA		= 0x00000100,
		RENDER_DEBUG_FACE_AREA			= 0x00000200,
		RENDER_DEBUG_PARTICLES			= 0x00000400,
		RENDER_DEBUG_TEXTURE_ANIM		= 0x00000800,
		RENDER_DEBUG_LIGHTS				= 0x00001000,
		RENDER_DEBUG_BATCH_SIZE			= 0x00002000,
		RENDER_DEBUG_RAYCAST			= 0x00004000,
		RENDER_DEBUG_AVATAR_DRAW_INFO	= 0x00008000,
		RENDER_DEBUG_SHADOW_FRUSTA		= 0x00010000,
		RENDER_DEBUG_SCULPTED			= 0x00020000,
		RENDER_DEBUG_AVATAR_VOLUME		= 0x00040000,
		RENDER_DEBUG_AVATAR_JOINTS		= 0x00080000,
		RENDER_DEBUG_AGENT_TARGET		= 0x00100000,
		RENDER_DEBUG_UPDATE_TYPE		= 0x00200000,
		RENDER_DEBUG_PHYSICS_SHAPES	 	= 0x00400000,
		RENDER_DEBUG_NORMALS			= 0x00800000,
		RENDER_DEBUG_LOD_INFO			= 0x01000000,
		RENDER_DEBUG_RENDER_COMPLEXITY	= 0x02000000,
		RENDER_DEBUG_ATTACHMENT_INFO	= 0x04000000,
		RENDER_DEBUG_TEXTURE_SIZE		= 0x08000000,
		RENDER_DEBUG_REFLECTION_PROBES	= 0x10000000,
	};

public:
	// Aligned members
	alignas(16) LLMatrix4a			mShadowModelview[6];
	alignas(16) LLMatrix4a			mShadowProjection[6];
	alignas(16) LLMatrix4a			mSunShadowMatrix[6];
	LLMatrix4a						mReflectionModelView;
	LLVector4a						mTransformedSunDir;
	LLVector4a						mTransformedMoonDir;

	LLReflectionMapManager			mReflectionMapManager;

	bool							mBackfaceCull;
	bool							mNeedsDrawStats;
	U32								mPoissonOffset;
	S32								mBatchCount;
	S32								mMatrixOpCount;
	S32								mTextureMatrixOps;
	U32								mMaxBatchSize;
	U32								mMinBatchSize;
	S32								mTrianglesDrawn;
	S32								mNumVisibleNodes;
	LLStat							mTrianglesDrawnStat;

	// 0 = no occlusion, 1 = read only, 2 = read/write:
	static S32						sUseOcclusion;
	static S32						sVisibleLightCount;
	static bool						sFreezeTime;
	static bool						sShowHUDAttachments;
	static bool						sAutoMaskAlphaDeferred;
	static bool						sAutoMaskAlphaNonDeferred;
	static bool						sUseFarClip;
	static bool						sShadowRender;
	static bool						sDynamicLOD;
	static bool						sPickAvatar;
	static bool						sReflectionRender;
	static bool						sImpostorRender;
	static bool						sImpostorRenderAlphaDepthPass;
	static bool						sAvatarPreviewRender;
	static bool						sUnderWaterRender;
	static bool						sCanRenderGlow;
	static bool						sRenderFrameTest;
	static bool						sRenderAttachedLights;
	static bool						sRenderAttachedParticles;
	static bool						sRenderDeferred;
	static bool						sRenderWater;	// Used by llvosky.cpp
	static bool						sRenderingHUDs;

	// Utility buffer for rendering post effects, gets abused by
	// renderDeferredLighting
	LLPointer<LLVertexBuffer> 			mDeferredVB;

	// A single triangle that covers the whole screen.
	LLPointer<LLVertexBuffer> 			mScreenTriangleVB;

	// Utility buffer for glow combine. Used for EE rendering.
	LLPointer<LLVertexBuffer> 			mGlowCombineVB;

	// Utility buffer for rendering cubes, 8 vertices are corners of a cube
	// [-1, 1]
	LLPointer<LLVertexBuffer>			mCubeVB;

	struct RenderTargetPack
	{
		RenderTargetPack()
		:	mWidth(0),
			mHeight(0)
		{
		}

		LLRenderTarget	mScreen;
		LLRenderTarget	mDeferredScreen;
		LLRenderTarget	mDeferredLight;
		LLRenderTarget	mFXAABuffer;
		// For SMAA and CAS shaders (for now, EE only).
		LLRenderTarget	mSMAAEdgeBuffer;
		LLRenderTarget	mSMAABlendBuffer;
		LLRenderTarget	mScratchBuffer;
		// PBR renderer only
		LLRenderTarget	mEdgeMap;
		LLRenderTarget	mSunShadow[4];

		U32				mWidth;
		U32				mHeight;
	};
	// Main, full resolution render targets pack
	RenderTargetPack					mMainRT;
	// Auxillary, 512x512 pixels render targets pack, for PBR only
	RenderTargetPack					mAuxillaryRT;
	// Currently used render targets pack
	RenderTargetPack*					mRT;

	// Texture for making the glow
	LLRenderTarget						mGlow[3];

	// Water distortion texture (refraction)
	LLRenderTarget						mWaterDis;

	// EE render targets

	LLRenderTarget						mWaterRef;			// Water reflection
	LLRenderTarget						mShadow[6];
	LLRenderTarget						mShadowOcclusion[6];
	LLRenderTarget						mDeferredDepth;
	LLRenderTarget						mOcclusionDepth;
	LLRenderTarget						mPhysicsDisplay;

	// PBR render targets

	LLRenderTarget						mSpotShadow[2];
	LLRenderTarget						mSceneMap;
	LLRenderTarget						mLuminanceMap;
	LLRenderTarget						mExposureMap;
	LLRenderTarget						mLastExposure;
	LLRenderTarget						mPostMap;
	LLRenderTarget						mPbrBrdfLut;

	// Sun shadow map
	std::vector<LLVector3>				mShadowFrustPoints[4];
	LLCamera							mShadowCamera[8];
	LLVector3							mShadowExtents[4][2];

	LLPointer<LLDrawable>				mTargetShadowSpotLight[2];
	LLPointer<LLDrawable>				mShadowSpotLight[2];
	F32									mSpotLightFade[2];

	LLVector4							mSunClipPlanes;

	LLCullResult						mSky;
	LLCullResult						mReflectedObjects;
	LLCullResult						mRefractedObjects;

	U32									mLightFunc;

	// Noise maps
	U32									mNoiseMap;
	U32									mTrueNoiseMap;

	// SMAA maps
	U32									mAreaMap;
	U32									mSearchMap;

	// -1 = failed, 0 = unloaded, 1 = loaded
	S32									mVertexShadersLoaded;

	// Cached sky environment and water height/camera values. HB
	LLColor4							mSunDiffuse;
	LLColor4							mMoonDiffuse;
	LLVector4							mSunDir;
	LLVector4							mMoonDir;
	LLColor4							mTotalAmbient;
	F32									mProbeAmbiance;
	F32									mSkyGamma;
	F32									mEyeAboveWater;
	F32									mWaterHeight;
	bool								mIsSunUp;
	bool								mIsMoonUp;

private:
	bool								mInitialized;

	bool								mRenderTypeEnabled[NUM_RENDER_TYPES];
	std::stack<std::string>				mRenderTypeEnableStack;

	U32									mRenderDebugMask;
	U32									mOldRenderDebugMask;
	U32									mRenderDebugFeatureMask;

	// Screen texture
	U32 								mScreenWidth;
	U32 								mScreenHeight;

	LLDrawable::draw_vec_t				mMovedList;
	LLDrawable::draw_vec_t				mMovedBridge;
	LLDrawable::draw_vec_t				mShiftList;

	struct Light
	{
		LL_INLINE Light(LLDrawable* drawablep, F32 d, F32 f = 0.f)
		:	drawable(drawablep),
			dist(d),
			fade(f)
		{
		}
		LLPointer<LLDrawable> drawable;
		F32 dist;
		F32 fade;

		struct compare
		{
			LL_INLINE bool operator()(const Light& a, const Light& b) const
			{
				if (a.dist < b.dist)
				{
					return true;
				}
				if (a.dist > b.dist)
				{
					return false;
				}
				return a.drawable < b.drawable;
			}
		};
	};
	typedef std::set<Light, Light::compare> light_set_t;

	LLDrawable::draw_set_t				mLights;
	light_set_t							mNearbyLights; // Lights near camera
	LLColor4							mHWLightColors[8];

	/////////////////////////////////////////////
	// Different queues of drawables being processed.

	LLDrawable::draw_list_t 			mBuildQ;
	LLSpatialGroup::sg_vector_t			mGroupQ;
	// A place to save mGroupQ until it is safe to unref
	LLSpatialGroup::sg_vector_t			mGroupSaveQ;

	// Drawables that need to update their spatial partition radius
	LLDrawable::draw_vec_t				mPartitionQ;

	// Groups that need rebuildMesh called
	LLSpatialGroup::sg_vector_t			mMeshDirtyGroup;
	U32 mMeshDirtyQueryObject;

	bool								mGroupQLocked;

	// If true, clear vertex buffers on next update
	bool								mResetVertexBuffers;

	LLViewerObject::vobj_list_t			mCreateQ;

	LLDrawable::draw_set_t				mRetexturedList;

	//////////////////////////////////////////////////
	// Draw pools are responsible for storing all rendered data and performing
	// the actual rendering of objects.

	struct compare_pools
	{
		LL_INLINE bool operator()(const LLDrawPool* a,
								  const LLDrawPool* b) const
		{
			if (!a)
			{
				return true;
			}
			if (!b)
			{
				return false;
			}

			S32 atype = a->getType();
			S32 btype = b->getType();
			if (atype < btype)
			{
				return true;
			}
			if (atype > btype)
			{
				return false;
			}

			return a->getId() < b->getId();
		}
	};
 	typedef std::set<LLDrawPool*, compare_pools> pool_set_t;
	pool_set_t							mPools;
	LLDrawPool*							mLastRebuildPool;

	// For quick-lookups into mPools (mapped by texture pointer). Note: no need
	// to keep an quick-lookup to avatar pools, since there is only one per
	// avatar.
	typedef fast_hmap<uintptr_t, LLDrawPool*> pool_tex_map_t;
	pool_tex_map_t						mTerrainPools;
	pool_tex_map_t						mTreePools;
	LLDrawPool*							mSkyPool;
	LLDrawPool*							mTerrainPool;
	LLDrawPool*							mWaterPool;
	LLRenderPass*						mSimplePool;
	LLRenderPass*						mGrassPool;
	LLRenderPass*						mAlphaMaskPool;
	LLRenderPass*						mFullbrightAlphaMaskPool;
	LLRenderPass*						mFullbrightPool;
	LLDrawPool*							mGlowPool;
	LLDrawPool*							mBumpPool;
	LLDrawPool*							mMaterialsPool;
	LLDrawPool*							mWLSkyPool;
	// EE renderer only
	LLDrawPoolAlpha*					mAlphaPool;
	LLDrawPool*							mInvisiblePool;
	// PBR renderer only
	LLDrawPoolAlpha*					mAlphaPoolPreWater;
	LLDrawPoolAlpha*					mAlphaPoolPostWater;
	LLDrawPool*							mPBROpaquePool;
	LLDrawPool*							mPBRAlphaMaskPool;
	

	std::vector<LLFace*>				mSelectedFaces;

	class DebugBlip
	{
	public:
		DebugBlip(const LLVector3& position, const LLColor4& color)
		:	mColor(color),
			mPosition(position),
			mAge(0.f)
		{
		}

	public:
		LLColor4	mColor;
		LLVector3	mPosition;
		F32			mAge;
	};
	std::list<DebugBlip>				mDebugBlips;

	LLPointer<LLViewerFetchedTexture>	mFaceSelectImagep;

	U32									mLightMask;

public:
	// Set in llenvsettings.cpp so that LLPipeline and LLDrawPoolAlpha have
	// quick access to the water plane in eye space.
	static LLVector4a					sWaterPlane;

	// Beacon highlights
	std::vector<LLFace*>				mHighlightFaces;

	static LLCullResult*				sCull;

	// Debug use
	static U32							sCurRenderPoolType;

	static LLRender::eTexIndex			sRenderHighlightTextureChannel;

	// Cached settings:

	static LLColor4						PreviewAmbientColor;
	static LLColor4						PreviewDiffuse0;
	static LLColor4						PreviewSpecular0;
	static LLColor4						PreviewDiffuse1;
	static LLColor4						PreviewSpecular1;
	static LLColor4						PreviewDiffuse2;
	static LLColor4						PreviewSpecular2;
	static LLVector3					PreviewDirection0;
	static LLVector3					PreviewDirection1;
	static LLVector3					PreviewDirection2;
	static LLVector3					RenderGlowLumWeights;
	static LLVector3					RenderGlowWarmthWeights;
	static LLVector3					RenderSSAOEffect;
	static LLVector3					RenderShadowGaussian;
	static LLVector3					RenderShadowClipPlanes;
	static LLVector3					RenderShadowOrthoClipPlanes;
	static LLVector3					RenderShadowSplitExponent;
	static U32							sRenderByOwner;
	static F32							RenderDeferredSunWash;
	static F32							RenderDeferredDisplayGamma;
	static U32							RenderFSAASamples;
	static S32							RenderDeferredAAQuality;
	static U32							RenderResolutionDivisor;
	static U32							RenderShadowDetail;
	static F32							RenderShadowResolutionScale;
	static U32							RenderLocalLightCount;
	static U32							DebugBeaconLineWidth;
	static F32							RenderGlowMinLuminance;
	static F32							RenderGlowMaxExtractAlpha;
	static F32							RenderGlowWarmthAmount;
	static U32							RenderGlowResolutionPow;
	static U32							RenderGlowIterations;
	static F32							RenderGlowWidth;
	static F32							RenderGlowStrength;
	static F32							RenderShadowNoise;
	static F32							RenderShadowBlurSize;
	static F32							RenderSSAOScale;
	static U32							RenderSSAOMaxScale;
	static F32							RenderSSAOFactor;
	static F32							RenderShadowBiasError;
	static F32							RenderShadowOffset;
	static F32							RenderShadowOffsetNoSSAO;
	static F32							RenderShadowBias;
	static F32							RenderSpotShadowOffset;
	static F32							RenderSpotShadowBias;
	static F32							RenderShadowBlurDistFactor;
	static U32							RenderWaterReflectionType;
	static F32							RenderFarClip;
	static F32							RenderShadowErrorCutoff;
	static F32							RenderShadowFOVCutoff;
	static F32							CameraMaxCoF;
	static F32							CameraDoFResScale;
	static U32							RenderAutoHideGeometryMemoryLimit;
	static F32							RenderAutoHideSurfaceAreaLimit;
	static S32							RenderBufferVisualization;
	static U32							RenderScreenSpaceReflectionIterations;
	static F32							RenderScreenSpaceReflectionRayStep;
	static F32							RenderScreenSpaceReflectionDistanceBias;
	static F32							RenderScreenSpaceReflectionDepthRejectBias;
	static F32							RenderScreenSpaceReflectionAdaptiveStepMultiplier;
	static U32							RenderScreenSpaceReflectionGlossySamples;
	static bool							RenderScreenSpaceReflections;
	static bool							RenderDeferred;
	static bool							RenderDeferredSSAO;
	static bool							RenderShadowSoften;
	static bool							RenderDelayCreation;
	static bool							RenderAnimateRes;
	static bool							RenderSpotLightsInNondeferred;
	static bool							RenderDepthOfField;
	static bool							RenderDepthOfFieldInEditMode;
	static bool							RenderDeferredAASharpen;
	static bool							RenderDeferredAtmospheric;
	static bool							RenderGlow;
	static bool							CameraOffset;
	// Only for use by the PBR renderer (EE uses RenderWaterReflectionType). HB
	static bool							RenderTransparentWater;
	static bool							sRenderScriptedBeacons;
	static bool							sRenderScriptedTouchBeacons;
	static bool							sRenderPhysicalBeacons;
	static bool							sRenderPermanentBeacons;
	static bool							sRenderCharacterBeacons;
	static bool							sRenderSoundBeacons;
	static bool							sRenderInvisibleSoundBeacons;
	static bool							sRenderParticleBeacons;
	static bool							sRenderMOAPBeacons;
	static bool							sRenderHighlight;
	static bool							sRenderBeacons;
	static bool							sRenderAttachments;

	static bool							sRenderBeaconsFloaterOpen;

	// IMPORTANT: this MUST always be false while in EE rendering mode. HB
	static bool							sReflectionProbesEnabled;
};

extern LLPipeline gPipeline;
extern bool gShiftFrame;
extern const LLMatrix4* gGLLastMatrix;

// Helper class for disabling occlusion culling for the current stack frame
class LLDisableOcclusionCulling
{
public:
	LL_INLINE LLDisableOcclusionCulling()
	:	mUseOcclusion(LLPipeline::sUseOcclusion)
	{
		LLPipeline::sUseOcclusion = 0;
	}

	LL_INLINE ~LLDisableOcclusionCulling()
	{
		LLPipeline::sUseOcclusion = mUseOcclusion;
	}

private:
	S32 mUseOcclusion;
};

// Helper class to allow rendering preview scenes (such as for preview avatars)
// with a lighting that is not inluenced by the environment settings.
class LLPreviewLighting
{
public:
	LL_INLINE LLPreviewLighting()
	{
		gPipeline.enableLightsPreview();
		gGL.freezeLightState(true);
		LLPipeline::sAvatarPreviewRender = true;
		mSavedSunUp = gPipeline.mIsSunUp;
		gPipeline.mIsSunUp = true;
		mSavedMoonUp = gPipeline.mIsMoonUp;
		gPipeline.mIsMoonUp = false;
		mSavedSunDiffuse = gPipeline.mSunDiffuse;
		gPipeline.mSunDiffuse.set(1.f, 1.f, 1.f, 1.f);
	}

	LL_INLINE ~LLPreviewLighting()
	{
		gGL.freezeLightState(false);
		LLPipeline::sAvatarPreviewRender = false;
		gPipeline.mIsSunUp = mSavedSunUp;
		gPipeline.mIsMoonUp = mSavedMoonUp;
		gPipeline.mSunDiffuse = mSavedSunDiffuse;
	}

private:
	LLColor4	mSavedSunDiffuse;
	bool		mSavedSunUp;
	bool		mSavedMoonUp;
};

void render_bbox(const LLVector3& min, const LLVector3& max);
void render_hud_elements();

#endif	// LL_LLPIPELINE_H
