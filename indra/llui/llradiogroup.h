/**
 * @file llradiogroup.h
 * @brief LLRadioGroup base class
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

#ifndef LL_LLRADIOGROUP_H
#define LL_LLRADIOGROUP_H

#include "llcheckboxctrl.h"
#include "llpreprocessor.h"
#include "lluictrl.h"

/*
 * A checkbox control with use_radio_style == true.
 */
class LLRadioCtrl final : public LLCheckBoxCtrl
{
public:
	LLRadioCtrl(const std::string& name, const LLRect& rect,
				const std::string& label, const LLFontGL* font = NULL,
				void (*commit_callback)(LLUICtrl*, void*) = NULL,
				void* callback_userdata = NULL)
:	LLCheckBoxCtrl(name, rect, label, font, commit_callback, callback_userdata,
				   false, RADIO_STYLE)
	{
		setTabStop(false);
	}

	LLXMLNodePtr getXML(bool save_children = true) const override;
	void setValue(const LLSD& value) override;
};

/*
 * An invisible view containing multiple mutually exclusive toggling buttons
 * (usually radio buttons). Automatically handles the mutex condition by
 * highlighting only one button at a time.
 */
class LLRadioGroup final : public LLUICtrl
{
protected:
	LOG_CLASS(LLRadioGroup);

public:
	enum EOperation
	{
		OP_DELETE = 1,
		OP_SELECT,
		OP_DESELECT,
	};

	// Builds a radio group. The number (0...n - 1) of the currently selected
	// element will be stored in the named control. After the control is
	// changed the callback will be called.
	LLRadioGroup(const std::string& name, const LLRect& rect,
				 const char* control_name, LLUICtrlCallback callback = NULL,
				 void* userdata = NULL,  bool border = true);

	// Another radio group constructor, but this one does not rely on needing a
	// control
	LLRadioGroup(const std::string& name, const LLRect& rect, S32 initial_idx,
				 LLUICtrlCallback callback = NULL, void* userdata = NULL,
				 bool border = true);

	bool handleKeyHere(KEY key, MASK mask) override;

	void setEnabled(bool enabled) override;
	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);
	void setIndexEnabled(S32 index, bool enabled);

	// Returns the index value of the selected item
	LL_INLINE S32 getSelectedIndex() const						{ return mSelectedIndex; }

	// Sets the index value programatically
	bool setSelectedIndex(S32 index, bool from_event = false);

	// Accepts and retrieves strings of the radio group control names
	void setValue(const LLSD& value) override;
	LLSD getValue() const override;

	// Draws the group, but also fix the highlighting based on the control.
	void draw() override;

	// You must use this method to add buttons to a radio group.
	// Do not use addChild: it would not set the callback function correctly.
	LLRadioCtrl* addRadioButton(const std::string& name,
								const std::string& label,
								const LLRect& rect, const LLFontGL* font);

	LL_INLINE LLRadioCtrl* getRadioButton(S32 index)			{ return mRadioButtons[index]; }

	// Update the control as needed. Userdata must be a pointer to the button.
	static void onClickButton(LLUICtrl* radio, void* userdata);


	LL_INLINE S32 getItemCount() const							{ return mRadioButtons.size(); }
	LL_INLINE bool getCanSelect()const			 				{ return true; }
	LL_INLINE bool selectFirstItem()							{ return setSelectedIndex(0); }
	LL_INLINE bool selectNthItem(S32 index)						{ return setSelectedIndex(index); }
	LL_INLINE bool selectItemRange(S32 first, S32)				{ return setSelectedIndex(first); }
	LL_INLINE S32 getFirstSelectedIndex() const					{ return getSelectedIndex(); }
	LL_INLINE bool setCurrentByID(const LLUUID& id)				{ return false; }
	LL_INLINE LLUUID getCurrentID() const						{ return LLUUID::null; }
	bool setSelectedByValue(const LLSD& value, bool selected);
	LL_INLINE LLSD getSelectedValue()							{ return getValue(); }

	bool isSelected(const LLSD& value) const;
	LL_INLINE bool operateOnSelection(EOperation)				{ return false; }
	LL_INLINE bool operateOnAll(EOperation)						{ return false; }

private:
	void init(bool border);

private:
	S32				mSelectedIndex;
	bool			mHasBorder;
	typedef std::vector<LLRadioCtrl*> button_list_t;
	button_list_t	mRadioButtons;
};

#endif
