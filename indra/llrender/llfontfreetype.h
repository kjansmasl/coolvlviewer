/**
 * @file llfontfreetype.h
 * @brief Font library wrapper
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

#ifndef LL_FONTFREETYPE_H
#define LL_FONTFREETYPE_H

#include "hbfastmap.h"
#include "llfontbitmapcache.h"
#include "llimagegl.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llstl.h"

// Disabled since this causes issues with large input lines. HB
#define LL_KERN_USING_FT_SIDE_BREARING 0

// Hack. FT_Face is just a typedef for a pointer to a struct, but there is no
// simple forward declarations file for FreeType and the main include file is
// 200K. We forward declare the struct here. JC
struct FT_FaceRec_;
typedef struct FT_FaceRec_* LLFT_Face;

class LLFontManager
{
public:
	static void initClass();
	static void cleanupClass();

public:
	LLFontManager();
	~LLFontManager();
};

struct LLFontGlyphInfo
{
	LL_INLINE LLFontGlyphInfo(U32 index, S32 bitmap_num,
							  S32 pos_x, S32 pos_y, S32 width, S32 height,
							  S32 x_bearing, S32 y_bearing,
#if LL_KERN_USING_FT_SIDE_BREARING
							  S32 left_bearing_delta, S32 right_bearing_delta,
#endif
							  F32 x_advance, F32 y_advance)
	:	mGlyphIndex(index),
		mBitmapNum(bitmap_num),
		mXBitmapOffset(pos_x),
		mYBitmapOffset(pos_y),
		mWidth(width),
		mHeight(height),
		mXBearing(x_bearing),
		mYBearing(y_bearing),
#if LL_KERN_USING_FT_SIDE_BREARING
		mLeftSideBearingDelta(left_bearing_delta),
		mRightSideBearingDelta(right_bearing_delta),
#endif
		mXAdvance(x_advance),
		mYAdvance(y_advance)
	{
	}

	U32 mGlyphIndex;

	// Which bitmap in the bitmap cache contains this glyph
	S32 mBitmapNum;

	// Metrics in pixels
	S32 mWidth;
	S32 mHeight;
	F32 mXAdvance;
	F32 mYAdvance;

	// Information for actually rendering
	S32 mXBitmapOffset;			// Offset to the origin in the bitmap
	S32 mYBitmapOffset;			// Offset to the origin in the bitmap
	S32 mXBearing;				// Distance from baseline to left in pixels
	S32 mYBearing;				// Distance from baseline to top in pixels
#if LL_KERN_USING_FT_SIDE_BREARING
	S32 mLeftSideBearingDelta;
	S32 mRightSideBearingDelta;
#endif
};

extern LLFontManager* gFontManagerp;

class LLFontFreetype final : public LLRefCount
{
protected:
	LOG_CLASS(LLFontFreetype);

public:
	LLFontFreetype();
	~LLFontFreetype() override;

	// is_fallback should be true for fallback fonts that aren't used
	// to render directly (Unicode backup, primarily)
	bool loadFace(const std::string& filename, F32 point_size, F32 vert_dpi,
				  F32 horz_dpi, S32 components, bool is_fallback);

	typedef std::vector<LLPointer<LLFontFreetype> > font_vector_t;

	LL_INLINE void setFallbackFonts(const font_vector_t& font)
	{
		mFallbackFonts = font;
	}

	const font_vector_t& getFallbackFonts() const	{ return mFallbackFonts; }

	// Global font metrics - in units of pixels
	LL_INLINE F32 getLineHeight() const				{ return mLineHeight; }
	LL_INLINE F32 getAscenderHeight() const			{ return mAscender; }
	LL_INLINE F32 getDescenderHeight() const		{ return mDescender; }

// For a lowercase "g":
//
//	------------------------------
//	                     ^     ^
//						 |     |
//				xxx x    |Ascender
//	           x   x     v     |
//	---------   xxxx-------------- Baseline
//	^		       x	       |
//  | Descender    x           |
//	v			xxxx           |LineHeight
//  -----------------------    |
//                             v
//	------------------------------

	enum
	{
		FIRST_CHAR = 32,
		NUM_CHARS = 127 - 32,
		LAST_CHAR_BASIC = 127,

		// Need full 8-bit ascii range for spanish
		NUM_CHARS_FULL = 255 - 32,
		LAST_CHAR_FULL = 255
	};

	LLFontGlyphInfo* getGlyphInfo(llwchar wch) const;

	LL_INLINE F32 getXAdvance(const LLFontGlyphInfo* glyph) const
	{
		return mFTFace && glyph ? glyph->mXAdvance : 0.f;
	}

	F32 getXAdvance(llwchar wc) const;

	F32 getXKerning(const LLFontGlyphInfo* left_glyph_info,
					const LLFontGlyphInfo* right_glyph_info) const;

	// Gets the kerning between the two characters
	LL_INLINE F32 getXKerning(llwchar char_left, llwchar char_right) const
	{
		return getXKerning(getGlyphInfo(char_left), getGlyphInfo(char_right));
	}

	void reset(F32 vert_dpi, F32 horz_dpi);

	void destroyGL();

	LL_INLINE const std::string& getName() const	{ return mName; }

	LL_INLINE const LLPointer<LLFontBitmapCache> getFontBitmapCache() const
	{
		return mFontBitmapCachep;
	}

	LL_INLINE void setStyle(U8 style)				{ mStyle = style; }
	LL_INLINE U8 getStyle() const					{ return mStyle; }

private:
	void resetBitmapCache();
	void setSubImageLuminanceAlpha(U32 x, U32 y, U32 bitmap_num,
								   U32 width, U32 height,
								   U8* data, S32 stride = 0) const;
	// Has a glyph for this character
	bool hasGlyph(const llwchar wch) const;
	// Add a new character to the font if necessary
	LLFontGlyphInfo* addGlyph(llwchar wch) const;
	// Add a glyph from this font to the other (returns the glyph_index, NULL
	// if not found)
	LLFontGlyphInfo* addGlyphFromFont(const LLFontFreetype* fontp,
									  llwchar wch, U32 glyph_index) const;

	void renderGlyph(U32 glyph_index) const;

private:
	std::string								mName;

	mutable LLPointer<LLFontBitmapCache>	mFontBitmapCachep;

	// A list of fallback fonts to look for glyphs in (for Unicode chars)
	font_vector_t							mFallbackFonts;

	// Note: using a safe map so that pointers to values (glyph info) do not
	// change whenever a new glyph is inserted and mandates a map rehash. HB
	typedef safe_hmap<llwchar, LLFontGlyphInfo> glyph_info_map_t;
	// Information about glyph location in bitmap
	mutable glyph_info_map_t				mCharGlyphInfoMap;

	typedef flat_hmap<U64, F32> kerning_cache_map_t;
	mutable kerning_cache_map_t				mKerningCache;

	LLFT_Face								mFTFace;

	F32										mPointSize;
	F32										mAscender;
	F32										mDescender;
	F32										mLineHeight;

	mutable S32								mRenderGlyphCount;
	mutable S32								mAddGlyphCount;

	U8										mStyle;

	bool									mIsFallback;
};

#endif // LL_FONTFREETYPE_H
