/**
 * @file llrect.h
 * @brief A rectangle in GL coordinates, with bottom,left = 0,0
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

#ifndef LL_LLRECT_H
#define LL_LLRECT_H

#include <iostream>

#include "llmath.h"
#include "llsd.h"

// Top > Bottom due to GL coords
template <class Type> class LLRectBase
{
public:
	typedef	Type tCoordType;

	LL_INLINE LLRectBase() noexcept
	:	mLeft(0),
		mTop(0),
		mRight(0),
		mBottom(0)
	{
	}

	LL_INLINE LLRectBase(const LLRectBase& r) noexcept
	:	mLeft(r.mLeft),
		mTop(r.mTop),
		mRight(r.mRight),
		mBottom(r.mBottom)
	{
	}

	LL_INLINE LLRectBase(Type left, Type top, Type right, Type bottom) noexcept
	:	mLeft(left),
		mTop(top),
		mRight(right),
		mBottom(bottom)
	{
	}

	// Allow the use of the default C++11 move constructor and assignation
	LLRectBase(LLRectBase&& other) noexcept = default;
	LLRectBase& operator=(LLRectBase&& other) noexcept = default;

	LLRectBase& operator=(const LLRectBase& other) = default;

	LL_INLINE explicit LLRectBase(const LLSD& sd)
	{
		setValue(sd);
	}

	// Note: follows GL_QUAD conventions: the top and right edges are not
	// considered part of the rect
	LL_INLINE Type getWidth() const				{ return mRight - mLeft; }
	LL_INLINE Type getHeight() const			{ return mTop - mBottom; }
	LL_INLINE Type getCenterX() const			{ return (mLeft + mRight) / 2; }
	LL_INLINE Type getCenterY() const			{ return (mTop + mBottom) / 2; }

	LL_INLINE void setValue(const LLSD& sd)
	{
		mLeft = (Type)sd[0].asInteger();
		mTop = (Type)sd[1].asInteger();
		mRight = (Type)sd[2].asInteger();
		mBottom = (Type)sd[3].asInteger();
	}

	LL_INLINE LLSD getValue() const
	{
		LLSD ret;
		ret[0] = mLeft;
		ret[1] = mTop;
		ret[2] = mRight;
		ret[3] = mBottom;
		return ret;
	}

	// Note: follows GL_QUAD conventions: the top and right edges are not
	// considered part of the rect
	LL_INLINE bool pointInRect(const Type x, const Type y) const
	{
		return  mLeft <= x && x < mRight && mBottom <= y && y < mTop;
	}

	// Note: follows GL_QUAD conventions: the top and right edges are not
	// considered part of the rect
	LL_INLINE bool localPointInRect(const Type x, const Type y) const
	{
		return  0 <= x && x < getWidth() && 0 <= y && y < getHeight();
	}

	LL_INLINE void clampPointToRect(Type& x, Type& y)
	{
		x = llclamp(x, mLeft, mRight);
		y = llclamp(y, mBottom, mTop);
	}

	void clipPointToRect(const Type start_x, const Type start_y,
						 Type& end_x, Type& end_y)
	{
		if (!pointInRect(start_x, start_y))
		{
			return;
		}
		Type clip_x = 0;
		Type clip_y = 0;
		Type delta_x = end_x - start_x;
		Type delta_y = end_y - start_y;

		if (end_x > mRight) clip_x = end_x - mRight;
		if (end_x < mLeft) clip_x = end_x - mLeft;
		if (end_y > mTop) clip_y = end_y - mTop;
		if (end_y < mBottom) clip_y = end_y - mBottom;

		// clip_? and delta_? should have same sign, since starting point is in
		// rect so ratios will be positive
		F32 ratio_x = delta_x ? (F32)clip_x / (F32)delta_x : 0.f;
		F32 ratio_y = delta_y ? (F32)clip_y / (F32)delta_y : 0.f;
		if (ratio_x > ratio_y)
		{
			// Clip along x direction
			end_x -= (Type)clip_x;
			end_y -= (Type)(delta_y * ratio_x);
		}
		else
		{
			// Clip along y direction
			end_x -= (Type)(delta_x * ratio_y);
			end_y -= (Type)clip_y;
		}
	}

	// Note: does NOT follow GL_QUAD conventions: the top and right edges ARE
	// considered part of the rect. Returns true if any part of rect is is
	// inside this LLRect
	LL_INLINE bool overlaps(const LLRectBase& rect) const
	{
		return !(mLeft > rect.mRight || mRight < rect.mLeft ||
				 mBottom > rect.mTop || mTop < rect.mBottom);
	}

	LL_INLINE bool contains(const LLRectBase& rect) const
	{
		return mLeft <= rect.mLeft && mRight >= rect.mRight &&
			   mBottom <= rect.mBottom && mTop >= rect.mTop;
	}

	LL_INLINE LLRectBase& set(Type left, Type top, Type right, Type bottom)
	{
		mLeft = left;
		mTop = top;
		mRight = right;
		mBottom = bottom;
		return *this;
	}

	// Note: follows GL_QUAD conventions: the top and right edges are not
	// considered part of the rect
	LL_INLINE LLRectBase& setOriginAndSize(Type left, Type bottom, Type width,
										   Type height)
	{
		mLeft = left;
		mTop = bottom + height;
		mRight = left + width;
		mBottom = bottom;
		return *this;
	}

	// Note: follows GL_QUAD conventions: the top and right edges are not
	// considered part of the rect
	LL_INLINE LLRectBase& setLeftTopAndSize(Type left, Type top, Type width,
											Type height)
	{
		mLeft = left;
		mTop = top;
		mRight = left + width;
		mBottom = top - height;
		return *this;
	}

	LL_INLINE LLRectBase& setCenterAndSize(Type x, Type y, Type width,
										   Type height)
	{
		// width and height could be odd, so favor top, right with extra pixel
		mLeft = x - width / 2;
		mBottom = y - height / 2;
		mTop = mBottom + height;
		mRight = mLeft + width;
		return *this;
	}

	LL_INLINE LLRectBase& translate(Type horiz, Type vertical)
	{
		mLeft += horiz;
		mRight += horiz;
		mTop += vertical;
		mBottom += vertical;
		return *this;
	}

	LL_INLINE LLRectBase& stretch(Type dx, Type dy)
	{
		mLeft -= dx;
		mRight += dx;
		mTop += dy;
		mBottom -= dy;
		return makeValid();
	}

	LL_INLINE LLRectBase& stretch(Type delta)
	{
		stretch(delta, delta);
		return *this;
	}

	LL_INLINE LLRectBase& makeValid()
	{
		mLeft = llmin(mLeft, mRight);
		mBottom = llmin(mBottom, mTop);
		return *this;
	}

	LL_INLINE bool isValid() const
	{
		return mLeft <= mRight && mBottom <= mTop;
	}

	LL_INLINE bool isEmpty() const
	{
		return mLeft == mRight || mBottom == mTop;
	}

	LL_INLINE bool notEmpty() const
	{
		return !isEmpty();
	}

	LL_INLINE void unionWith(const LLRectBase& other)
	{
		mLeft = llmin(mLeft, other.mLeft);
		mRight = llmax(mRight, other.mRight);
		mBottom = llmin(mBottom, other.mBottom);
		mTop = llmax(mTop, other.mTop);
	}

	LL_INLINE void intersectWith(const LLRectBase& other)
	{
		mLeft = llmax(mLeft, other.mLeft);
		mRight = llmin(mRight, other.mRight);
		mBottom = llmax(mBottom, other.mBottom);
		mTop = llmin(mTop, other.mTop);
		if (mLeft > mRight)
		{
			mLeft = mRight;
		}
		if (mBottom > mTop)
		{
			mBottom = mTop;
		}
	}

	friend std::ostream& operator<<(std::ostream& s, const LLRectBase& rect)
	{
		s << "{ L " << rect.mLeft << " B " << rect.mBottom
		  << " W " << rect.getWidth() << " H " << rect.getHeight() << " }";
		return s;
	}

	LL_INLINE bool operator==(const LLRectBase& b) const
	{
		return mLeft == b.mLeft && mTop == b.mTop && mRight == b.mRight &&
			   mBottom == b.mBottom;
	}

	LL_INLINE bool operator!=(const LLRectBase& b) const
	{
		return mLeft != b.mLeft || mTop != b.mTop || mRight != b.mRight ||
			   mBottom != b.mBottom;
	}

public:
	Type					mLeft;
	Type					mTop;
	Type					mRight;
	Type					mBottom;

	static LLRectBase<Type>	null;
};

template <class Type> LLRectBase<Type> LLRectBase<Type>::null(0, 0, 0, 0);

typedef LLRectBase<S32> LLRect;
typedef LLRectBase<F32> LLRectf;

#endif
