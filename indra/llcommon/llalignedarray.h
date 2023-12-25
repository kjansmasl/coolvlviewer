/**
 * @file llalignedarray.h
 * @brief A static array which obeys alignment restrictions and mimics std::vector accessors.
 *
 * $LicenseInfo:firstyear=2013&license=viewergpl$
 *
 * Copyright (c) 2013, Linden Research, Inc.
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

#ifndef LL_LLALIGNEDARRAY_H
#define LL_LLALIGNEDARRAY_H

#include "llmemory.h"

LL_NO_INLINE static void ll_aligned_array_out_of_bounds(U32 idx, U32 count,
														U32 loc)
{
	llwarns << "Out of bounds LLAlignedArray index requested (" << loc << "): "
			<< idx << " - size: " << count << llendl;
	llassert(false);
}

template <class T, U32 alignment>
class LLAlignedArray
{
protected:
	LOG_CLASS(LLAlignedArray);

	// Sets the container to the requested 'size' if possible. When successful
	// returns true (at which point mElementCount has also been set to 'size'),
	// or false when there has been a failure to expand the capacity to fit the
	// requested size (mElementCount is then left untouched).
	bool expand(U32 size);

public:
	LL_INLINE LLAlignedArray() noexcept
	:	mArray(NULL),
		mElementCount(0),
		mCapacity(0)
	{
	}

	LL_INLINE ~LLAlignedArray() noexcept
	{
		if (mArray)
		{
			ll_aligned_free((void*)mArray);
			mArray = NULL;
		}
		mElementCount = mCapacity = 0;
	}

	LL_INLINE void push_back(const T& elem)
	{
		if (mCapacity <= mElementCount)
		{
			if (expand(mElementCount + 1))
			{
				// Because expand() just incremented it...
				--mElementCount;
			}
		}
		if (!mArray || mCapacity <= mElementCount)
		{
			// Cannot go further...
			llassert(false);
			return;
		}

		mArray[mElementCount++] = elem;
	}

	LL_INLINE U32 size() const
	{
		return mElementCount;
	}

	LL_INLINE U32 empty() const
	{
		return mElementCount == 0;
	}

	LL_INLINE void resize(U32 size)
	{
		expand(size);
	}

	LL_INLINE T* append(S32 N)
	{
		U32 sz = size();
		resize(sz + N);
		return &((*this)[sz]);
	}

	LL_INLINE T& operator[](U32 idx)
	{
		if (idx >= mElementCount)
		{
			ll_aligned_array_out_of_bounds(idx, mElementCount, 1);
			// Avoids crashing for release builds...
			return mDummy;
		}
		return mArray[idx];
	}

	LL_INLINE const T& operator[](U32 idx) const
	{
		if (idx >= mElementCount)
		{
			ll_aligned_array_out_of_bounds(idx, mElementCount, 2);
			// Avoids crashing for release builds...
			return mDummy;
		}
		return mArray[idx];
	}

public:
	T*	mArray;
	T	mDummy;
	U32	mElementCount;
	U32	mCapacity;
};

template <class T, U32 alignment>
LL_NO_INLINE bool LLAlignedArray<T, alignment>::expand(U32 size)
{
	if (mCapacity < size)
	{
		U32 new_capacity = size <= 128 ? 2 * size + 16 : size + size / 8;
		T* new_buf = (T*)ll_aligned_malloc(new_capacity * sizeof(T),
										   alignment);
		if (!new_buf)
		{
			// Try with the strict required number of elements
			new_capacity = size;
			new_buf = (T*)ll_aligned_malloc(new_capacity * sizeof(T),
											alignment);
		}
		if (!new_buf)
		{
			llwarns << "Failure to resize to " << size << " elements !"
					<< llendl;
			return false;
		}
		// Zero out the new allocated elements in the new array. HB
		memset((void*)(new_buf + mCapacity), 0,
				(size - mCapacity) * sizeof(T));
		if (mArray && mElementCount)
		{
			ll_memcpy_nonaliased_aligned_16((char*)new_buf,
											(char*)mArray,
											sizeof(T) * mElementCount);
		}
		if (mArray)
		{
			ll_aligned_free((void*)mArray);
		}
		mArray = new_buf;
		mCapacity = new_capacity;
	}
	mElementCount = size;
	return true;
}

#endif
