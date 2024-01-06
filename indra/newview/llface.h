/**
 * @file llface.h
 * @brief LLFace class definition
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

#ifndef LL_LLFACE_H
#define LL_LLFACE_H

#include "llmodel.h"
#include "llrender.h"
#include "llstrider.h"
#include "llvertexbuffer.h"
#include "llcolor4u.h"

#include "lldrawable.h"
#include "llviewertexture.h"

// Defined to 1 to reinstate the putative fix for MAINT-4773/SL-5842 i.e.
// "transparent alpha being white" in some materials: this fix got reverted
// in LL's PBR viewer due to "critical flaw of the fix replacing material
// (sometimes server side included) and ignoring user and script input in some
// cases that makes scripts misbehave". However, without this fix, some rigged
// meshes disappear when in forward rendering mode. See this forum post:
// http://sldev.free.fr/forum/viewtopic.php?f=4&p=11950#p11950 - HB
#define LL_FIX_MAT_TRANSPARENCY 1

class LLDrawInfo;
class LLFacePool;
class LLGeometryManager;
class LLTextureEntry;
class LLVertexProgram;
class LLViewerTexture;
class LLVolume;

constexpr F32 MIN_ALPHA_SIZE = 1024.f;
constexpr F32 MIN_TEX_ANIM_SIZE = 512.f;
constexpr U8 FACE_DO_NOT_BATCH_TEXTURES = 255;

class LLFace
{
protected:
	LOG_CLASS(LLFace);

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

	LLFace(const LLFace& rhs)
	{
		*this = rhs;
	}

	const LLFace& operator=(const LLFace& rhs)
	{
		llerrs << "Illegal operation !" << llendl;
		return *this;
	}

	enum EMasks
	{
		LIGHT			= 0x0001,
		GLOBAL			= 0x0002,
		FULLBRIGHT		= 0x0004,
		HUD_RENDER		= 0x0008,
		USE_FACE_COLOR	= 0x0010,
		TEXTURE_ANIM	= 0x0020,
		RIGGED			= 0x0040,
	};

public:
	LL_INLINE LLFace(LLDrawable* drawablep, LLViewerObject* objp)
	{
		init(drawablep, objp);
	}

	LL_INLINE ~LLFace()									{ destroy(); }

	LL_INLINE const LLMatrix4& getWorldMatrix() const	{ return mVObjp->getWorldMatrix(mXform); }
	const LLMatrix4& getRenderMatrix() const;
	LL_INLINE U32 getIndicesCount() const				{ return mIndicesCount; }
	LL_INLINE S32 getIndicesStart() const				{ return mIndicesIndex; }
	// Vertex count for this face
	LL_INLINE U16 getGeomCount() const					{ return mGeomCount; }
	// Index into draw pool
	LL_INLINE U16 getGeomIndex() const					{ return mGeomIndex; }
	// Index into draw pool
	LL_INLINE U16 getGeomStart() const					{ return mGeomIndex; }
	void setTextureIndex(U8 index);
	LL_INLINE U8 getTextureIndex() const				{ return mTextureIndex; }

	void setTexture(U32 ch, LLViewerTexture* texp);

	LL_INLINE void setDiffuseMap(LLViewerTexture* texp)
	{
		setTexture(LLRender::DIFFUSE_MAP, texp);
	}

	LL_INLINE void setNormalMap(LLViewerTexture* texp)
	{
		setTexture(LLRender::NORMAL_MAP, texp);
	}

	LL_INLINE void setSpecularMap(LLViewerTexture* texp)
	{
		setTexture(LLRender::SPECULAR_MAP, texp);
	}

	// Used to switch between diffuse and media textures.
	void switchTexture(U32 ch, LLViewerTexture* texp);
	// Used to switch between diffuse and base color textures. HB
	void switchDiffuseTex(const LLUUID& tex_id);

	void dirtyTexture();

#if LL_FIX_MAT_TRANSPARENCY
	void notifyAboutCreatingTexture(LLViewerTexture* texp);
	void notifyAboutMissingAsset(LLViewerTexture* texp);
#endif

	// Used to preserve draw order of faces that are batched together. Allows
	// content creators to manipulate linked sets and face ordering for
	// consistent alpha sorting results, particularly for rigged attachments.
	LL_INLINE void setDrawOrderIndex(U32 index)			{ mDrawOrderIndex = index; }
	LL_INLINE U32 getDrawOrderIndex() const				{ return mDrawOrderIndex; }

	LL_INLINE LLXformMatrix* getXform() const			{ return mXform; }
	LL_INLINE bool hasGeometry() const					{ return mGeomCount > 0; }
	LLVector3 getPositionAgent() const;

	LLVector2 surfaceToTexture(LLVector2 surface_coord, const LLVector4a& pos,
							   const LLVector4a& normal);

	void getPlanarProjectedParams(LLQuaternion* face_rot, LLVector3* face_pos,
								  F32* scale) const;

	bool calcAlignedPlanarTE(const LLFace* align_to, LLVector2* st_offset,
							 LLVector2* st_scale, F32* st_rot,
							 S32 map = LLRender::DIFFUSE_MAP) const;

	LL_INLINE U32 getState() const						{ return mState; }
	LL_INLINE void setState(U32 state)					{ mState |= state; }
	LL_INLINE void clearState(U32 state)				{ mState &= ~state; }
	LL_INLINE bool isState(U32 state)	const			{ return (mState & state) != 0; }
	LL_INLINE void setVirtualSize(F32 size)				{ mVSize = size; }
	LL_INLINE void setPixelArea(F32 area)				{ mPixelArea = area; }
	LL_INLINE F32 getVirtualSize() const				{ return mVSize; }
	LL_INLINE F32 getPixelArea() const					{ return mPixelArea; }

	LL_INLINE S32 getIndexInTex(U32 ch) const
	{
		return ch < LLRender::NUM_TEXTURE_CHANNELS ? mIndexInTex[ch] : 0;
	}

	LL_INLINE void setIndexInTex(U32 ch, S32 idx)
	{
		if (ch < LLRender::NUM_TEXTURE_CHANNELS)
		{
			mIndexInTex[ch] = idx;
		}
	}

	void renderIndexed(U32 mask = 0);

	LL_INLINE const LLTextureEntry* getTextureEntry() const
	{
		return mTEOffset >= 0 && mVObjp.notNull() ? mVObjp->getTE(mTEOffset)
												  : NULL;
	}

	// Returns true when the face texture can be safely included in a render
	// batch. This used to be a static (local) can_batch_texture() helper
	// function in llvovolume.cpp. HB
	bool canBatchTexture() const;

	LL_INLINE LLFacePool* getPool() const				{ return mDrawPoolp; }
	LL_INLINE void setPoolType(U32 type)				{ mPoolType = type; }
	LL_INLINE U32 getPoolType() const					{ return mPoolType; }
	// Returns true if this face is in an alpha draw pool.
	bool isInAlphaPool() const;

	LL_INLINE LLPointer<LLDrawable> getDrawable() const	{ return mDrawablep; }
	LL_INLINE LLViewerObject* getViewerObject() const	{ return mVObjp; }

	LL_INLINE S32 getLOD() const
	{
		return mVObjp.notNull() ? mVObjp->getLOD() : 0;
	}

	LL_INLINE S32 getTEOffset() const					{ return mTEOffset; }

	LLViewerTexture* getTexture(U32 ch = LLRender::DIFFUSE_MAP) const;

	LL_INLINE void setViewerObject(LLViewerObject* obj)	{ mVObjp = obj; }
	void setPool(LLFacePool* poolp, LLViewerTexture* texp);
	LL_INLINE void setPool(LLFacePool* poolp)			{ mDrawPoolp = poolp; }

	void setDrawable(LLDrawable* drawablep);
	LL_INLINE void setTEOffset(S32 te_offset)			{ mTEOffset = te_offset; }

	// Override material color
	LL_INLINE void setFaceColor(const LLColor4& color)
	{
		mFaceColor = color;
		setState(USE_FACE_COLOR);
	}

	// Switch back to material color
	LL_INLINE void unsetFaceColor()						{ clearState(USE_FACE_COLOR); }

	LL_INLINE const LLColor4& getFaceColor() const		{ return mFaceColor; }

	const LLColor4& getRenderColor() const;

	// For volumes
	void updateRebuildFlags();
	bool canRenderAsMask();	// Logic helper
	bool getGeometryVolume(const LLVolume& volume, S32 f,
						   const LLMatrix4& mat_vert,
						   const LLMatrix3& mat_normal,
						   const U16& index_offset,
						   bool force_rebuild = false);

	// For avatar
	U16 getGeometryAvatar(LLStrider<LLVector3>& vertices,
						  LLStrider<LLVector3>& normals,
						  LLStrider<LLVector2>& texCoords,
						  LLStrider<F32>& vertex_weights,
						  LLStrider<LLVector4a>& clothing_weights);

	// For volumes, etc.
	U16 getGeometry(LLStrider<LLVector3>& vertices,
					LLStrider<LLVector3>& normals,
					LLStrider<LLVector2>& texCoords,
					LLStrider<U16>& indices);

	S32 getColors(LLStrider<LLColor4U>& colors);
	S32 getIndices(LLStrider<U16>& indices);

	void setSize(U32 num_vertices, U32 num_indices = 0, bool align = false);

	bool genVolumeBBoxes(const LLVolume& volume, S32 f, const LLMatrix4& mat,
						 bool global_volume = false);

	void init(LLDrawable* drawablep, LLViewerObject* objp);
	void destroy();
	void update();

	// Updates center when xform has changed.
	void updateCenterAgent();

	void renderSelected(LLViewerTexture* image, const LLColor4& color);

	LL_INLINE F32 getKey() const						{ return mDistance; }

	LL_INLINE S32 getReferenceIndex() const				{ return mReferenceIndex; }
	LL_INLINE void setReferenceIndex(S32 index)			{ mReferenceIndex = index; }

	bool verify(const U32* indices_array = NULL) const;
	void printDebugInfo() const;

	void setGeomIndex(U16 idx);
	void setIndicesIndex(U32 idx);

	LL_INLINE void setDrawInfo(LLDrawInfo* infop)		{ mDrawInfo = infop; }

    // Return mSkinInfo->mHash or 0 if mSkinInfo is null
    U64 getSkinHash() const;

	F32	 getTextureVirtualSize();

	LL_INLINE F32 getImportanceToCamera() const			{ return mImportanceToCamera; }
	void resetVirtualSize();

	LL_INLINE void setHasMedia(bool has_media)			{ mHasMedia = has_media; }
	bool hasMedia() const;

	LL_INLINE void setMediaAllowed(bool allowed)		{ mIsMediaAllowed = allowed; }
	LL_INLINE bool isMediaAllowed() const				{ return mIsMediaAllowed; }

	// Vertex buffer tracking
	void setVertexBuffer(LLVertexBuffer* buffer);
	// Sets mVertexBuffer to NULL
	void clearVertexBuffer();
	LL_INLINE LLVertexBuffer* getVertexBuffer() const	{ return mVertexBuffer; }

	S32 getRiggedIndex(U32 type) const;

	static F32 calcImportanceToCamera(F32 to_view_dir, F32 dist);
	static F32 adjustPixelArea(F32 importance, F32 pixel_area);

	struct CompareDistanceGreater
	{
		LL_INLINE bool operator()(const LLFace* const& lhs,
								  const LLFace* const& rhs)
		{
			// Farthest = first
			return !lhs || (rhs && lhs->mDistance > rhs->mDistance);
		}
	};

	struct CompareTexture
	{
		LL_INLINE bool operator()(const LLFace* const& lhs,
								  const LLFace* const& rhs)
		{
			return lhs->getTexture() < rhs->getTexture();
		}
	};

	struct CompareBatchBreaker
	{
		LL_INLINE bool operator()(const LLFace* const& lhs,
								  const LLFace* const& rhs)
		{
			const LLTextureEntry* lte = lhs->getTextureEntry();
			const LLTextureEntry* rte = rhs->getTextureEntry();

			if (lhs->getTexture() != rhs->getTexture())
			{
				return lhs->getTexture() < rhs->getTexture();
			}
			return lte->getBumpShinyFullbright() <
					rte->getBumpShinyFullbright();
		}
	};

	struct CompareTextureAndGeomCount
	{
		LL_INLINE bool operator()(const LLFace* const& lhs,
								  const LLFace* const& rhs)
		{
			// Smallest = first
			if (lhs->getTexture() == rhs->getTexture())
			{
				return lhs->getGeomCount() < rhs->getGeomCount();
			}
			return lhs->getTexture() > rhs->getTexture();
		}
	};

	struct CompareTextureAndLOD
	{
		LL_INLINE bool operator()(const LLFace* const& lhs,
								  const LLFace* const& rhs)
		{
			if (lhs->getTexture() == rhs->getTexture())
			{
				return lhs->getLOD() < rhs->getLOD();
			}
			return lhs->getTexture() < rhs->getTexture();
		}
	};

	struct CompareTextureAndTime
	{
		LL_INLINE bool operator()(const LLFace* const& lhs,
								  const LLFace* const& rhs)
		{
			if (lhs->getTexture() == rhs->getTexture())
			{
				return lhs->mLastUpdateTime < rhs->mLastUpdateTime;
			}
			return lhs->getTexture() < rhs->getTexture();
		}
	};

private:
	F32 adjustPartialOverlapPixelArea(F32 cos_angle_to_view_dir, F32 radius);
	bool calcPixelArea(F32& cos_angle_to_view_dir, F32& radius);

public:
	// Aligned member
	alignas(16) LLVector4a		mExtents[2];

	LLVector3					mCenterLocal;
	LLVector3					mCenterAgent;

	LLVector2					mTexExtents[2];
	F32							mDistance;
	F32							mLastUpdateTime;
	F32							mLastSkinTime;
	F32							mLastMoveTime;
	LLMatrix4*					mTextureMatrix;
	LLDrawInfo*					mDrawInfo;
	LLVOAvatar*					mAvatar;
	LLPointer<LLMeshSkinInfo>	mSkinInfo;

private:
	LLPointer<LLVertexBuffer>	mVertexBuffer;
	LLPointer<LLDrawable>		mDrawablep;
	LLPointer<LLViewerObject>	mVObjp;

	LLPointer<LLViewerTexture>	mTexture[LLRender::NUM_TEXTURE_CHANNELS];

	std::vector<S32> mRiggedIndex;

	// Overrides material color if state |= USE_FACE_COLOR
	LLColor4					mFaceColor;

	LLXformMatrix*				mXform;

	LLFacePool*					mDrawPoolp;
	U32							mPoolType;
	U32							mState;

	U32							mDrawOrderIndex;

	F32							mVSize;
	F32							mPixelArea;

	// Importance factor, in the range [0, 1.0]. 1.0: the most important.
	// Based on the distance from the face to the view point and the angle from
	// the face center to the view direction.
	F32							mImportanceToCamera;

	F32							mBoundingSphereRadius;

	S32							mTEOffset;

	S32							mReferenceIndex;

	// Index into draw pool for indices (yeah, I know !)
	U32							mIndicesIndex;
	U32							mIndicesCount;

	// Vertex count for this face
	U16							mGeomCount;
	// Index into draw pool
	U16							mGeomIndex;

	S32							mIndexInTex[LLRender::NUM_TEXTURE_CHANNELS];

	// Index of texture channel to use for pseudo-atlasing
	U8							mTextureIndex;

	bool						mHasMedia;
	bool						mIsMediaAllowed;
};

#endif // LL_LLFACE_H
