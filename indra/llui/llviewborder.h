/**
 * @file llviewborder.h
 * @brief A customizable decorative border.  Does not interact with mouse events.
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

#ifndef LL_LLVIEWBORDER_H
#define LL_LLVIEWBORDER_H

#include "llview.h"

class LLViewBorder : public LLView
{
public:
	enum EBevel { BEVEL_IN, BEVEL_OUT, BEVEL_BRIGHT, BEVEL_NONE };
	enum EStyle { STYLE_LINE, STYLE_TEXTURE };

	LLViewBorder(const std::string& name,
				 const LLRect& rect,
				 EBevel bevel = BEVEL_OUT,
				 EStyle style = STYLE_LINE,
				 S32 width = 1);

	// LLUICtrl functionality

	LL_INLINE void setValue(const LLSD& val) override		{ setRect(LLRect(val)); }

	// LLView functionality
	LL_INLINE bool isCtrl() const override					{ return false; }
	void draw() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static  LLView* fromXML(LLXMLNodePtr node, LLView* parent,
							class LLUICtrlFactory* factory);
	static bool getBevelFromAttribute(LLXMLNodePtr node,
									  LLViewBorder::EBevel& bevel_style);

	LL_INLINE void setBorderWidth(S32 width)				{ mBorderWidth = width; }
	LL_INLINE S32 getBorderWidth() const					{ return mBorderWidth; }
	LL_INLINE void setBevel(EBevel bevel)					{ mBevel = bevel; }
	LL_INLINE EBevel getBevel() const						{ return mBevel; }

	void setColors(const LLColor4& shadow_dark,
				   const LLColor4& highlight_light);
	void setColorsExtended(const LLColor4& shadow_light,
						   const LLColor4& shadow_dark,
				  		   const LLColor4& highlight_light,
						   const LLColor4& highlight_dark);

	void setTexture(const class LLUUID& image_id);

	LL_INLINE LLColor4 getHighlightLight()					{ return mHighlightLight; }
	LL_INLINE LLColor4 getShadowDark()						{ return mHighlightDark; }

	LL_INLINE EStyle getStyle() const						{ return mStyle; }

	LL_INLINE void setKeyboardFocusHighlight(bool b)		{ mHasKeyboardFocus = b; }

private:
	void drawOnePixelLines();
	void drawTwoPixelLines();

private:
	LLColor4		mHighlightLight;
	LLColor4		mHighlightDark;
	LLColor4		mShadowLight;
	LLColor4		mShadowDark;
	LLColor4		mBackgroundColor;
	S32				mBorderWidth;
	LLUIImagePtr	mTexture;
	EBevel			mBevel;
	const EStyle	mStyle;
	bool			mHasKeyboardFocus;
};

#endif // LL_LLVIEWBORDER_H
