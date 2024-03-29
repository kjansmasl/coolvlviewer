/**
 * @file llfontbitmapcache.h
 * @brief Storage for previously rendered glyphs.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#ifndef LL_LLFONTBITMAPCACHE_H
#define LL_LLFONTBITMAPCACHE_H

#include <vector>

#include "llimagegl.h"

// Maintain a collection of bitmaps containing rendered glyphs.
// Generalizes the single-bitmap logic from LLFontFreetype and LLFontGL.
class LLFontBitmapCache : public LLRefCount
{
public:
	LLFontBitmapCache();

	// This must be called once, before caching any glyphs.
 	void init(S32 num_components, S32 max_char_width, S32 max_char_height);

	void reset();

	void nextOpenPos(S32 width, S32& posX, S32& posY, S32& bitmap_num);

	void destroyGL();

 	LL_INLINE LLImageRaw* getImageRaw(U32 bitmap_num = 0) const
	{
		return bitmap_num < mImageRawVec.size() ? mImageRawVec[bitmap_num].get()
												: NULL;
	}

 	LL_INLINE LLImageGL* getImageGL(U32 bitmap_num = 0) const
	{
		return bitmap_num < mImageGLVec.size() ? mImageGLVec[bitmap_num].get()
											   : NULL;
	}

	LL_INLINE S32 getMaxCharWidth() const			{ return mMaxCharWidth; }
	LL_INLINE S32 getNumComponents() const			{ return mNumComponents; }
	LL_INLINE S32 getBitmapWidth() const			{ return mBitmapWidth; }
	LL_INLINE S32 getBitmapHeight() const			{ return mBitmapHeight; }

private:
	// Note: the first member variable is 32 bits in order to align on 64 bits
	// for the next variables, counting the 32 bits counter from LLRefCount. HB

	S32									mNumComponents;

	std::vector<LLPointer<LLImageRaw> >	mImageRawVec;
	std::vector<LLPointer<LLImageGL> >	mImageGLVec;

	S32									mBitmapWidth;
	S32									mBitmapHeight;
	S32									mBitmapNum;

	S32									mMaxCharWidth;
	S32									mMaxCharHeight;

	S32									mCurrentOffsetX;
	S32									mCurrentOffsetY;
	S32									mCurrentBitmapNum;
};

#endif //LL_LLFONTBITMAPCACHE_H
