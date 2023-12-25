/**
 * @file llhandle.h
 * @brief "Handle" to an object (usually a floater) whose lifetime you don't
 *        control.
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

#ifndef LLHANDLE_H
#define LLHANDLE_H

#include <type_traits>

#include "llpointer.h"
#include "llrefcount.h"

// Helper object for LLHandle. Do not instantiate these directly. Used
// exclusively by LLHandle.
class LLTombStone : public LLRefCount
{
public:
	LLTombStone(void* target = NULL)
	:	mTarget(target)
	{
	}

	LL_INLINE void setTarget(void* target)			{ mTarget = target; }
	LL_INLINE void* getTarget() const				{ return mTarget; }

private:
	mutable void* mTarget;
};

// LLHandles are used to refer to objects whose lifetime you do not control or
// influence. Calling get() on a handle will return a pointer to the referenced
// object or NULL, if the object no longer exists. Note that during the
// lifetime of the returned pointer, you are assuming that the object will not
// be deleted by any action you perform, or any other thread, as normal when
// using pointers, so avoid using that pointer outside of the local code block.
//
//  https://wiki.lindenlab.com/mediawiki/index.php?title=LLHandle&oldid=79669
//
// The implementation is like some "weak pointer" implementations. When we
// cannot control the lifespan of the referenced object of interest, we can
// still instantiate a proxy object whose lifespan we DO control, and store in
// the proxy object a dumb pointer to the actual target. Then we just have to
// ensure that on destruction of the target object, the proxy's dumb pointer
// is set NULL.
//
// LLTombStone is our proxy object. LLHandle contains an LLPointer to the
// LLTombStone, so every copy of an LLHandle increments the LLTombStone's ref
// count as usual.
//
// One copy of the LLHandle, specifically the LLRootHandle, must be stored in
// the referenced object. Destroying the LLRootHandle is what NULLs the
// proxy's target pointer.
//
// Minor optimization: we want LLHandle's mTombStone to always be a valid
// LLPointer, saving some conditionals in dereferencing. That is the
// getDefaultTombStone() mechanism. The default LLTombStone object's target
// pointer is always NULL, so it is semantically identical to allowing
// mTombStone to be invalid.

template <typename T>
class LLHandle
{
	template <typename U> friend class LLHandle;
	template <typename U> friend class LLHandleProvider;

public:
	LLHandle()
	:	mTombStone(getDefaultTombStone())
	{
	}

	template<typename U>
	LLHandle(const LLHandle<U>& other,
			 typename std::enable_if<std::is_convertible<U*, T*>::value>::type* dummy = 0)
	:	mTombStone(other.mTombStone)
	{
	}

	LL_INLINE bool isDead() const
	{
		return mTombStone->getTarget() == NULL;
	}

	LL_INLINE void markDead()
	{
		mTombStone = getDefaultTombStone();
	}

	LL_INLINE T* get() const
	{
		return reinterpret_cast<T*>(mTombStone->getTarget());
	}

	LL_INLINE friend bool operator==(const LLHandle<T>& lhs,
									 const LLHandle<T>& rhs)
	{
		return lhs.mTombStone == rhs.mTombStone;
	}

	LL_INLINE friend bool operator!=(const LLHandle<T>& lhs,
									 const LLHandle<T>& rhs)
	{
		return lhs.mTombStone != rhs.mTombStone;
	}

	LL_INLINE friend bool operator<(const LLHandle<T>& lhs,
									const LLHandle<T>& rhs)
	{
		return lhs.mTombStone < rhs.mTombStone;
	}

	LL_INLINE friend bool operator>(const LLHandle<T>& lhs,
									const LLHandle<T>& rhs)
	{
		return lhs.mTombStone > rhs.mTombStone;
	}

private:
	typedef T* pointer_t;
	static LLPointer<LLTombStone>& getDefaultTombStone()
	{
		static LLPointer<LLTombStone> sDefaultTombStone = new LLTombStone;
		return sDefaultTombStone;
	}

protected:
	LLPointer<LLTombStone> mTombStone;
};

// LLRootHandle is a LLHandle which must be stored in the referenced object.
// You can either store it directly and explicitly bind(this), or derive from
// LLHandleProvider (q.v.) which automates that for you. The essential point is
// that destroying the LLRootHandle (as a consequence of destroying the
// referenced object) calls unbind(), setting the LLTombStone's target pointer
// NULL.

template <typename T>
class LLRootHandle : public LLHandle<T>
{
public:
	typedef LLRootHandle<T> self_t;
	typedef LLHandle<T> base_t;

	LLRootHandle() = default;
	LL_INLINE LLRootHandle(T* object)				{ bind(object); }
	// Do not allow copying of root handles, since there should only be one
	LLRootHandle(const LLRootHandle&) = delete;

	LL_INLINE ~LLRootHandle()						{ unbind(); }

#if 0	// This is redundant, since a LLRootHandle *is* an LLHandle
	LL_INLINE LLHandle<T> getHandle()				{ return LLHandle<T>(*this); }
#endif

	void bind(T* object)
	{
		// Unbind existing tombstone
		if (LLHandle<T>::mTombStone.notNull())
		{
			if (LLHandle<T>::mTombStone->getTarget() == (void*)object)
			{
				return;
			}
			LLHandle<T>::mTombStone->setTarget(NULL);
		}
		// Tombstone reference counted, so no paired delete
		LLHandle<T>::mTombStone = new LLTombStone((void*)object);
	}

	LL_INLINE void unbind()
	{
		LLHandle<T>::mTombStone->setTarget(NULL);
	}
};

// Use this as a mixin for simple classes that need handles and when you do not
// want handles at multiple points of the inheritance hierarchy.

template <typename T>
class LLHandleProvider
{
public:
	LL_INLINE LLHandle<T> getHandle() const
	{
		// Perform lazy binding to avoid small tombstone allocations for handle
		// providers whose handles are never referenced
		mHandle.bind(static_cast<T*>(const_cast<LLHandleProvider<T>*>(this)));
		return mHandle;
	}

	template <typename U>
	LLHandle<U> getDerivedHandle(typename std::enable_if<std::is_convertible<U*, T*>::value>::type* dummy = 0) const
	{
		LLHandle<U> downcast_handle;
		downcast_handle.mTombStone = getHandle().mTombStone;
		return downcast_handle;
	}

protected:
	typedef LLHandle<T> handle_type_t;
	LL_INLINE LLHandleProvider()
	{
		// Provided here to enforce T deriving from LLHandleProvider<T>
	}

private:
	mutable LLRootHandle<T> mHandle;
};

#endif	// LLHANDLE_H
