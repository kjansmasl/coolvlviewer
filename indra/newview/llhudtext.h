/**
 * @file llhudtext.h
 * @brief LLHUDText class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLHUDTEXT_H
#define LL_LLHUDTEXT_H

#include <set>
#include <vector>

#include "llfontgl.h"
#include "llrect.h"
#include "llcolor4u.h"
#include "llvector2.h"

#include "llhudobject.h"

// Renders a 2D text billboard floating at the location specified.
class LLDrawable;
class LLHUDText;

class LLHUDText : public LLHUDObject
{
	friend class LLHUDObject;

protected:
	LOG_CLASS(LLHUDText);

	class LLHUDTextSegment
	{
	public:
		LLHUDTextSegment(const LLWString& text,
						 const LLFontGL::StyleFlags style,
						 const LLColor4& color)
		:	mLastFont(NULL),
			mColor(color),
			mStyle(style),
			mText(text)
		{
		}

		LL_INLINE const LLWString& getText() const		{ return mText; };

		LL_INLINE F32 getWidth(const LLFontGL* fontp)
		{
			if (LL_UNLIKELY(fontp != mLastFont))
			{
				mLastFont = fontp;
				mLastWidth = fontp->getWidthF32(mText.c_str());
			}
			return mLastWidth;
		}

		LL_INLINE void clearFontWidthCache()			{ mLastFont = NULL; }

	public:
		LLColor4				mColor;
		LLFontGL::StyleFlags	mStyle;

	private:
		LLWString				mText;
		const LLFontGL*			mLastFont;
		F32						mLastWidth;
	};

public:
	typedef enum e_text_alignment
	{
		ALIGN_TEXT_LEFT,
		ALIGN_TEXT_CENTER
	} ETextAlignment;

	typedef enum e_vert_alignment
	{
		ALIGN_VERT_TOP,
		ALIGN_VERT_CENTER
	} EVertAlignment;

public:
	void markDead() override;

	void setStringUTF8(const std::string& utf8string);
	void setString(const LLWString& wstring);
	void clearString();
	void addLine(const std::string& text, const LLColor4& color,
				 const LLFontGL::StyleFlags style = LLFontGL::NORMAL);
	void addLine(const LLWString& wtext, const LLColor4& color,
				 const LLFontGL::StyleFlags style = LLFontGL::NORMAL);
	void setLabel(const std::string& label);
	void setLabel(const LLWString& label);
	LL_INLINE void setDropShadow(bool b)				{ mDropShadow = b; }
	LL_INLINE void setFont(const LLFontGL* font)		{ mFontp = font; }
	void setColor(const LLColor4& color);
	LL_INLINE void setUsePixelSize(bool b)				{ mUsePixelSize = b; }
	LL_INLINE void setZCompare(bool zcompare)			{ mZCompare = zcompare; }
	LL_INLINE void setDoFade(bool do_fade)				{ mDoFade = do_fade; }
	LL_INLINE bool getDoFade() const					{ return mDoFade; }
	LL_INLINE void setVisibleOffScreen(bool b)			{ mVisibleOffScreen = b; }
	// mMaxLines of -1 means unlimited lines.
	void setMaxLines(S32 max_lines)						{ mMaxLines = max_lines; }

	LL_INLINE void setFadeDistance(F32 dist, F32 range)
	{
		mFadeDistance = dist;
		mFadeRange = range;
	}

	void updateVisibility();
	LLVector2 updateScreenPos(LLVector2& offset_target);
	void updateSize();
	LL_INLINE void setMass(F32 mass)					{ mMass = llmax(0.1f, mass); }
	LL_INLINE void setTextAlignment(ETextAlignment a)	{ mTextAlignment = a; }
	LL_INLINE void setVertAlignment(EVertAlignment a)	{ mVertAlignment = a; }
	LL_INLINE F32 getDistance() const override			{ return mLastDistance; }
	LL_INLINE void setUseBubble(bool use_bubble)		{ mUseBubble = use_bubble; }
	LL_INLINE S32 getLOD()								{ return mLOD; }
	LL_INLINE bool getVisible()							{ return mVisible; }
	LL_INLINE bool getHidden() const					{ return mHidden; }
	LL_INLINE void setHidden(bool hide)					{ mHidden = hide; }
	LL_INLINE void setOnHUDAttachment(bool on_hud)		{ mOnHUDAttachment = on_hud; }
	LL_INLINE void shift(const LLVector3& offset)		{ mPositionAgent += offset; }

	bool lineSegmentIntersect(const LLVector4a& start,
							  const LLVector4a& end,
							  LLVector4a& intersection,
							  bool debug_render = false);

	static void shiftAll(const LLVector3& offset);
	static void renderAllHUD();
	static void addPickable(std::set<LLViewerObject*>& pick_list);
	static void reshape();
	LL_INLINE static void setDisplayText(bool flag)		{ sDisplayText = flag ; }

private:
	~LLHUDText() override								{}

protected:
	LLHUDText(U8 type);

	void render() override;
	void renderText();
	static void updateAll();
	LL_INLINE void setLOD(S32 lod)						{ mLOD = lod; }
	S32 getMaxLines();

//MK
public:
	// This variable is here to allow one to refresh a HUD text by calling
	// setStringUTF8, it is set when an update message is received
	std::string mLastMessageText;
//mk

private:
	const LLFontGL*	mFontp;
	const LLFontGL*	mBoldFontp;
	LLVector3		mScale;
	LLColor4		mColor;
	LLColor4U		mPickColor;
	LLRectf			mSoftScreenRect;
	LLVector3		mPositionAgent;
	LLVector2		mPositionOffset;
	LLVector2		mTargetPositionOffset;

	typedef std::vector<LLHUDTextSegment> segments_vec_t;
	segments_vec_t	mTextSegments;
	segments_vec_t	mLabelSegments;

	S32				mLOD;
	S32				mMaxLines;
	S32				mOffsetY;
	F32				mWidth;
	F32				mHeight;
	F32				mRadius;
	F32				mFadeRange;
	F32				mFadeDistance;
	F32				mLastDistance;
	F32				mMass;

	ETextAlignment	mTextAlignment;
	EVertAlignment	mVertAlignment;

	bool			mHidden;
	bool			mUseBubble;
	bool			mDropShadow;
	bool			mDoFade;
	bool			mUsePixelSize;
	bool			mZCompare;
	bool			mVisibleOffScreen;
	bool			mOffScreen;

public:	// Needed for mkrlinterface.cpp
	typedef std::set<LLPointer<LLHUDText> > htobj_list_t;
	typedef htobj_list_t::iterator htobj_list_it_t;
	static htobj_list_t		sTextObjects;

private:
	typedef std::vector<LLPointer<LLHUDText> > visible_list_t;
	static visible_list_t	sVisibleTextObjects;
	static visible_list_t	sVisibleHUDTextObjects;

	static bool				sDisplayText;
};

#endif // LL_LLHUDTEXT_H
