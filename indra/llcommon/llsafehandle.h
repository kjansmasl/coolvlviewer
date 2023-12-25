/**
 * @file llsafehandle.h
 * @brief Reference-counted object where Object() is valid, not NULL.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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
#ifndef LLSAFEHANDLE_H
#define LLSAFEHANDLE_H

#include "llpreprocessor.h"

// This is to avoid inlining a llwarns...
LL_NO_INLINE void warnUnreferenceDidAssignment2();

// Expands LLPointer to return a pointer to a special instance of class Type
// instead of NULL. This is useful in instances where operations on NULL
// pointers are semantically safe and/or when error checking occurs at a
// different granularity or in a different part of the code than when
// referencing an object via a LLSafeHandle.

template <class Type>
class LLSafeHandle
{
public:
	LL_INLINE LLSafeHandle() noexcept
	:	mPointer(NULL)
	{
	}

	LL_INLINE LLSafeHandle(Type* ptr) noexcept
	:	mPointer(NULL)
	{
		assign(ptr);
	}

	LL_INLINE LLSafeHandle(const LLSafeHandle<Type>& ptr) noexcept
	:	mPointer(NULL)
	{
		assign(ptr.mPointer);
	}

	// This C++11 move constructor saves us an unref() on the moved handle
	// and consequently a corresponding ref() on the constructed handle.
	// Note: LLSafeHandle::unref() still happens in ~LLSafeHandle() of the
	// moved handle, but it is not propagated down to the unref() method of the
	// object we point at, since mPointer is NULL when ~LLSafeHandle() is
	// called.
	LL_INLINE LLSafeHandle(LLSafeHandle<Type>&& ptr) noexcept
	:	mPointer(ptr.mPointer)
	{
		ptr.mPointer = NULL;
	}

	// Support conversion up the type hierarchy. See Item 45 in Effective C++,
	// 3rd Ed.
	template<typename Subclass>
	LL_INLINE LLSafeHandle(const LLSafeHandle<Subclass>& ptr) noexcept
	:	mPointer(NULL)
	{
		assign(ptr.get());
	}

	LL_INLINE ~LLSafeHandle()
	{
		unref();
	}

	LL_INLINE const Type* operator->() const	{ return nonNull(mPointer); }
	LL_INLINE Type* operator->()				{ return nonNull(mPointer); }

	LL_INLINE Type* get() const					{ return mPointer; }
	LL_INLINE void clear()						{ assign(NULL); }

#if 0	// we disallow these operations as they expose our null objects to
		// direct manipulation and bypass the reference counting semantics
	LL_INLINE const Type& operator*() const		{ return *nonNull(mPointer); }
	LL_INLINE Type& operator*()					{ return *nonNull(mPointer); }
#endif

	LL_INLINE operator bool()  const			{ return mPointer != NULL; }
	LL_INLINE bool operator!() const			{ return mPointer == NULL; }
	LL_INLINE bool isNull() const				{ return mPointer == NULL; }
	LL_INLINE bool notNull() const				{ return mPointer != NULL; }

	LL_INLINE operator Type*() const			{ return mPointer; }
	LL_INLINE operator const Type*() const		{ return mPointer; }
	LL_INLINE bool operator!=(Type* ptr) const	{ return mPointer != ptr; }
	LL_INLINE bool operator==(Type* ptr) const	{ return mPointer == ptr; }

	LL_INLINE bool operator==(const LLSafeHandle<Type>& ptr) const
	{
		return mPointer == ptr.mPointer;
	}

	LL_INLINE bool operator<(const LLSafeHandle<Type>& ptr) const
	{
		return mPointer < ptr.mPointer;
	}

	LL_INLINE bool operator>(const LLSafeHandle<Type>& ptr) const
	{
		return mPointer > ptr.mPointer;
	}

	LL_INLINE LLSafeHandle<Type>& operator=(Type* ptr)
	{
		assign(ptr);
		return *this;
	}

	LL_INLINE LLSafeHandle<Type>& operator=(const LLSafeHandle<Type>& ptr)
	{
		assign(ptr.mPointer);
		return *this;
	}

	// Support assignment up the type hierarchy. See Item 45 in Effective C++,
	// 3rd Ed.
	template<typename Subclass>
	LL_INLINE LLSafeHandle<Type>& operator=(const LLSafeHandle<Subclass>& ptr)
	{
		assign(ptr.get());
		return *this;
	}

protected:
	LL_INLINE void ref() noexcept
	{
		if (mPointer)
		{
			mPointer->ref();
		}
	}

	LL_INLINE void unref()
	{
		if (mPointer)
		{
			Type* tempp = mPointer;
			mPointer = NULL;
			tempp->unref();
			if (mPointer != NULL)
			{
				warnUnreferenceDidAssignment2();
				unref();
			}
		}
	}

	LL_INLINE void assign(Type* ptr)
	{
		if (mPointer != ptr)
		{
			unref();
			mPointer = ptr;
			ref();
		}
	}

	LL_INLINE static Type* nonNull(Type* ptr)
	{
		return ptr ? ptr : sNullFunc();
	}

	static Type* sNullFunc()
	{
		static Type sInstance;
		return &sInstance;
	}

protected:
	Type*	mPointer;
};

#endif
