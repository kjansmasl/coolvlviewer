/**
 * @file lldrawable.h
 * @brief LLDrawable class definition
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

#ifndef LL_DRAWABLE_H
#define LL_DRAWABLE_H

#include <map>
#include <vector>

#include "hbfastset.h"

#include "llviewerobject.h"
#include "llvieweroctree.h"

class LLCamera;
class LLDrawPool;
class LLDrawable;
class LLFace;
class LLFacePool;
class LLSpatialGroup;
class LLSpatialBridge;
class LLSpatialPartition;
class LLViewerTexture;
class LLVOVolume;

// Can have multiple silhouettes for each object
constexpr U32 SILHOUETTE_HIGHLIGHT = 0;

// All data for new renderer goes into this class.
class alignas(16) LLDrawable : public LLViewerOctreeEntryData
{
	friend class LLDrawPool;
	friend class LLPipeline;
	friend class LLSpatialBridge;

protected:
	LOG_CLASS(LLDrawable);

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

	LLDrawable(const LLDrawable& rhs)
	:	LLViewerOctreeEntryData(rhs)
	{
		*this = rhs;
	}

	const LLDrawable& operator=(const LLDrawable& rhs)
	{
		llerrs << "Illegal operation !" << llendl;
		return *this;
	}

	LLDrawable(LLViewerObject* vobj, bool new_entry = false);

	void markDead();		// Mark this drawable as dead
	LL_INLINE bool isDead() const						{ return isState(DEAD); }
	LL_INLINE bool isNew() const						{ return !isState(BUILT); }

	bool isLight() const;

	bool isVisible() const override;
	bool isRecentlyVisible() const override;
	virtual void setVisible(LLCamera& camera_in,
							std::vector<LLDrawable*>* results = NULL,
							bool for_select = false);

	LL_INLINE LLSpatialGroup* getSpatialGroup() const	{ return (LLSpatialGroup*)getGroup(); }
	LL_INLINE LLViewerRegion* getRegion() const			{ return mVObjp.notNull() ? mVObjp->getRegion() : NULL; }

	LL_INLINE const LLTextureEntry* getTextureEntry(U8 which) const
	{
		return mVObjp.notNull() ? mVObjp->getTE(which) : NULL;
	}

	LL_INLINE LLPointer<LLViewerObject>& getVObj()		{ return mVObjp; }
	LL_INLINE const LLViewerObject* getVObj() const		{ return mVObjp; }
	LLVOVolume*	getVOVolume() const; // cast mVObjp tp LLVOVolume if OK

	LL_INLINE const LLMatrix4& getWorldMatrix() const	{ return mXform.getWorldMatrix(); }
	const LLMatrix4& getRenderMatrix() const;
	LL_INLINE void setPosition(LLVector3 v) const		{}
	LL_INLINE const LLVector3& getPosition() const		{ return mXform.getPosition(); }
	LL_INLINE const LLVector3& getWorldPosition() const	{ return mXform.getPositionW(); }
	const LLVector3 getPositionAgent() const;
	LL_INLINE const LLVector3& getScale() const			{ return mCurrentScale; }
	LL_INLINE void setScale(const LLVector3& scale)		{ mCurrentScale = scale; }

	LL_INLINE const LLQuaternion& getWorldRotation() const
	{
		return mXform.getWorldRotation();
	}

	LL_INLINE const LLQuaternion& getRotation() const	{ return mXform.getRotation(); }
	LL_INLINE F32 getIntensity() const					{ return llmin(mXform.getScale().mV[0], 4.f); }
	LL_INLINE S32 getLOD() const						{ return mVObjp.notNull() ? mVObjp->getLOD() : 1; }

	LL_INLINE void getMinMax(LLVector3& min,LLVector3& max) const
	{
		mXform.getMinMax(min, max);
	}

	LL_INLINE LLXformMatrix* getXform()					{ return &mXform; }

	LL_INLINE U32 getState() const						{ return mState; }
	LL_INLINE bool isState(U32 bits) const				{ return (mState & bits) != 0; }
	LL_INLINE void setState(U32 bits)					{ mState |= bits; }
	LL_INLINE void clearState(U32 bits)					{ mState &= ~bits; }

	LLDrawable* getRoot();
	LL_INLINE bool isRoot() const						{ return !mParent || mParent->isAvatar(); }
	LL_INLINE bool isAvatar() const						{ return mVObjp.notNull() && mVObjp->isAvatar(); }
	LL_INLINE bool isSpatialRoot() const				{ return !mParent || mParent->isAvatar(); }
	LL_INLINE virtual bool isSpatialBridge() const		{ return false; }
	LL_INLINE virtual LLSpatialPartition* asPartition()	{ return NULL; }

	// Note: parent must be set only via LLViewerObject::setParent()
	LL_INLINE LLDrawable* getParent() const				{ return mParent; }

	LLFace* getFace(S32 i) const;
	LL_INLINE S32 getNumFaces() const					{ return (S32)mFaces.size(); }
	typedef std::vector<LLFace*> face_list_t;
	LL_INLINE face_list_t& getFaces()					{ return mFaces; }
	LL_INLINE const face_list_t& getFaces() const		{ return mFaces; }

	LLFace* addFace(LLFacePool* poolp, LLViewerTexture* texturep);
	LLFace* addFace(const LLTextureEntry* te, LLViewerTexture* texturep);
	LLFace* addFace(const LLTextureEntry* te, LLViewerTexture* texturep,
					LLViewerTexture* normalp);
	LLFace* addFace(const LLTextureEntry* te, LLViewerTexture* texturep,
					LLViewerTexture* normalp, LLViewerTexture* specularp);
	void deleteFaces(S32 offset, S32 count);
	void setNumFaces(S32 num_faces, LLFacePool* poolp, LLViewerTexture* texp);
	void setNumFacesFast(S32 num, LLFacePool* poolp, LLViewerTexture* texp);
	void mergeFaces(LLDrawable* src);

	void init(bool new_entry);

	void update();
	F32 updateXform(bool undamped);

	virtual void makeActive();
	void makeStatic(bool warning_enabled = true);

	LL_INLINE bool isActive() const						{ return isState(ACTIVE); }
	LL_INLINE bool isStatic() const						{ return !isActive(); }
	bool isAnimating() const;

	virtual bool updateMove();
	virtual void movePartition();

	void updateTexture();
	LL_INLINE void updateMaterial()						{}
	virtual void updateDistance(LLCamera& camera, bool force_update);
	bool updateGeometry();

	void updateFaceSize(S32 idx);

	virtual void shiftPos(const LLVector4a& shift_vector);

	LL_INLINE S32 getGeneration() const					{ return mGeneration; }

	LL_INLINE bool getLit() const						{ return !isState(UNLIT); }
	LL_INLINE void setLit(bool lit)						{ lit ? clearState(UNLIT) : setState(UNLIT); }

	virtual void cleanupReferences();

	void setGroup(LLViewerOctreeGroup* group) override;
	LL_INLINE void setRadius(F32 radius)				{ mRadius = radius; }
	LL_INLINE F32 getRadius() const						{ return mRadius; }
	F32 getVisibilityRadius() const;

	// Updates the cache of sun space bounding box.
	LL_INLINE void updateUVMinMax()						{}

	const LLVector3& getBounds(LLVector3& min, LLVector3& max) const;
	virtual void updateSpatialExtents();
	virtual void updateBinRadius();

	LL_INLINE void setRenderType(S32 type) 				{ mRenderType = type; }
	LL_INLINE bool isRenderType(S32 type) 				{ return mRenderType == type; }
	LL_INLINE S32 getRenderType()						{ return mRenderType; }

	LLSpatialPartition* getSpatialPartition();

	void removeFromOctree();

	LL_INLINE void setSpatialBridge(LLSpatialBridge* brg)
	{
		mSpatialBridge = (LLDrawable*)brg;
	}

	LL_INLINE LLSpatialBridge* getSpatialBridge()
	{
		return (LLSpatialBridge*)((LLDrawable*)mSpatialBridge);
	}

#if LL_DEBUG && 0
	// Debugging methods
	S32 findReferences(LLDrawable* drawablep);
	static void cleanupDeadDrawables();
#endif

	// Statics
	static void incrementVisible();

protected:
	~LLDrawable() override;

	void moveUpdatePipeline(bool moved);
	void updatePartition();
	bool updateMoveDamped();
	bool updateMoveUndamped();

public:
	typedef fast_hset<LLPointer<LLDrawable> > draw_set_t;
	typedef std::vector<LLPointer<LLDrawable> > draw_vec_t;
	typedef std::list<LLPointer<LLDrawable> > draw_list_t;

	struct CompareDistanceGreater
	{
		LL_INLINE bool operator()(const LLDrawable* const& lhs,
								  const LLDrawable* const& rhs)
		{
			// Farthest = last
			return lhs->mDistanceWRTCamera < rhs->mDistanceWRTCamera;
		}
	};

	struct CompareDistanceGreaterVisibleFirst
	{
		LL_INLINE bool operator()(const LLDrawable* const& lhs,
								  const LLDrawable* const& rhs)
		{
			if (lhs->isVisible() && !rhs->isVisible())
			{
				return true; // visible things come first
			}
			if (!lhs->isVisible() && rhs->isVisible())
			{
				return false; // rhs is visible, comes first
			}
			// Farthest = last
			return lhs->mDistanceWRTCamera < rhs->mDistanceWRTCamera;
		}
	};

	typedef enum e_drawable_flags
	{
 		IN_REBUILD_QUEUE= 0x00000001,
		EARLY_MOVE		= 0x00000004,
		MOVE_UNDAMPED	= 0x00000008,
		ON_MOVE_LIST	= 0x00000010,
		UV				= 0x00000020,
		UNLIT			= 0x00000040,
		LIGHT			= 0x00000080,
		REBUILD_VOLUME  = 0x00000100,	// volume changed LOD or parameters, or vertex buffer changed
		REBUILD_TCOORD	= 0x00000200,	// texture coordinates changed
		REBUILD_COLOR	= 0x00000400,	// color changed
		REBUILD_POSITION= 0x00000800,	// vertex positions/normals changed
		REBUILD_GEOMETRY= REBUILD_POSITION | REBUILD_TCOORD | REBUILD_COLOR,
		REBUILD_MATERIAL= REBUILD_TCOORD | REBUILD_COLOR,
		REBUILD_ALL		= REBUILD_GEOMETRY | REBUILD_VOLUME,
		REBUILD_RIGGED	= 0x00001000,
		ON_SHIFT_LIST	= 0x00002000,
		ACTIVE			= 0x00004000,
		DEAD			= 0x00008000,
		INVISIBLE		= 0x00010000, // stay invisible until flag is cleared
 		NEARBY_LIGHT	= 0x00020000, // In gPipeline.mNearbyLightSet
		BUILT			= 0x00040000,
		FORCE_INVISIBLE = 0x00080000,
		HAS_ALPHA		= 0x00100000,
		RIGGED			= 0x00200000, // has a rigged face
		RIGGED_CHILD	= 0x00400000, // has a child with a rigged face
		PARTITION_MOVE	= 0x00800000,
		ANIMATED_CHILD  = 0x01000000,
		ACTIVE_CHILD	= 0x02000000,
	} EDrawableFlags;

public:
	LLXformMatrix				mXform;
	LLPointer<LLDrawable>		mParent;
	F32							mDistanceWRTCamera;

	static F32					sCurPixelAngle;	// current pixels per radian

private:
	U32							mState;
	S32							mRenderType;
	S32							mGeneration;
	F32							mRadius;
	LLVector3					mCurrentScale;
	LLPointer<LLViewerObject>	mVObjp;
	LLPointer<LLDrawable>		mSpatialBridge;

	face_list_t					mFaces;

	static U32					sNumZombieDrawables;
#if LL_DEBUG
	static std::vector<LLPointer<LLDrawable> > sDeadList;
#endif
};

#endif
