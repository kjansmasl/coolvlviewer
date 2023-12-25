/**
 * @file llscrolllistctrl.h
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

#ifndef LL_SCROLLLISTCTRL_H
#define LL_SCROLLLISTCTRL_H

#include <vector>
#include <deque>

#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldate.h"
#include "lleditmenuhandler.h"
#include "llframetimer.h"
#include "llimagegl.h"
#include "llpreprocessor.h"
#include "llresizebar.h"
#include "llscrollbar.h"
#include "llstring.h"
#include "lluictrl.h"
#include "lluuid.h"

/*
 * Represents a cell in a scrollable table.
 *
 * Sub-classes must return height and other properties though width accessors
 * are implemented by the base class. It is therefore important for sub-class
 * constructors to call setWidth() with realistic values.
 */
class LLScrollListCell
{
public:
	LLScrollListCell(S32 width = 0)
	:	mWidth(width)
	{
	}

	virtual ~LLScrollListCell() = default;

	virtual void draw(const LLColor4& color,
					  const LLColor4& highlight_color) const = 0;

	LL_INLINE virtual S32 getWidth() const					{ return mWidth; }
	LL_INLINE virtual S32 getContentWidth() const			{ return 0; }
	virtual S32 getHeight() const = 0;
	LL_INLINE virtual LLSD getValue() const					{ return LLStringUtil::null; }
	LL_INLINE virtual void setValue(const LLSD& value)		{}
	LL_INLINE virtual bool getVisible() const				{ return true; }
	LL_INLINE virtual void setWidth(S32 width)				{ mWidth = width; }
	LL_INLINE virtual void highlightText(S32 offset,
										 S32 num_chars)		{}
	virtual bool isText() const = 0;
	LL_INLINE virtual void setColor(const LLColor4&)		{}
	virtual void onCommit()									{}

	virtual bool handleClick()								{ return false; }
	LL_INLINE virtual void setEnabled(bool enable)			{}

private:
	S32 mWidth;
};

/*
 * Draws a horizontal line.
 */
class LLScrollListSeparator : public LLScrollListCell
{
public:
	LLScrollListSeparator(S32 width);

	virtual void draw(const LLColor4& color,
					  const LLColor4& highlight_color) const;

	virtual S32 getHeight() const;
	LL_INLINE virtual bool isText() const					{ return false; }
};

/*
 * Cell displaying a text label.
 */
class LLScrollListText : public LLScrollListCell
{
public:
	LLScrollListText(const std::string& text, const LLFontGL* font,
					 S32 width = 0, U8 font_style = LLFontGL::NORMAL,
					 LLFontGL::HAlign font_alignment = LLFontGL::LEFT,
					 LLColor4& color = LLColor4::black, bool use_color = false,
					 bool visible = true);
	~LLScrollListText() override;

	void draw(const LLColor4& color,
			  const LLColor4& highlight_color) const override;

	LL_INLINE void setText(const std::string& text)			{ mText = text; }
	LL_INLINE void setValue(const LLSD& value) override		{ setText(value.asString()); }

	LL_INLINE LLSD getValue() const override				{ return LLSD(mText.getString()); }

	LL_INLINE S32 getContentWidth() const override			{ return mFont->getWidth(mText.getString()); }

	S32 getHeight() const override;

	LL_INLINE bool getVisible() const override				{ return mVisible; }
	void highlightText(S32 offset, S32 num_chars) override;

	LL_INLINE void setColor(const LLColor4& c) override		{ mColor = c; mUseColor = true; }
	LL_INLINE bool isText() const override					{ return true; }

	LL_INLINE void setFontStyle(U8 font_style)				{ mFontStyle = font_style; }

private:
	LLColor4			mColor;
	const LLFontGL*		mFont;
	LLFontGL::HAlign	mFontAlignment;
	LLUIString			mText;
	S32					mHighlightCount;
	S32					mHighlightOffset;
	U8					mFontStyle;
	bool				mUseColor;
	bool				mVisible;

	static U32			sCount;
};

class LLScrollListDate : public LLScrollListText
{
public:
	LLScrollListDate(const LLDate& date, const std::string& format,
					 const LLFontGL* font, S32 width = 0,
					 U8 font_style = LLFontGL::NORMAL,
					 LLFontGL::HAlign font_alignment = LLFontGL::LEFT,
					 LLColor4& color = LLColor4::black, bool use_color = false,
					 bool visible = true);

	void setValue(const LLSD& value) override;
	LL_INLINE LLSD getValue() const override				{ return mDate; }

private:
	LLDate		mDate;
	std::string	mFormat;
};

/*
 * Cell displaying an image.
 */
class LLScrollListIcon : public LLScrollListCell
{
public:
	LLScrollListIcon(LLUIImagePtr icon, S32 width = 0);
	LLScrollListIcon(const LLSD& value, S32 width = 0);

	void draw(const LLColor4& color, const LLColor4& highlight) const override;

	S32 getWidth() const override;
	LL_INLINE S32 getHeight() const override				{ return mIcon ? mIcon->getHeight() : 0; }
	LL_INLINE LLSD getValue() const override				{ return mIcon.isNull() ? LLStringUtil::null : mIcon->getName(); }
	void setColor(const LLColor4& color) override;
	LL_INLINE bool isText() const override					{ return false; }

	void setValue(const LLSD& value) override;
	LL_INLINE void setImage(LLUIImagePtr image)				{ mIcon = image; }

private:
	LLUIImagePtr	mIcon;
	LLColor4		mColor;
};

/*
 * An interactive cell containing a check box.
 */
class LLScrollListCheck : public LLScrollListCell
{
public:
	LLScrollListCheck(LLCheckBoxCtrl* check_box, S32 width = 0);
	~LLScrollListCheck() override;

	void draw(const LLColor4& color, const LLColor4& highlight) const override;

	LL_INLINE S32 getHeight() const override			{ return 0; }
	LL_INLINE LLSD getValue() const override			{ return mCheckBox->getValue(); }
	LL_INLINE void setValue(const LLSD& v) override		{ mCheckBox->setValue(v); }

	void onCommit() override							{ mCheckBox->onCommit(); }

	bool handleClick() override;

	LL_INLINE void setEnabled(bool enable) override		{ mCheckBox->setEnabled(enable); }

	LL_INLINE LLCheckBoxCtrl* getCheckBox()				{ return mCheckBox; }
	LL_INLINE bool isText() const override				{ return false; }

private:
	LLCheckBoxCtrl* mCheckBox;
};

/*
 * A simple data class describing a column within a scroll list.
 */
class LLScrollListColumn
{
public:
	LLScrollListColumn();
	LLScrollListColumn(const LLSD& sd, LLScrollListCtrl* parent);

	void setWidth(S32 width);
	LL_INLINE S32 getWidth() const						{ return mWidth; }

private:
	S32						mWidth;

public:
	// Public data is fine so long as this remains a simple struct-like data
	// class. If it ever gets any smarter than that, these should all become
	// private with protected or public accessor methods added as needed. -MG
	LLScrollListCtrl*		mParentCtrl;
	class LLColumnHeader*	mHeader;
	LLFontGL::HAlign		mFontAlignment;
	std::string				mName;
	std::string				mSortingColumn;
	std::string				mLabel;
	S32						mMaxContentWidth;
	S32						mIndex;
	F32						mRelWidth;
	bool					mDynamicWidth;
	bool					mSortAscending;
};

class LLColumnHeader : public LLComboBox
{
public:
	LLColumnHeader(const std::string& label, const LLRect& rect,
				   LLScrollListColumn* column, const LLFontGL* font = NULL);

	void draw() override;

	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;

	void showList() override;

	LLView*	findSnapEdge(S32& new_edge_val, const LLCoordGL& mouse_dir,
						 ESnapEdge snap_edge, ESnapType snap_type,
						 S32 threshold, S32 padding) override;
	void userSetShape(const LLRect& new_rect) override;

	void setImage(const std::string &image_name);
	LL_INLINE LLScrollListColumn* getColumn()				{ return mColumn; }
	void setHasResizableElement(bool resizable);
	void updateResizeBars();
	bool canResize();
	void enableResizeBar(bool enable);
	LL_INLINE std::string getLabel()						{ return mOrigLabel; }

	static void onSelectSort(LLUICtrl* ctrl, void* user_data);
	static void onClick(void* user_data);
	static void onMouseDown(void* user_data);
	static void onHeldDown(void* user_data);

private:
	LLScrollListColumn* mColumn;
	LLResizeBar*		mResizeBar;
	std::string			mOrigLabel;
	LLUIString			mAscendingText;
	LLUIString			mDescendingText;
	bool				mShowSortOptions;
	bool				mHasResizableElement;
};

class LLScrollListItem
{
protected:
	LOG_CLASS(LLScrollListItem);

public:
	LLScrollListItem(bool enabled = true, void* userdata = NULL,
					 const LLUUID& id = LLUUID::null)
	:	mSelected(false),
		mItemEnabled(enabled),
		mUserdata(userdata),
		mItemValue(id),
		mItemID(id)
	{
	}

	LLScrollListItem(const LLSD& item_value, void* userdata = NULL)
	:	mSelected(false),
		mItemEnabled(true),
		mUserdata(userdata),
		mItemValue(item_value)
	{
		if (mItemValue.isUUID())
		{
			mItemID = mItemValue.asUUID();
		}
	}

	virtual ~LLScrollListItem();

	LL_INLINE void setSelected(bool b)						{ mSelected = b; }
	LL_INLINE bool getSelected() const						{ return mSelected; }

	LL_INLINE void setEnabled(bool b)						{ mItemEnabled = b; }

	LL_INLINE bool getEnabled() const 						{ return mItemEnabled; }

	LL_INLINE void setUserdata(void* userdata)				{ mUserdata = userdata; }
	LL_INLINE void* getUserdata() const 					{ return mUserdata; }

	LL_INLINE void setToolTip(const std::string& tool_tip)	{ mToolTip = tool_tip; }
	LL_INLINE const std::string& getToolTip() const 		{ return mToolTip; }

	LL_INLINE const LLUUID& getUUID() const					{ return mItemID; }
	LL_INLINE const LLSD& getValue() const					{ return mItemValue; }

	// If width = 0, just use the width of the text. Otherwise override with
	// specified width in pixels.
	LL_INLINE void addColumn(const std::string& text, const LLFontGL* font,
							 S32 width = 0, U8 font_style = LLFontGL::NORMAL,
							 LLFontGL::HAlign font_alignment = LLFontGL::LEFT,
							 bool visible = true)
	{
		mColumns.push_back(new LLScrollListText(text, font, width, font_style,
												font_alignment,
												LLColor4::black, false,
												visible));
	}

	LL_INLINE void addColumn(LLUIImagePtr icon, S32 width = 0)
	{
		mColumns.push_back(new LLScrollListIcon(icon, width));
	}

	LL_INLINE void addColumn(LLCheckBoxCtrl* check, S32 width = 0)
	{
		mColumns.push_back(new LLScrollListCheck(check, width));
	}

	void setNumColumns(S32 columns);

	void setColumn(S32 column, LLScrollListCell* cell);

	LL_INLINE S32 getNumColumns() const						{ return (S32)mColumns.size(); }

	LL_INLINE LLScrollListCell* getColumn(S32 i) const
	{
		return i >= 0 && i < getNumColumns() ? mColumns[i] : NULL;
	}

	std::string getContentsCSV() const;

	virtual void draw(const LLRect& rect, const LLColor4& fg_color,
					  const LLColor4& bg_color, const LLColor4& highlight_col,
					  S32 column_padding);

private:
	void*							mUserdata;
	std::string 					mToolTip;
	std::vector<LLScrollListCell*>	mColumns;
	LLUUID							mItemID;
	LLSD							mItemValue;
	bool							mSelected;
	bool							mItemEnabled;
};

/*
 * A graphical control representing a scrollable table. Cells in the table can
 * be simple text or more complicated things such as icons or even interactive
 * elements like check boxes.
 */
class LLScrollListItemComment : public LLScrollListItem
{
public:
	LLScrollListItemComment(const std::string& comment_string,
							const LLColor4& color);

	void draw(const LLRect& rect, const LLColor4& fg_color,
			  const LLColor4& bg_color, const LLColor4& highlight_color,
			  S32 column_padding) override;
private:
	LLColor4 mColor;
};

class LLScrollListItemSeparator : public LLScrollListItem
{
public:
	LLScrollListItemSeparator();

	void draw(const LLRect& rect, const LLColor4& fg_color,
			  const LLColor4& bg_color, const LLColor4& highlight_color,
			  S32 column_padding) override;
};

class LLScrollListCtrl : public LLUICtrl, public LLEditMenuHandler
{
public:
	enum EOperation
	{
		OP_DELETE = 1,
		OP_SELECT,
		OP_DESELECT,
	};

	LLScrollListCtrl(const std::string& name, const LLRect& rect,
					 void (*commit_callback)(LLUICtrl*, void*),
					 void* callback_userdata,
					 bool allow_multiple_selection, bool draw_border = true);

	~LLScrollListCtrl() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);
	void setScrollListParameters(LLXMLNodePtr node);

	LL_INLINE bool isEmpty() const							{ return mItemList.empty(); }

	LL_INLINE void deleteAllItems()							{ clearRows(); }

	// Sets an array of column descriptors
	void setColumnHeadings(LLSD headings);
	void sortByColumnIndex(U32 column, bool ascending);

	LL_INLINE S32 getItemCount() const						{ return mItemList.size(); }

	// Adds a single column descriptor: ["name" : string, "label" : string,
	// "width" : integer, "relwidth" : integer ]
	void addColumn(const LLSD& column, EAddPosition pos = ADD_BOTTOM);
	void clearColumns();
	void setColumnLabel(const std::string& col, const std::string& l);

	virtual LLScrollListColumn* getColumn(S32 index);

	LL_INLINE S32 getNumColumns() const						{ return mColumnsIndexed.size(); }

	// Adds a single element, from an array of:
	// "columns" => [ "column" => column name, "value" => value,
	// "type" => type, "font" => font, "font-style" => style ], "id" => uuid
	// Creates missing columns automatically.
	virtual LLScrollListItem* addElement(const LLSD& value,
										 EAddPosition pos = ADD_BOTTOM,
										 void* userdata = NULL);
	void clearRows();	// Clears all elements
	void sortByColumn(const std::string& name, bool ascending);

	// This method takes an array of arrays of elements, as above
	void setValue(const LLSD& value) override;

	// Returns the value of the first selected item
	LLSD getValue() const override;

	// DEPRECATED: Use selectByID() below.
	LL_INLINE bool setCurrentByID(const LLUUID& id)
	{
		return selectByValue(LLSD(id));
	}

	LL_INLINE virtual LLUUID getCurrentID() const
	{
		return getStringUUIDSelectedItem();
	}

	bool operateOnSelection(EOperation op);
	bool operateOnAll(EOperation op);

	// Returns false if unable to set the max count so low
	bool setMaxItemCount(S32 max_count);

	// Returns false if item not found
	LL_INLINE bool selectByID(const LLUUID& id)				{ return selectByValue(LLSD(id)); }

	// Match item by value.asString(), which should work for string, integer,
	// UUID. Returns false if not found.
	bool setSelectedByValue(const LLSD& value, bool selected);

	LL_INLINE bool selectByValue(const LLSD value)			{ return setSelectedByValue(value, true); }

	LL_INLINE bool isSorted() const							{ return mSorted; }

	bool isSelected(const LLSD& value) const;

	bool handleClick(S32 x, S32 y, MASK mask);
	bool selectFirstItem();
	bool selectNthItem(S32 index);
	bool selectItemRange(S32 first, S32 last);
	bool selectItemAt(S32 x, S32 y, MASK mask);
	void selectItem(LLScrollListItem* itemp, bool single_select = true);

	void deleteItem(LLScrollListItem* item);
	void deleteSingleItem(S32 index);
	void deleteItems(const LLSD& sd);
	void deleteSelectedItems();
	// By default, goes ahead and commits on selection change
	void deselectAllItems(bool no_commit_on_change = false);

	void highlightNthItem(S32 index);

	void setDoubleClickCallback(void (*cb)(void*)) override	{ mOnDoubleClickCallback = cb; }
	void setMaximumSelectCallback(void (*cb)(void*))		{ mOnMaximumSelectCallback = cb; }
	void setSortChangedCallback(void (*cb)(void*))			{ mOnSortChangedCallback = cb; }

	void swapWithNext(S32 index);
	void swapWithPrevious(S32 index);

	LL_INLINE void setCanSelect(bool can_select)			{ mCanSelect = can_select; }
	LL_INLINE bool getCanSelect() const						{ return mCanSelect; }

	S32 getItemIndex(LLScrollListItem* item) const;
	S32 getItemIndex(const LLUUID& item_id) const;

	LLScrollListItem* addCommentText(const std::string& comment_text,
									 EAddPosition pos = ADD_BOTTOM);
	LLScrollListItem* addSeparator(EAddPosition pos);

	// "Simple" interface: use this when you are creating a list that contains
	// only unique strings, only one of which can be selected at a time.
	LLScrollListItem* addSimpleElement(const std::string& value,
									   EAddPosition pos = ADD_BOTTOM,
									   const LLSD& id = LLSD());

	// false if item not found
	bool selectItemByLabel(const std::string& item,
						   bool case_sensitive = true, S32 column = 0);
	bool selectItemByPrefix(const std::string& target,
							bool case_sensitive = true);
	bool selectItemByPrefix(const LLWString& target,
							bool case_sensitive = true);

	LLScrollListItem* getItemByLabel(const std::string& item,
									 bool case_sensitive = true,
									 S32 column = 0) const;

	LLScrollListItem* getItemByIndex(S32 index) const;

	const std::string getSelectedItemLabel(S32 column = 0) const;
	LLSD getSelectedValue();

	// DEPRECATED: Use LLSD versions of addCommentText() and getSelectedValue().
	// "StringUUID" interface: use this when you are creating a list that
	// contains non-unique strings each of which has an associated, unique
	// UUID, and only one of which can be selected at a time.
	LLScrollListItem* addStringUUIDItem(const std::string& item_text,
										const LLUUID& id,
										EAddPosition pos = ADD_BOTTOM,
										bool enabled = true,
										S32 column_width = 0);
	LLUUID getStringUUIDSelectedItem() const;

	LLScrollListItem* getFirstSelected() const;
	S32 getFirstSelectedIndex() const;
	std::vector<LLScrollListItem*> getAllSelected() const;
	S32 getNumSelected() const;
	uuid_vec_t getSelectedIDs();
	LL_INLINE LLScrollListItem* getLastSelectedItem() const	{ return mLastSelected; }

	// To iterate over all items
	LLScrollListItem* getFirstData() const;
	LLScrollListItem* getLastData() const;
	std::vector<LLScrollListItem*> getAllData() const;

	LLScrollListItem* getItem(const LLSD& sd) const;

	LL_INLINE void setAllowMultipleSelection(bool mult)		{ mAllowMultipleSelection = mult; }

	LL_INLINE void setBgWriteableColor(const LLColor4 &c)	{ mBgWriteableColor = c; }
	LL_INLINE void setReadOnlyBgColor(const LLColor4 &c)	{ mBgReadOnlyColor = c; }
	LL_INLINE void setBgSelectedColor(const LLColor4 &c)	{ mBgSelectedColor = c; }
	LL_INLINE void setBgStripeColor(const LLColor4& c)		{ mBgStripeColor = c; }
	LL_INLINE void setFgSelectedColor(const LLColor4 &c)	{ mFgSelectedColor = c; }
	LL_INLINE void setFgUnselectedColor(const LLColor4 &c)	{ mFgUnselectedColor = c; }
	LL_INLINE void setHighlightedColor(const LLColor4 &c)	{ mHighlightedColor = c; }
	LL_INLINE void setFgDisableColor(const LLColor4 &c)		{ mFgDisabledColor = c; }

	LL_INLINE void setBackgroundVisible(bool b)				{ mBackgroundVisible = b; }
	LL_INLINE void setDrawStripes(bool b)					{ mDrawStripes = b; }
	LL_INLINE void setColumnPadding(S32 c)          		{ mColumnPadding = c; }
	LL_INLINE S32 getColumnPadding()						{ return mColumnPadding; }
	LL_INLINE void setCommitOnKeyboardMovement(bool b)		{ mCommitOnKeyboardMovement = b; }
	LL_INLINE void setCommitOnSelectionChange(bool b)		{ mCommitOnSelectionChange = b; }
	LL_INLINE void setAllowKeyboardMovement(bool b)			{ mAllowKeyboardMovement = b; }

	LL_INLINE void setMaxSelectable(U32 max_selected)		{ mMaxSelectable = max_selected; }
	LL_INLINE S32 getMaxSelectable()						{ return mMaxSelectable; }


	S32 getScrollPos() const;
	void setScrollPos(S32 pos);
	void scrollToShowSelected();

	S32 getSearchColumn();
	LL_INLINE void setSearchColumn(S32 column)				{ mSearchColumn = column; }
	S32 getColumnIndexFromOffset(S32 x);
	S32 getColumnOffsetFromIndex(S32 index);
	S32 getRowOffsetFromIndex(S32 index);

	LL_INLINE void clearSearchString()						{ mSearchString.clear(); }

	// Overridden from LLView
	void draw() override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleUnicodeCharHere(llwchar uni_char) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	bool handleToolTip(S32 x, S32 y, std::string& msg, LLRect* rect) override;
	void setEnabled(bool enabled) override;
	void setFocus(bool b) override;
	void onFocusReceived() override;
	void onFocusLost() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	bool isDirty() const override;
	void resetDirty() override;		// Clears dirty state

	void updateLayout();
	void fitContents(S32 max_width, S32 max_height);

	LLRect getRequiredRect() override;

	LL_INLINE LLRect getItemListRect()						{ return mItemListRect; }

	// Used "internally" by the scroll bar.
	static void onScrollChange(S32 new_pos, LLScrollbar* src, void* userdata);

	static void onClickColumn(void* userdata);

	void updateColumns(bool force_update = false);
	S32 calcMaxContentWidth();
	bool updateColumnWidths();

	void setDisplayHeading(bool display);
	void setHeadingHeight(S32 heading_height);

	LLScrollListItem* hitItem(S32 x,S32 y);

	void scrollToShowLast();

	// LLEditMenuHandler methods
	void copy() override;
	bool canCopy() const override;
	void cut() override;
	bool canCut() const override;
	void selectAll() override;
	bool canSelectAll() const override;
	void deselect() override;
	LL_INLINE bool canDeselect() const override				{ return getCanSelect(); }

	LL_INLINE void setNumDynamicColumns(S32 num)			{ mNumDynamicWidthColumns = num; }
	void updateStaticColumnWidth(LLScrollListColumn* col, S32 new_width);
	LL_INLINE S32 getTotalStaticColumnWidth()				{ return mTotalStaticColumnWidth; }

	std::string getSortColumnName();
	LL_INLINE bool getSortAscending()						{ return mSortColumns.empty() || mSortColumns.back().second; }

	LL_INLINE bool needsSorting()							{ return !mSortColumns.empty(); }
 	LL_INLINE bool hasSortOrder() const						{ return !mSortColumns.empty(); }
	LL_INLINE void clearSortOrder()							{ mSortColumns.clear(); }

	S32 selectMultiple(uuid_vec_t ids);
	void sortItems();

	// Sorts a list without affecting the permanent sort order (so further list
	// insertions can be unsorted, for example)
	void sortOnce(S32 column, bool ascending);

	// Manually call this whenever editing list items in place to flag need for
	// resorting
	LL_INLINE void setSorted(bool sorted)					{ mSorted = sorted; }

	// Some operation has potentially affected column layout or ordering
	void dirtyColumns();

	// When passed false, suspends dirty columns refreshing (useful to gain
	// time when adding many elements in sequence). Passing true also triggers
	// a call to dirtyColumns().
	void setAllowRefresh(bool b);

protected:
	// "Full" interface: use this when you are creating a list that has one or
	// more of the following:
	// * contains icons
	// * contains multiple columns
	// * allows multiple selection
	// * has items that are not guarenteed to have unique names
	// * has additional per-item data (e.g. a UUID or void* userdata)
	//
	// To add items using this approach, create new LLScrollListItems and
	// LLScrollListCells. Add the cells (column entries) to each item, and add
	// the item to the LLScrollListCtrl.
	//
	// The LLScrollListCtrl owns its items and is responsible for deleting them
	// (except in the case that the addItem() call fails, in which case it is
	// up to the caller to delete the item)
	//
	// Returns false if item failed to be added to list, does NOT delete 'item'
	bool addItem(LLScrollListItem* item, EAddPosition pos = ADD_BOTTOM,
				 bool requires_column = true);

	typedef std::deque<LLScrollListItem*> item_list;
	LL_INLINE item_list& getItemList()						{ return mItemList; }

private:
	void selectPrevItem(bool extend_selection);
	void selectNextItem(bool extend_selection);
	void drawItems();
	void updateLineHeight();
	void updateLineHeightInsert(LLScrollListItem* item);
	void reportInvalidInput();
	bool isRepeatedChars(const LLWString& string) const;
	void deselectItem(LLScrollListItem* itemp);
	void commitIfChanged();
	bool setSort(S32 column, bool ascending);

private:
	class LLViewBorder*	mBorder;
	LLScrollListItem*	mLastSelected;
	LLScrollbar*		mScrollbar;

	item_list			mItemList;

	LLWString			mSearchString;

	typedef std::map<std::string, LLScrollListColumn> column_map_t;
	column_map_t		mColumns;

	typedef std::vector<LLScrollListColumn*> ordered_columns_t;
	ordered_columns_t	mColumnsIndexed;

	typedef std::pair<S32, bool> sort_column_t;
	std::vector<sort_column_t> mSortColumns;

	void				(*mOnDoubleClickCallback)(void* userdata);
	void				(*mOnMaximumSelectCallback)(void* userdata);
	void				(*mOnSortChangedCallback)(void* userdata);

	LLFrameTimer		mSearchTimer;

	LLRect				mItemListRect;

	LLColor4			mBgWriteableColor;
	LLColor4			mBgReadOnlyColor;
	LLColor4			mBgSelectedColor;
	LLColor4			mBgStripeColor;
	LLColor4			mFgSelectedColor;
	LLColor4			mFgUnselectedColor;
	LLColor4			mFgDisabledColor;
	LLColor4			mHighlightedColor;

	S32					mLineHeight;	// The maximum height of a single line
	S32					mScrollLines;	// How many lines we have scrolled down
	// Max number of lines is it possible to see on the screen given mRect and
	// mLineHeight:
	S32					mPageLines;
	// The height of the column header buttons, if visible:
	S32					mHeadingHeight;
	U32					mMaxSelectable;

	S32					mMaxItemCount;

	S32             	mColumnPadding;

	S32					mBorderThickness;

	S32					mHighlightedItem;

	S32					mSearchColumn;
	S32					mNumDynamicWidthColumns;
	S32					mTotalStaticColumnWidth;
	S32					mTotalColumnPadding;

	S32					mOriginalSelection;

	bool 				mAllowMultipleSelection;
	bool				mAllowKeyboardMovement;
	bool				mCommitOnKeyboardMovement;
	bool				mCommitOnSelectionChange;
	bool				mSelectionChanged;
	bool				mNeedsScroll;
	bool				mCanSelect;
	bool				mDisplayColumnHeaders;
	bool				mColumnsDirty;
	bool				mColumnWidthsDirty;
	bool				mAllowRefresh;

	bool				mBackgroundVisible;
	bool				mDrawStripes;

	bool				mDirty;
	bool				mSorted;
};

#endif  // LL_SCROLLLISTCTRL_H
