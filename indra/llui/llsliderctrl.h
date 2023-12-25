/**
 * @file llsliderctrl.h
 * @brief Decorated wrapper for a LLSlider.
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

#ifndef LL_LLSLIDERCTRL_H
#define LL_LLSLIDERCTRL_H

#include "llslider.h"

class LLLineEditor;
class LLTextBox;
class LLUICtrlFactory;

//
// Constants
//
constexpr S32 SLIDERCTRL_SPACING = 4; // Space between label, slider, and text
constexpr S32 SLIDERCTRL_HEIGHT = 16;

class LLSliderCtrl : public LLUICtrl
{
public:
	LLSliderCtrl(const std::string& name, const LLRect& rect,
				 const std::string& label, const LLFontGL* font,
				 S32 slider_left, S32 text_left,
				 bool show_text, bool can_edit_text, bool volume,
				 void (*commit_callback)(LLUICtrl*, void*), void* userdata,
				 F32 initial_value, F32 min_value, F32 max_value,
				 F32 incr, const char* ctrl_name = NULL);

	~LLSliderCtrl() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory*);

	LL_INLINE F32 getValueF32() const				{ return mSlider->getValueF32(); }
	void setValue(F32 v, bool from_event = false);

	LL_INLINE void setValue(const LLSD& v) override	{ setValue((F32)v.asReal(), true); }
	LL_INLINE LLSD getValue() const override		{ return LLSD(getValueF32()); }

	void setLabel(const std::string& label);
	bool setLabelArg(const std::string& key, const std::string& text) override;

	LL_INLINE void setMinValue(LLSD v) override		{ setMinValue((F32)v.asReal()); }
	LL_INLINE void setMaxValue(LLSD v) override		{ setMaxValue((F32)v.asReal()); }

	LL_INLINE bool isMouseHeldDown() const			{ return mSlider->hasMouseCapture(); }

	void setEnabled(bool b) override;

	void setRect(const LLRect& rect) override;
	void reshape(S32 width, S32 height, bool from_parent = true) override;

	void clear() override;

	void setPrecision(S32 precision);

	LL_INLINE void setMinValue(F32 v)					{ mSlider->setMinValue(v); updateText(); }
	LL_INLINE void setMaxValue(F32 v)					{ mSlider->setMaxValue(v); updateText(); }
	LL_INLINE void setIncrement(F32 inc)				{ mSlider->setIncrement(inc);}

	LL_INLINE F32 getMinValue()							{ return mSlider->getMinValue(); }
	LL_INLINE F32 getMaxValue()							{ return mSlider->getMaxValue(); }

	LL_INLINE void setLabelColor(const LLColor4& c)		{ mTextEnabledColor = c; }

	LL_INLINE void setDisabledLabelColor(const LLColor4& c)
	{
		mTextDisabledColor = c;
	}

	void setSliderMouseDownCallback(void (*cb)(LLUICtrl*, void*));
	void setSliderMouseUpCallback(void (*cb)(LLUICtrl*, void*));

	void setOffLimit(const std::string& off_text, F32 off_value = 0.f);

	void onTabInto() override;

	// Marks value as tentative
	void setTentative(bool b) override;

	// Marks not tentative, then commits
	void onCommit() override;

	void setControlName(const char* ctrl, LLView* context) override;

	LL_INLINE const std::string& getControlName() const override
	{
		return mSlider->getControlName();
	}

private:
	void updateText();
	void updateSliderRect();
	void reportInvalidData();

	static void onSliderCommit(LLUICtrl* caller, void* userdata);
	static void onSliderMouseDown(LLUICtrl* caller, void* userdata);
	static void onSliderMouseUp(LLUICtrl* caller, void* userdata);

	static void onEditorCommit(LLUICtrl* caller, void* userdata);
	static void onEditorGainFocus(LLFocusableElement* caller, void* userdata);
	static void onEditorChangeFocus(LLUICtrl* caller, S32 direction,
									void* userdata);
private:
	const LLFontGL*	mFont;

	LLSlider*		mSlider;
	LLLineEditor*	mEditor;
	LLTextBox*		mTextBox;
	LLTextBox*		mLabelBox;

	void			(*mSliderMouseUpCallback)(LLUICtrl*, void*);
	void			(*mSliderMouseDownCallback)(LLUICtrl*, void*);

	S32				mPrecision;
	S32				mLabelWidth;

	F32				mValue;
	F32				mOffValue;

	std::string		mOffText;

	LLColor4		mTextEnabledColor;
	LLColor4		mTextDisabledColor;

	bool			mShowText;
	bool			mCanEditText;
	bool			mVolumeSlider;
	bool			mDisplayOff;
};

#endif  // LL_LLSLIDERCTRL_H
