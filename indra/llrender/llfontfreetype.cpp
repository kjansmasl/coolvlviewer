/**
 * @file llfontfreetype.cpp
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

#include "linden_common.h"

#include <utility>

#include "llfontfreetype.h"

// Freetype stuff
#include "ft2build.h"
#include "freetype/freetype.h"

#include "llfontgl.h"
#include "llfontbitmapcache.h"
#include "llgl.h"
#include "llimage.h"
#include "llmath.h"
#include "llstring.h"

FT_Render_Mode gFontRenderMode = FT_RENDER_MODE_NORMAL;

LLFontManager* gFontManagerp = NULL;

FT_Library gFTLibrary = NULL;

///////////////////////////////////////////////////////////////////////////////
// LLFontManager class
///////////////////////////////////////////////////////////////////////////////

//static
void LLFontManager::initClass()
{
	if (!gFontManagerp)
	{
		gFontManagerp = new LLFontManager;
	}
}

//static
void LLFontManager::cleanupClass()
{
	delete gFontManagerp;
	gFontManagerp = NULL;
}

LLFontManager::LLFontManager()
{
	int error = FT_Init_FreeType(&gFTLibrary);
	if (error)
	{
		llerrs << "Freetype initialization failure !" << llendl;
	}
}

LLFontManager::~LLFontManager()
{
	FT_Done_FreeType(gFTLibrary);
}

///////////////////////////////////////////////////////////////////////////////
// LLFontFreetype class
///////////////////////////////////////////////////////////////////////////////

LLFontFreetype::LLFontFreetype()
:	mFontBitmapCachep(new LLFontBitmapCache),
	mIsFallback(false),
	mAscender(0.f),
	mDescender(0.f),
	mLineHeight(0.f),
	mFTFace(NULL),
	mRenderGlyphCount(0),
	mAddGlyphCount(0),
	mStyle(0),
	mPointSize(0)
{
}

LLFontFreetype::~LLFontFreetype()
{
	// Clean up freetype libs.
	if (mFTFace)
	{
		FT_Done_Face(mFTFace);
		mFTFace = NULL;
	}

	// Delete glyph info
	mCharGlyphInfoMap.clear();

	// mFontBitmapCachep will be cleaned up by LLPointer destructor.
	// mFallbackFonts cleaned up by LLPointer destructor
}

bool LLFontFreetype::loadFace(const std::string& filename, F32 point_size,
							  F32 vert_dpi, F32 horz_dpi, S32 components,
							  bool is_fallback)
{
	// Do not leak face objects. This is also needed to deal with changed font
	// file names.
	if (mFTFace)
	{
		FT_Done_Face(mFTFace);
		mFTFace = NULL;
	}

	int error = FT_New_Face(gFTLibrary, filename.c_str(), 0, &mFTFace);
	if (error)
	{
		return false;
	}

	error = FT_Select_Charmap(mFTFace, FT_ENCODING_UNICODE);
	if (error)
	{
		// Note: failures *will* happen (and are harmless) for Windows TTF
		// fonts. So, do not spam the log with them... HB
		LL_DEBUGS("Freetype") << "Failure to select Unicode char map for font: "
							  << filename << LL_ENDL;
	}

	mIsFallback = is_fallback;
	// Please, keep the following calculation in this order; while it would be
	// better to use "point_size * vert_dpi / 72.0f" to lower math rounding
	// errors, the latter gives a different result than what viewers are used
	// to give and would mean having to change font vertical justification in
	// the UI code and/or XML menu definitions...
	F32 pixels_per_em = (point_size / 72.f) * vert_dpi; // Size in inches * dpi

	error = FT_Set_Char_Size(mFTFace,					// handle to face object
							 0,							// char_width in 1/64th of pt
							 (S32)(point_size * 64.f),	// char_height in 1/64th of pt
							 (U32)horz_dpi,				// horizontal device res
							 (U32)vert_dpi);			// vertical device res

	if (error)
	{
		// Clean up freetype libs.
		FT_Done_Face(mFTFace);
		mFTFace = NULL;
		return false;
	}

	F32 y_max, y_min, x_max, x_min;
	F32 ems_per_unit = 1.f / mFTFace->units_per_EM;
	F32 pixels_per_unit = pixels_per_em * ems_per_unit;

	// Get size of bbox in pixels
	y_max = mFTFace->bbox.yMax * pixels_per_unit;
	y_min = mFTFace->bbox.yMin * pixels_per_unit;
	x_max = mFTFace->bbox.xMax * pixels_per_unit;
	x_min = mFTFace->bbox.xMin * pixels_per_unit;
	mAscender = mFTFace->ascender * pixels_per_unit;
	mDescender = -mFTFace->descender * pixels_per_unit;
	mLineHeight = mFTFace->height * pixels_per_unit;

	S32 max_char_width = ll_roundp(0.5f + x_max - x_min);
	S32 max_char_height = ll_roundp(0.5f + y_max - y_min);

	mFontBitmapCachep->init(components, max_char_width, max_char_height);

	if (!mFTFace->charmap)
	{
		FT_Set_Charmap(mFTFace, mFTFace->charmaps[0]);
	}

	if (!mIsFallback)
	{
		// Add the default glyph
		addGlyphFromFont(this, 0, 0);
	}

	mName = filename;
	mPointSize = point_size;

	mStyle = LLFontGL::NORMAL;
	if (mFTFace->style_flags & FT_STYLE_FLAG_BOLD)
	{
		mStyle |= LLFontGL::BOLD;
	}

	if (mFTFace->style_flags & FT_STYLE_FLAG_ITALIC)
	{
		mStyle |= LLFontGL::ITALIC;
	}

	return true;
}

F32 LLFontFreetype::getXAdvance(llwchar wch) const
{
	if (!mFTFace)
	{
		return 0.f;
	}

	// Return existing info only if it is current
	LLFontGlyphInfo* gi = getGlyphInfo(wch);
	if (gi)
	{
		return gi->mXAdvance;
	}

	glyph_info_map_t::iterator it = mCharGlyphInfoMap.find((llwchar)0);
	if (it != mCharGlyphInfoMap.end())
	{
		return it->second.mXAdvance;
	}

	// Last ditch fallback - no glyphs defined at all.
	return (F32)mFontBitmapCachep->getMaxCharWidth();
}

F32 LLFontFreetype::getXKerning(const LLFontGlyphInfo* left_glyph_info,
								const LLFontGlyphInfo* right_glyph_info) const
{
	if (!mFTFace)
	{
		return 0.f;
	}

	U32 left_glyph = left_glyph_info ? left_glyph_info->mGlyphIndex : 0;
	U32 right_glyph = right_glyph_info ? right_glyph_info->mGlyphIndex : 0;
	U64 kerning_key = right_glyph;
	kerning_key |= U64(left_glyph) << 32;
	kerning_cache_map_t::iterator it = mKerningCache.find(kerning_key);
	if (it != mKerningCache.end())
	{
		return it->second;
	}

	FT_Vector delta;
	delta.x = delta.y = 0;
	if (FT_HAS_KERNING(mFTFace))
	{
		FT_Get_Kerning(mFTFace, left_glyph, right_glyph, ft_kerning_unfitted,
					   &delta);
	}

	constexpr F32 k = 1.f / 64.f;
#if LL_KERN_USING_FT_SIDE_BREARING
	F32 ret = delta.x;
	if (FT_IS_SCALABLE(mFTFace))
	{
		if (right_glyph_info)
		{
			ret += right_glyph_info->mLeftSideBearingDelta;
		}
		if (left_glyph_info)
		{
			ret -= left_glyph_info->mRightSideBearingDelta;
		}
		ret = llfloor((ret + 32.f) * k);
	}
#else
	F32 ret = delta.x * k;
#endif

	mKerningCache[kerning_key] = ret;
	return ret;
}

bool LLFontFreetype::hasGlyph(const llwchar wch) const
{
	llassert(!mIsFallback);
	return mCharGlyphInfoMap.count(wch) != 0;
}

LLFontGlyphInfo* LLFontFreetype::addGlyph(llwchar wch) const
{
	if (!mFTFace)
	{
		return NULL;
	}

	llassert(!mIsFallback);
	//lldebugs << "Adding new glyph for " << wch << " to font" << llendl;

	FT_UInt glyph_index;

	// Initialize char to glyph map
	glyph_index = FT_Get_Char_Index(mFTFace, wch);
	if (glyph_index == 0)
	{
		// Try looking it up in the backup Unicode font
		//llinfos << "Trying to add glyph from fallback font!" << llendl;
		for (font_vector_t::const_iterator iter = mFallbackFonts.begin(),
										   end = mFallbackFonts.end();
			 iter != end; ++iter)
		{
			glyph_index = FT_Get_Char_Index((*iter)->mFTFace, wch);
			if (glyph_index)
			{
				return addGlyphFromFont(*iter, wch, glyph_index);
			}
		}
	}

	if (!mCharGlyphInfoMap.count(wch))
	{
		return addGlyphFromFont(this, wch, glyph_index);
	}

	return NULL;
}

LLFontGlyphInfo* LLFontFreetype::addGlyphFromFont(const LLFontFreetype* fontp,
												  llwchar wch,
												  U32 glyph_index) const
{
	if (!mFTFace)
	{
		return NULL;
	}

	llassert(!mIsFallback);
	fontp->renderGlyph(glyph_index);
	S32 width = fontp->mFTFace->glyph->bitmap.width;
	S32 height = fontp->mFTFace->glyph->bitmap.rows;

	S32 pos_x, pos_y;
	S32 bitmap_num;
	mFontBitmapCachep->nextOpenPos(width, pos_x, pos_y, bitmap_num);
	++mAddGlyphCount;

	// Convert these from 26.6 units to float pixels.
	constexpr F32 k = 1.f / 64.f;
	F32 x_advance = fontp->mFTFace->glyph->advance.x * k;
	F32 y_advance = fontp->mFTFace->glyph->advance.y * k;
	// C++11 is not the most limpid language... This whole gymnastic with
	// piecewise_construct and forward_as_tuple is here to ensure that
	// LLFontGlyphInfo is constructed emplace. HB
	glyph_info_map_t::iterator it =
		mCharGlyphInfoMap.emplace(std::piecewise_construct,
								  std::forward_as_tuple(wch),
								  std::forward_as_tuple(wch, bitmap_num,
									pos_x, pos_y, width, height,
									fontp->mFTFace->glyph->bitmap_left,
									fontp->mFTFace->glyph->bitmap_top,
#if LL_KERN_USING_FT_SIDE_BREARING
									fontp->mFTFace->glyph->lsb_delta,
									fontp->mFTFace->glyph->rsb_delta,
#endif
									x_advance, y_advance)).first;

	if (fontp->mFTFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO ||
		fontp->mFTFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
	{
		U8* buffer_data = fontp->mFTFace->glyph->bitmap.buffer;
		S32 buffer_row_stride = fontp->mFTFace->glyph->bitmap.pitch;
		U8* tmp_graydata = NULL;

		if (fontp->mFTFace->glyph->bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
		{
			// Need to expand 1-bit bitmap to 8-bit graymap.
			tmp_graydata = new U8[width * height];
			S32 xpos, ypos;
			for (ypos = 0; ypos < height; ++ypos)
			{
				S32 bm_row_offset = buffer_row_stride * ypos;
				for (xpos = 0; xpos < width; ++xpos)
				{
					U32 bm_col_offsetbyte = xpos / 8;
					U32 bm_col_offsetbit = 7 - (xpos % 8);
					U32 bit = !!(buffer_data[bm_row_offset + bm_col_offsetbyte] &
								 (1 << bm_col_offsetbit));
					tmp_graydata[width * ypos + xpos] = 255 * bit;
				}
			}
			// use newly-built graymap.
			buffer_data = tmp_graydata;
			buffer_row_stride = width;
		}

		switch (mFontBitmapCachep->getNumComponents())
		{
			case 1:
				mFontBitmapCachep->getImageRaw(bitmap_num)->setSubImage(pos_x,
																		pos_y,
																		width,
																		height,
																		buffer_data,
																		buffer_row_stride,
																		true);
				break;

			case 2:
				setSubImageLuminanceAlpha(pos_x, pos_y, bitmap_num,
										  width, height,
										  buffer_data, buffer_row_stride);
				break;

			default:
				break;
		}

		if (tmp_graydata)
		{
			delete[] tmp_graydata;
		}
	}
	else
	{
		// We do not know how to handle this pixel format from Freetype; omit
		// it from the font-image.
		llwarns_once << "Unknown pixel format for font: "
					 << fontp->mName << ". Will not render..." << llendl;
	}

	LLImageGL* image_gl = mFontBitmapCachep->getImageGL(bitmap_num);
	LLImageRaw* image_raw = mFontBitmapCachep->getImageRaw(bitmap_num);
	if (image_gl && image_raw)
	{
		image_gl->setSubImage(image_raw, 0, 0, image_gl->getWidth(),
							  image_gl->getHeight());
	}
	else
	{
		llwarns << "Failed to add glyph image for character: " << std::hex
				<< (S32)wch << std::dec << " !  Out of memory ?" << llendl;
	}

	return &it->second;
}

LLFontGlyphInfo* LLFontFreetype::getGlyphInfo(llwchar wch) const
{
	glyph_info_map_t::iterator iter = mCharGlyphInfoMap.find(wch);
	if (iter != mCharGlyphInfoMap.end())
	{
		return &iter->second;
	}

	// This glyph does not yet exist, so render it and return the result
	return addGlyph(wch);
}

void LLFontFreetype::renderGlyph(U32 glyph_index) const
{
	if (mFTFace)
	{
#if 1
		FT_Int32 load_flags = FT_LOAD_DEFAULT;
#else
		FT_Int32 load_flags = FT_LOAD_FORCE_AUTOHINT;
#endif
		FT_Error error = FT_Load_Glyph(mFTFace, glyph_index, load_flags);
		if (error)
		{
			LL_DEBUGS("Freetype") << "Error loading glyph, index: "
								  << glyph_index << LL_ENDL;
			glyph_index = FT_Get_Char_Index(mFTFace, (FT_ULong)'?');
			FT_Load_Glyph(mFTFace, glyph_index, load_flags);
		}

		error = FT_Render_Glyph(mFTFace->glyph, gFontRenderMode);
		if (error)
		{
			LL_DEBUGS("Freetype") << "Error rendering glyph, index: "
								  << glyph_index << LL_ENDL;
			llassert(false);
		}

		++mRenderGlyphCount;
	}
}

void LLFontFreetype::reset(F32 vert_dpi, F32 horz_dpi)
{
	resetBitmapCache();
	loadFace(mName, mPointSize, vert_dpi ,horz_dpi,
			 mFontBitmapCachep->getNumComponents(), mIsFallback);
	if (!mIsFallback)
	{
		// This is the head of the list - need to rebuild ourself and all fallbacks.
		if (mFallbackFonts.empty())
		{
			llwarns << "No fallback fonts present" << llendl;
		}
		else
		{
			for (font_vector_t::iterator it = mFallbackFonts.begin();
				 it != mFallbackFonts.end(); ++it)
			{
				(*it)->reset(vert_dpi, horz_dpi);
			}
		}
	}
}

void LLFontFreetype::resetBitmapCache()
{
	mCharGlyphInfoMap.clear();
	mFontBitmapCachep->reset();

	if (!mIsFallback)
	{
		// Add the empty glyph
		addGlyphFromFont(this, 0, 0);
	}
}

void LLFontFreetype::destroyGL()
{
	mFontBitmapCachep->destroyGL();
}

void LLFontFreetype::setSubImageLuminanceAlpha(U32 x, U32 y, U32 bitmap_num,
											   U32 width, U32 height,
											   U8* data, S32 stride) const
{
	LLImageRaw* image_raw = mFontBitmapCachep->getImageRaw(bitmap_num);
	if (!image_raw)
	{
		return;
	}

	llassert(!mIsFallback);
	llassert(image_raw->getComponents() == 2);

	U8* target = image_raw->getData();

	if (!data || !target)
	{
		return;
	}

	if (stride == 0)
	{
		stride = width;
	}

	U32 target_width = image_raw->getWidth();
	for (U32 i = 0; i < height; ++i)
	{
		U32 to_offset = (y + i) * target_width + x;
		U32 from_offset = (height - 1 - i) * stride;
		for (U32 j = 0; j < width; ++j)
		{
			*(target + to_offset++ * 2 + 1) = *(data + from_offset++);
		}
	}
}
