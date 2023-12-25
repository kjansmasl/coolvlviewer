/**
 * @file llatomic.h
 * @brief Atomic data handling.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLATOMIC_H
#define LL_LLATOMIC_H

#include <atomic>

#include "llpreprocessor.h"
#include "stdtypes.h"

template <typename Type,
		  typename AtomicType = std::atomic<Type> > class LLAtomic
{
public:
	LL_INLINE LLAtomic()				{}
	LL_INLINE LLAtomic(Type x)			{ mData.store(x); }

	LL_INLINE ~LLAtomic() = default;

	LL_INLINE operator const Type()		{ return mData.load(); }
	LL_INLINE Type get() const			{ return mData.load(); }

	LL_INLINE Type operator=(Type x)
	{
		mData.store(x);
		return x;
	}

	LL_INLINE void operator-=(Type x)	{ mData -= x; }
	LL_INLINE void operator+=(Type x)	{ mData += x; }
	LL_INLINE Type operator++(int)		{ return mData++; }
	LL_INLINE Type operator--(int)		{ return mData--; }
	LL_INLINE Type operator++()			{ return ++mData; }
	LL_INLINE Type operator--()			{ return --mData; }

	LL_INLINE Type swap(Type x)			{ return mData.exchange(x); }

private:
	AtomicType mData;
};

typedef LLAtomic<U32>	LLAtomicU32;
typedef LLAtomic<S32>	LLAtomicS32;
typedef LLAtomic<U64>	LLAtomicU64;
typedef LLAtomic<S64>	LLAtomicS64;
typedef LLAtomic<bool>	LLAtomicBool;

#endif // LL_LLATOMIC_H
