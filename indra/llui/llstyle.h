/**
 * @file llstyle.h
 * @brief Text style class
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

#ifndef LL_LLSTYLE_H
#define LL_LLSTYLE_H

#include "llcolor4.h"
#include "llfontgl.h"
#include "llui.h"

class LLFontGL;

class LLStyle : public LLRefCount
{
public:
	LLStyle();
	LLStyle(const LLStyle& style);
	LLStyle(bool is_visible, const LLColor4& color,
			const std::string& font_name);

protected:
	~LLStyle() override = default;

public:
	LLStyle& operator=(const LLStyle& rhs);

	virtual void init(bool is_visible, const LLColor4& color,
					  const std::string& font_name);

	LL_INLINE virtual const LLColor4& getColor() const			{ return mColor; }
	LL_INLINE virtual void setColor(const LLColor4& color)		{ mColor = color; }

	LL_INLINE virtual bool isVisible() const					{ return mVisible; }
	LL_INLINE virtual void setVisible(bool is_visible)			{ mVisible = is_visible; }

	LL_INLINE virtual const std::string& getFontString() const	{ return mFontName; }
	virtual void setFontName(const std::string& fontname);
	LL_INLINE virtual LLFONT_ID getFontID() const				{ return mFontID; }

	LL_INLINE virtual const std::string& getLinkHREF() const	{ return mLink; }
	LL_INLINE virtual void setLinkHREF(const std::string& href)	{ mLink = href; }
	LL_INLINE virtual bool isLink() const						{ return mLink.size(); }

	LL_INLINE virtual LLUIImagePtr getImage() const				{ return mImagep; }
	LL_INLINE virtual void setImage(const LLUUID& src)			{ mImagep = LLUI::getUIImageByID(src); }

	LL_INLINE virtual bool isImage() const						{ return mImageWidth != 0 && mImageHeight != 0; }
	LL_INLINE virtual void setImageSize(S32 width, S32 height)	{ mImageWidth = width; mImageHeight = height; }

	LL_INLINE bool getIsEmbeddedItem() const					{ return mIsEmbeddedItem; }
	LL_INLINE void setIsEmbeddedItem(bool b)					{ mIsEmbeddedItem = b; }

	// Inlined here to make it easier to compare to member data below. - MG
	LL_INLINE bool operator==(const LLStyle& rhs) const
	{
		return mVisible == rhs.isVisible() && mColor == rhs.getColor() &&
			   mFontName == rhs.getFontString()	&&
			   mLink == rhs.getLinkHREF() && mImagep == rhs.mImagep &&
			   mImageHeight == rhs.mImageHeight	&&
			   mImageWidth == rhs.mImageWidth && mItalic == rhs.mItalic &&
			   mBold == rhs.mBold && mUnderline == rhs.mUnderline &&
			   mDropShadow == rhs.mDropShadow &&
			   mIsEmbeddedItem == rhs.mIsEmbeddedItem;
	}

	LL_INLINE bool operator!=(const LLStyle& rhs) const			{ return !(*this == rhs); }

private:
	// Note: the first member variable is 32 bits in order to align on 64 bits
	// for the next variables, counting the 32 bits counter from LLRefCount. HB

	LLFONT_ID   	mFontID;

	std::string		mFontName;
	std::string		mLink;

	LLColor4		mColor;

	LLUIImagePtr	mImagep;

public:
	S32         	mImageWidth;
	S32         	mImageHeight;

public:
	bool        	mItalic;
	bool        	mBold;
	bool        	mUnderline;
	bool			mDropShadow;

private:
	bool			mVisible;
	bool			mIsEmbeddedItem;
};

typedef LLPointer<LLStyle> LLStyleSP;

#endif  // LL_LLSTYLE_H
