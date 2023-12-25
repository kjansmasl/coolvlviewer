/**
 * @file lltextbox.h
 * @brief A single text item display
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

#ifndef LL_LLTEXTBOX_H
#define LL_LLTEXTBOX_H

#include "llpreprocessor.h"
#include "llstring.h"
#include "lluictrl.h"
#include "lluistring.h"
#include "llcolor4.h"

class LLTextBox : public LLUICtrl
{
public:
	// By default, follows top and left and is mouse-opaque. If no text,
	// text = name. If no font, uses default system font.
	LLTextBox(const std::string& name,
			  const LLRect& rect,
			  const std::string& text,
			  const LLFontGL* font = NULL,
			  bool mouse_opaque = true);

	// Construct a textbox which handles word wrapping for us.
	LLTextBox(const std::string& name,
			  const std::string& text,
			  F32 max_width = 200,
			  const LLFontGL* font = NULL,
			  bool mouse_opaque = true);

	// "Simple" constructors for text boxes that have the same name and label
	// *TO BE DEPRECATED*
	LLTextBox(const std::string& name_and_label,
			  const LLRect& rect);

	// Consolidate common member initialization
	void initDefaults();

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   class LLUICtrlFactory* factory);

	void draw() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;

	LL_INLINE void setColor(const LLColor4& c) override		{ mTextColor = c; }
	LL_INLINE void setDisabledColor(const LLColor4& c)		{ mDisabledColor = c; }
	LL_INLINE void setBackgroundColor(const LLColor4& c)	{ mBackgroundColor = c; }
	LL_INLINE void setBorderColor(const LLColor4& c)		{ mBorderColor = c; }

	LL_INLINE void setHoverColor(const LLColor4& c)			{ mHoverColor = c; }
	LL_INLINE void setHoverActive(bool active)				{ mHoverActive = active; }

	void setText(const std::string& text);
	// max_width = -1 means use existing control width
	void setWrappedText(const std::string& text, F32 max_width = -1.f);

	LL_INLINE void setUseEllipses(bool b)					{ mUseEllipses = b; }
	LL_INLINE void setBackgroundVisible(bool b)				{ mBackgroundVisible = b; }
	LL_INLINE void setBorderVisible(bool b)					{ mBorderVisible = b; }
	LL_INLINE void setFontStyle(U8 style)					{ mFontStyle = style; }
	LL_INLINE void setBorderDropshadowVisible(bool b)		{ mBorderDropShadowVisible = b; }
	LL_INLINE void setHPad(S32 pixels)						{ mHPad = pixels; }
	LL_INLINE void setVPad(S32 pixels)						{ mVPad = pixels; }
	LL_INLINE void setRightAlign()							{ mHAlign = LLFontGL::RIGHT; }
	LL_INLINE void setHAlign(LLFontGL::HAlign align)		{ mHAlign = align; }

	// Mouse down and up within text area
	LL_INLINE void setClickedCallback(void (*cb)(void*), void* data = NULL)
	{
		mClickedCallback = cb;
		mCallbackUserData = data;
	}

	LL_INLINE const LLFontGL* getFont() const				{ return mFontGL; }

	void reshapeToFitText();

	LL_INLINE const std::string& getText() const			{ return mText.getString(); }
	S32 getTextPixelWidth();
	S32 getTextPixelHeight();

	LL_INLINE void setValue(const LLSD& value) override		{ setText(value.asString()); }
	LL_INLINE LLSD getValue() const override				{ return LLSD(getText()); }

	bool setTextArg(const std::string& key, const std::string& text) override;

private:
	void setLineLengths();
	void drawText(S32 x, S32 y, const LLColor4& color);

private:
	LLUIString			mText;
	const LLFontGL*		mFontGL;
	LLColor4			mTextColor;
	LLColor4			mDisabledColor;
	LLColor4			mBackgroundColor;
	LLColor4			mBorderColor;
	LLColor4			mHoverColor;

	S32					mLineSpacing;

	S32					mHPad;
	S32					mVPad;
	LLFontGL::HAlign	mHAlign;
	LLFontGL::VAlign	mVAlign;

	void				(*mClickedCallback)(void* data);
	void*				mCallbackUserData;

	bool				mHoverActive;
	bool				mHasHover;
	bool				mBackgroundVisible;
	bool				mBorderVisible;

	bool				mBorderDropShadowVisible;
	bool				mUseEllipses;
	U8					mFontStyle;					// style bit flags for font

	std::vector<S32>	mLineLengthList;
};

#endif
