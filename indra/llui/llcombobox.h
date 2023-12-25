/**
 * @file llcombobox.h
 * @brief LLComboBox base class
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

// A control that displays the name of the chosen item, which when clicked
// shows a scrolling box of choices.

#ifndef LL_LLCOMBOBOX_H
#define LL_LLCOMBOBOX_H

#include "lluictrl.h"

class LLButton;
class LLLineEditor;
class LLScrollListItem;
class LLScrollListCtrl;

class LLComboBox : public LLUICtrl
{
public:
	enum EOperation
	{
		OP_DELETE = 1,
		OP_SELECT,
		OP_DESELECT,
	};

	typedef enum e_preferred_position
	{
		ABOVE,
		BELOW
	} EPreferredPosition;

	LLComboBox(const std::string& name, const LLRect &rect,
			   const std::string& label,
			   void (*commit_callback)(LLUICtrl*, void*) = NULL,
			   void* callback_userdata = NULL);
	~LLComboBox() override;

	// LLView interface

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	void draw() override;
	void onFocusLost() override;
	void onLostTop() override;

	void setEnabled(bool enabled) override;

	bool handleToolTip(S32 x, S32 y, std::string& msg, LLRect* r) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleUnicodeCharHere(llwchar uni_char) override;

	// LLUICtrl interface
	void clear() override;				// To select nothing
	void onCommit() override;

	LL_INLINE bool acceptsTextInput() const override	{ return mAllowTextEntry; }

	// Returns true if the user has modified this control.
	bool isDirty() const override;
	// Clears dirty state
	void resetDirty() override;

	void setFocus(bool b) override;

	// Allow prevalidation of text input field
	void setPrevalidate(bool (*func)(const LLWString&));

	// Selects item by underlying LLSD value, using LLSD::asString() matching.
	// For simple items, this is just the name of the label.
	void setValue(const LLSD& value) override;

	// Gets underlying LLSD value for currently selected items. For simple
	// items, this is just the label.
	LLSD getValue() const override;

	void setAllowTextEntry(bool allow, S32 max_chars = 50,
						   bool make_tentative = true);
	void setTextEntry(const std::string& text);
	// Sets focus to the text input area instead of the list
	void setFocusText(bool b);
	// Returns true if the user has modified the text input area
	bool isTextDirty() const;
	// Resets the dirty flag on the input field
	void resetTextDirty();

	// Add item "name" to menu:
	LLScrollListItem* add(const std::string& name,
						  EAddPosition pos = ADD_BOTTOM, bool enabled = true);
	LLScrollListItem* add(const std::string& name, const LLUUID& id,
						  EAddPosition pos = ADD_BOTTOM, bool enabled = true);
	LLScrollListItem* add(const std::string& name, void* userdata,
						  EAddPosition pos = ADD_BOTTOM, bool enabled = true);
	LLScrollListItem* add(const std::string& name, LLSD value,
						  EAddPosition pos = ADD_BOTTOM, bool enabled = true);
	LLScrollListItem* addSeparator(EAddPosition pos = ADD_BOTTOM);

	// Removes item by index, return true if found and removed:
	bool remove(S32 index);

	LL_INLINE void removeall()							{ clearRows(); }
	bool itemExists(const std::string& name);

	// Sort the entries in the combobox by name:
	void sortByName(bool ascending = true);

	// Selects current item by name using selectItemByLabel. Returns false if
	// not found:
	bool setSimple(const std::string& name);
	// Gets name of current item. Returns an empty string if not found:
	const std::string getSimple() const;
	// Gets contents of column x of selected row:
	const std::string getSelectedItemLabel(S32 column = 0) const;

	// Sets the label, which doesn't have to exist in the label.
	// This is probably an UI abuse.
	void setLabel(const std::string& name);

	// Removes item "name", return true if found and removed:
	bool remove(const std::string& name);

	bool setCurrentByIndex(S32 index);
	S32 getCurrentIndex() const;

	virtual void updateLayout();

	S32 getItemCount() const;

	// Overwrites the default column (See LLScrollListCtrl for format)
	void addColumn(const LLSD& column, EAddPosition pos = ADD_BOTTOM);
	void clearColumns();

	void setColumnLabel(const std::string& col, const std::string& l);
	LLScrollListItem* addElement(const LLSD& value,
								 EAddPosition pos = ADD_BOTTOM,
								 void* userdata = NULL);

	LLScrollListItem* addSimpleElement(const std::string& value,
									   EAddPosition pos = ADD_BOTTOM,
									   const LLSD& id = LLSD());

	void clearRows();

	void sortByColumn(const std::string& name, bool ascending);

	LLScrollListItem* getItemByIndex(S32 index) const;

	LL_INLINE bool getCanSelect() const 				{ return true; }
	LL_INLINE bool selectFirstItem()					{ return setCurrentByIndex(0); }
	LL_INLINE bool selectNthItem(S32 index)				{ return setCurrentByIndex(index); }
	bool selectItemRange(S32 first, S32 last);

	LL_INLINE S32 getFirstSelectedIndex() const			{ return getCurrentIndex(); }

	bool setCurrentByID(const LLUUID& id);
	LLUUID getCurrentID() const;

	bool setSelectedByValue(const LLSD& value, bool selected);

	LL_INLINE bool selectByValue(const LLSD value)		{ return setSelectedByValue(value, true); }

	LLSD getSelectedValue();

	bool isSelected(const LLSD& value) const;

	bool operateOnSelection(EOperation op);
	bool operateOnAll(EOperation op);

	void* getCurrentUserdata();

	LL_INLINE void setPrearrangeCallback(void (*cb)(LLUICtrl*, void*))
	{
		mPrearrangeCallback = cb;
	}

	LL_INLINE void setTextEntryCallback(void (*cb)(LLLineEditor*, void*))
	{
		mTextEntryCallback = cb;
	}

	void setButtonVisible(bool visible);

	void setSuppressTentative(bool suppress);

	void updateSelection();
	virtual void showList();
	virtual void hideList();

protected:
	static void onButtonDown(void* userdata);
	static void onItemSelected(LLUICtrl* item, void* userdata);
	static void onTextEntry(LLLineEditor* line_editor, void* user_data);
	static void onTextCommit(LLUICtrl* caller, void* user_data);

protected:
	LLButton*			mButton;
	LLScrollListCtrl*	mList;
	EPreferredPosition	mListPosition;
	LLUIImagePtr		mArrowImage;
	std::string			mLabel;

private:
	LLLineEditor*		mTextEntry;
	void				(*mPrearrangeCallback)(LLUICtrl*, void*);
	void				(*mTextEntryCallback)(LLLineEditor*, void*);
	S32					mMaxChars;
	bool				mTextEntryTentative;
	bool				mSuppressTentative;
	bool				mAllowTextEntry;
};

class LLFlyoutButton final : public LLComboBox
{
public:
	LLFlyoutButton(const std::string& name, const LLRect& rect,
				   const std::string& label,
				   void (*commit_callback)(LLUICtrl*, void*) = NULL,
				   void* callback_userdata = NULL);

	void updateLayout() override;
	void draw() override;
	void setEnabled(bool enabled) override;

	LL_INLINE void setToggleState(bool b)				{ mToggleState = b; }

	// Allows to change the label of the action button:
	void setLabel(const std::string& label);

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

protected:
	static void onActionButtonClick(void* userdata);
	static void onSelectAction(LLUICtrl* ctrl, void* userdata);

protected:
	LLButton*		mActionButton;
	LLUIImagePtr	mActionButtonImage;
	LLUIImagePtr	mExpanderButtonImage;
	LLUIImagePtr	mActionButtonImageSelected;
	LLUIImagePtr	mExpanderButtonImageSelected;
	LLUIImagePtr	mActionButtonImageDisabled;
	LLUIImagePtr	mExpanderButtonImageDisabled;
	bool			mToggleState;
};

#endif
