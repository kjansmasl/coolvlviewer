/**
 * @file llspinctrl.h
 * @brief Typical spinner with "up" and "down" arrow buttons.
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

#ifndef LL_LLSPINCTRL_H
#define LL_LLSPINCTRL_H

#include "stdtypes.h"

#include "llpreprocessor.h"
#include "llrect.h"
#include "lluictrl.h"
#include "llcolor4.h"

//
// Constants
//
constexpr S32 SPINCTRL_BTN_HEIGHT = 8;
constexpr S32 SPINCTRL_BTN_WIDTH = 16;
// Space between label right and button left:
constexpr S32 SPINCTRL_SPACING = 2;
constexpr S32 SPINCTRL_HEIGHT = 2 * SPINCTRL_BTN_HEIGHT;
constexpr S32 SPINCTRL_DEFAULT_LABEL_WIDTH = 10;

class LLSpinCtrl : public LLUICtrl
{
protected:
	LOG_CLASS(LLSpinCtrl);

public:
	LLSpinCtrl(const std::string& name, const LLRect& rect,
			   const std::string& label, const LLFontGL* font,
			   void (*commit_callback)(LLUICtrl*, void*),  void* userdata,
			   F32 initial_value, F32 min_value, F32 max_value, F32 increment,
			   const std::string& control_name = std::string(),
			   S32 label_width = SPINCTRL_DEFAULT_LABEL_WIDTH);

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   class LLUICtrlFactory* factory);

	virtual void forceSetValue(const LLSD& value);
	void setValue(const LLSD& value) override;
	LL_INLINE LLSD getValue() const override			{ return mValue; }
	LL_INLINE F32 get() const							{ return (F32)getValue().asReal(); }
	LL_INLINE void set(F32 value)						{ setValue(value); mInitialValue = value; }

	void setMinValue(LLSD min_value) override			{ setMinValue((F32)min_value.asReal()); }
	void setMaxValue(LLSD max_value) override			{ setMaxValue((F32)max_value.asReal());  }

	bool isMouseHeldDown() const;

	void setEnabled(bool b) override;
	void setFocus(bool b) override;
	void clear() override;
	LL_INLINE bool isDirty() const override				{ return mValue != mInitialValue; }
	LL_INLINE void resetDirty() override				{ mInitialValue = mValue; }

	virtual void setPrecision(S32 precision);
	LL_INLINE virtual void setMinValue(F32 min)			{ mMinValue = min; }
	LL_INLINE virtual void setMaxValue(F32 max)			{ mMaxValue = max; }
	LL_INLINE virtual void setIncrement(F32 inc)		{ mIncrement = inc; }
	LL_INLINE virtual F32 getMinValue()					{ return mMinValue; }
	LL_INLINE virtual F32 getMaxValue()					{ return mMaxValue; }
	LL_INLINE virtual F32 getIncrement()          		{ return mIncrement; }

	void setLabel(const std::string& label);
	LL_INLINE void setLabelColor(const LLColor4& c)		{ mTextEnabledColor = c; mDirty = true; }

	LL_INLINE void setDisabledLabelColor(const LLColor4& c)
	{
		mTextDisabledColor = c;
		mDirty = true;
	}

	void setAllowEdit(bool allow_edit);

	void onTabInto() override;

	void setTentative(bool b) override;	// Marks value as tentative
	void onCommit() override;			// Marks not tentative, then commits

	void forceEditorCommit();			// For commit on external button

	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	bool handleKeyHere(KEY key, MASK mask) override;

	void draw() override;

	static void onEditorCommit(LLUICtrl* caller, void* userdata);
	static void onEditorGainFocus(LLFocusableElement* caller, void* userdata);
	static void	onEditorLostFocus(LLFocusableElement* caller, void* userdata);
	static void onEditorChangeFocus(LLUICtrl* caller, S32 dir, void* userdata);

	static void onUpBtn(void* userdata);
	static void onDownBtn(void* userdata);

private:
	void updateEditor();
	void reportInvalidData();

private:
	class LLButton*		mUpBtn;
	class LLButton*		mDownBtn;
	class LLLineEditor*	mEditor;
	class LLTextBox*	mLabelBox;

	LLColor4			mTextEnabledColor;
	LLColor4			mTextDisabledColor;

	F32					mValue;
	F32					mInitialValue;
	F32					mMaxValue;
	F32					mMinValue;
	F32					mIncrement;

	S32					mPrecision;

	bool				mHasBeenSet;
	bool				mDirty;
};

#endif  // LL_LLSPINCTRL_H
