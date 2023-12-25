/**
 * @file llslider.h
 * @brief A simple slider with no label.
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

#ifndef LL_LLSLIDER_H
#define LL_LLSLIDER_H

#include "lluictrl.h"
#include "llcolor4.h"

class LLUICtrlFactory;

class LLSlider : public LLUICtrl
{
public:
	LLSlider(const std::string& name, const LLRect& rect,
			 void (*on_commit_callback)(LLUICtrl*, void*), void* userdata,
			 F32 initial_value, F32 min_value, F32 max_value, F32 increment,
			 bool volume, const char* ctrl_name = NULL);

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory*);

	void draw() override;

	void setValue(F32 value, bool from_event = false);
	F32 getValueF32() const							{ return mValue; }

	LL_INLINE void setValue(const LLSD& v) override	{ setValue((F32)v.asReal(), true); }
	LL_INLINE LLSD getValue() const override		{ return LLSD(getValueF32()); }

	LL_INLINE void setMinValue(LLSD v) override		{ setMinValue((F32)v.asReal()); }
	LL_INLINE void setMaxValue(LLSD v) override		{ setMaxValue((F32)v.asReal());  }

	LL_INLINE F32 getInitialValue() const			{ return mInitialValue; }
	LL_INLINE F32 getMinValue() const				{ return mMinValue; }
	LL_INLINE F32 getMaxValue() const				{ return mMaxValue; }
	LL_INLINE F32 getIncrement() const				{ return mIncrement; }
	LL_INLINE void setMinValue(F32 v)				{ mMinValue = v; updateThumbRect(); }
	LL_INLINE void setMaxValue(F32 v)				{ mMaxValue = v; updateThumbRect(); }
	LL_INLINE void setIncrement(F32 increment)		{ mIncrement = increment; }

	LL_INLINE void setMouseDownCallback(void (*cb)(LLUICtrl*, void*))
	{
		mMouseDownCallback = cb;
	}

	LL_INLINE void setMouseUpCallback(void (*cb)(LLUICtrl*, void*))
	{
		mMouseUpCallback = cb;
	}

	LL_INLINE void setMouseHoverCallback(void (*cb)(LLUICtrl*, void*))
	{
		mMouseHoverCallback = cb;
	}

	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleKeyHere(KEY key, MASK mask) override;

private:
	void setValueAndCommit(F32 value);
	void updateThumbRect();

private:
	LLUIImage*	mThumbImage;
	LLUIImage*	mTrackImage;
	LLUIImage*	mTrackHighlightImage;

	LLRect		mDragStartThumbRect;
	LLRect		mThumbRect;

	void		(*mMouseDownCallback)(LLUICtrl*, void*);
	void		(*mMouseUpCallback)(LLUICtrl*, void*);
	void		(*mMouseHoverCallback)(LLUICtrl*, void*);

	F32			mValue;
	F32			mInitialValue;
	F32			mMinValue;
	F32			mMaxValue;
	F32			mIncrement;

	S32			mMouseOffset;

	bool		mVolumeSlider;
};

#endif  // LL_LLSLIDER_H
