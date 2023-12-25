/**
 * @file llvertexbuffer.h
 * @brief LLVertexBuffer wrapper for OpengGL vertex buffer objects
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

#ifndef LL_LLVERTEXBUFFER_H
#define LL_LLVERTEXBUFFER_H

#include <list>
#include <vector>

// Set to 1 to debug VB allocations and freeing on reset vertex buffer.
#define LL_DEBUG_VB_ALLOC 0

#if LL_DEBUG
# undef LL_DEBUG_VB_ALLOC
# define LL_DEBUG_VB_ALLOC 1
#endif
#if LL_DEBUG_VB_ALLOC
# include "hbfastset.h"
#endif

#include "llcolor4u.h"
#include "llgl.h"
#include "llrefcount.h"
#include "llrender.h"
#include "llstrider.h"

#define LL_MAX_VERTEX_ATTRIB_LOCATION 64

// Note: *not* thread-safe. HB
class LLVertexBuffer final : public LLRefCount
{
	friend class LLRender;

protected:
	LOG_CLASS(LLVertexBuffer);

public:
	LLVertexBuffer(U32 typemask);

	// Make this class no-copy.
	LLVertexBuffer(const LLVertexBuffer&) = delete;
	const LLVertexBuffer& operator=(const LLVertexBuffer&) = delete;

	static void initClass();
	static void cleanupClass();
	static void setupClientArrays(U32 data_mask);
	static void drawArrays(U32 mode, const std::vector<LLVector3>& pos);
	static void drawArrays(U32 mode, const std::vector<LLVector3>& pos,
						   const std::vector<LLVector3>& norm);
	// Draws triangles: used only in llselectmgr.cpp and llspatialpartition.cpp
	static void drawElements(U32 num_vertices, const LLVector4a* posp,
							 const LLVector2* tcp, U32 num_indices,
							 const U16* indicesp);

	static void unbind(); // Unbind any bound vertex buffer

	// Gets the size of a vertex with the given typemask
	static U32 calcVertexSize(U32 typemask);

	// Gets the size of a buffer with the given typemask and vertex count,
	// fills offsets with the offset of each vertex component array into the
	// buffer indexed by the following enum.
	static U32 calcOffsets(U32 typemask, U32* offsets, U32 num_vertices);

	// WARNING: when updating these enums you MUST
	// 1 - update LLVertexBuffer::sTypeSize
	// 2 - add a strider accessor
	// 3 - modify LLVertexBuffer::setupVertexBuffer
	// 4 - modify LLVertexBuffer::setupClientArray
	// 5 - modify LLViewerShaderMgr::sReservedAttribs
	// 6 - update LLVertexBuffer::setupVertexArray
	enum
	{
		TYPE_VERTEX = 0,
		TYPE_NORMAL,
		TYPE_TEXCOORD0,
		TYPE_TEXCOORD1,
		TYPE_TEXCOORD2,
		TYPE_TEXCOORD3,
		TYPE_COLOR,
		TYPE_EMISSIVE,
		TYPE_TANGENT,
		TYPE_WEIGHT,
		TYPE_WEIGHT4,
		TYPE_CLOTHWEIGHT,
		TYPE_TEXTURE_INDEX,
		// TYPE_MAX is the size/boundary marker for attributes that go in the
		// vertex buffer
		TYPE_MAX,
		// TYPE_INDEX is beyond _MAX because it lives in a separate (index)
		// buffer
		TYPE_INDEX,
	};

	enum
	{
		MAP_VERTEX			= 1 << TYPE_VERTEX,
		MAP_NORMAL			= 1 << TYPE_NORMAL,
		MAP_TEXCOORD0		= 1 << TYPE_TEXCOORD0,
		MAP_TEXCOORD1		= 1 << TYPE_TEXCOORD1,
		MAP_TEXCOORD2		= 1 << TYPE_TEXCOORD2,
		MAP_TEXCOORD3		= 1 << TYPE_TEXCOORD3,
		MAP_COLOR			= 1 << TYPE_COLOR,
		MAP_EMISSIVE		= 1 << TYPE_EMISSIVE,
		// These use VertexAttribPointer and should possibly be made generic
		MAP_TANGENT			= 1 << TYPE_TANGENT,
		MAP_WEIGHT			= 1 << TYPE_WEIGHT,
		MAP_WEIGHT4			= 1 << TYPE_WEIGHT4,
		MAP_CLOTHWEIGHT		= 1 << TYPE_CLOTHWEIGHT,
		MAP_TEXTURE_INDEX	= 1 << TYPE_TEXTURE_INDEX,
	};

	// Maps for data access
	U8* mapVertexBuffer(S32 type, U32 index, S32 count);
	U8* mapIndexBuffer(U32 index, S32 count);
	void unmapBuffer();

	// Sets for rendering; calls setupVertexBuffer() if data_mask is not 0.
	// For the legacy EE renderer only.
	void setBuffer(U32 data_mask);
	// Assumes data_mask is not 0 among other assumptions.
	// For the legacy EE renderer only.
	void setBufferFast(U32 data_mask);

	// For the new PBR renderer only.
	void setBuffer();

	// For external (non-rendering) use only, such as by GLOD, in the model
	// upload preview floater: for this kind of use, we do need shader-less
	// vertex buffers and to set them up like we do with the EE renderer, even
	// while the PBR renderer is active. This method must be used for such
	// cases, and only for them. HB
	void setBufferNoShader(U32 data_mask);

	bool allocateBuffer(U32 nverts, U32 nindices);

	// Only call each getVertexPointer, etc, once before calling unmapBuffer().
	// Call unmapBuffer() after calls to getXXXStrider() and before any calls
	// to setBuffer(). Example:
	//   vb->getVertexBuffer(verts);
	//   vb->getNormalStrider(norms);
	//   setVertsNorms(verts, norms);
	//   vb->unmapBuffer();
	bool getVertexStrider(LLStrider<LLVector3>& strider, U32 index = 0,
						  S32 count = -1);
	bool getVertexStrider(LLStrider<LLVector4a>& strider, U32 index = 0,
						  S32 count = -1);
	bool getIndexStrider(LLStrider<U16>& strider, U32 index = 0,
						 S32 count = -1);
	bool getTexCoord0Strider(LLStrider<LLVector2>& strider, U32 index = 0,
							 S32 count = -1);
	bool getTexCoord1Strider(LLStrider<LLVector2>& strider, U32 index = 0,
							 S32 count = -1);
	bool getTexCoord2Strider(LLStrider<LLVector2>& strider, U32 index = 0,
							 S32 count = -1);
	bool getNormalStrider(LLStrider<LLVector3>& strider, U32 index = 0,
						  S32 count = -1);
	bool getNormalStrider(LLStrider<LLVector4a>& strider, U32 index = 0,
						  S32 count = -1);
	bool getTangentStrider(LLStrider<LLVector3>& strider, U32 index = 0,
						   S32 count = -1);
	bool getTangentStrider(LLStrider<LLVector4a>& strider, U32 index = 0,
						   S32 count = -1);
	bool getColorStrider(LLStrider<LLColor4U>& strider, U32 index = 0,
						 S32 count = -1);
	bool getEmissiveStrider(LLStrider<LLColor4U>& strider, U32 index = 0,
							S32 count = -1);
	bool getWeightStrider(LLStrider<F32>& strider, U32 index = 0,
						  S32 count = -1);
	bool getWeight4Strider(LLStrider<LLVector4a>& strider, U32 index = 0,
						   S32 count = -1);
	bool getClothWeightStrider(LLStrider<LLVector4a>& strider, U32 index = 0,
							   S32 count = -1);

	// A buffer is "locked" while it is mapped.
	LL_INLINE bool isLocked() const
	{
		return !mMappedVertexRegions.empty() ||
			   !mMappedIndexRegions.empty();
	}

	LL_INLINE U32 getNumVerts() const				{ return mNumVerts; }
	LL_INLINE U32 getNumIndices() const				{ return mNumIndices; }

	void setPositionData(const LLVector4a* data);
	void setTexCoordData(const LLVector2* data);
	void setColorData(const LLColor4U* data);

	LL_INLINE U32 getTypeMask() const				{ return mTypeMask; }
	LL_INLINE bool hasDataType(S32 type) const		{ return (1 << type) & getTypeMask(); }
	// This method allows to specify a mask that is negated and ANDed to
	// data_mask in setupVertexBuffer(). It allows to avoid using a derived
	// class and virtual method for the latter. HB
	LL_INLINE void setTypeMaskMask(U32 mask)		{ mTypeMaskMask = mask; }

	LL_INLINE U32 getSize() const					{ return mSize; }
	LL_INLINE U32 getIndicesSize() const			{ return mIndicesSize; }
	LL_INLINE U8* getMappedData() const				{ return mMappedData; }
	LL_INLINE U8* getMappedIndices() const			{ return mMappedIndexData; }
	LL_INLINE U32 getOffset(S32 type) const			{ return mOffsets[type]; }

	void resetVertexData();
	void resetIndexData();

	void draw(U32 mode, U32 count, U32 indices_offset) const;
	void drawArrays(U32 mode, U32 offset, U32 count) const;
	void drawRange(U32 mode, U32 start, U32 end, U32 count,
				   U32 indices_offset) const;
	// Implementation for inner loops: does not do any safety check and always
	// renders in LLRender::TRIANGLES mode.
	void drawRangeFast(U32 start, U32 end, U32 count, U32 idx_offset) const;

	// For debugging, checks range validity.
	bool validateRange(U32 start, U32 end, U32 count, U32 offset) const;

#if LL_DEBUG_VB_ALLOC
	void setOwner(const char* owner)				{ mOwner = owner; }
	static void dumpInstances();
#endif

	static std::string listMissingBits(U32 unsatisfied_mask);

	// Statistics accessors used in some newview/*.cpp modules.
	LL_INLINE static S32 getGLCount()				{ return sGLCount; }
	LL_INLINE static U32 getBindCount()				{ return sBindCount; }
	LL_INLINE static U32 getSetCount()				{ return sSetCount; }
	// Used in llviewerwindow.cpp
	LL_INLINE static void resetPerFrameStats()		{ sBindCount = sSetCount = 0; }

	static void cleanupVBOPool();

	static S32 getVRAMMegabytes();

protected:
	~LLVertexBuffer() override; // use unref()

	void setupVertexBuffer(U32 data_mask);
	void setupVertexArray();

	void genBuffer(U32 size);
	void genIndices(U32 size);
	bool bindGLBuffer(bool force_bind = false);
	bool bindGLBufferFast();
	bool bindGLIndices(bool force_bind = false);
	bool bindGLIndicesFast();
	bool createGLBuffer(U32 size);
	bool createGLIndices(U32 size);
	void destroyGLBuffer();
	void destroyGLIndices();
	bool updateNumVerts(U32 nverts);
	bool updateNumIndices(U32 nindices);

	void placeFence() const;
	void waitFence() const;

public:
	struct MappedRegion
	{
		LL_INLINE MappedRegion(U32 start, U32 end)
		:	mStart(start),
			mEnd(end)
		{
		}

		U32 mStart;
		U32 mEnd;
	};

	static const U32		sTypeSize[TYPE_MAX];
	static const U32		sGLMode[LLRender::NUM_MODES];
	static U32				sGLRenderBuffer;
	static U32				sGLRenderIndices;
	static U32				sLastMask;
	static U32				sVertexCount;
	static U32				sIndexCount;
	static U32				sBindCount;
	static U32				sSetCount;
	static S32				sGLCount;
	static bool				sVBOActive;
	static bool				sIBOActive;

protected:
	// Note: the first member variable is 32 bits in order to align on 64 bits
	// for the next variables, counting the 32 bits counter from LLRefCount. HB
	U32						mTypeMask;

	typedef std::vector<MappedRegion> region_map_t;
	region_map_t			mMappedVertexRegions;
	region_map_t			mMappedIndexRegions;

#if LL_DEBUG_VB_ALLOC
	std::string				mOwner;
#endif

	U32						mNumVerts;		// Number of vertices allocated
	U32						mNumIndices;	// Number of indices allocated

	U32						mSize;
	U32						mIndicesSize;
	// This is negated and ANDed to data_mask in setupVertexBuffer(). It allows
	// to avoid using a derived class and virtual method for the latter. HB
	U32						mTypeMaskMask;

	U32						mGLBuffer;		// GL VBO handle
	U32						mGLIndices;		// GL IBO handle

	U32						mOffsets[TYPE_MAX];

	// Pointer to currently mapped data (NULL if unmapped)
	U8*						mMappedData;
	// Pointer to currently mapped indices (NULL if unmapped)
	U8*						mMappedIndexData;

	// true when setPositionData() has been used (see LLRender::flush()). HB
	bool					mCachedBuffer;

private:
	static LLPointer<LLVertexBuffer> sUtilityBuffer;
#if LL_DEBUG_VB_ALLOC
	typedef fast_hset<LLVertexBuffer*> instances_set_t;
	static instances_set_t	sInstances;
#endif
};

// Also used by LLPipeline
U32 nhpo2(U32 v);
// Also used by LLImageGL
U32 wpo2(U32 i);

#endif // LL_LLVERTEXBUFFER_H
