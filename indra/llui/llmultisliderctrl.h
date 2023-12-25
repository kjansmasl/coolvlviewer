/**
 * @file llmultisliderctrl.h
 * @brief LLMultiSliderCtrl base class
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

#ifndef LL_MULTI_SLIDERCTRL_H
#define LL_MULTI_SLIDERCTRL_H

#include "llcolor4.h"
#include "llmultislider.h"
#include "lluictrl.h"

class LLFontGL;
class LLLineEditor;
class LLSlider;
class LLTextBox;
class LLUICtrlFactory;

class LLMultiSliderCtrl : public LLUICtrl
{
protected:
	LOG_CLASS(LLMultiSliderCtrl);

public:
	LLMultiSliderCtrl(const std::string& name, const LLRect& rect,
					  const std::string& label, const LLFontGL* font,
					  S32 slider_left, S32 text_left, bool show_text,
					  bool can_edit_text,
					  void (*commit_callback)(LLUICtrl*, void*),
					  void* callback_userdata, F32 initial_value,
					  F32 min_value, F32 max_value, F32 increment,
					  S32 max_sliders, F32 overlap_threshold,
					  bool allow_overlap, bool loop_overlap,
					  bool draw_track, bool use_triangle, bool vertical,
					  const char* ctrl_name = NULL);

	~LLMultiSliderCtrl() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	LL_INLINE F32 getSliderValue(const std::string& name) const
	{
		return mMultiSlider->getSliderValue(name);
	}

	void setSliderValue(const std::string& name, F32 value,
						bool from_event = false);

	void setValue(const LLSD& value) override;
	LL_INLINE LLSD getValue() const override		{ return mMultiSlider->getValue(); }

	void setLabel(const std::string& label);
	bool setLabelArg(const std::string& key, const std::string& text) override;

	LL_INLINE const std::string& getCurSlider() const
	{
		return mMultiSlider->getCurSlider();
	}

	LL_INLINE F32 getCurSliderValue() const			{ return mCurValue; }
	void setCurSlider(const std::string& name);

	LL_INLINE void setCurSliderValue(F32 val, bool from_event = false)
	{
		setSliderValue(mMultiSlider->getCurSlider(), val, from_event);
	}

	LL_INLINE void setMinValue(LLSD v) override		{ setMinValue((F32)v.asReal()); }
	LL_INLINE void setMaxValue(LLSD v) override		{ setMaxValue((F32)v.asReal());  }

	LL_INLINE void resetCurSlider()					{ mMultiSlider->resetCurSlider(); }

	bool isMouseHeldDown();

	void setEnabled(bool b) override;
	void clear() override;

	void setPrecision(S32 precision);
	LL_INLINE void setMinValue(F32 min_value)		{ mMultiSlider->setMinValue(min_value); }
	LL_INLINE void setMaxValue(F32 max_value)		{ mMultiSlider->setMaxValue(max_value); }
	LL_INLINE void setIncrement(F32 increment)		{ mMultiSlider->setIncrement(increment); }

	LL_INLINE F32 getSliderValueFromPos(S32 x, S32 y) const
	{
		return mMultiSlider->getSliderValueFromPos(x, y);
	}

	// For adding and deleting sliders

	LL_INLINE S32 getMaxNumSliders()				{ return mMultiSlider->getMaxNumSliders(); }
	LL_INLINE S32 getCurNumSliders()				{ return mMultiSlider->getCurNumSliders(); }
	LL_INLINE bool canAddSliders()					{ return mMultiSlider->canAddSliders(); }

	const std::string& addSlider();
	const std::string& addSlider(F32 val);
	bool addSlider(F32 val, const std::string& name);

	void deleteSlider(const std::string& name);
	LL_INLINE void deleteCurSlider()				{ deleteSlider(mMultiSlider->getCurSlider()); }

	LL_INLINE F32 getMinValue()						{ return mMultiSlider->getMinValue(); }
	LL_INLINE F32 getMaxValue()						{ return mMultiSlider->getMaxValue(); }
	LL_INLINE F32 getIncrement()					{ return mMultiSlider->getIncrement(); }

	LL_INLINE void setLabelColor(const LLColor4& c)	{ mTextEnabledColor = c; }

	LL_INLINE void setDisabledLabelColor(const LLColor4& c)
	{
		mTextDisabledColor = c;
	}

	void setSliderMouseDownCallback(void (*cb)(S32, S32, void*));
	void setSliderMouseUpCallback(void (*cb)(S32, S32, void*));

	void onTabInto() override;

	void setTentative(bool b) override;	// Marks value as tentative
	void onCommit() override;			// Marks not tentative, then commit

	void setControlName(const char* name, LLView* context) override;
	const std::string& getControlName() const override;

	static void onSliderCommit(LLUICtrl* caller, void* userdata);
	static void onSliderMouseDown(S32 x, S32 y, void* userdata);
	static void onSliderMouseUp(S32 x, S32 y, void* userdata);

	static void onEditorCommit(LLUICtrl* caller, void* userdata);
	static void onEditorGainFocus(LLFocusableElement* caller, void* userdata);
	static void onEditorChangeFocus(LLUICtrl* caller, S32 direction,
									void* userdata);

private:
	void updateText();
	void reportInvalidData();

private:
	const LLFontGL*	mFont;

	F32				mCurValue;
	S32				mPrecision;
	S32				mLabelWidth;

	LLMultiSlider*	mMultiSlider;
	LLLineEditor*	mEditor;
	LLTextBox*		mLabelBox;
	LLTextBox*		mTextBox;

	void			(*mSliderMouseUpCallback)(S32 x, S32 y, void* userdata);
	void			(*mSliderMouseDownCallback)(S32 x, S32 y, void* userdata);

	LLColor4		mTextEnabledColor;
	LLColor4		mTextDisabledColor;

	bool			mShowText;
	bool			mCanEditText;
};

#endif  // LL_MULTI_SLIDERCTRL_H
