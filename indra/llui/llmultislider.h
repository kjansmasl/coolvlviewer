/**
 * @file llmultislider.h
 * @brief A simple multislider
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_MULTI_SLIDER_H
#define LL_MULTI_SLIDER_H

#include "lluictrl.h"
#include "llcolor4.h"

class LLUICtrlFactory;

class LLMultiSlider : public LLUICtrl
{
	friend class LLMultiSliderCtrl;

protected:
	LOG_CLASS(LLMultiSlider);

public:
	LLMultiSlider(const std::string& name, const LLRect& rect,
				  void (*on_commit_callback)(LLUICtrl*, void*), void* userdata,
				  F32 initial_value, F32 min_value, F32 max_value,
				  F32 increment, S32 max_sliders, F32 overlap_threshold,
				  bool allow_overlap, bool loop_overlap, bool draw_track,
				  bool use_triangle, bool vertical,
				  const char* control_name = NULL);

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	void setSliderValue(const std::string& name, F32 value,
						bool from_event = false);
	F32 getSliderValue(const std::string& name) const;
	F32 getSliderValueFromPos(S32 xpos, S32 ypos) const;

	LL_INLINE const std::string& getCurSlider() const
	{
		return mCurSlider;
	}

	LL_INLINE F32 getCurSliderValue() const		{ return getSliderValue(mCurSlider); }
	void setCurSlider(const std::string& name);

	LL_INLINE void setCurSliderValue(F32 val, bool from_event = false)
	{
		setSliderValue(mCurSlider, val, from_event);
	}

	void setValue(const LLSD& value) override;
	LL_INLINE LLSD getValue() const override	{ return mValue; }

	LL_INLINE void resetCurSlider()				{ mCurSlider.clear(); }

	LL_INLINE void setMinValue(LLSD min_value) override
	{
		setMinValue((F32)min_value.asReal());
	}

	LL_INLINE void setMaxValue(LLSD max_value) override
	{
		setMaxValue((F32)max_value.asReal());
	}

	LL_INLINE F32 getInitialValue() const		{ return mInitialValue; }
	LL_INLINE F32 getMinValue() const			{ return mMinValue; }
	LL_INLINE F32 getMaxValue() const			{ return mMaxValue; }
	LL_INLINE F32 getIncrement() const			{ return mIncrement; }
	LL_INLINE void setMinValue(F32 min_value)	{ mMinValue = min_value; }
	LL_INLINE void setMaxValue(F32 max_value)	{ mMaxValue = max_value; }
	LL_INLINE void setIncrement(F32 increment)	{ mIncrement = increment; }

	LL_INLINE S32 getMaxNumSliders()			{ return mMaxNumSliders; }
	LL_INLINE S32 getCurNumSliders()			{ return mValue.size(); }
	LL_INLINE bool canAddSliders()				{ return mValue.size() < mMaxNumSliders; }

	LL_INLINE void setMouseDownCallback(void (*cb)(S32 x, S32 y, void*))
	{
		mMouseDownCallback = cb;
	}

	LL_INLINE void setMouseUpCallback(void (*cb)(S32 x, S32 y, void*))
	{
		mMouseUpCallback = cb;
	}

	bool findUnusedValue(F32& init_val);

	const std::string& addSlider(F32 val);
	LL_INLINE const std::string& addSlider()	{ return addSlider(mInitialValue); }
	bool addSlider(F32 val, const std::string& name);

	void deleteSlider(const std::string& name);
	LL_INLINE void deleteCurSlider()			{ deleteSlider(mCurSlider); }
	void clear() override;

	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	void draw() override;

protected:
	size_t		mMaxNumSliders;
	F32			mInitialValue;
	F32			mMinValue;
	F32			mMaxValue;
	F32			mIncrement;
	S32			mMouseOffset;
	F32			mOverlapThreshold;

	void		(*mMouseDownCallback)(S32 x, S32 y, void* userdata);
	void		(*mMouseUpCallback)(S32 x, S32 y, void* userdata);

	LLRect		mDragStartThumbRect;

	LLSD		mValue;

	typedef std::map<std::string, LLRect> rect_map_t;
	rect_map_t	mThumbRects;

	std::string	mCurSlider;

	bool		mAllowOverlap;
	bool		mLoopOverlap;
	bool		mDrawTrack;
	bool		mUseTriangle;	// Hacked in toggle to use a triangle
	bool		mVertical;

	static S32	mNameCounter;
};

#endif  // LL_LLSLIDER_H
