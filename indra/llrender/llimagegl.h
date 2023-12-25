/**
 * @file llimagegl.h
 * @brief Object for managing images and their textures
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


#ifndef LL_LLIMAGEGL_H
#define LL_LLIMAGEGL_H

#include <vector>

#include "llatomic.h"
#include "llimage.h"	// For allocate_texture_mem() and free_texture_mem()
#include "llmutex.h"
#include "llrefcount.h"
#include "llrender.h"
#include "llthreadpool.h"

class LLGLTexture;
class LLImageGLThread;
class LLWindow;

// Note: LLImageGL must now derive from LLThreadSafeRefCount instead of
// LLRefCount because ref() and unref() are used in GL threads. HB
class LLImageGL : public LLThreadSafeRefCount
{
	friend class LLTexUnit;

protected:
	LOG_CLASS(LLImageGL);

	virtual ~LLImageGL();

	void analyzeAlpha(const void* data_in, U32 w, U32 h);
	void calcAlphaChannelOffsetAndStride();

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

	LLImageGL(bool usemipmaps = true);
	LLImageGL(U32 width, U32 height, U8 components, bool usemipmaps = true);
	LLImageGL(const LLImageRaw* imagerawp, bool usemipmaps = true);

	// To allow tracking owners, for periodic image list cleanup. HB
	LL_INLINE void setOwner(LLGLTexture* ownerp)		{ mOwnerp = ownerp; }
	LL_INLINE LLGLTexture* getOwner() const				{ return mOwnerp; }

	static void initThread(LLWindow* windowp, S32 threads);
	static void stopThread();

	// These 2 functions replace glGenTextures() and glDeleteTextures()
	static void generateTextures(U32 num_textures, U32* textures);
	static void deleteTextures(U32 num_textures, const U32* textures);

	// Size calculation
	static U32 dataFormatBits(S32 dataformat);
	static S64 dataFormatBytes(S32 dataformat, U32 width, U32 height);
	static U32 dataFormatComponents(S32 dataformat);

	// Needs to be called every frame
	static void updateStats(F32 current_time);

	bool updateBindStats() const;
	LL_INLINE void forceUpdateBindStats() const
	{
		mLastBindTime = sLastFrameTime;
	}

	LL_INLINE F32 getTimePassedSinceLastBound()
	{
		return sLastFrameTime - mLastBindTime;
	}

	// Save off / restore GL textures
	static void destroyGL(bool save_state = true);
	static void restoreGL();
	static void dirtyTexOptions();

	static bool checkSize(S32 width, S32 height);

	virtual void dump();	// debugging info to llinfos

	bool setSize(U32 width, U32 height, U8 ncomponents,
				 S32 discard_level = -1);
	LL_INLINE void setComponents(U8 comps)				{ mComponents = comps; }
	LL_INLINE void setAllowCompression(bool allow)		{ mAllowCompression = allow; }

	static void setManualImage(U32 target, S32 miplevel, S32 intformat,
							   U32 width, U32 height,
							   U32 pixformat, U32 pixtype,
							   const void* pixels,
							   bool allow_compression = true);

	bool createGLTexture();
	bool createGLTexture(S32 discard_level, const LLImageRaw* imagerawp,
						 S32 usename = 0, bool to_create = true,
						 bool defer_copy = false, U32* tex_name = NULL);
	bool createGLTexture(S32 discard_level, const U8* data,
						 bool data_hasmips = false, S32 usename = 0,
						 bool defer_copy = false, U32* tex_name = NULL);
	void setImage(const LLImageRaw* imagerawpp);
	bool setImage(const U8* data_in, bool data_hasmips = false,
				  S32 usename = 0);
	bool setSubImage(const LLImageRaw* imagerawpp, S32 x_pos, S32 y_pos,
					 U32 width, U32 height, bool force_fast_update = false,
					 U32 use_name = 0);
	bool setSubImage(const U8* datap, U32 data_width, U32 data_height,
					 S32 x_pos, S32 y_pos, U32 width, U32 height,
					 bool force_fast_update = false,
					 U32 use_name = 0);
	bool setSubImageFromFrameBuffer(S32 fb_x, S32 fb_y, S32 x_pos, S32 y_pos,
									U32 width, U32 height);

	// Wait for GL commands to finish on current thread and push a lambda to
	// the main thread to set mTexName with the new name.
	void syncToMainThread(U32 new_tex_name);

	// Read back a raw image for this discard level, if it exists
	bool readBackRaw(S32 discard_level, LLImageRaw* imagerawp,
					 bool compressed_ok) const;
	void destroyGLTexture();
	void forceToInvalidateGLTexture();

	void setExplicitFormat(S32 internal_format, U32 primary_format,
						   U32 type_format = 0, bool swap_bytes = false);
	LL_INLINE void setComponents(S8 ncomponents)		{ mComponents = ncomponents; }

	LL_INLINE S32 getDiscardLevel() const				{ return mCurrentDiscardLevel; }
	LL_INLINE S32 getMaxDiscardLevel() const			{ return mMaxDiscardLevel; }

	LL_INLINE U32 getCurrentWidth() const				{ return mWidth;}
	LL_INLINE U32 getCurrentHeight() const				{ return mHeight;}
	U32 getWidth(S32 discard_level = -1) const;
	U32 getHeight(S32 discard_level = -1) const;
	LL_INLINE U8 getComponents() const					{ return mComponents; }
	S64 getBytes(S32 discard_level = -1) const;
	S64 getMipBytes(S32 discard_level = -1) const;

	LL_INLINE bool getBoundRecently() const
	{
		constexpr F32 MIN_TEXTURE_LIFETIME = 10.f;
		return sLastFrameTime - mLastBindTime < MIN_TEXTURE_LIFETIME;
	}

	LL_INLINE bool isJustBound() const					{ return sLastFrameTime - mLastBindTime < 0.5f; }

	LL_INLINE bool hasExplicitFormat() const			{ return mHasExplicitFormat; }
	LL_INLINE U32 getPrimaryFormat() const				{ return mFormatPrimary; }
	LL_INLINE U32 getFormatType() const					{ return mFormatType; }

	bool isCompressed() const;

	LL_INLINE bool getHasGLTexture() const
	{
		const_cast<LLImageGL*>(this)->syncTexName();
		return mTexName != 0;
	}

	LL_INLINE U32 getTexName() const
	{
		const_cast<LLImageGL*>(this)->syncTexName();
		return mTexName;
	}

	LL_INLINE void setTexName(U32 name)
	{
		syncTexName();
		mTexName = name;
	}

	// Similar to setTexName, but will call deleteTextures on mTexName if
	// mTexName is not 0 or texname. Used by LLBumpImageList.
	void syncTexName(U32 texname);

	LL_INLINE bool getIsAlphaMask() const				{ return mIsMask; }

	void setTarget(U32 target, LLTexUnit::eTextureType bind_target);

	LL_INLINE LLTexUnit::eTextureType getTarget() const	{ return mBindTarget; }
	LL_INLINE bool isGLTextureCreated() const			{ return mGLTextureCreated; }
	LL_INLINE void setGLTextureCreated(bool b)			{ mGLTextureCreated = b; }

	LL_INLINE bool getUseMipMaps() const				{ return mUseMipMaps; }
	LL_INLINE void setUseMipMaps(bool b)				{ mUseMipMaps = b; }

	LL_INLINE void setHasMipMaps(bool b)				{ mHasMipMaps = b; }

	void updatePickMask(U32 width, U32 height, const U8* data_in);
	bool getMask(const LLVector2& tc);

	// Sets the addressing mode used to sample the texture (such as wrapping,
	// mirrored wrapping, and clamp).
	// Note: this actually gets set the next time the texture is bound.
	void setAddressMode(LLTexUnit::eTextureAddressMode mode);

	LL_INLINE LLTexUnit::eTextureAddressMode getAddressMode() const
	{
		return mAddressMode;
	}

	// Sets the filtering options used to sample the texture (such as point
	// sampling, bilinear interpolation, mipmapping and anisotropic filtering).
	// Note: this actually gets set the next time the texture is bound.
	void setFilteringOption(LLTexUnit::eTextureFilterOptions option);

	LL_INLINE LLTexUnit::eTextureFilterOptions getFilteringOption() const
	{
		return mFilterOption;
	}

	LL_INLINE U32 getTexTarget()const					{ return mTarget; }

	void init(bool usemipmaps);

	 // Clean up the LLImageGL so it can be reinitialized. Be careful when
	// using this in derived class destructors
	virtual void cleanup();

	void setNeedsAlphaAndPickMask(bool need_mask);

	// Without regular calls to this method, the viewer piles up NO_DELETE
	// textures in memory (both RAM and, much worst, VRAM), causing a "leakage"
	// that ruins textures LoDs in the long term (since the discard bias must
	// then be kept high to avoid VRAM from overflowing), and even VRAM
	// exhaustion in the long term. HB
	static U32 activateStaleTextures();

	// For debugging, logs the stales images list. HB
	static void dumpStaleList();

private:
	void syncTexName();

	U32 createPickMask(U32 width, U32 height);
	void freePickMask();

private:
	LLPointer<LLImageRaw>	mSaveData;	// Used for destroyGL/restoreGL

	LLGLTexture*			mOwnerp;

	LLAtomicBool			mTexNameDirty;
	U32						mTexName;
	U32						mNewTexName;
	GLsync					mTexNameSync;

	// Downsampled bitmap approximation of alpha channel. NULL if no alpha
	// channel:
	U8*						mPickMask;

	U16						mPickMaskWidth;
	U16						mPickMaskHeight;

	U16						mWidth;
	U16						mHeight;

	bool					mUseMipMaps;

	// If false (default), GL format is f(mComponents):
	bool					mHasExplicitFormat;

	bool					mAutoGenMips;

	bool					mIsMask;
	bool					mNeedsAlphaAndPickMask;
	S8						mAlphaStride;
	S8						mAlphaOffset;

	bool					mGLTextureCreated;
	S8						mCurrentDiscardLevel;

	bool					mAllowCompression;

protected:
	bool					mHasMipMaps;

	bool					mTexOptionsDirty;

	// If true, use glPixelStorei(GL_UNPACK_SWAP_BYTES, 1)
	bool					mFormatSwapBytes;

	U8						mComponents;
	S8						mMaxDiscardLevel;

	// Normally GL_TEXTURE2D, sometimes something else (ex. cube maps):
	U32						mTarget;

	// Normally TT_TEXTURE, sometimes something else (ex. cube maps)
	LLTexUnit::eTextureType	mBindTarget;

	S32						mMipLevels;

	// Defaults to TAM_WRAP
	LLTexUnit::eTextureAddressMode mAddressMode;
	// Defaults to TFO_ANISOTROPIC
	LLTexUnit::eTextureFilterOptions mFilterOption;

	S32						mFormatInternal;	// = GL internal format
	U32						mFormatPrimary;		// = GL pixel data format
	U32						mFormatType;

public:
	// Various GL/Rendering options
	S64						mTextureMemory;

	// Last time this was bound, by discard level
	mutable F32				mLastBindTime;

#if DEBUG_MISS
	bool					mMissed	// Missed on last bind ?
#endif

	static S32				sCount;
	static F32				sLastFrameTime;
	static LLImageGL*		sDefaultGLImagep;

	// Global memory statistics

	// Tracks main memory texmem (atomic since accessed from GL threads)
	static LLAtomicS64		sGlobalTexMemBytes;
	// Tracks bound texmem for last completed frame;
	static S64				sBoundTexMemBytes;
	// Tracks number of texture binds for current frame
	static U32				sBindCount;
	// Tracks number of unique texture binds for current frame
	static U32				sUniqueCount;
	// GL textures compression
	static U32				sCompressThreshold;
	static bool				sCompressTextures;

	static bool				sGlobalUseAnisotropic;

	// This flag *must* be set to true before stopping GL and can only be reset
	// to false again once GL is restarted (else, GL textures may get recreated
	// while GL is stopped, which leads to a crash !).
	static bool				sPreserveDiscard;

	// When this is true, and in main thread, and the image is not compressed,
	// setSubImage() and setManualImage() set the image line by line to avoid
	// large data transfers in the GL queue.
	static bool				sSetSubImagePerLine;

	// For NVIDIA, when this is true, we sync GL in the thread after the GL
	// image creation, to avoid stalling at all the main thread GL pipeline. HB
	static bool				sSyncInThread;

private:
	typedef fast_hset<LLImageGL*> glimage_list_t;
	static glimage_list_t	sImageList;
	static LLImageGLThread*	sThread;
};

class LLImageGLThread : public LLThreadPool
{
	friend class LLImageGL;

protected:
	LOG_CLASS(LLImageGLThread);

	LLImageGLThread(LLWindow* window, U32 threads);

public:
	// LLThreadPool overrides
	void run() override;
	// This is a no-op since we must perform complex initialization during
	// run(), that *will* be interrupted by the OS scheduler at some point;
	// we instead call LLThreadPool::doIncStartedThreads() in run(), once
	// our initialization process is finished. HB
	LL_INLINE void maybeIncStartedThreads() override
	{
	}

	// Posts a function to be executed on the LLImageGL background thread
	template <typename CALLABLE>
	bool post(CALLABLE&& func)
	{
		return getQueue().postIfOpen(std::forward<CALLABLE>(func));
	}

	// App should call this method periodically to update the available VRAM
	static void updateFreeVRAM();

	LL_INLINE static S32 getFreeVRAMMegabytes()			{ return sFreeVRAMMegabytes; }

	// Use to destroy remaining instances after their shutdown is done. HB
	static void cleanup();

private:
	static void readFreeVRAM();

private:
	LLWindow*			mWindow;
	// We need a mutex to avoid a race with GL context changes during the
	// threads initialization, which is itself a threaded operation. This mutex
	// also protects mTheadCounter, which is used in the same part of the code
	// and does not need to be made atomic as a result. HB
	LLMutex				mThreadsMutex;
	std::vector<void*>	mContexts;
	U32					mTheadCounter;

public:
	// Free video memory in megabytes
	static LLAtomicS32	sFreeVRAMMegabytes;
	static bool			sEnabled;
};

extern LLWorkQueue::weak_t gImageQueuep;

#endif // LL_LLIMAGEGL_H
