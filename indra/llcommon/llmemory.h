/**
 * @file llmemory.h
 * @brief Memory allocation/deallocation header-stuff goes here.
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

#ifndef LL_MEMORY_H
#define LL_MEMORY_H

#include <set>
#include <string.h>		// For memcpy()
#if !LL_WINDOWS
# include <stdint.h>
#endif

// Note: even though no intrinsec function is used directly by this header,
// #include'ing this here allows to ensure the proper intrinsec header will be
// used by the rest of the viewer code, as long as llmemory.h is included. HB
#if SSE2NEON
# include "sse2neon.h"
#else
# include <immintrin.h>
#endif

#if LL_JEMALLOC
# include "jemalloc/jemalloc.h"
#elif !LL_WINDOWS
# include <stdlib.h>
#endif

#include "llerror.h"
#include "hbtracy.h"

// Utilities and macros used for SSE2 optimized maths

#if LL_DEBUG
void ll_assert_aligned_error();
# define ll_assert_aligned(ptr,alignment) \
	if (LL_UNLIKELY(reinterpret_cast<uintptr_t>(ptr) % (U32)alignment != 0)) \
		ll_assert_aligned_error()
#else
# define ll_assert_aligned(ptr,alignment)
#endif

// Purely static class
class LLMemory
{
	LLMemory() = delete;
	~LLMemory() = delete;

protected:
	LOG_CLASS(LLMemory);

public:
	// These two methods must only be called from the main thread. Called from
	// indra/newview/llappviewer*.cpp:
	static void initClass();
	static void cleanupClass();

	// Return the resident set size of the current process, in bytes, or zero
	// if not known.
	static U64 getCurrentRSS();

	static void updateMemoryInfo(bool trim_heap = false);
	static void logMemoryInfo();

	static U32 getMaxPhysicalMemKB()			{ return sMaxPhysicalMemInKB; }
	static U32 getMaxVirtualMemKB()				{ return sMaxVirtualMemInKB; }
	static U32 getAvailablePhysicalMemKB()		{ return sAvailPhysicalMemInKB; }
	static U32 getAvailableVirtualMemKB()		{ return sAvailVirtualMemInKB; }
	static U32 getAllocatedMemKB()				{ return sAllocatedMemInKB; }
	static U32 getAllocatedPageSizeKB()			{ return sAllocatedPageSizeInKB; }

	static void allocationFailed(size_t size = 0);
	static void resetFailedAllocation()			{ sFailedAllocation = false; }
	LL_INLINE static bool hasFailedAllocation()	{ return sFailedAllocation; }
	LL_INLINE static bool gotFailedAllocation()	{ return sFailedAllocationOnce; }

	// The four methods below used to be part of LLMemoryInfo (in llsys.h/cpp),
	// which I removed to merge here, since they did not even relate with the
	// global system info, but instead with the memory consumption of the
	// viewer itself... HB

	static U32 getPhysicalMemoryKB();	// Memory size in KiloBytes

	// Get the available memory infomation in KiloBytes.
	static void getMaxMemoryKB(U32& max_physical_mem_kb,
							   U32& max_virtual_mem_kb);

	static void getAvailableMemoryKB(U32& avail_physical_mem_kb,
									 U32& avail_virtual_mem_kb);

	static std::string getInfo();

private:
	static bool sFailedAllocation;
	static bool sFailedAllocationOnce;

	static U32	sMaxPhysicalMemInKB;
	static U32	sMaxVirtualMemInKB;
	static U32	sAvailPhysicalMemInKB;
	static U32	sAvailVirtualMemInKB;
	static U32	sAllocatedMemInKB;
	static U32	sAllocatedPageSizeInKB;
};

// Generic aligned memory management. Note that other, usage-specific functions
// are used to allocate (pooled) memory for images, vertex buffers and volumes.
// See: llimage/llimage.h, llrender/llvertexbuffer.h and llmath/llvolume.h. HB

// NOTE: since the memory functions below use void* pointers instead of char*
// (because void* is the type used by malloc and jemalloc), strict aliasing is
// not possible on structures allocated with them. Make sure you forbid your
// compiler to optimize with strict aliasing assumption (i.e. for gcc, DO use
// the -fno-strict-aliasing option) !  HB

// IMPORTANT: returned hunk MUST be freed with ll_aligned_free_16().
LL_INLINE void* ll_aligned_malloc_16(size_t size, bool track_failure = true)
{
	if (LL_UNLIKELY(size <= 0)) return NULL;

	void* ptr;
#if LL_JEMALLOC || LL_MIMALLOC || LL_DARWIN
	// With jemalloc, mimalloc or macOS, all malloc() calls are 16-bytes
	// aligned.
	ptr = malloc(size);
#elif LL_WINDOWS
	ptr = _aligned_malloc(size, 16);
#else
	if (LL_UNLIKELY(posix_memalign(&ptr, 16, size) != 0))
	{
		// Out of memory
		if (track_failure)
		{
			LLMemory::allocationFailed(size);
		}
		return NULL;
	}
#endif
	if (LL_UNLIKELY(ptr == NULL && track_failure))
	{
		LLMemory::allocationFailed(size);
	}

	LL_TRACY_ALLOC(ptr, size, trc_mem_align16);

	return ptr;
}

LL_INLINE void ll_aligned_free_16(void* p) noexcept
{
	LL_TRACY_FREE(p, trc_mem_align16);

	if (LL_LIKELY(p))
	{
#if LL_JEMALLOC || LL_MIMALLOC || LL_DARWIN
		// With jemalloc, mimalloc or macOS, all malloc() calls are 16-bytes
		// aligned.
		free(p);
#elif LL_WINDOWS
		_aligned_free(p);
#else
		free(p); // posix_memalign() is compatible with free()
#endif
	}
}

// IMPORTANT: returned hunk MUST be freed with ll_aligned_free_16().
LL_INLINE void* ll_aligned_realloc_16(void* ptr, size_t size, size_t old_size)
{
	if (LL_UNLIKELY(size == old_size && ptr))
	{
		return ptr;
	}

	LL_TRACY_FREE(ptr, trc_mem_align16);

	void* ret;
#if LL_JEMALLOC || LL_MIMALLOC || LL_DARWIN
	// With jemalloc, mimalloc or macOS, all realloc() calls are 16-bytes
	// aligned.
	ret = realloc(ptr, size);
#elif LL_WINDOWS
	ret = _aligned_realloc(ptr, size, 16);
#else
	if (LL_LIKELY(posix_memalign(&ret, 16, size) == 0))
	{
		if (LL_LIKELY(ptr))
		{
			// FIXME: memcpy is SLOW
			memcpy(ret, ptr, old_size < size ? old_size : size);
			free(ptr);
		}
		LL_TRACY_ALLOC(ret, size, trc_mem_align16);
		return ret;
	}
	LLMemory::allocationFailed(size);
	return NULL;
#endif
	if (LL_UNLIKELY(ret == NULL))
	{
		LLMemory::allocationFailed(size);
	}
	LL_TRACY_ALLOC(ret, size, trc_mem_align16);
	return ret;
}

LL_INLINE void* ll_aligned_malloc(size_t size, int align)
{
	if (LL_UNLIKELY(size <= 0)) return NULL;

	void* addr;

#if LL_JEMALLOC
	addr = mallocx(size, MALLOCX_ALIGN((size_t)align) | MALLOCX_TCACHE_NONE);
#elif LL_WINDOWS
	addr = _aligned_malloc(size, align);
#else
	addr = malloc(size + (align - 1) + sizeof(void*));
	if (LL_LIKELY(addr))
	{
		char* aligned = ((char*)addr) + sizeof(void*);
		aligned += align - ((uintptr_t)aligned & (align - 1));
		((void**)aligned)[-1] = addr;
		addr = (void*)aligned;
	}
#endif

	if (LL_UNLIKELY(addr == NULL))
	{
		LLMemory::allocationFailed(size);
	}

	LL_TRACY_ALLOC(addr, size, trc_mem_align);

	return addr;
}

LL_INLINE void ll_aligned_free(void* addr) noexcept
{
	LL_TRACY_FREE(addr, trc_mem_align);

	if (LL_LIKELY(addr))
	{
#if LL_JEMALLOC
		dallocx(addr, 0);
#elif LL_WINDOWS
		_aligned_free(addr);
#else
		free(((void**)addr)[-1]);
#endif
	}
}

// Copy words 16-bytes blocks from src to dst. Source and destination MUST NOT
// OVERLAP. Source and dest must be 16-byte aligned and size must be multiple
// of 16.
void ll_memcpy_nonaliased_aligned_16(char* __restrict dst,
									 const char* __restrict src, size_t bytes);

// Handy conversion macro. HB
#define BYTES2MEGABYTES(x) ((x) >> 20)

#endif	// LL_MEMORY_H
