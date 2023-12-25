/**
 * @file llcheckboxctrl.h
 * @brief LLCheckBoxCtrl base class
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

#ifndef LL_LLCHECKBOXCTRL_H
#define LL_LLCHECKBOXCTRL_H

#include "stdtypes.h"

#include "llbutton.h"
#include "llrect.h"
#include "lluictrl.h"
#include "llcolor4.h"

class LLFontGL;
class LLTextBox;
class LLViewBorder;

// Constants
constexpr S32 LLCHECKBOXCTRL_BTN_SIZE = 13;
constexpr S32 LLCHECKBOXCTRL_VPAD = 2;
constexpr S32 LLCHECKBOXCTRL_HPAD = 2;
constexpr S32 LLCHECKBOXCTRL_SPACING = 5;
constexpr S32 LLCHECKBOXCTRL_HEIGHT = 16;
constexpr bool RADIO_STYLE = true;
constexpr bool CHECK_STYLE = false;

class LLCheckBoxCtrl : public LLUICtrl
{
public:
	LLCheckBoxCtrl(const std::string& name, const LLRect& rect,
				   const std::string& label, const LLFontGL* font = NULL,
				   void (*commit_callback)(LLUICtrl*, void*) = NULL,
				   void* callback_userdata = NULL, bool initial_value = false,
				   // If true, draw radio button style icons
				   bool use_radio_style = false,
				   const char* control_name = NULL);

	~LLCheckBoxCtrl() override;

	// LLView interface

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	void setEnabled(bool b) override;

	void draw() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	void setControlName(const char* ctrl_name, LLView* context) override;
	const std::string& getControlName() const override;

	// LLUICtrl interface

	void setValue(const LLSD& value) override;
	LLSD getValue() const override;
	// Shortcuts to the above
	LL_INLINE bool get()							{ return getValue().asBoolean(); }
	LL_INLINE void set(bool value)					{ setValue(LLSD(value)); }

	LL_INLINE void setTentative(bool b) override	{ mButton->setTentative(b); }
	LL_INLINE bool getTentative() const override	{ return mButton->getTentative(); }

	bool setLabelArg(const std::string& key, const std::string& text) override;

	void clear() override;
	void onCommit() override;

	// Returns true if the user has modified this control
	bool isDirty() const override;
	// Clears dirty state
	void resetDirty() override;

	// LLCheckBoxCtrl interface

	// Returns the new state
	LL_INLINE virtual bool toggle()						{ return mButton->toggleState(); }

	LL_INLINE void setEnabledColor(const LLColor4& c)	{ mTextEnabledColor = c; }
	LL_INLINE void setDisabledColor(const LLColor4& c)	{ mTextDisabledColor = c; }

	void setLabel(const std::string& label);
	std::string getLabel() const;

	static void onButtonPress(void* userdata);

protected:
	// Note: value is stored in toggle state of button
	LLButton*		mButton;
	LLTextBox*		mLabel;
	LLViewBorder*	mBorder;
	const LLFontGL* mFont;
	LLColor4		mTextEnabledColor;
	LLColor4		mTextDisabledColor;
	bool			mRadioStyle;
	bool			mInitialValue;			// Value set in constructor
	bool			mSetValue;				// Value set programmatically
};

#endif  // LL_LLCHECKBOXCTRL_H
