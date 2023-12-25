/**
 * @file llfontgl.h
 * @author Doug Soo
 * @brief Wrapper around FreeType
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

#ifndef LL_LLFONTGL_H
#define LL_LLFONTGL_H

#include "llcoord.h"
#include "hbfastmap.h"
#include "llfontregistry.h"
#include "llimagegl.h"

class LLColor4;
class LLFontDescriptor;
class LLFontFreetype;
class LLGLTexture;

// Structure used to store previously requested fonts.
class LLFontRegistry;

// IMPORTANT: if you change this, also change LLFontGL::getFont() accordingly !
enum LLFONT_ID
{
	LLFONT_SANSSERIF,
	LLFONT_SANSSERIF_SMALL,
	LLFONT_SANSSERIF_BIG,
	LLFONT_SMALL,
};

class LLFontGL
{
	friend class LLFontRegistry;
	friend class LLTextBillboard;
	friend class LLHUDText;

protected:
	LOG_CLASS(LLFontGL);

public:
	enum HAlign
	{
		// Horizontal location of x, y coord to render.
		LEFT = 0,		// Left align
		RIGHT = 1,		// Right align
		HCENTER = 2,	// Center
	};

	enum VAlign
	{
		// Vertical location of x, y coord to render.
		TOP = 3,		// Top align
		VCENTER = 4,	// Center
		BASELINE = 5,	// Baseline
		BOTTOM = 6		// Bottom
	};

	enum StyleFlags : U32
	{
		// Text style to render. May be combined (these are bit flags)
		NORMAL = 0,
		BOLD = 1,
		ITALIC = 2,
		UNDERLINE = 4,
		DROP_SHADOW = 8,
		DROP_SHADOW_SOFT = 16
	};

	// Takes a string with potentially several flags, i.e. "NORMAL|BOLD|ITALIC"
	static U8 getStyleFromString(const std::string& style);

	LLFontGL();
	~LLFontGL();

	LLFontGL(const LLFontGL&) = delete;
	LLFontGL& operator=(const LLFontGL&) = delete;

	void init(); // Internal init, or reinitialization

	// Reset a font after GL cleanup. ONLY works on an already loaded font.
	void reset();

	void destroyGL();

	bool loadFace(const std::string& filename, F32 point_size,
				  F32 vert_dpi, F32 horz_dpi, S32 components,
				  bool is_fallback);

	S32 render(const LLWString& text, S32 begin_offset, F32 x, F32 y,
			   const LLColor4& color,
			   HAlign halign = LEFT, VAlign valign = BASELINE,
			   U8 style = NORMAL,
			   S32 max_chars = S32_MAX, S32 max_pixels = S32_MAX,
			   F32* right_x = NULL,
			   bool use_embedded = false,
			   bool use_ellipses = false) const;

	S32 render(const LLWString& text, S32 begin_offset, F32 x, F32 y,
			   const LLColor4& color) const;

	// renderUTF8 does a conversion, so it is slower !
	S32 renderUTF8(const std::string& text, S32 begin_offset, F32 x, F32 y,
				   const LLColor4& color, HAlign halign, VAlign valign,
				   U8 style, S32 max_chars, S32 max_pixels, F32* right_x,
				   bool use_ellipses) const;

	S32 renderUTF8(const std::string& text, S32 begin_offset, S32 x, S32 y,
				   const LLColor4& color) const;

	S32 renderUTF8(const std::string& text, S32 begin_offset, S32 x, S32 y,
				   const LLColor4& color,
				   HAlign halign, VAlign valign, U8 style = NORMAL) const;

	// Font metrics - override for LLFontFreetype that returns units of virtual
	// pixels
	F32 getAscenderHeight() const;
	F32 getDescenderHeight() const;
	F32 getLineHeight() const;

	S32 getWidth(const std::string& utf8text) const;
	S32 getWidth(const llwchar* wchars) const;
	S32 getWidth(const std::string& utf8text, S32 offset, S32 max_chars) const;
	S32 getWidth(const llwchar* wchars, S32 offset, S32 max_chars,
				 bool use_embedded = false) const;

	F32 getWidthF32(const std::string& utf8text) const;
	F32 getWidthF32(const llwchar* wchars) const;
	F32 getWidthF32(const std::string& text, S32 offset, S32 max_chars) const;
	F32 getWidthF32(const llwchar* wchars, S32 offset, S32 max_chars,
					bool use_embedded = false) const;

	// The following are called often, frequently with large buffers, so do not
	// use a string interface

	// Returns the max number of complete characters from text (up to
	// max_chars) that can be drawn in max_pixels
	S32	maxDrawableChars(const llwchar* wchars, F32 max_pixels,
						 S32 max_chars = S32_MAX,
						 bool end_on_word_boundary = false,
						 bool use_embedded = false,
						 F32* drawn_pixels = NULL) const;

	// Returns the index of the first complete characters from text that can be
	// drawn in max_pixels given that the character at start_pos should be the
	// last character (or as close to last as possible).
	S32	firstDrawableChar(const llwchar* wchars, F32 max_pixels,
						  S32 text_len, S32 start_pos = S32_MAX,
						  S32 max_chars = S32_MAX) const;

	// Returns the index of the character closest to pixel position x (ignoring
	// text to the right of max_pixels and max_chars)
	S32 charFromPixelOffset(const llwchar* wchars, S32 char_offset, F32 x,
							F32 max_pixels = F32_MAX, S32 max_chars = S32_MAX,
							bool round = true, bool embedded = false) const;

	LL_INLINE const LLFontDescriptor& getFontDesc() const
	{
		return mFontDescriptor;
	}

	void generateASCIIglyphs();

	static void initClass(F32 screen_dpi, F32 x_scale, F32 y_scale,
						  const std::vector<std::string>& xui_paths,
						  bool create_gl_textures = true);

	// Load sans-serif, sans-serif-small, etc.
	// Slow, requires multiple seconds to load fonts.
	static bool loadDefaultFonts();
	static void	destroyDefaultFonts();
	static void destroyAllGL();

	LLImageGL* getImageGL() const;

	void addEmbeddedChar(llwchar wc, LLGLTexture* image,
						 const std::string& label) const;
	void addEmbeddedChar(llwchar wc, LLGLTexture* image,
						 const LLWString& label) const;
	void removeEmbeddedChar(llwchar wc) const;

	static std::string nameFromFont(const LLFontGL* fontp);

	static const std::string& nameFromHAlign(LLFontGL::HAlign align);
	static LLFontGL::HAlign hAlignFromName(const std::string& name);

	static const std::string& nameFromVAlign(LLFontGL::VAlign align);
	static LLFontGL::VAlign vAlignFromName(const std::string& name);

	static void setFontDisplay(bool flag)				{ sDisplayFont = flag; }

	static LLFontGL* getFontMonospace();
	static LLFontGL* getFontSansSerifSmall();
	static LLFontGL* getFontSansSerif();
	static LLFontGL* getFontSansSerifBig();
	static LLFontGL* getFontSansSerifHuge();
	static LLFontGL* getFontSansSerifBold();
	static LLFontGL* getFont(const LLFontDescriptor& desc,
							 bool normalize = true);
	// Only to try and use other fonts than the default ones. HB
	static LLFontGL* getFont(const char* name, const char* size = NULL,
							 U8 style = 0);
	// Use with names like "SANSSERIF_SMALL"
	static LLFontGL* getFont(const std::string& name);
	// Use with font ids like LLFONT_SANSSERIF_SMALL
	static LLFontGL* getFont(S32 font_id);

	// Fallback to sans serif as default font
	LL_INLINE static LLFontGL* getFontDefault()			{ return getFontSansSerif(); }

	static void setUseBatchedRender(bool enable)		{ sUseBatchedRender = enable; }

private:
	struct embedded_data_t
	{
		embedded_data_t(LLImageGL* image, const LLWString& label)
		:	mImage(image),
			mLabel(label)
		{
		}

		LLPointer<LLImageGL> mImage;
		LLWString			 mLabel;
	};

	// New, optimized routines for texts without embedded data:
	S32 newrender(const LLWString& wstr, S32 begin_offset, F32 x, F32 y,
				  const LLColor4& color, HAlign halign, VAlign valign,
				  U8 style, S32 max_chars, S32 max_pixels, F32* right_x,
				  bool use_ellipses) const;

	void renderQuad(LLVector3* vertex_out, LLVector2* uv_out,
					LLColor4U* colors_out, const LLRectf& screen_rect,
					const LLRectf& uv_rect, const LLColor4U& color,
					F32 slant_amt) const;
	void drawGlyph(S32& glyph_count, LLVector3* vertex_out, LLVector2* uv_out,
				   LLColor4U* colors_out, const LLRectf& screen_rect,
				   const LLRectf& uv_rect, const LLColor4U& color, U8 style,
				   F32 drop_shadow_fade) const;

	// Old, slower routines for texts with embedded data:
	// *TODO: change the UI code to allow getting fully rid of these
	S32 oldrender(const LLWString& wstr, S32 begin_offset, F32 x, F32 y,
				  const LLColor4& color, HAlign halign, VAlign valign,
				  U8 style, S32 max_chars, S32 max_pixels, F32* right_x,
				  bool use_embedded, bool use_ellipses) const;

	void renderQuad(const LLRectf& screen_rect, const LLRectf& uv_rect,
					F32 slant_amt) const;
	void drawGlyph(const LLRectf& screen_rect, const LLRectf& uv_rect,
				   const LLColor4& color, U8 style,
				   F32 drop_shadow_fade) const;
	const embedded_data_t* getEmbeddedCharData(llwchar wch) const;
	F32 getEmbeddedCharAdvance(const embedded_data_t* ext_data) const;

public:
	static LLColor4				sShadowColor;
	// Converted value of sShadowColor, for speed
	static LLColor4U			sShadowColorU;

	static LLCoordGL			sCurOrigin;
	static F32					sCurDepth;
	static F32					sVertDPI;
	static F32					sHorizDPI;
	static F32					sScaleX;
	static F32					sScaleY;
	static bool					sDisplayFont;

	static std::vector<std::pair<LLCoordGL, F32> > sOriginStack;

private:
	LLFontDescriptor			mFontDescriptor;
	LLPointer<LLFontFreetype>	mFontFreetype;

	typedef fast_hmap<llwchar, embedded_data_t> embedded_map_t;
	mutable embedded_map_t		mEmbeddedChars;

	// Registry holds all instantiated fonts:
	static LLFontRegistry*		sFontRegistry;

	static bool					sUseBatchedRender;
};

#endif
