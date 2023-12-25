/**
 * @file llimage.h
 * @brief Object for managing images and their textures.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab. Terms of
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

#ifndef LL_LLIMAGE_H
#define LL_LLIMAGE_H

#if LL_JEMALLOC
# include "jemalloc/jemalloc.h"
#endif

#include "llatomic.h"
#include "llmemory.h"
#include "llpointer.h"
#include "llpreprocessor.h"
#include "llstring.h"
#include "llthread.h"
#include "hbtracy.h"

// Forward declarations
LL_INLINE void* allocate_texture_mem(size_t size);
LL_INLINE void free_texture_mem(void* addr) noexcept;

constexpr S32 MIN_IMAGE_MIP = 2;  // 4x4, only used for expand/contract power of 2
constexpr S32 MAX_IMAGE_MIP = 11; // 2048x2048
constexpr S32 MAX_DISCARD_LEVEL = 5;

// MIN_IMAGE_SIZE = 4: only used for expand/contract power of 2:
constexpr S32 MIN_IMAGE_SIZE = 1 << MIN_IMAGE_MIP;
constexpr S32 MAX_IMAGE_SIZE = 1 << MAX_IMAGE_MIP; // 2048
constexpr S32 MIN_IMAGE_AREA = MIN_IMAGE_SIZE * MIN_IMAGE_SIZE;
constexpr S32 MAX_IMAGE_AREA = MAX_IMAGE_SIZE * MAX_IMAGE_SIZE;
constexpr S32 MAX_IMAGE_COMPONENTS = 8;
constexpr S32 MAX_IMAGE_DATA_SIZE = MAX_IMAGE_AREA * MAX_IMAGE_COMPONENTS;

// Note: these CANNOT be changed without modifying simulator code
// *TODO: change both to 1024 when SIM texture fetching is deprecated
constexpr S32 FIRST_PACKET_SIZE = 600;
constexpr S32 MAX_IMG_PACKET_SIZE = 1000;

// Base classes for images. There are two major parts for the image: the
// compressed representation and the decompressed representation.

class LLImageFormatted;
class LLImageRaw;
class LLColor4U;

typedef enum e_image_codec
{
	IMG_CODEC_INVALID  = 0,
	IMG_CODEC_RGB  = 1,
	IMG_CODEC_J2C  = 2,
	IMG_CODEC_BMP  = 3,
	IMG_CODEC_TGA  = 4,
	IMG_CODEC_JPEG = 5,
	IMG_CODEC_PNG  = 6,
	IMG_CODEC_EOF  = 7
} EImageCodec;

//============================================================================
// Library initialization class

class LLImage
{
protected:
	LOG_CLASS(LLImage);

public:
	static void initClass();
	static void cleanupClass();
	static void dumpStats();

	static const std::string& getLastError();
	static void setLastError(const std::string& message);

#if LL_JEMALLOC
	LL_INLINE static U32 getMallocxFlags()			{ return sMallocxFlags; }
#endif

protected:
	static LLMutex*				sMutex;
	static std::string			sLastErrorMessage;

public:
	static U8*					sTempDataBuffer;
	static U32					sTempDataBufferUsageCount;
	static U32					sDynamicBufferAllocationsCount;
	static S32					sMaxMainThreadTempBufferSizeRequest;

#if LL_JEMALLOC
private:
	static U32					sMallocxFlags;
#endif
};

//============================================================================
// Image base class

class LLImageBase : public LLThreadSafeRefCount
{
protected:
	LOG_CLASS(LLImageBase);

	virtual ~LLImageBase();

public:
#if LL_JEMALLOC
	LL_INLINE void* operator new(size_t size)
	{
		return allocate_texture_mem(size);
	}

	LL_INLINE void* operator new[](size_t size)
	{
		return allocate_texture_mem(size);
	}

	LL_INLINE void operator delete(void* ptr) noexcept
	{
		free_texture_mem(ptr);
	}

	LL_INLINE void operator delete[](void* ptr) noexcept
	{
		free_texture_mem(ptr);
	}
#endif

	LLImageBase();

	enum
	{
		TYPE_NORMAL = 0,
		TYPE_AVATAR_BAKE = 1,
	};

	void deleteData();
	U8* allocateData(S32 size = -1);
	U8* reallocateData(S32 size = -1);

	virtual void dump();
	virtual void sanityCheck();

	U16 getWidth() const							{ return mWidth; }
	U16 getHeight() const							{ return mHeight; }
	S8 getComponents() const						{ return mComponents; }
	S32 getDataSize() const							{ return mDataSize; }

	const U8* getData() const;
	U8* getData();
	bool isBufferInvalid();

	void setSize(S32 width, S32 height, S32 ncomponents);
	// setSize() + allocateData()
	U8* allocateDataSize(S32 width, S32 height, S32 ncomponents,
						 S32 size = -1);

protected:
	// Special accessor to allow direct setting of mData and mDataSize by
	// LLImageFormatted
	void setDataAndSize(U8* data, S32 size);

public:
	static void generateMip(const U8* indata, U8* mipdata,
							int width, int height, S32 nchannels);

	// Function for calculating the download priority for textures
	// <= 0 priority means that there's no need for more data.
	static F32 calc_download_priority(F32 virtual_size, F32 visible_area,
									  S32 bytes_sent);

	static void setSizeOverride(bool enabled)		{ sSizeOverride = enabled; }

	static EImageCodec getCodecFromExtension(const std::string& exten);

private:
	U8* mData;
	S32 mDataSize;

	U16 mWidth;
	U16 mHeight;

	S8 mComponents;

	bool mBadBufferAllocation;

public:
	S16 mMemType; // kept for compatibility with Snowglobe's KDU

	static bool sSizeOverride;
};

// Raw representation of an image used for textures, and other uncompressed
// formats
class LLImageRaw : public LLImageBase
{
protected:
	LOG_CLASS(LLImageRaw);

	~LLImageRaw() override;

public:
	LLImageRaw();
	LLImageRaw(U16 width, U16 height, S8 components);
	LLImageRaw(U8* data, U16 width, U16 height, S8 components,
			   bool no_copy = false);
	LLImageRaw(const U8* data, U16 width, U16 height, S8 components);
	// Construct using createFromFile (used by tools)
	LLImageRaw(const std::string& filename, bool j2c_lowest_mip_only = false);

	// Use in conjunction with "no_copy" constructor to release data pointer
	// before deleting so that deletion of this LLImageRaw will not free the
	// memory at the "data" parameter provided to "no_copy" constructor.
	void releaseData();

	bool resize(U16 width, U16 height, S8 components);

	U8* getSubImage(U32 x_pos, U32 y_pos, U32 width, U32 height) const;
	bool setSubImage(U32 x_pos, U32 y_pos, U32 width, U32 height,
					 const U8* data, U32 stride = 0, bool reverse_y = false);

	void clear(U8 r = 0, U8 g = 0, U8 b = 0, U8 a = 255);

	void verticalFlip();

	// When all pixels are opaque, deletes the alpha channel and returns true,
	// or does nothing and returns false otherwise.
	bool optimizeAwayAlpha();

	void expandToPowerOfTwo(S32 max_dim = MAX_IMAGE_SIZE,
							bool scale_image = true);
	void contractToPowerOfTwo(S32 max_dim = MAX_IMAGE_SIZE,
							  bool scale_image = true);
	void biasedScaleToPowerOfTwo(S32 max_dim = MAX_IMAGE_SIZE);
	bool scale(S32 new_width, S32 new_height, bool scale_image = true);
	LLPointer<LLImageRaw> scaled(S32 new_width, S32 new_height);

	// Fill the buffer with a constant color
	void fill(const LLColor4U& color);

	// Copy operations

	// Duplicate this raw image if refCount > 1.
	LLPointer<LLImageRaw> duplicate();

	// Src and dst can be any size. Src and dst can each have 3 or 4
	// components.
	void copy(LLImageRaw* src);

	// Src and dst are same size. Src and dst have same number of components.
	void copyUnscaled(LLImageRaw* src);

	// Src and dst are same size. Src has 4 components. Dst has 3 components.
	void copyUnscaled4onto3(LLImageRaw* src);

	// Src and dst are same size. Src has 3 components. Dst has 4 components.
	void copyUnscaled3onto4(LLImageRaw* src);

	// Src and dst are same size. Src has 1 component. Dst has 4 components.
	// Alpha component is set to source alpha mask component.
	// RGB components are set to fill color.
	void copyUnscaledAlphaMask(LLImageRaw* src, const LLColor4U& fill);

	// Src and dst can be any size. Src and dst have same number of components.
	void copyScaled(LLImageRaw* src);

	// Src and dst can be any size. Src has 3 components. Dst has 4 components.
	void copyScaled3onto4(LLImageRaw* src);

	// Src and dst can be any size. Src has 4 components. Dst has 3 components.
	void copyScaled4onto3(LLImageRaw* src);

	// Composite operations

	// Src and dst can be any size. Src and dst can each have 3 or 4
	// components.
	void composite(LLImageRaw* src);

	// Src and dst can be any size. Src has 4 components. Dst has 3 components.
	void compositeScaled4onto3(LLImageRaw* src);

	// Src and dst are same size. Src has 4 components. Dst has 3 components.
	void compositeUnscaled4onto3(LLImageRaw* src);

protected:
	// Create an image from a local file (generally used in tools)
	bool createFromFile(const std::string& filename,
						bool j2c_lowest_mip_only = false);

	U8* getTempBuffer(S32 size);
	void freeTempBuffer(U8* addr);

	void copyLineScaled(U8* in, U8* out, S32 in_pixel_len, S32 out_pixel_len,
						S32 in_pixel_step, S32 out_pixel_step);
	void compositeRowScaled4onto3(U8* in, U8* out, S32 in_pixel_len,
								  S32 out_pixel_len);

	U8 fastFractionalMult(U8 a,U8 b);

	void setDataAndSize(U8* data, S32 width, S32 height, S8 components);

public:
	// NOTE: written by several image decode threads, so need to be atomic
	static LLAtomicS32 sRawImageCount;
};

// Compressed representation of image.
// Subclass from this class for the different representations (J2C, bmp)
class LLImageFormatted : public LLImageBase
{
protected:
	LOG_CLASS(LLImageFormatted);

public:
	static LLImageFormatted* createFromType(S8 codec);
	static LLImageFormatted* createFromExtension(const std::string& instring);

public:
	LLImageFormatted(S8 codec);

	void dump() override;
	void sanityCheck() override;

	// Subclasses must return a prefered file extension (lowercase without a
	// leading dot)
	virtual std::string getExtension() = 0;
	// Returns the maximum size of header; 0 indicates we don't know have a
	// header and have to lead the entire file
	virtual S32 calcHeaderSize() { return 0; };
	// Returns how many bytes to read to load discard_level (including header)
	virtual S32 calcDataSize(S32 discard_level);
	// Returns the smallest valid discard level based on the number of input
	// bytes
	virtual S32 calcDiscardLevelBytes(S32 bytes);
	// By default getRawDiscardLevel() returns mDiscardLevel, but may be
	// overridden (LLImageJ2C)
	virtual S8  getRawDiscardLevel()				{ return mDiscardLevel; }

	bool load(const std::string& filename);
	bool save(const std::string& filename);

	virtual bool updateData() = 0;		// Pure virtual
 	void setData(U8* data, S32 size);
 	void appendData(U8* data, S32 size);

	// Loads first 4 channels.
	virtual bool decode(LLImageRaw* raw_image) = 0;
	// Subclasses that can handle more than 4 channels should override this
	// function.
	virtual bool decodeChannels(LLImageRaw* raw_image, S32 first_channel,
								S32 max_channel);

	virtual bool encode(const LLImageRaw* raw_image) = 0;

	S8 getCodec() const;
	bool isDecoding() const							{ return mDecoding != 0; }
	bool isDecoded() const							{ return mDecoded != 0; }
	void setDiscardLevel(S8 discard_level)			{ mDiscardLevel = discard_level; }
	S8 getDiscardLevel() const						{ return mDiscardLevel; }

	// setLastError needs to be deferred for J2C images since it may be called
	// from a DLL
	virtual void resetLastError();
	virtual void setLastError(const std::string& message,
							  const std::string& filename = std::string());

protected:
	bool copyData(U8* data, S32 size);	// calls updateData()

protected:
	S8 mCodec;
	S8 mDecoding;
	// mDecoded is unused, but changing LLImage layout requires recompiling
	// static Mac/Linux libs. 2009-01-30 JC
	S8 mDecoded;
	S8 mDiscardLevel;
};

// NOTE: since the memory functions below use void* pointers instead of char*
// (because void* is the type used by malloc and jemalloc), strict aliasing is
// not possible on structures allocated with them. Make sure you forbid your
// compiler to optimize with strict aliasing assumption (i.e. for gcc, DO use
// the -fno-strict-aliasing option) !

LL_INLINE void* allocate_texture_mem(size_t size)
{
	if (LL_UNLIKELY(size <= 0)) return NULL;

	void* addr;
#if LL_JEMALLOC
	addr = mallocx(size, LLImage::getMallocxFlags());
#else
	addr = malloc(size);
#endif
	LL_TRACY_ALLOC(addr, size, trc_mem_image);

	if (LL_UNLIKELY(addr == NULL))
	{
		LLMemory::allocationFailed(size);
	}

	return addr;
}

LL_INLINE void free_texture_mem(void* addr) noexcept
{
	if (LL_UNLIKELY(addr == NULL)) return;

	LL_TRACY_FREE(addr, trc_mem_image);
#if LL_JEMALLOC
	dallocx(addr, 0);
#else
	free(addr);
#endif
}

#endif
