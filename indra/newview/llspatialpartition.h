/**
 * @file llspatialpartition.h
 * @brief LLSpatialGroup header file including definitions for supporting functions
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLSPATIALPARTITION_H
#define LL_LLSPATIALPARTITION_H

#include <queue>

#include "llcubemap.h"
#include "hbfastmap.h"
#include "llmaterial.h"
#include "lloctree.h"
#include "llrefcount.h"
#include "llvertexbuffer.h"

#include "lldrawable.h"
#include "lldrawpool.h"
#include "llface.h"
#include "llfetchedgltfmaterial.h"
#include "llviewercamera.h"
#include "llvoavatar.h"

#define SG_MIN_DIST_RATIO 0.00001f
#define SG_STATE_INHERIT_MASK (OCCLUDED)
#define SG_INITIAL_STATE_MASK (DIRTY | GEOM_DIRTY)

class LLColor4U;
class LLSpatialBridge;
class LLSpatialPartition;
class LLViewerOctreePartition;
class LLViewerRegion;

// NOTE: mask is ignored for PBR rendering. HB
void pushVerts(LLFace* face, U32 mask);

class alignas(16) LLDrawInfo final : public LLRefCount
{
protected:
	LOG_CLASS(LLDrawInfo);

	~LLDrawInfo();

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

	LL_INLINE LLDrawInfo(const LLDrawInfo& rhs)
	{
		*this = rhs;
	}

	LL_INLINE const LLDrawInfo& operator=(const LLDrawInfo& rhs)
	{
		llerrs << "Illegal operation !" << llendl;
		return *this;
	}

	LLDrawInfo(U16 start, U16 end, U32 count, U32 offset,
			   LLViewerTexture* texp, LLVertexBuffer* bufferp,
			   bool fullbright = false, U8 bump = 0);

	void validate();

	// Returns mSkinHash->mHash, or 0 if mSkinHash is NULL
	U64 getSkinHash();

	// Returns a hash of this LLDrawInfo as a debug color
	LLColor4U getDebugColor();

	struct CompareTexture
	{
		LL_INLINE bool operator()(const LLDrawInfo& lhs, const LLDrawInfo& rhs)
		{
			return lhs.mTexture > rhs.mTexture;
		}
	};

	struct CompareTexturePtr
	{
		// Sort by texture
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			// sort by pointer, sort NULL down to the end
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() &&
					 lhs->mTexture.get() > rhs->mTexture.get()));
		}
	};

	struct CompareVertexBuffer
	{
		// Sort by texture
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			// sort by pointer, sort NULL down to the end
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() &&
					 lhs->mVertexBuffer.get() > rhs->mVertexBuffer.get()));
		}
	};

	struct CompareTexturePtrMatrix
	{
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() &&
					 (lhs->mTexture.get() > rhs->mTexture.get() ||
					  (lhs->mTexture.get() == rhs->mTexture.get() &&
					   lhs->mModelMatrix > rhs->mModelMatrix))));
		}
	};

	struct CompareMatrixTexturePtr
	{
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() &&
					 (lhs->mModelMatrix > rhs->mModelMatrix ||
					  (lhs->mModelMatrix == rhs->mModelMatrix &&
					   lhs->mTexture.get() > rhs->mTexture.get()))));
		}
	};

	struct CompareBump
	{
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			// Sort by mBump value, sort NULL down to the end
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() && lhs->mBump > rhs->mBump));
		}
	};

	struct CompareDistanceGreater
	{
		LL_INLINE bool operator()(const LLPointer<LLDrawInfo>& lhs,
								  const LLPointer<LLDrawInfo>& rhs)
		{
			// Sort by mBump value, sort NULL down to the end
			return lhs.get() != rhs.get() &&
				   (lhs.isNull() ||
					(rhs.notNull() && lhs->mDistance > rhs->mDistance));
		}
	};

public:
	// Note: before these variables, we find the 32 bits counter from
	// LLRefCount... Since mExtents will be 16-bytes aligned, fill-up the gap
	// in the cache line with other member variables. HB
	U32									mShaderMask;

	U32									mBlendFuncSrc;
	U32									mBlendFuncDst;

	alignas(16) LLVector4a				mExtents[2];

	LLPointer<LLVertexBuffer>			mVertexBuffer;
	LLPointer<LLViewerTexture>			mTexture;
	LLPointer<LLVOAvatar>				mAvatar;
	LLPointer<LLMeshSkinInfo>			mSkinInfo;
	// PBR material parameters, for the PBR renderer only.
	LLPointer<LLFetchedGLTFMaterial>	mGLTFMaterial;

	const LLMatrix4*					mTextureMatrix;
	const LLMatrix4*					mModelMatrix;
	U16									mStart;
	U16									mEnd;
	U32									mCount;
	U32									mOffset;
	F32									mVSize;
	F32									mDistance;

	typedef std::vector<LLPointer<LLViewerTexture> > tex_vec_t;
	tex_vec_t							mTextureList;

	// Virtual size of mTexture and mTextureList textures used to update the
	// decode priority of textures in this DrawInfo.
	std::vector<F32>					mTextureListVSize;

	// If mMaterial is null, the following parameters are unused:
	LLMaterialPtr						mMaterial;
	LLUUID								mMaterialID;
	LLPointer<LLViewerTexture>			mSpecularMap;
	LLPointer<LLViewerTexture>			mNormalMap;
	// XYZ = Specular RGB, W = Specular Exponent:
	LLVector4							mSpecColor;
	F32									mEnvIntensity;
	F32									mAlphaMaskCutoff;

	// Cache for getDebugColor(). HB
	LLColor4U							mDebugColor;

	bool								mFullbright;
	bool								mHasGlow;
	U8									mBump;
	U8									mShiny;
	U8									mDiffuseAlphaMode;
};

class alignas(16) LLSpatialGroup final : public LLOcclusionCullingGroup
{
	friend class LLSpatialPartition;
	friend class LLOctreeStateCheck;

protected:
	LOG_CLASS(LLSpatialGroup);

	~LLSpatialGroup() override;

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

	LLSpatialGroup(const LLSpatialGroup& rhs)
	:	LLOcclusionCullingGroup(rhs)
	{
		*this = rhs;
	}

	const LLSpatialGroup& operator=(const LLSpatialGroup& rhs)
	{
		llerrs << "Illegal operation!" << llendl;
		return *this;
	}

	typedef std::vector<LLPointer<LLSpatialGroup> > sg_vector_t;
	typedef std::vector<LLPointer<LLSpatialBridge> > bridge_list_t;
	typedef std::vector<LLPointer<LLDrawInfo> > drawmap_elem_t;
	typedef fast_hmap<U32, drawmap_elem_t > draw_map_t;
	typedef std::vector<LLPointer<LLVertexBuffer> > buffer_list_t;
	typedef fast_hmap<LLFace*, buffer_list_t> buffer_texture_map_t;
	typedef fast_hmap<U32, buffer_texture_map_t> buffer_map_t;

	struct CompareDistanceGreater
	{
		LL_INLINE bool operator()(const LLSpatialGroup* const& lhs,
								  const LLSpatialGroup* const& rhs)
		{
			return lhs->mDistance > rhs->mDistance;
		}
	};

	struct CompareDepthGreater
	{
		LL_INLINE bool operator()(const LLSpatialGroup* const& lhs,
								  const LLSpatialGroup* const& rhs)
		{
			return lhs->mDepth > rhs->mDepth;
		}
	};

	struct CompareRenderOrder
	{
		LL_INLINE bool operator()(const LLSpatialGroup* const& lhs,
								  const LLSpatialGroup* const& rhs)
		{
			if (lhs->mAvatarp != rhs->mAvatarp)
			{
				return lhs->mAvatarp < rhs->mAvatarp;
			}
			return lhs->mRenderOrder < rhs->mRenderOrder;
		}
	};

	typedef enum
	{
		GEOM_DIRTY				= LLViewerOctreeGroup::INVALID_STATE,
		ALPHA_DIRTY				= GEOM_DIRTY << 1,
		IN_IMAGE_QUEUE			= ALPHA_DIRTY << 1,
		IMAGE_DIRTY				= IN_IMAGE_QUEUE << 1,
		MESH_DIRTY				= IMAGE_DIRTY << 1,
		NEW_DRAWINFO			= MESH_DIRTY << 1,
		IN_BUILD_QUEUE			= NEW_DRAWINFO << 1,
		STATE_MASK				= 0x0000FFFF,
	} eSpatialState;

	LLSpatialGroup(OctreeNode* node, LLSpatialPartition* part);

	bool isHUDGroup();

	void clearDrawMap();

	void setState(U32 state, S32 mode);
	void clearState(U32 state, S32 mode);
	LL_INLINE void clearState(U32 state)				{ mState &= ~state; }

	LLSpatialGroup* getParent();

	bool addObject(LLDrawable* drawablep);
	bool removeObject(LLDrawable* drawablep, bool from_octree = false);

	// Update position if it's in the group:
	bool updateInGroup(LLDrawable* drawablep, bool immediate = false);

	void shift(const LLVector4a& offset);
	void destroyGL(bool keep_occlusion = false);

	void updateDistance(LLCamera& camera);
	bool changeLOD();
	void rebuildGeom();
	void rebuildMesh();

	LL_INLINE void setState(U32 state)					{ mState |= state; }
	LL_INLINE void dirtyGeom()							{ setState(GEOM_DIRTY); }
	LL_INLINE void dirtyMesh()							{ setState(MESH_DIRTY); }

	void drawObjectBox(LLColor4 col);

	LLDrawable* lineSegmentIntersect(const LLVector4a& start,
									 const LLVector4a& end,
									 bool pick_transparent, bool pick_rigged,
									 S32* face_hit,
									 LLVector4a* intersection = NULL,
									 LLVector2* tex_coord = NULL,
									 LLVector4a* normal = NULL,
									 LLVector4a* tangent = NULL);

	LL_INLINE LLSpatialPartition* getSpatialPartition()
	{
		return (LLSpatialPartition*)mSpatialPartition;
	}

	// LLOcclusionCullingGroup overrides
	void handleInsertion(const TreeNode* node,
						 LLViewerOctreeEntry* entry) override;
	void handleRemoval(const TreeNode* node,
					   LLViewerOctreeEntry* entry) override;
	void handleDestruction(const TreeNode* node) override;
	void handleChildAddition(const OctreeNode* parent,
							 OctreeNode* child) override;

public:
	LLVector4a					mViewAngle;
	LLVector4a					mLastUpdateViewAngle;

	// Cached llmax(mObjectBounds[1].getLength3(), 10.f)
	F32							mObjectBoxSize;

protected:
	static S32 sLODSeed;

public:
	LLPointer<LLVertexBuffer>	mVertexBuffer;

	// Reflection Probe associated with this node (if any)
	LLPointer<LLReflectionMap>	mReflectionProbe;

	bridge_list_t				mBridgeList;

	// Used by volume buffers to attempt to reuse vertex buffers:
	buffer_map_t				mBufferMap;

	draw_map_t					mDrawMap;

	// Used by LLVOAvatar to set render order in alpha draw pool to preserve
	// legacy render order behaviour.
	LLVOAvatar*					mAvatarp;
	U32							mRenderOrder;

	F32							mBuilt;
	F32							mDistance;
	F32							mDepth;
	F32							mLastUpdateDistance;
	F32							mLastUpdateTime;
	F32							mPixelArea;
	F32							mRadius;

	// Used by volumes to track how many bytes of geometry data are in this
	// node:
	U32							mGeometryBytes;
	// Used by volumes to track estimated surface area of geometry in this
	// node:
	F32							mSurfaceArea;

	static U32					sNodeCount;

	// Deletion of spatial groups and draw info not allowed if true:
	static bool					sNoDelete;
};

class LLGeometryManager
{
public:
	std::vector<LLFace*> mFaceList;
	virtual ~LLGeometryManager() = default;
	virtual void rebuildGeom(LLSpatialGroup* group) = 0;
	virtual void rebuildMesh(LLSpatialGroup* group) = 0;
	virtual void getGeometry(LLSpatialGroup* group) = 0;
	virtual void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
								  U32& index_count);

	// Note: not for PBR rendering
	virtual LLVertexBuffer* createVertexBuffer(U32 type_mask);
};

class LLSpatialPartition : public LLViewerOctreePartition,
						   public LLGeometryManager
{
protected:
	LOG_CLASS(LLSpatialPartition);

public:
	LLSpatialPartition(U32 data_mask, bool render_by_group,
					   LLViewerRegion* regionp);
	~LLSpatialPartition() override;

	LLSpatialGroup* put(LLDrawable* drawablep, bool was_visible = false);
	bool remove(LLDrawable* drawablep, LLSpatialGroup* curp);

	LLDrawable* lineSegmentIntersect(const LLVector4a& start,
									 const LLVector4a& end,
									 bool pick_transparent, bool pick_rigged,
									 S32* face_hit,
									 LLVector4a* intersection = NULL,
									 LLVector2* tex_coord = NULL,
									 LLVector4a* normal = NULL,
									 LLVector4a* tangent = NULL);

	// If the drawable moves, move it here.
	virtual void move(LLDrawable* drawablep, LLSpatialGroup* curp,
					  bool immediate = false);
	virtual void shift(const LLVector4a& offset);

	virtual F32 calcDistance(LLSpatialGroup* group, LLCamera& camera);
	virtual F32 calcPixelArea(LLSpatialGroup* group, LLCamera& camera);

	void rebuildGeom(LLSpatialGroup* group) override;

	LL_INLINE void rebuildMesh(LLSpatialGroup*) override
	{
	}

	// Cull on arbitrary frustum
	S32 cull(LLCamera& camera, bool do_occlusion = false) override;
	S32 cull(LLCamera& camera, std::vector<LLDrawable*>* res, bool for_sel);

	bool isVisible(const LLVector3& v);
	bool isHUDPartition();

	LL_INLINE LLSpatialBridge* asBridge()			{ return mBridge; }
	LL_INLINE bool isBridge()						{ return asBridge() != NULL; }

	void renderPhysicsShapes(bool wireframe = false);
	void renderDebug();

	void restoreGL();
	void resetVertexBuffers();

	bool getVisibleExtents(LLCamera& camera, LLVector3& visMin,
						   LLVector3& visMax);

public:
	// NULL for non-LLSpatialBridge instances, otherwise, mBridge == this. Uses
	// a pointer instead of making "isBridge" and "asBridge" virtual so it is
	// safe to call asBridge() from the destructor:
	LLSpatialBridge*	mBridge;

	U32					mVertexDataMask;

	// Percentage distance must change before drawables receive LOD update
	// (default is 0.25):
	F32					mSlopRatio;

	// If true, frustum culling ignores far clip plane:
	bool				mInfiniteFarClip;

	// If true, objects in this partition will be written to depth during alpha
	// rendering:
	bool				mDepthMask;

	const bool			mRenderByGroup;

	// Started to issue a teleport request
	static bool			sTeleportRequested;
};

// Class for creating bridges between spatial partitions
class LLSpatialBridge : public LLDrawable, public LLSpatialPartition
{
protected:
	LOG_CLASS(LLSpatialBridge);

	~LLSpatialBridge() override;

public:
	typedef std::vector<LLPointer<LLSpatialBridge> > bridge_vector_t;

	LLSpatialBridge(LLDrawable* root, bool render_by_group, U32 data_mask,
					LLViewerRegion* regionp);

	void destroyTree();

	// Transforms agent space camera into this Spatial Bridge coordinate frame
	LLCamera transformCamera(LLCamera& camera);

	// LLDrawable overrides

	LL_INLINE bool isSpatialBridge() const override		{ return true; }

	LL_INLINE LLSpatialPartition* asPartition() override
	{
		return this;
	}

	void updateBinRadius() override;
	void updateSpatialExtents() override;
	// Transform agent space camera into this Spatial Bridge's coordinate frame
	void transformExtents(const LLVector4a* src, LLVector4a* dst);

	void setVisible(LLCamera& camera_in,
					std::vector<LLDrawable*>* results = NULL,
					bool for_select = false) override;
	void updateDistance(LLCamera& camera_in, bool force_update) override;
	void makeActive() override;
	void shiftPos(const LLVector4a& vec) override;
	void cleanupReferences() override;

	bool updateMove() override;

	// LLSpatialPartition override
	void move(LLDrawable* drawablep, LLSpatialGroup* curp,
			  bool immediate = false) override;

public:
	LLDrawable*				mDrawable;
	LLPointer<LLVOAvatar>	mAvatar;
};

class LLCullResult
{
protected:
	LOG_CLASS(LLCullResult);

public:
	LLCullResult() = default;

	typedef std::vector<LLSpatialGroup*> sg_list_t;
	typedef std::vector<LLDrawable*> drawable_list_t;
	typedef std::vector<LLSpatialBridge*> bridge_list_t;
	typedef std::vector<LLDrawInfo*> drawinfo_list_t;

	// Note: whenever possible, avoid using such an iterator since all the
	// "lists" are now of the std::vector type, which elements are way faster
	// to access via their index. I provided get*() methods to instead work on
	// the vectors themselves. This iterator type (and the associated begin*()
	// and end*() methods are only kept because we need to use std::sort in a
	// couple of places, on the alpha group "lists". HB
	typedef sg_list_t::iterator sg_iterator;

	void clear();

	LL_INLINE sg_list_t& getAlphaGroups()				{ return mAlphaGroups; }
	LL_INLINE sg_iterator beginAlphaGroups()			{ return mAlphaGroups.begin(); }
	LL_INLINE sg_iterator endAlphaGroups()				{ return mAlphaGroups.end(); }

	LL_INLINE sg_list_t& getRiggedAlphaGroups()			{ return mRiggedAlphaGroups; }
	LL_INLINE sg_iterator beginRiggedAlphaGroups()		{ return mRiggedAlphaGroups.begin(); }
	LL_INLINE sg_iterator endRiggedAlphaGroups()		{ return mRiggedAlphaGroups.end(); }

	LL_INLINE sg_list_t& getDrawableGroups()			{ return mDrawableGroups; }

	LL_INLINE sg_list_t& getOcclusionGroups()			{ return mOcclusionGroups; }
	LL_INLINE bool hasOcclusionGroups() const			{ return !mOcclusionGroups.empty(); }

	LL_INLINE sg_list_t& getVisibleGroups()				{ return mVisibleGroups; }
	LL_INLINE drawable_list_t& getVisibleList()			{ return mVisibleList; }
	LL_INLINE bridge_list_t& getVisibleBridge()			{ return mVisibleBridge; }

	LL_INLINE drawinfo_list_t& getRenderMap(U32 type)	{ return mRenderMap[type]; }

	LL_INLINE bool hasRenderMap(U32 type) const
	{
		return type < LLRenderPass::NUM_RENDER_TYPES &&
			   !mRenderMap[type].empty();
	}

	LL_INLINE void pushVisibleGroup(LLSpatialGroup* g)	{ mVisibleGroups.push_back(g); }

	LL_INLINE void pushAlphaGroup(LLSpatialGroup* g)	{ mAlphaGroups.push_back(g); }

	LL_INLINE void pushRiggedAlphaGroup(LLSpatialGroup* g)
	{
		mRiggedAlphaGroups.push_back(g);
	}

	LL_INLINE void pushOcclusionGroup(LLSpatialGroup* g)
	{
		mOcclusionGroups.push_back(g);
	}

	LL_INLINE void pushDrawableGroup(LLSpatialGroup* g)	{ mDrawableGroups.push_back(g); }
	LL_INLINE void pushDrawable(LLDrawable* drawable)	{ mVisibleList.push_back(drawable); }
	LL_INLINE void pushBridge(LLSpatialBridge* bridge)	{ mVisibleBridge.push_back(bridge); }
	void pushDrawInfo(U32 type, LLDrawInfo* draw_info);

	void assertDrawMapsEmpty();

private:
	sg_list_t		mVisibleGroups;
	sg_list_t		mAlphaGroups;
	sg_list_t		mRiggedAlphaGroups;
	sg_list_t		mOcclusionGroups;
	sg_list_t		mDrawableGroups;
	drawable_list_t	mVisibleList;
	bridge_list_t	mVisibleBridge;
	drawinfo_list_t	mRenderMap[LLRenderPass::NUM_RENDER_TYPES];
};

// Spatial partition for water (implemented in llvowater.cpp)
class LLWaterPartition : public LLSpatialPartition
{
public:
	LLWaterPartition(LLViewerRegion* regionp);

	LL_INLINE void getGeometry(LLSpatialGroup*) override
	{
	}

	LL_INLINE void addGeometryCount(LLSpatialGroup*, U32&, U32&) override
	{
	}
};

// Spatial partition for hole and edge water (implemented in llvowater.cpp)
class LLVoidWaterPartition : public LLWaterPartition
{
public:
	LLVoidWaterPartition(LLViewerRegion* regionp);
};

// Spatial partition for terrain (impelmented in llvosurfacepatch.cpp)
class LLTerrainPartition final : public LLSpatialPartition
{
public:
	LLTerrainPartition(LLViewerRegion* regionp);
	void getGeometry(LLSpatialGroup* group) override;
	// Note: not for PBR rendering
	LLVertexBuffer* createVertexBuffer(U32 type_mask) override;
};

// Spatial partition for trees (implemented in llvotree.cpp)
class LLTreePartition final : public LLSpatialPartition
{
public:
	LLTreePartition(LLViewerRegion* regionp);

	LL_INLINE void getGeometry(LLSpatialGroup*) override
	{
	}

	LL_INLINE void addGeometryCount(LLSpatialGroup*, U32&, U32&) override
	{
	}
};

// Spatial partition for particles (implemented in llvopartgroup.cpp)
class LLParticlePartition : public LLSpatialPartition
{
public:
	LLParticlePartition(LLViewerRegion* regionp);
	void rebuildGeom(LLSpatialGroup* group) override;
	void getGeometry(LLSpatialGroup* group) override;
	void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
						  U32& index_count) override;

	LL_INLINE F32 calcPixelArea(LLSpatialGroup*, LLCamera&) override
	{
		return 1024.f;
	}

protected:
	static bool createVB(LLPointer<LLVertexBuffer>& vb, U32 vert_count,
						 U32 idx_count);

protected:
	U32 mRenderPass;
};

class LLHUDParticlePartition final : public LLParticlePartition
{
public:
	LLHUDParticlePartition(LLViewerRegion* regionp);
};

// Spatial partition for grass (implemented in llvograss.cpp)
class LLGrassPartition final : public LLSpatialPartition
{
public:
	LLGrassPartition(LLViewerRegion* regionp);
	void getGeometry(LLSpatialGroup* group) override;
	void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
						  U32& index_count) override;

protected:
	U32 mRenderPass;
};

// Spatial partition for clouds (implemented in llvoclouds.cpp)
class LLCloudPartition final : public LLParticlePartition
{
public:
	LLCloudPartition(LLViewerRegion* regionp);
};

// Class for wrangling geometry out of volumes (implemented in llvovolume.cpp)
class LLVolumeGeometryManager : public LLGeometryManager
{
protected:
	LOG_CLASS(LLVolumeGeometryManager);

public:
	typedef enum
	{
		NONE = 0,
		BATCH_SORT,
		DISTANCE_SORT
	} eSortType;

	LLVolumeGeometryManager();
	~LLVolumeGeometryManager() override;

	void rebuildGeom(LLSpatialGroup* group) override;
	void rebuildMesh(LLSpatialGroup* group) override;

	LL_INLINE void getGeometry(LLSpatialGroup*) override	{}

	LL_INLINE void addGeometryCount(LLSpatialGroup*, U32&, U32&) override
	{
	}

	void genDrawInfo(LLSpatialGroup* group, U32 mask, LLFace** faces,
					 U32 face_count, bool distance_sort = false,
					 bool batch_textures = false, bool rigged = false);
	void registerFace(LLSpatialGroup* group, LLFace* facep, U32 type);

private:
	void allocateFaces(U32 max_face_count);
	void freeFaces();

private:
	static S32		sInstanceCount;
	static LLFace**	sFullbrightFaces[2];
	static LLFace**	sBumpFaces[2];
	static LLFace**	sSimpleFaces[2];
	static LLFace**	sNormFaces[2];
	static LLFace**	sSpecFaces[2];
	static LLFace**	sNormSpecFaces[2];
	static LLFace**	sPbrFaces[2];
	static LLFace** sAlphaFaces[2];
};

// Spatial partition that uses volume geometry manager (implemented in
// llvovolume.cpp)
class LLVolumePartition final : public LLSpatialPartition,
								public LLVolumeGeometryManager
{
public:
	LLVolumePartition(LLViewerRegion* regionp);

	LL_INLINE void rebuildGeom(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::rebuildGeom(group);
	}

	LL_INLINE void getGeometry(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::getGeometry(group);
	}

	LL_INLINE void rebuildMesh(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::rebuildMesh(group);
	}

	LL_INLINE void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
									U32& index_count) override
	{
		LLVolumeGeometryManager::addGeometryCount(group, vertex_count,
												  index_count);
	}
};

// Spatial bridge that uses volume geometry manager (implemented in
// llvovolume.cpp)
class LLVolumeBridge : public LLSpatialBridge, public LLVolumeGeometryManager
{
public:
	LLVolumeBridge(LLDrawable* drawable, LLViewerRegion* regionp);

	LL_INLINE void rebuildGeom(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::rebuildGeom(group);
	}

	LL_INLINE void getGeometry(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::getGeometry(group);
	}

	LL_INLINE void rebuildMesh(LLSpatialGroup* group) override
	{
		LLVolumeGeometryManager::rebuildMesh(group);
	}

	LL_INLINE void addGeometryCount(LLSpatialGroup* group, U32& vertex_count,
									U32& index_count) override
	{
		LLVolumeGeometryManager::addGeometryCount(group, vertex_count,
												  index_count);
	}
};

// Spatial attachment bridge that uses volume geometry manager (implemented in
// llvovolume.cpp)
class LLAvatarBridge final : public LLVolumeBridge
{
public:
	LLAvatarBridge(LLDrawable* drawable, LLViewerRegion* regionp);
};

class LLPuppetBridge final : public LLVolumeBridge
{
public:
	LLPuppetBridge(LLDrawable* drawable, LLViewerRegion* regionp);
};

class LLHUDBridge final : public LLVolumeBridge
{
public:
	LLHUDBridge(LLDrawable* drawablep, LLViewerRegion* regionp);

	// HUD objects do not shift with region crossing. That would be silly.
	LL_INLINE void shiftPos(const LLVector4a&) override		{}

	LL_INLINE F32 calcPixelArea(LLSpatialGroup*, LLCamera&) override
	{
		return 1024.f;
	}
};

// Spatial partition that holds nothing but spatial bridges
class LLBridgePartition : public LLSpatialPartition
{
public:
	LLBridgePartition(LLViewerRegion* regionp);

	LL_INLINE void getGeometry(LLSpatialGroup*) override
	{
	}

	LL_INLINE void addGeometryCount(LLSpatialGroup*, U32&, U32&) override
	{
	}
};

// Spatial partition that holds nothing but spatial bridges
class LLAvatarPartition final : public LLBridgePartition
{
public:
	LLAvatarPartition(LLViewerRegion* regionp);
};

// Spatial partition that holds nothing but spatial bridges
class LLPuppetPartition final : public LLBridgePartition
{
public:
	LLPuppetPartition(LLViewerRegion* regionp);
};

class LLHUDPartition final : public LLBridgePartition
{
public:
	LLHUDPartition(LLViewerRegion* regionp);

	// HUD objects do not shift with region crossing. That would be silly.
	LL_INLINE void shift(const LLVector4a&) override	{}
};

// Also called from LLPipeline
void drawBox(const LLVector4a& c, const LLVector4a& r);
void drawBoxOutline(const LLVector3& pos, const LLVector3& size);

typedef fast_hset<LLSpatialGroup*> spatial_groups_set_t;
extern spatial_groups_set_t gVisibleSelectedGroups;

#endif //LL_LLSPATIALPARTITION_H
