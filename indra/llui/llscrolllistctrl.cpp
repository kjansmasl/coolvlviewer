 /**
 * @file llscrolllistctrl.cpp
 * @brief LLScrollListCtrl base class
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

#include "linden_common.h"

#include <algorithm>
#include "boost/tokenizer.hpp"

#include "llscrolllistctrl.h"

#include "indra_constants.h"
#include "llcheckboxctrl.h"
#include "llclipboard.h"
#include "llcontrol.h"
#include "llkeyboard.h"
#include "llrender.h"
#include "llresizebar.h"
#include "llscrollbar.h"
#include "llstl.h"
#include "llstring.h"
#include "lltimer.h"			// For timeToFormattedString()
#include "lluictrlfactory.h"
#include "llwindow.h"

constexpr S32 MIN_COLUMN_WIDTH = 20;

static const std::string LL_SCROLL_LIST_CTRL_TAG = "scroll_list";
static LLRegisterWidget<LLScrollListCtrl> r20(LL_SCROLL_LIST_CTRL_TAG);

// Local structures & classes.
struct SortScrollListItem
{
	SortScrollListItem(const std::vector<std::pair<S32, bool> >& sort_orders)
	:	mSortOrders(sort_orders)
	{
	}

	bool operator()(const LLScrollListItem* i1, const LLScrollListItem* i2)
	{
		// Sort over all columns in order specified by mSortOrders
		S32 sort_result = 0;
		for (sort_order_t::const_reverse_iterator it = mSortOrders.rbegin(),
			 									  rend = mSortOrders.rend();
			 it != rend; ++it)
		{
			S32 col_idx = it->first;
			bool sort_ascending = it->second;

			const LLScrollListCell* cell1 = i1->getColumn(col_idx);
			const LLScrollListCell* cell2 = i2->getColumn(col_idx);
			// Ascending or descending sort for this column ?
			S32 order = sort_ascending ? 1 : -1;
			if (cell1 && cell2)
			{
				sort_result =
					order *
					LLStringUtil::compareDict(cell1->getValue().asString(),
											  cell2->getValue().asString());
				if (sort_result != 0)
				{
					break; // We have a sort order !
				}
			}
		}

		return sort_result < 0;
	}

	typedef std::vector<std::pair<S32, bool> > sort_order_t;
	const sort_order_t& mSortOrders;
};

//
// LLScrollListIcon
//
LLScrollListIcon::LLScrollListIcon(LLUIImagePtr icon, S32 width)
:	LLScrollListCell(width),
	mIcon(icon),
	mColor(LLColor4::white)
{
}

LLScrollListIcon::LLScrollListIcon(const LLSD& value, S32 width)
:	LLScrollListCell(width),
	mColor(LLColor4::white)
{
	setValue(value);
}

void LLScrollListIcon::setValue(const LLSD& value)
{
	if (value.isUUID())
	{
		// Do not use default image specified by LLUUID::null, use no image in
		// that case
		LLUUID image_id = value.asUUID();
		mIcon = image_id.notNull() ? LLUI::getUIImageByID(image_id)
								   : LLUIImagePtr(NULL);
	}
	else
	{
		std::string value_string = value.asString();
		if (LLUUID::validate(value_string))
		{
			setValue(LLUUID(value_string));
		}
		else if (!value_string.empty())
		{
			mIcon = LLUI::getUIImage(value.asString());
		}
		else
		{
			mIcon = NULL;
		}
	}
}

void LLScrollListIcon::setColor(const LLColor4& color)
{
	mColor = color;
}

S32	LLScrollListIcon::getWidth() const
{
	// if no specified fix width, use width of icon
	if (LLScrollListCell::getWidth() == 0 && mIcon.notNull())
	{
		return mIcon->getWidth();
	}
	return LLScrollListCell::getWidth();
}

void LLScrollListIcon::draw(const LLColor4& color,
							const LLColor4& highlight_color) const
{
	if (mIcon)
	{
		mIcon->draw(0, 0, mColor);
	}
}

//
// LLScrollListCheck
//
LLScrollListCheck::LLScrollListCheck(LLCheckBoxCtrl* check_box, S32 width)
{
	mCheckBox = check_box;
	LLRect rect(mCheckBox->getRect());
	if (width)
	{
		rect.mRight = rect.mLeft + width;
		mCheckBox->setRect(rect);
		setWidth(width);
	}
	else
	{
		setWidth(rect.getWidth());	//check_box->getWidth();
	}
}

LLScrollListCheck::~LLScrollListCheck()
{
	delete mCheckBox;
}

void LLScrollListCheck::draw(const LLColor4& color,
							 const LLColor4& highlight_color) const
{
	mCheckBox->draw();
}

bool LLScrollListCheck::handleClick()
{
	if (mCheckBox->getEnabled())
	{
		mCheckBox->toggle();
	}

	// Do not change selection when clicking on embedded checkbox
	return true;
}

//
// LLScrollListSeparator
//
LLScrollListSeparator::LLScrollListSeparator(S32 width)
:	LLScrollListCell(width)
{
}

//virtual
S32 LLScrollListSeparator::getHeight() const
{
	return 5;
}

void LLScrollListSeparator::draw(const LLColor4& color,
								 const LLColor4& highlight_color) const
{
	// *FIXME: use dynamic item heights and make separators narrow, and
	// inactive
	gl_line_2d(5, 8, llmax(5, getWidth() - 5), 8, color);
}

//
// LLScrollListText
//
U32 LLScrollListText::sCount = 0;

LLScrollListText::LLScrollListText(const std::string& text,
								   const LLFontGL* font, S32 width, U8 style,
								   LLFontGL::HAlign alignment, LLColor4& color,
								   bool use_color, bool visible)
:	LLScrollListCell(width),
	mText(text),
	mFont(font),
	mColor(color),
	mUseColor(use_color),
	mFontStyle(style),
	mFontAlignment(alignment),
	mVisible(visible),
	mHighlightCount(0),
	mHighlightOffset(0)
{
	++sCount;
}

//virtual
LLScrollListText::~LLScrollListText()
{
	--sCount;
}

//virtual
void LLScrollListText::highlightText(S32 offset, S32 num_chars)
{
	mHighlightOffset = offset;
	mHighlightCount = num_chars;
}

//virtual
S32 LLScrollListText::getHeight() const
{
	return ll_roundp(mFont->getLineHeight());
}

void LLScrollListText::draw(const LLColor4& color,
							const LLColor4& highlight_color) const
{
	LLColor4 display_color;
	if (mUseColor)
	{
		display_color = mColor;
	}
	else
	{
		display_color = color;
	}

	if (mHighlightCount > 0)
	{
		S32 left = 0;
		switch (mFontAlignment)
		{
			case LLFontGL::LEFT:
				left = mFont->getWidth(mText.getString(), 0, mHighlightOffset);
				break;

			case LLFontGL::RIGHT:
				left = getWidth() - mFont->getWidth(mText.getString(),
													mHighlightOffset, S32_MAX);
				break;

			case LLFontGL::HCENTER:
				left = (getWidth() - mFont->getWidth(mText.getString())) / 2;
		}
		LLRect highlight_rect(left - 2, ll_roundp(mFont->getLineHeight()) + 1,
							  left + mFont->getWidth(mText.getString(),
							  mHighlightOffset, mHighlightCount) + 1, 1);
		LLUIImage::sRoundedSquare->draw(highlight_rect, highlight_color);
	}

	// Try to draw the entire string
	F32 right_x;
	U32 string_chars = mText.length();
	F32 start_x = 0.f;
	switch (mFontAlignment)
	{
		case LLFontGL::LEFT:
			start_x = 0.f;
			break;

		case LLFontGL::RIGHT:
			start_x = (F32)getWidth();
			break;

		case LLFontGL::HCENTER:
			start_x = (F32)getWidth() * 0.5f;
	}
	mFont->render(mText.getWString(), 0, start_x, 2.f, display_color,
				  mFontAlignment, LLFontGL::BOTTOM, mFontStyle, string_chars,
				  getWidth(), &right_x, false, true);
}

LLScrollListDate::LLScrollListDate(const LLDate& date,
								   const std::string& format,
								   const LLFontGL* font, S32 width, U8 style,
								   LLFontGL::HAlign alignment, LLColor4& color,
								   bool use_color, bool visible)
:	LLScrollListText("", font, width, style, alignment, color, use_color,
					 visible),
	mDate(date),
	mFormat(format)
{
	std::string text;
	if (mFormat.empty())
	{
		text = mDate.asTimeStamp(false);
	}
	else
	{
		timeToFormattedString(mDate.secondsSinceEpoch(), mFormat.c_str(),
							  text);
	}
	LLScrollListText::setValue(text);
}

//virtual
void LLScrollListDate::setValue(const LLSD& value)
{
	mDate = value.asDate();

	std::string text;
	if (mFormat.empty())
	{
		text = mDate.asTimeStamp(false);
	}
	else
	{
		timeToFormattedString(mDate.secondsSinceEpoch(), mFormat.c_str(),
							  text);
	}
	LLScrollListText::setValue(text);
}

//virtual
LLScrollListItem::~LLScrollListItem()
{
	std::for_each(mColumns.begin(), mColumns.end(), DeletePointer());
	mColumns.clear();
}

void LLScrollListItem::setNumColumns(S32 columns)
{
	S32 prev_columns = mColumns.size();
	if (columns < prev_columns)
	{
		std::for_each(mColumns.begin() + columns, mColumns.end(),
					  DeletePointer());
	}

	mColumns.resize(columns);

	for (S32 col = prev_columns; col < columns; ++col)
	{
		mColumns[col] = NULL;
	}
}

void LLScrollListItem::setColumn(S32 column, LLScrollListCell* cell)
{
	if (column < (S32)mColumns.size())
	{
		delete mColumns[column];
		mColumns[column] = cell;
	}
	else
	{
		llwarns << "Bad column number: " << column << " - Ignored." << llendl;
		llassert(false);
	}
}

std::string LLScrollListItem::getContentsCSV() const
{
	std::string ret;

	S32 count = getNumColumns();
	for (S32 i = 0; i < count; ++i)
	{
		if (i)
		{
			ret += ",";
		}
		ret += getColumn(i)->getValue().asString();
	}

	return ret;
}

void LLScrollListItem::draw(const LLRect& rect,
							const LLColor4& fg_color,
							const LLColor4& bg_color,
							const LLColor4& highlight_color,
							S32 column_padding)
{
	// Draw background rect
	LLRect bg_rect = rect;
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4fv(bg_color.mV);
		gl_rect_2d(bg_rect);
	}

	S32 cur_x = rect.mLeft;
	S32 num_cols = getNumColumns();
	S32 cur_col = 0;

	for (LLScrollListCell* cell = getColumn(0); cur_col < num_cols;
		 cell = getColumn(++cur_col))
	{
		// Two ways a cell could be hidden
		if (cell->getWidth() < 0 || !cell->getVisible()) continue;

		LLUI::pushMatrix();
		{
			LLUI::translate((F32) cur_x, (F32) rect.mBottom, 0.0f);

			cell->draw(fg_color, highlight_color);
		}
		LLUI::popMatrix();

		cur_x += cell->getWidth() + column_padding;
	}
}

//---------------------------------------------------------------------------
// LLScrollListItemComment
//---------------------------------------------------------------------------
LLScrollListItemComment::LLScrollListItemComment(const std::string& comment,
												 const LLColor4& color)
:	LLScrollListItem(false),
	mColor(color)
{
	static const LLFontGL* font = LLFontGL::getFontSansSerifSmall();
	addColumn(comment, font);
}

void LLScrollListItemComment::draw(const LLRect& rect,
								   const LLColor4& fg_color,
								   const LLColor4& bg_color,
								   const LLColor4& highlight_color,
								   S32 column_padding)
{
	LLScrollListCell* cell = getColumn(0);
	if (cell)
	{
		// Two ways a cell could be hidden
		if (cell->getWidth() < 0 || !cell->getVisible()) return;

		LLUI::pushMatrix();
		{
			LLUI::translate((F32)rect.mLeft, (F32)rect.mBottom, 0.0f);

			// Force first cell to be width of entire item
			cell->setWidth(rect.getWidth());
			cell->draw(mColor, highlight_color);
		}
		LLUI::popMatrix();
	}
}

//---------------------------------------------------------------------------
// LLScrollListItemSeparator
//---------------------------------------------------------------------------
LLScrollListItemSeparator::LLScrollListItemSeparator()
:	LLScrollListItem(false)
{
	LLScrollListSeparator* cell = new LLScrollListSeparator(0);
	setNumColumns(1);
	setColumn(0, cell);
}

void LLScrollListItemSeparator::draw(const LLRect& rect,
									 const LLColor4& fg_color,
									 const LLColor4& bg_color,
									 const LLColor4& highlight_color,
									 S32 column_padding)
{
	// *TODO: move LLScrollListSeparator::draw into here and get rid of it
	LLScrollListCell* cell = getColumn(0);
	if (cell)
	{
		// Two ways a cell could be hidden
		if (cell->getWidth() < 0 || !cell->getVisible()) return;

		LLUI::pushMatrix();
		{
			LLUI::translate((F32)rect.mLeft, (F32)rect.mBottom, 0.0f);

			// Force first cell to be width of entire item
			cell->setWidth(rect.getWidth());
			cell->draw(fg_color, highlight_color);
		}
		LLUI::popMatrix();
	}
}

//----------------------------------------------------------------------------
// LLScrollListCtrl
//----------------------------------------------------------------------------

LLScrollListCtrl::LLScrollListCtrl(const std::string& name, const LLRect& rect,
								   void (*commit_callback)(LLUICtrl*, void*),
								   void* userdata, bool multi_select,
								   bool show_border)
:	LLUICtrl(name, rect, true, commit_callback, userdata),
	mLineHeight(0),
	mScrollLines(0),
	mPageLines(0),
	mHeadingHeight(20),
	mMaxSelectable(0),
	mAllowMultipleSelection(multi_select),
	mAllowKeyboardMovement(true),
	mCommitOnKeyboardMovement(true),
	mCommitOnSelectionChange(false),
	mSelectionChanged(false),
	mDirty(true),
	mNeedsScroll(false),
	mCanSelect(true),
	mDisplayColumnHeaders(false),
	mColumnsDirty(false),
	mColumnWidthsDirty(true),
	mSorted(true),
	mAllowRefresh(true),
	mMaxItemCount(INT_MAX),
	mBackgroundVisible(true),
	mDrawStripes(true),
	mBgWriteableColor(LLUI::sScrollBgWriteableColor),
	mBgReadOnlyColor(LLUI::sScrollBgReadOnlyColor),
	mBgSelectedColor(LLUI::sScrollSelectedBGColor),
	mBgStripeColor(LLUI::sScrollBGStripeColor),
	mFgSelectedColor(LLUI::sScrollSelectedFGColor),
	mFgUnselectedColor(LLUI::sScrollUnselectedColor),
	mFgDisabledColor(LLUI::sScrollDisabledColor),
	mHighlightedColor(LLUI::sScrollHighlightedColor),
	mBorderThickness(2),
	mOnDoubleClickCallback(NULL),
	mOnMaximumSelectCallback(NULL),
	mOnSortChangedCallback(NULL),
	mHighlightedItem(-1),
	mBorder(NULL),
	mSearchColumn(0),
	mNumDynamicWidthColumns(0),
	mTotalStaticColumnWidth(0),
	mTotalColumnPadding(0),
	mColumnPadding(5),
	mLastSelected(NULL),
	mOriginalSelection(-1)
{
	mItemListRect.setOriginAndSize(mBorderThickness, mBorderThickness,
								   getRect().getWidth() - 2 * mBorderThickness,
								   getRect().getHeight() - 2 * mBorderThickness);

	updateLineHeight();

	mPageLines = mLineHeight ? mItemListRect.getHeight() / mLineHeight : 0;

	// Initialize the scrollbar
	LLRect scroll_rect;
	scroll_rect.setOriginAndSize(getRect().getWidth() - mBorderThickness -
								 SCROLLBAR_SIZE,
								 mItemListRect.mBottom, SCROLLBAR_SIZE,
								 mItemListRect.getHeight());
	mScrollbar = new LLScrollbar("Scrollbar", scroll_rect,
								 LLScrollbar::VERTICAL, getItemCount(),
								 mScrollLines, mPageLines,
								 &LLScrollListCtrl::onScrollChange, this);
	mScrollbar->setFollowsRight();
	mScrollbar->setFollowsTop();
	mScrollbar->setFollowsBottom();
	mScrollbar->setEnabled(true);
	// Scrollbar is visible only when needed
	mScrollbar->setVisible(false);
	addChild(mScrollbar);

	// Border
	if (show_border)
	{
		LLRect border_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
		mBorder = new LLViewBorder("dlg border", border_rect,
								   LLViewBorder::BEVEL_IN,
								   LLViewBorder::STYLE_LINE, 1);
		addChild(mBorder);
	}
}

S32 LLScrollListCtrl::getSearchColumn()
{
	// Search for proper search column
	if (mSearchColumn < 0)
	{
		LLScrollListItem* itemp = getFirstData();
		if (itemp)
		{
			for (S32 column = 0; column < getNumColumns(); ++column)
			{
				LLScrollListCell* cell = itemp->getColumn(column);
				if (cell && cell->isText())
				{
					mSearchColumn = column;
					break;
				}
			}
		}
	}
	return llclamp(mSearchColumn, 0, getNumColumns());
}

LLScrollListCtrl::~LLScrollListCtrl()
{
	std::for_each(mItemList.begin(), mItemList.end(), DeletePointer());
	mItemList.clear();
	clearColumns();	// Clears columns and deletes headers
}

bool LLScrollListCtrl::setMaxItemCount(S32 max_count)
{
	if (max_count >= getItemCount())
	{
		mMaxItemCount = max_count;
	}
	return max_count == mMaxItemCount;
}

// LLScrolListInterface method (was deleteAllItems)
//virtual
void LLScrollListCtrl::clearRows()
{
	std::for_each(mItemList.begin(), mItemList.end(), DeletePointer());
	mItemList.clear();

	// Scroll the bar back up to the top.
	mScrollbar->setDocParams(0, 0);

	mScrollLines = 0;
	mLastSelected = NULL;
	updateLayout();
	mDirty = mSorted = false;
}

LLScrollListItem* LLScrollListCtrl::getFirstSelected() const
{
	if (!getCanSelect())
	{
		return NULL;
	}

	for (item_list::const_iterator iter = mItemList.begin(),
								   end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		if (item && item->getSelected())
		{
			return item;
		}
	}

	return NULL;
}

std::vector<LLScrollListItem*> LLScrollListCtrl::getAllSelected() const
{
	std::vector<LLScrollListItem*> ret;

	if (!getCanSelect())
	{
		return ret;
	}

	for (item_list::const_iterator iter = mItemList.begin(),
								   end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		if (item && item->getSelected())
		{
			ret.push_back(item);
		}
	}

	return ret;
}

uuid_vec_t LLScrollListCtrl::getSelectedIDs()
{
	uuid_vec_t ids;

	std::vector<LLScrollListItem*> selected = getAllSelected();
	for (std::vector<LLScrollListItem*>::iterator it = selected.begin(),
												  end = selected.end();
		 it != end; ++it)
	{
		ids.emplace_back((*it)->getUUID());
	}

	return ids;
}

S32 LLScrollListCtrl::getNumSelected() const
{
	S32 selected = 0;

	for (item_list::const_iterator iter = mItemList.begin(),
								   end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		if (item && item->getSelected())
		{
			++selected;
		}
	}

	return selected;
}

S32 LLScrollListCtrl::getFirstSelectedIndex() const
{
	if (!getCanSelect())
	{
		return -1;
	}

	S32 cur_selected_idx = 0;
	item_list::const_iterator iter;
	for (item_list::const_iterator it = mItemList.begin(),
								   end = mItemList.end();
		 it != end; ++it)
	{
		LLScrollListItem* item = *it;
		if (item && item->getSelected())
		{
			return cur_selected_idx;
		}
		++cur_selected_idx;
	}

	return -1;
}

LLScrollListItem* LLScrollListCtrl::getFirstData() const
{
	return mItemList.empty() ? NULL : mItemList[0];
}

LLScrollListItem* LLScrollListCtrl::getLastData() const
{
	size_t count = mItemList.size();
	if (count == 0)
	{
		return NULL;
	}
	return mItemList[count - 1];
}

std::vector<LLScrollListItem*> LLScrollListCtrl::getAllData() const
{
	std::vector<LLScrollListItem*> ret;

	for (item_list::const_iterator it = mItemList.begin(),
								   end = mItemList.end();
		 it != end; ++it)
	{
		LLScrollListItem* item = *it;
		if (item)
		{
			ret.push_back(item);
		}
	}

	return ret;
}

// Returns the first matching item
LLScrollListItem* LLScrollListCtrl::getItem(const LLSD& sd) const
{
	std::string string_val = sd.asString();

	for (item_list::const_iterator it = mItemList.begin(),
								   end = mItemList.end();
		 it != end; ++it)
	{
		LLScrollListItem* item = *it;
		// Assumes string representation is good enough for comparison
		if (item && item->getValue().asString() == string_val)
		{
			return item;
		}
	}

	return NULL;
}

void LLScrollListCtrl::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLUICtrl::reshape(width, height, called_from_parent);
	updateLayout();
}

void LLScrollListCtrl::updateLayout()
{
	// Reserve room for column headers, if needed
	S32 heading_size = mDisplayColumnHeaders ? mHeadingHeight : 0;
	mItemListRect.setOriginAndSize(mBorderThickness, mBorderThickness,
								   getRect().getWidth() - 2 * mBorderThickness,
								   getRect().getHeight() -
								   2 * mBorderThickness - heading_size);

	// How many lines of content in a single "page" ?
	mPageLines = mLineHeight ? mItemListRect.getHeight() / mLineHeight : 0;
	bool scrollbar_visible = getItemCount() > mPageLines;
	if (scrollbar_visible)
	{
		// Provide space on the right for scrollbar
		mItemListRect.mRight = getRect().getWidth() - mBorderThickness -
							   SCROLLBAR_SIZE;

		mScrollbar->reshape(SCROLLBAR_SIZE,
							mItemListRect.getHeight() +
							(mDisplayColumnHeaders ? mHeadingHeight : 0));
	}
	mScrollbar->setPageSize(mPageLines);
	mScrollbar->setDocSize(getItemCount());
	mScrollbar->setVisible(scrollbar_visible);

	dirtyColumns();
}

// Attempt to size the control to show all items. Do not make larger than width
// or height.
void LLScrollListCtrl::fitContents(S32 max_width, S32 max_height)
{
	S32 height = llmin(getRequiredRect().getHeight(), max_height);
	S32 width = getRect().getWidth();
	reshape(width, height);
}

LLRect LLScrollListCtrl::getRequiredRect()
{
	S32 hsize = mDisplayColumnHeaders ? mHeadingHeight : 0;
	S32 height = mLineHeight * getItemCount() + 2 * mBorderThickness + hsize;
	S32 width = getRect().getWidth();
	return LLRect(0, height, width, 0);
}

bool LLScrollListCtrl::addItem(LLScrollListItem* item, EAddPosition pos,
							   bool requires_column)
{
	bool not_too_big = getItemCount() < mMaxItemCount;
	if (not_too_big)
	{
		switch (pos)
		{
			case ADD_TOP:
				mItemList.push_front(item);
				break;

			case ADD_SORTED:
			{
				// Sort by column 0, in ascending order
				std::vector<sort_column_t> single_sort_column;
				single_sort_column.emplace_back(0, true);

				mItemList.push_back(item);
				std::stable_sort(mItemList.begin(), mItemList.end(),
								 SortScrollListItem(single_sort_column));

				// ADD_SORTED just sorts by first column...
				// this might not match user sort criteria, so flag list as
				// being in unsorted state
				break;
			}

			case ADD_BOTTOM:
				mItemList.push_back(item);
				break;

			default:
				llwarns << "Invalid position: " << pos << " - For list: "
						<< getName() << ". Item added at bottom." << llendl;
				llassert(false);
				mItemList.push_back(item);
		}

		setSorted(false);

		// Create new column on demand
		if (mColumns.empty() && requires_column)
		{
			LLSD new_column;
			new_column["name"] = "default_column";
			new_column["label"] = "";
			new_column["dynamicwidth"] = true;
			addColumn(new_column);
		}

		S32 num_cols = item->getNumColumns();
		S32 i = 0;
		for (LLScrollListCell* cell = item->getColumn(i); i < num_cols;
			 cell = item->getColumn(++i))
		{
			if (i >= (S32)mColumnsIndexed.size()) break;

			cell->setWidth(mColumnsIndexed[i]->getWidth());
		}

		updateLineHeightInsert(item);

		updateLayout();
	}

	return not_too_big;
}

// NOTE: This is *very* expensive for large lists, especially when we are
// dirtying the list every frame while receiving a long list of names.
// *TODO: Use bookkeeping to make this an incremental cost with item additions
S32 LLScrollListCtrl::calcMaxContentWidth()
{
	static const LLFontGL* font = LLFontGL::getFontSansSerifSmall();

	constexpr S32 HEADING_TEXT_PADDING = 25;
	constexpr S32 COLUMN_TEXT_PADDING = 10;

	S32 max_item_width = 0;

	for (ordered_columns_t::iterator it = mColumnsIndexed.begin(),
									 end = mColumnsIndexed.end();
		 it != end; ++it)
	{
		LLScrollListColumn* column = *it;
		if (!column) continue;	// Paranoia

		if (mColumnWidthsDirty)
		{
			// Update max content width for this column, by looking at all
			// items
			S32 new_width = 0;
			if (column->mHeader)
			{
				new_width = font->getWidth(column->mLabel) + mColumnPadding +
							HEADING_TEXT_PADDING;
			}

			for (item_list::iterator it2 = mItemList.begin(),
									 end2 = mItemList.end();
				 it2 != end2; ++it2)
			{
				LLScrollListCell* cellp = (*it2)->getColumn(column->mIndex);
				if (!cellp) continue;

				new_width =
					llmax(font->getWidth(cellp->getValue().asString()) +
						  mColumnPadding + COLUMN_TEXT_PADDING,
						  new_width);
			}

			column->mMaxContentWidth = new_width;
		}

		max_item_width += column->mMaxContentWidth;
	}

	mColumnWidthsDirty = false;

	return max_item_width;
}

bool LLScrollListCtrl::updateColumnWidths()
{
	bool width_changed = false;

	for (ordered_columns_t::iterator column_it = mColumnsIndexed.begin(),
									 end = mColumnsIndexed.end();
		 column_it != end; ++column_it)
	{
		LLScrollListColumn* column = *column_it;
		if (!column) continue;

		// Update column width
		S32 new_width = 0;
		if (column->mRelWidth >= 0)
		{
			new_width = ll_roundp(column->mRelWidth *
								  (mItemListRect.getWidth() -
								   mTotalStaticColumnWidth -
								   mTotalColumnPadding));
		}
		else if (column->mDynamicWidth && mNumDynamicWidthColumns > 0)
		{
			new_width = (mItemListRect.getWidth() - mTotalStaticColumnWidth -
						 mTotalColumnPadding) / mNumDynamicWidthColumns;
		}
		else
		{
			new_width = column->getWidth();
		}

		if (column->getWidth() != new_width)
		{
			column->setWidth(new_width);
			width_changed = true;
		}
	}

	return width_changed;
}

constexpr S32 SCROLL_LIST_ROW_PAD = 2;

// Line height is the max height of all the cells in all the items.
void LLScrollListCtrl::updateLineHeight()
{
	mLineHeight = 0;
	for (item_list::iterator iter = mItemList.begin(), end = mItemList.end();
		 iter != end; ++iter)
	{
		updateLineHeightInsert(*iter);
	}
}

// When the only change to line height is from an insert, we do not need to
// scan the entire list
void LLScrollListCtrl::updateLineHeightInsert(LLScrollListItem* itemp)
{
	if (!itemp) return;

	for (S32 i = 0, count = itemp->getNumColumns(); i < count; ++i)
	{
		const LLScrollListCell* cell = itemp->getColumn(i);
		if (cell)
		{
			mLineHeight = llmax(mLineHeight,
								cell->getHeight() + SCROLL_LIST_ROW_PAD);
		}
	}
}

void LLScrollListCtrl::updateColumns(bool force_update)
{
	if (!mColumnsDirty && !force_update)
	{
		return;
	}
	mColumnsDirty = false;

	bool columns_changed_width = updateColumnWidths();

	// Update column headers
	S32 left = mItemListRect.mLeft;
	S32 top = mItemListRect.mTop;
	S32 width = mItemListRect.getWidth();
	LLColumnHeader* last_header = NULL;
	for (size_t i = 0, count = mColumnsIndexed.size(); i < count; ++i)
	{
		LLScrollListColumn* column = mColumnsIndexed[i];
		if (column && column->mHeader && column->getWidth() >= 0)
		{
			last_header = column->mHeader;
			last_header->updateResizeBars();

			S32 right = left + column->getWidth();
			if (column->mIndex != (S32)mColumnsIndexed.size() - 1)
			{
				right += mColumnPadding;
			}
			right = llmax(left, llmin(width, right));

			S32 header_width = right - left;
			last_header->reshape(header_width, mHeadingHeight);
			last_header->translate(left - last_header->getRect().mLeft,
								   top - last_header->getRect().mBottom);
			last_header->setVisible(mDisplayColumnHeaders && header_width > 0);
			left = right;
		}
	}

	// Expand last column header we encountered to full list width
	if (last_header && last_header->canResize())
	{
		S32 new_width =
			llmax(0, mItemListRect.mRight - last_header->getRect().mLeft);
		last_header->reshape(new_width, last_header->getRect().getHeight());
		last_header->setVisible(mDisplayColumnHeaders && new_width > 0);
		last_header->getColumn()->setWidth(new_width);
	}

	if (columns_changed_width || force_update)
 	{
		// Propagate column widths to individual cells
		for (item_list::iterator iter = mItemList.begin(), end = mItemList.end();
			 iter != end; ++iter)
		{
			LLScrollListItem* itemp = *iter;
			S32 num_cols = itemp->getNumColumns();
			S32 i = 0;
			for (LLScrollListCell* cell = itemp->getColumn(i); i < num_cols;
				 cell = itemp->getColumn(++i))
			{
				if (i >= (S32)mColumnsIndexed.size()) break;

				cell->setWidth(mColumnsIndexed[i]->getWidth());
			}
		}
	}
}

void LLScrollListCtrl::setDisplayHeading(bool display)
{
	mDisplayColumnHeaders = display;
	updateLayout();
}

void LLScrollListCtrl::setHeadingHeight(S32 heading_height)
{
	mHeadingHeight = heading_height;
	updateLayout();
}

bool LLScrollListCtrl::selectFirstItem()
{
	bool success = false;

	// Our $%&@#$()^%#$()*^ iterators do not let us check against the first
	// item inside out iteration
	bool first_item = true;

	for (item_list::iterator iter = mItemList.begin(), end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* itemp = *iter;
		if (!itemp) continue;	// Paranoia

		if (first_item && itemp->getEnabled())
		{
			if (!itemp->getSelected())
			{
				selectItem(itemp);
			}
			success = true;
			mOriginalSelection = 0;
		}
		else
		{
			deselectItem(itemp);
		}
		first_item = false;
	}
	if (mCommitOnSelectionChange)
	{
		commitIfChanged();
	}
	return success;
}

// Deselects all other items
//virtual
bool LLScrollListCtrl::selectNthItem(S32 target_index)
{
	S32 count = mItemList.size();
	if (!count || target_index < 0 || target_index >= count)
	{
		return false;
	}
	return selectItemRange(target_index, target_index);
}

//virtual
bool LLScrollListCtrl::selectItemRange(S32 first_index, S32 last_index)
{
	if (mItemList.empty())
	{
		return false;
	}

	S32 listlen = (S32)mItemList.size();
	first_index = llclamp(first_index, 0, listlen - 1);

	if (last_index < 0)
	{
		last_index = listlen - 1;
	}
	else
	{
		last_index = llclamp(last_index, first_index, listlen - 1);
	}

	bool success = false;
	S32 index = 0;
	for (item_list::iterator iter = mItemList.begin(),
							 end = mItemList.end(); iter != end; )
	{
		LLScrollListItem* itemp = *iter;
		if (!itemp)
		{
			iter = mItemList.erase(iter);
			continue;
		}

		if (index >= first_index && index <= last_index)
		{
			if (itemp->getEnabled())
			{
				selectItem(itemp, false);
				success = true;
			}
		}
		else
		{
			deselectItem(itemp);
		}
		++index;
		++iter;
	}

	if (mCommitOnSelectionChange)
	{
		commitIfChanged();
	}

	mSearchString.clear();

	return success;
}

void LLScrollListCtrl::swapWithNext(S32 index)
{
	if (index >= (S32)mItemList.size() - 1)
	{
		// At end of list, does not do anything
		return;
	}
	LLScrollListItem* cur_itemp = mItemList[index];
	mItemList[index] = mItemList[index + 1];
	mItemList[index + 1] = cur_itemp;
}

void LLScrollListCtrl::swapWithPrevious(S32 index)
{
	if (index <= 0)
	{
		// At beginning of list, don't do anything
	}

	LLScrollListItem* cur_itemp = mItemList[index];
	mItemList[index] = mItemList[index - 1];
	mItemList[index - 1] = cur_itemp;
}

void LLScrollListCtrl::deleteSingleItem(S32 target_index)
{
	if (target_index < 0 || target_index >= (S32)mItemList.size())
	{
		return;
	}

	LLScrollListItem* itemp = mItemList[target_index];
	if (itemp == mLastSelected)
	{
		mLastSelected = NULL;
	}
	delete itemp;
	mItemList.erase(mItemList.begin() + target_index);
	dirtyColumns();
}

void LLScrollListCtrl::deleteItem(LLScrollListItem* item)
{
	if (item)
	{
		S32 index = getItemIndex(item);
		if (index >= 0)
		{
			deleteSingleItem(index);
		}
	}
}

//FIXME: refactor item deletion
void LLScrollListCtrl::deleteItems(const LLSD& sd)
{
	item_list::iterator iter;
	for (iter = mItemList.begin(); iter < mItemList.end(); )
	{
		LLScrollListItem* itemp = *iter;
		if (itemp && itemp->getValue().asString() == sd.asString())
		{
			if (itemp == mLastSelected)
			{
				mLastSelected = NULL;
			}
			delete itemp;
			iter = mItemList.erase(iter);
		}
		else
		{
			++iter;
		}
	}

	dirtyColumns();
}

void LLScrollListCtrl::deleteSelectedItems()
{
	item_list::iterator iter;
	for (iter = mItemList.begin(); iter < mItemList.end(); )
	{
		LLScrollListItem* itemp = *iter;
		if (itemp && itemp->getSelected())
		{
			delete itemp;
			iter = mItemList.erase(iter);
		}
		else
		{
			++iter;
		}
	}
	mLastSelected = NULL;
	dirtyColumns();
}

void LLScrollListCtrl::highlightNthItem(S32 target_index)
{
	if (mHighlightedItem != target_index)
	{
		mHighlightedItem = target_index;
	}
}

S32	LLScrollListCtrl::selectMultiple(uuid_vec_t ids)
{
	S32 count = 0;

	for (item_list::iterator iter = mItemList.begin(), end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		uuid_vec_t::iterator iditr;
		uuid_vec_t::iterator end2 = ids.end();
		for (iditr = ids.begin(); iditr != end2; ++iditr)
		{
			if (item && item->getEnabled() && item->getUUID() == (*iditr))
			{
				selectItem(item, false);
				++count;
				break;
			}
		}
		if (iditr != end2)
		{
			ids.erase(iditr);
		}
	}

	if (mCommitOnSelectionChange)
	{
		commitIfChanged();
	}

	return count;
}

S32 LLScrollListCtrl::getItemIndex(LLScrollListItem* target_item) const
{
	S32 index = 0;
	for (item_list::const_iterator iter = mItemList.begin(),
								   end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* itemp = *iter;
		if (target_item == itemp)
		{
			return index;
		}
		++index;
	}

	return -1;
}

S32 LLScrollListCtrl::getItemIndex(const LLUUID& target_id) const
{
	S32 index = 0;
	for (item_list::const_iterator iter = mItemList.begin(),
								   end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* itemp = *iter;
		if (target_id == itemp->getUUID())
		{
			return index;
		}
		++index;
	}

	return -1;
}

void LLScrollListCtrl::selectPrevItem(bool extend_selection)
{
	LLScrollListItem* prev_item = NULL;

	if (!getFirstSelected())
	{
		// Select last item
		selectNthItem(getItemCount() - 1);
	}
	else
	{
		for (item_list::iterator iter = mItemList.begin(),
								 end = mItemList.end();
			 iter != end; ++iter)
		{
			LLScrollListItem* cur_item = *iter;

			if (cur_item->getSelected())
			{
				if (prev_item)
				{
					selectItem(prev_item, !extend_selection);
				}
				else
				{
					reportInvalidInput();
				}
				break;
			}

			// Do not allow navigation to disabled elements
			prev_item = cur_item->getEnabled() ? cur_item : prev_item;
		}
	}

	if (mCommitOnSelectionChange)
	{
		commitIfChanged();
	}

	mSearchString.clear();
}

void LLScrollListCtrl::selectNextItem(bool extend_selection)
{
	LLScrollListItem* next_item = NULL;

	if (!getFirstSelected())
	{
		selectFirstItem();
	}
	else
	{
		for (item_list::reverse_iterator iter = mItemList.rbegin(),
										 rend = mItemList.rend();
			 iter != rend; ++iter)
		{
			LLScrollListItem* cur_item = *iter;

			if (cur_item->getSelected())
			{
				if (next_item)
				{
					selectItem(next_item, !extend_selection);
				}
				else
				{
					reportInvalidInput();
				}
				break;
			}

			// Do not allow navigation to disabled items
			next_item = cur_item->getEnabled() ? cur_item : next_item;
		}
	}

	if (mCommitOnSelectionChange)
	{
		commitIfChanged();
	}

	mSearchString.clear();
}

void LLScrollListCtrl::deselectAllItems(bool no_commit_on_change)
{
	for (item_list::iterator iter = mItemList.begin(), end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		deselectItem(item);
	}

	if (mCommitOnSelectionChange && !no_commit_on_change)
	{
		commitIfChanged();
	}
}

////////////////////////////////////////////////////////////////////////////////
// Use this to add comment text such as "Searching", which ignores column
// settings of list

LLScrollListItem* LLScrollListCtrl::addCommentText(const std::string& comment_text,
												   EAddPosition pos)
{
	LLScrollListItem* item = NULL;
	if (getItemCount() < mMaxItemCount)
	{
		// Always draw comment text with "enabled" color
		item = new LLScrollListItemComment(comment_text, mFgUnselectedColor);
		addItem(item, pos, false);
	}
	return item;
}

LLScrollListItem* LLScrollListCtrl::addSeparator(EAddPosition pos)
{
	LLScrollListItem* item = new LLScrollListItemSeparator();
	addItem(item, pos, false);
	return item;
}

// Selects first enabled item of the given name.
// Returns false if item not found.
bool LLScrollListCtrl::selectItemByLabel(const std::string& label,
										 bool case_sensitive, S32 column)
{
	// Ensure that no stale items are selected, even if we don't find a match
	deselectAllItems(true);

	// RN: assume no empty items
	if (label.empty())
	{
		return false;
	}

	std::string target_text = label;
	if (!case_sensitive)
	{
		LLStringUtil::toLower(target_text);
	}

	bool found = false;

	for (item_list::iterator iter = mItemList.begin(), end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		// Only select enabled items with matching names
		std::string item_text = item->getColumn(column)->getValue().asString();
		if (!case_sensitive)
		{
			LLStringUtil::toLower(item_text);
		}
		found = item->getEnabled() && item_text == target_text;
		if (found)
		{
			selectItem(item);
			break;
		}
	}

	if (mCommitOnSelectionChange)
	{
		commitIfChanged();
	}

	return found;
}

LLScrollListItem* LLScrollListCtrl::getItemByLabel(const std::string& label,
												   bool case_sensitive,
												   S32 column) const
{
	// RN: assume no empty items
	if (label.empty())
	{
		return NULL;
	}

	std::string target_text = label;
	if (!case_sensitive)
	{
		LLStringUtil::toLower(target_text);
	}

	for (item_list::const_iterator iter = mItemList.begin(),
								   end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		// Only select enabled items with matching names
		std::string item_text = item->getColumn(column)->getValue().asString();
		if (!case_sensitive)
		{
			LLStringUtil::toLower(item_text);
		}
		if (item_text == target_text)
		{
			return item;
		}
	}

	return NULL;
}

LLScrollListItem* LLScrollListCtrl::getItemByIndex(S32 index) const
{
	if (index < 0 || mItemList.empty())
	{
		return NULL;
	}

	item_list::const_iterator iter = mItemList.begin();
	item_list::const_iterator end = mItemList.end();
	while (index-- > 0 && iter != end)
	{
		++iter;
	}

	return iter == end ? NULL : *iter;
}

bool LLScrollListCtrl::selectItemByPrefix(const std::string& target,
										  bool case_sensitive)
{
	return selectItemByPrefix(utf8str_to_wstring(target), case_sensitive);
}

// Selects first enabled item that has a name where the name's first part
// matched the target string. Returns false if item not found.
bool LLScrollListCtrl::selectItemByPrefix(const LLWString& target,
										  bool case_sensitive)
{
	bool found = false;

	LLWString target_trimmed(target);
	S32 target_len = target_trimmed.size();

	if (0 == target_len)
	{
		// Is "" a valid choice?
		for (item_list::iterator iter = mItemList.begin(),
								 end = mItemList.end();
			 iter != end; ++iter)
		{
			LLScrollListItem* item = *iter;
			// Only select enabled items with matching names
			LLScrollListCell* cellp = item->getColumn(getSearchColumn());
			bool select = cellp ? item->getEnabled() &&
								  cellp->getValue().asString()[0] == '\0'
								: false;
			if (select)
			{
				selectItem(item);
				found = true;
				break;
			}
		}
	}
	else
	{
		if (!case_sensitive)
		{
			// Do comparisons in lower case
			LLWStringUtil::toLower(target_trimmed);
		}

		for (item_list::iterator iter = mItemList.begin(),
								 end = mItemList.end();
			 iter != end; ++iter)
		{
			LLScrollListItem* item = *iter;

			// Only select enabled items with matching names
			LLScrollListCell* cellp = item->getColumn(getSearchColumn());
			if (!cellp)
			{
				continue;
			}
			LLWString item_label = utf8str_to_wstring(cellp->getValue().asString());
			if (!case_sensitive)
			{
				LLWStringUtil::toLower(item_label);
			}
			// remove extraneous whitespace from searchable label
			LLWString trimmed_label = item_label;
			LLWStringUtil::trim(trimmed_label);

			bool select = item->getEnabled() &&
						  trimmed_label.compare(0, target_trimmed.size(),
												target_trimmed) == 0;

			if (select)
			{
				// find offset of matching text (might have leading whitespace)
				S32 offset = item_label.find(target_trimmed);
				cellp->highlightText(offset, target_trimmed.size());
				selectItem(item);
				found = true;
				break;
			}
		}
	}

	if (mCommitOnSelectionChange)
	{
		commitIfChanged();
	}

	return found;
}

const std::string LLScrollListCtrl::getSelectedItemLabel(S32 column) const
{
	LLScrollListItem* item = getFirstSelected();
	if (item && item->getColumn(column))
	{
		return item->getColumn(column)->getValue().asString();
	}

	return LLStringUtil::null;
}

////////////////////////////////////////////////////////////////////////////////
// "StringUUID" interface: use this when you're creating a list that contains
// non-unique strings each of which has an associated, unique UUID, and only one
// of which can be selected at a time.

LLScrollListItem* LLScrollListCtrl::addStringUUIDItem(const std::string& item_text,
													  const LLUUID& id,
													  EAddPosition pos,
													  bool enabled,
													  S32 column_width)
{
	static const LLFontGL* font = LLFontGL::getFontSansSerifSmall();

	LLScrollListItem* item = NULL;
	if (getItemCount() < mMaxItemCount)
	{
		item = new LLScrollListItem(enabled, NULL, id);
		item->addColumn(item_text, font, column_width);
		addItem(item, pos);
	}
	return item;
}

// Select the line or lines that match this UUID
bool LLScrollListCtrl::setSelectedByValue(const LLSD& value, bool selected)
{
	bool found = false;

	if (selected && !mAllowMultipleSelection)
	{
		deselectAllItems(true);
	}

	for (item_list::iterator iter = mItemList.begin(), end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		if (item && item->getEnabled() &&
			item->getValue().asString() == value.asString())
		{
			if (selected)
			{
				selectItem(item);
			}
			else
			{
				deselectItem(item);
			}
			found = true;
			break;
		}
	}

	if (mCommitOnSelectionChange)
	{
		commitIfChanged();
	}

	return found;
}

bool LLScrollListCtrl::isSelected(const LLSD& value) const
{
	for (item_list::const_iterator iter = mItemList.begin(),
								   end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		if (item && item->getValue().asString() == value.asString())
		{
			return item->getSelected();
		}
	}
	return false;
}

LLUUID LLScrollListCtrl::getStringUUIDSelectedItem() const
{
	LLScrollListItem* item = getFirstSelected();
	return item ? item->getUUID() : LLUUID::null;
}

LLSD LLScrollListCtrl::getSelectedValue()
{
	LLScrollListItem* item = getFirstSelected();
	return item ? item->getValue() : LLSD();
}

void LLScrollListCtrl::drawItems()
{
	S32 first_line = mScrollLines;
	S32 count = mItemList.size();
	if (first_line >= count)
	{
		return;
	}

	LLGLSUIDefault gls_ui;
	LLLocalClipRect clip(mItemListRect);

	static LLColor4 highlight_color = LLColor4::white;
	highlight_color.mV[VALPHA] =
		clamp_rescale(mSearchTimer.getElapsedTimeF32(),
					  LLUI::sTypeAheadTimeout * 0.7f,
					  LLUI::sTypeAheadTimeout, 0.4f, 0.f);

	LLRect item_rect;
	LLColor4* fg_color;
	LLColor4* bg_color;
	S32 list_width = mItemListRect.getWidth();
	S32 x = mItemListRect.mLeft;
	S32 y = mItemListRect.mTop - mLineHeight;
	S32 cur_y = y;
	S32 max_columns = 0;
	// Allow for partial line at bottom
	S32 num_page_lines = mPageLines + 1;
	S32 last_line = llmin(count - 1, mScrollLines + num_page_lines);
	for (S32 line = first_line; line <= last_line; ++line)
	{
		LLScrollListItem* item = mItemList[line];
		if (!item) continue;

		item_rect.setOriginAndSize(x, cur_y, list_width, mLineHeight);

		max_columns = llmax(max_columns, item->getNumColumns());

		if (mScrollLines <= line && line < mScrollLines + num_page_lines)
		{
			if (mCanSelect && item->getSelected())
			{
				if (item->getEnabled())
				{
					fg_color = &mFgSelectedColor;
				}
				else
				{
					fg_color = &mFgDisabledColor;
				}
				bg_color = &mBgSelectedColor;
			}
			else if (!item->getEnabled())
			{
				fg_color = &mFgDisabledColor;
				bg_color = &mBgReadOnlyColor;
			}
			else if (mHighlightedItem == line && mCanSelect)
			{
				fg_color = &mFgUnselectedColor;
				bg_color = &mHighlightedColor;
			}
			else if (mDrawStripes && line % 2 == 0)
			{
				fg_color = &mFgUnselectedColor;
				bg_color = &mBgStripeColor;
			}
			else
			{
				fg_color = &mFgUnselectedColor;
				bg_color = &LLColor4::transparent;
			}

			item->draw(item_rect, *fg_color, *bg_color, highlight_color,
					   mColumnPadding);

			cur_y -= mLineHeight;
		}
	}
}

void LLScrollListCtrl::draw()
{
	LLLocalClipRect clip(getLocalRect());

	// If user specifies sort, make sure it is maintained
	if (!mSorted && !mSortColumns.empty())
	{
		sortItems();
	}

	if (mNeedsScroll)
	{
		scrollToShowSelected();
		mNeedsScroll = false;
	}

	// Draw background
	if (mBackgroundVisible)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4fv(getEnabled() ? mBgWriteableColor.mV : mBgReadOnlyColor.mV);
		const LLRect& rect = getRect();
		LLRect background(0, rect.getHeight(), rect.getWidth(), 0);
		gl_rect_2d(background);
	}

	updateColumns();

	drawItems();

	if (mBorder)
	{
		mBorder->setKeyboardFocusHighlight(gFocusMgr.getKeyboardFocus() == this);
	}

	LLUICtrl::draw();
}

void LLScrollListCtrl::setEnabled(bool enabled)
{
	mCanSelect = enabled;
	setTabStop(enabled);
	mScrollbar->setTabStop(!enabled &&
						   mScrollbar->getPageSize() < mScrollbar->getDocSize());
}

bool LLScrollListCtrl::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	// Pretend the mouse is over the scrollbar
	return mScrollbar->handleScrollWheel(0, 0, clicks);
}

bool LLScrollListCtrl::handleToolTip(S32 x, S32 y, std::string& msg,
									 LLRect* sticky_rect_screen)
{
	S32 column_index = getColumnIndexFromOffset(x);
	LLScrollListColumn* columnp = getColumn(column_index);
	if (!columnp) return false;

	// Show tooltip for full name of hovered item if it has been truncated
	LLScrollListItem* hit_item = hitItem(x, y);
	if (hit_item)
	{
		// If the item has a specific tool tip set by XUI use that first
		std::string tooltip = hit_item->getToolTip();
		if (!tooltip.empty())
		{
			msg = tooltip;
			return true;
		}

		LLScrollListCell* hit_cell = hit_item->getColumn(column_index);
		if (!hit_cell)
		{
			return false;
		}

		if (hit_cell && hit_cell->isText())
		{
			S32 rect_left = getColumnOffsetFromIndex(column_index) +
							mItemListRect.mLeft;
			S32 rect_bottom = getRowOffsetFromIndex(getItemIndex(hit_item));
			LLRect cell_rect;
			cell_rect.setOriginAndSize(rect_left, rect_bottom,
									   rect_left + columnp->getWidth(),
									   mLineHeight);
			// Convert rect local to screen coordinates
			localPointToScreen(cell_rect.mLeft, cell_rect.mBottom,
							   &(sticky_rect_screen->mLeft),
							   &(sticky_rect_screen->mBottom));
			localPointToScreen(cell_rect.mRight, cell_rect.mTop,
							   &(sticky_rect_screen->mRight),
							   &(sticky_rect_screen->mTop));

			msg = hit_cell->getValue().asString();
		}
		return true;
	}

	// Otherwise, look for a tooltip associated with this column
	LLColumnHeader* headerp = columnp->mHeader;
	if (headerp)
	{
		headerp->handleToolTip(x, y, msg, sticky_rect_screen);
		return !msg.empty();
	}

	return false;
}

bool LLScrollListCtrl::selectItemAt(S32 x, S32 y, MASK mask)
{
	if (!mCanSelect) return false;

	bool selection_changed = false;

	LLScrollListItem* hit_item = hitItem(x, y);
	if (hit_item)
	{
		if (mAllowMultipleSelection)
		{
			if (mask & MASK_SHIFT)
			{
				if (mLastSelected == NULL)
				{
					selectItem(hit_item);
				}
				else
				{
					// Select everything between mLastSelected and hit_item
					bool selecting = false;
					// If we multiselect backwards, we will stomp on
					// mLastSelected, meaning that we never stop selecting
					// until hitting max or the end of the list.
					LLScrollListItem* last_selected = mLastSelected;
					for (item_list::iterator it = mItemList.begin(),
											 end = mItemList.end();
						 it != end; ++it)
					{
						if (mMaxSelectable > 0 &&
							getAllSelected().size() >= mMaxSelectable)
						{
							if (mOnMaximumSelectCallback)
							{
								mOnMaximumSelectCallback(mCallbackUserData);
							}
							break;
						}

						LLScrollListItem* item = *it;
						if (!item) continue;

                        if (item == hit_item || item == last_selected)
						{
							selectItem(item, false);
							selecting = !selecting;
							if (hit_item == last_selected)
							{
								// Stop selecting now, since we just clicked on
								// our last selected item
								selecting = false;
							}
						}
						if (selecting)
						{
							selectItem(item, false);
						}
					}
				}
			}
			else if (mask & MASK_CONTROL)
			{
				if (hit_item->getSelected())
				{
					deselectItem(hit_item);
				}
				else
				{
					if (!(mMaxSelectable > 0 &&
						  getAllSelected().size() >= mMaxSelectable))
					{
						selectItem(hit_item, false);
					}
					else
					{
						if (mOnMaximumSelectCallback)
						{
							mOnMaximumSelectCallback(mCallbackUserData);
						}
					}
				}
			}
			else if (mLastSelected != hit_item)
			{
				deselectAllItems(true);
				selectItem(hit_item);
			}
		}
		// This allows to de-select an item in single-selection lists. HB
		else if ((mask & MASK_CONTROL) && hit_item->getSelected())
		{
			deselectItem(hit_item);
		}
		else
		{
			selectItem(hit_item);
		}

		selection_changed = mSelectionChanged;
		if (mCommitOnSelectionChange)
		{
			commitIfChanged();
		}

		// Clear search string on mouse operations
		mSearchString.clear();
	}
#if 0
	else
	{
		mLastSelected = NULL;
		deselectAllItems(true);
	}
#endif

	return selection_changed;
}

bool LLScrollListCtrl::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (childrenHandleMouseDown(x, y, mask) == NULL)
	{
		// Set keyboard focus first, in case click action wants to move focus
		// elsewhere
		setFocus(true);

		// Clear selection changed flag because user is starting a selection
		// operation
		mSelectionChanged = false;

		handleClick(x, y, mask);
	}

	return true;
}

bool LLScrollListCtrl::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		// Release mouse capture immediately so that the "scroll to show
		// selected" logic can work
		gFocusMgr.setMouseCapture(NULL);
		if (mask == MASK_NONE)
		{
			selectItemAt(x, y, mask);
			mNeedsScroll = true;
		}
	}

	// When not committing already on selection change, always commit when
	// mouse operation is completed inside the list (required for combo
	// scrolldown lists, for example), but do not do it when
	// mCommitOnSelectionChange is true, to avoid duplicate onCommit() events.
	if (!mCommitOnSelectionChange && mItemListRect.pointInRect(x, y))
	{
		mDirty |= mSelectionChanged;
		mSelectionChanged = false;
		onCommit();
	}

	return LLUICtrl::handleMouseUp(x, y, mask);
}

bool LLScrollListCtrl::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (!handleClick(x, y, mask))
	{
		// Offer the click to the children, even if we are not enabled, so that
		// the scroll bars will work.
		if (LLView::childrenHandleDoubleClick(x, y, mask) == NULL)
		{
			if (mCanSelect && mOnDoubleClickCallback)
			{
				mOnDoubleClickCallback(mCallbackUserData);
			}
		}
	}

	return true;
}

bool LLScrollListCtrl::handleClick(S32 x, S32 y, MASK mask)
{
	// Which row was clicked on ?
	LLScrollListItem* hit_item = hitItem(x, y);
	if (!hit_item) return false;

	// Get appropriate cell from that row
	S32 column_index = getColumnIndexFromOffset(x);
	LLScrollListCell* hit_cell = hit_item->getColumn(column_index);
	if (!hit_cell) return false;

	// If cell handled click directly (i.e. clicked on an embedded checkbox)
	if (hit_cell->handleClick())
	{
		// If item not currently selected, select it
		if (!hit_item->getSelected())
		{
			selectItemAt(x, y, mask);
			gFocusMgr.setMouseCapture(this);
			mNeedsScroll = true;
		}

		// Propagate value of this cell to other selected items and commit the
		// respective widgets
		LLSD item_value = hit_cell->getValue();
		for (item_list::iterator iter = mItemList.begin(),
								 end = mItemList.end();
			 iter != end; ++iter)
		{
			LLScrollListItem* item = *iter;
			if (item && item->getSelected())
			{
				LLScrollListCell* cellp = item->getColumn(column_index);
				if (cellp)
				{
					cellp->setValue(item_value);
					cellp->onCommit();
					if (!mLastSelected)
					{
						break;
					}
				}
			}
		}

		// *FIXME: find a better way to signal cell changes
		onCommit();

		// Eat click (e.g. do not trigger double click callback)
		return true;
	}
	else
	{
		// Treat this as a normal single item selection
		selectItemAt(x, y, mask);
		gFocusMgr.setMouseCapture(this);
		mNeedsScroll = true;

		// Do not eat click (allow double click callback)
		return false;
	}
}

LLScrollListItem* LLScrollListCtrl::hitItem(S32 x, S32 y)
{
	// Excludes disabled items.
	LLScrollListItem* hit_item = NULL;

	LLRect item_rect;
	item_rect.setLeftTopAndSize(mItemListRect.mLeft, mItemListRect.mTop,
								mItemListRect.getWidth(), mLineHeight);

	// Allow for partial line at bottom
	S32 num_page_lines = mPageLines + 1;

	S32 line = 0;
	for (item_list::iterator iter = mItemList.begin(), end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* item = *iter;
		if (mScrollLines <= line && line < mScrollLines + num_page_lines)
		{
			if (item && item->getEnabled() && item_rect.pointInRect(x, y))
			{
				hit_item = item;
				break;
			}

			item_rect.translate(0, -mLineHeight);
		}
		++line;
	}

	return hit_item;
}

S32 LLScrollListCtrl::getColumnIndexFromOffset(S32 x)
{
	// Which column did we hit ?
	S32 left = 0;
	S32 right = 0;
	S32 width = 0;
	S32 column_index = 0;

	for (ordered_columns_t::const_iterator iter = mColumnsIndexed.begin(),
										   end = mColumnsIndexed.end();
		 iter != end; ++iter)
	{
		width = (*iter)->getWidth() + mColumnPadding;
		right += width;
		if (left <= x && x < right)
		{
			break;
		}

		// Set left for next column as right of current column
		left = right;
		++column_index;
	}

	return llclamp(column_index, 0, getNumColumns() - 1);
}

S32 LLScrollListCtrl::getColumnOffsetFromIndex(S32 index)
{
	S32 column_offset = 0;

	for (ordered_columns_t::const_iterator iter = mColumnsIndexed.begin(),
										   end = mColumnsIndexed.end();
		 iter != end; ++iter)
	{
		if (index-- <= 0)
		{
			return column_offset;
		}
		column_offset += (*iter)->getWidth() + mColumnPadding;
	}

	// When running off the end, return the rightmost pixel
	return mItemListRect.mRight;
}

S32 LLScrollListCtrl::getRowOffsetFromIndex(S32 index)
{
	return (mItemListRect.mTop - index + mScrollLines) * mLineHeight -
		   mLineHeight;
}

bool LLScrollListCtrl::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		if (mask == MASK_NONE)
		{
			selectItemAt(x, y, mask);
			mNeedsScroll = true;
		}
	}
	else if (mCanSelect)
	{
		LLScrollListItem* item = hitItem(x, y);
		if (item)
		{
			highlightNthItem(getItemIndex(item));
		}
		else
		{
			highlightNthItem(-1);
		}
	}

	return LLUICtrl::handleHover(x, y, mask);
}

bool LLScrollListCtrl::handleKeyHere(KEY key, MASK mask)
{
	bool handled = false;

	// Not called from parent means we have keyboard focus or a child does
	if (mCanSelect)
	{
		if (mask == MASK_NONE)
		{
			switch (key)
			{
				case KEY_UP:
					if (mAllowKeyboardMovement || hasFocus())
					{
						// commit implicit in call
						selectPrevItem(false);
						mNeedsScroll = true;
						if (mCommitOnKeyboardMovement &&
							!mCommitOnSelectionChange)
						{
							commitIfChanged();
						}
						handled = true;
					}
					break;

				case KEY_DOWN:
					if (mAllowKeyboardMovement || hasFocus())
					{
						// commit implicit in call
						selectNextItem(false);
						mNeedsScroll = true;
						if (mCommitOnKeyboardMovement &&
							!mCommitOnSelectionChange)
						{
							commitIfChanged();
						}
						handled = true;
					}
					break;

				case KEY_PAGE_UP:
					if (mAllowKeyboardMovement || hasFocus())
					{
						selectNthItem(getFirstSelectedIndex() -
									  mScrollbar->getPageSize() + 1);
						mNeedsScroll = true;
						if (mCommitOnKeyboardMovement &&
							!mCommitOnSelectionChange)
						{
							commitIfChanged();
						}
						handled = true;
					}
					break;

				case KEY_PAGE_DOWN:
					if (mAllowKeyboardMovement || hasFocus())
					{
						selectNthItem(getFirstSelectedIndex() +
									  mScrollbar->getPageSize() - 1);
						mNeedsScroll = true;
						if (mCommitOnKeyboardMovement &&
							!mCommitOnSelectionChange)
						{
							commitIfChanged();
						}
						handled = true;
					}
					break;

				case KEY_HOME:
					if (mAllowKeyboardMovement || hasFocus())
					{
						selectFirstItem();
						mNeedsScroll = true;
						if (mCommitOnKeyboardMovement &&
							!mCommitOnSelectionChange)
						{
							commitIfChanged();
						}
						handled = true;
					}
					break;

				case KEY_END:
					if (mAllowKeyboardMovement || hasFocus())
					{
						selectNthItem(getItemCount() - 1);
						mNeedsScroll = true;
						if (mCommitOnKeyboardMovement &&
							!mCommitOnSelectionChange)
						{
							commitIfChanged();
						}
						handled = true;
					}
					break;

				case KEY_RETURN:
					// JC - Special case: Only claim to have handled it if we
					// are the special non-commit-on-move type AND we are
					// visible
				  	if (!mCommitOnKeyboardMovement && mask == MASK_NONE)
					{
						onCommit();
						mSearchString.clear();
						handled = true;
					}
					break;

				case KEY_BACKSPACE:
					mSearchTimer.reset();
					if (mSearchString.size())
					{
						mSearchString.erase(mSearchString.size() - 1, 1);
					}
					if (mSearchString.empty())
					{
						if (getFirstSelected())
						{
							LLScrollListCell* cellp =
								getFirstSelected()->getColumn(getSearchColumn());
							if (cellp)
							{
								cellp->highlightText(0, 0);
							}
						}
					}
					else if (selectItemByPrefix(wstring_to_utf8str(mSearchString),
												false))
					{
						mNeedsScroll = true;
						// Update search string only on successful match
						mSearchTimer.reset();

						if (mCommitOnKeyboardMovement &&
							!mCommitOnSelectionChange)
						{
							commitIfChanged();
						}
					}
					break;

				default:
					break;
			}
		}
		// *TODO: multiple: shift-up, shift-down, shift-home, shift-end,
		// select all
	}

	return handled;
}

bool LLScrollListCtrl::handleUnicodeCharHere(llwchar uni_char)
{
	if (uni_char < 0x20 || uni_char == 0x7F)	// Control character or DEL
	{
		return false;
	}

	bool handled = false;

	// Perform incremental search based on keyboard input
	if (mSearchTimer.getElapsedTimeF32() > LLUI::sTypeAheadTimeout)
	{
		mSearchString.clear();
	}

	// Type ahead search is case insensitive
	uni_char = LLStringOps::toLower((llwchar)uni_char);

	if (selectItemByPrefix(wstring_to_utf8str(mSearchString +
											  (llwchar)uni_char), false))
	{
		// Update search string only on successful match
		mNeedsScroll = true;
		mSearchString += uni_char;
		mSearchTimer.reset();

		if (mCommitOnKeyboardMovement && !mCommitOnSelectionChange)
		{
			commitIfChanged();
		}
		handled = true;
	}
	// Handle iterating over same starting character
	else if (isRepeatedChars(mSearchString + (llwchar)uni_char) &&
			 !mItemList.empty())
	{
		// Start from last selected item, in case we previously had a
		// successful match against duplicated characters ('AA' matches
		// 'Aaron')
		item_list::iterator start_iter = mItemList.begin();
		S32 first_selected = getFirstSelectedIndex();

		// If we have a selection (> -1) then point iterator at the selected
		// item
		if (first_selected > 0)
		{
			// Point iterator to first selected item
			start_iter += first_selected;
		}

		// Start search at first item after current selection
		item_list::iterator iter = start_iter;
		++iter;
		if (iter == mItemList.end())
		{
			iter = mItemList.begin();
		}

		bool needs_commit = false;
		// Loop around once, back to previous selection
		while (iter != start_iter)
		{
			LLScrollListItem* item = *iter;

			LLScrollListCell* cellp = NULL;
			if (item)
			{
				cellp = item->getColumn(getSearchColumn());
			}
			if (cellp)
			{
				// Only select enabled items with matching first characters
				LLWString item_label =
					utf8str_to_wstring(cellp->getValue().asString());
				if (item->getEnabled() &&
					LLStringOps::toLower(item_label[0]) == uni_char)
				{
					selectItem(item);
					mNeedsScroll = true;
					cellp->highlightText(0, 1);
					mSearchTimer.reset();
					needs_commit = mCommitOnKeyboardMovement &&
								   !mCommitOnSelectionChange;
					handled = true;
					break;
				}
			}

			++iter;
			if (iter == mItemList.end())
			{
				iter = mItemList.begin();
			}
		}
		if (needs_commit)
		{
			onCommit();
		}
	}

	return handled;
}

void LLScrollListCtrl::reportInvalidInput()
{
	make_ui_sound("UISndBadKeystroke");
}

bool LLScrollListCtrl::isRepeatedChars(const LLWString& string) const
{
	if (string.empty())
	{
		return false;
	}

	llwchar first_char = string[0];

	for (U32 i = 0, count = string.size(); i < count; ++i)
	{
		if (string[i] != first_char)
		{
			return false;
		}
	}

	return true;
}

void LLScrollListCtrl::selectItem(LLScrollListItem* itemp,
								  bool select_single_item)
{
	if (!itemp) return;

	if (!itemp->getSelected())
	{
		if (mLastSelected)
		{
			LLScrollListCell* cellp =
				mLastSelected->getColumn(getSearchColumn());
			if (cellp)
			{
				cellp->highlightText(0, 0);
			}
		}
		if (select_single_item)
		{
			deselectAllItems(true);
		}
		itemp->setSelected(true);
		mLastSelected = itemp;
		mSelectionChanged = true;
	}
}

void LLScrollListCtrl::deselectItem(LLScrollListItem* itemp)
{
	if (!itemp) return;

	if (itemp->getSelected())
	{
		if (mLastSelected == itemp)
		{
			mLastSelected = NULL;
		}

		itemp->setSelected(false);
		LLScrollListCell* cellp = itemp->getColumn(getSearchColumn());
		if (cellp)
		{
			cellp->highlightText(0, 0);
		}
		mSelectionChanged = true;
	}
}

void LLScrollListCtrl::commitIfChanged()
{
	if (mSelectionChanged)
	{
		mDirty = true;
		mSelectionChanged = false;
		onCommit();
	}
}

struct SameSortColumn
{
	SameSortColumn(S32 column)
	:	mColumn(column)
	{
	}

	LL_INLINE bool operator()(std::pair<S32, bool> sort_column)
	{
		return sort_column.first == mColumn;
	}

	S32 mColumn;
};

bool LLScrollListCtrl::setSort(S32 column_idx, bool ascending)
{
	LLScrollListColumn* sort_column = getColumn(column_idx);
	if (!sort_column) return false;

	sort_column->mSortAscending = ascending;

	sort_column_t new_sort_column(column_idx, ascending);

	if (mSortColumns.empty())
	{
		mSortColumns.push_back(new_sort_column);
		return true;
	}
	else
	{
		// Grab current sort column
		sort_column_t cur_sort_column = mSortColumns.back();

		// Remove any existing sort criterion referencing this column and add
		// the new one
		mSortColumns.erase(remove_if(mSortColumns.begin(),
									 mSortColumns.end(),
									 SameSortColumn(column_idx)),
									 mSortColumns.end());
		mSortColumns.push_back(new_sort_column);

		// Did the sort criteria change ?
		return cur_sort_column != new_sort_column;
	}
}

// Called by scrollbar
//static
void LLScrollListCtrl::onScrollChange(S32 new_pos, LLScrollbar* scrollbar,
									  void* userdata)
{
	LLScrollListCtrl* self = (LLScrollListCtrl*) userdata;
	self->mScrollLines = new_pos;
}

void LLScrollListCtrl::sortByColumn(const std::string& name, bool ascending)
{
	column_map_t::iterator it = mColumns.find(name);
	if (it != mColumns.end())
	{
		sortByColumnIndex(it->second.mIndex, ascending);
	}
}

// First column is column 0
void LLScrollListCtrl::sortByColumnIndex(U32 column, bool ascending)
{
	if (setSort(column, ascending))
	{
		sortItems();
	}
}

void LLScrollListCtrl::sortItems()
{
	// Do stable sort to preserve any previous sorts
	std::stable_sort(mItemList.begin(), mItemList.end(),
					 SortScrollListItem(mSortColumns));
	setSorted(true);
}

// For one-shot sorts; does not save sort column/order.
void LLScrollListCtrl::sortOnce(S32 column, bool ascending)
{
	std::vector<std::pair<S32, bool> > sort_column;
	sort_column.emplace_back(column, ascending);

	// Do stable sort to preserve any previous sorts
	std::stable_sort(mItemList.begin(), mItemList.end(),
					 SortScrollListItem(sort_column));
}

void LLScrollListCtrl::setAllowRefresh(bool allow)
{
	mAllowRefresh = allow;
	if (allow)
	{
		dirtyColumns();
	}
}

void LLScrollListCtrl::dirtyColumns()
{
	if (!mAllowRefresh) return;	// lazy updates

	mColumnsDirty = mColumnWidthsDirty = true;

	// We need to keep mColumnsIndexed up to date just in case someone indexes
	// into it immediately
	mColumnsIndexed.resize(mColumns.size());

	for (column_map_t::iterator it = mColumns.begin(), end = mColumns.end();
		 it != end; ++it)
	{
		LLScrollListColumn* column = &it->second;
		mColumnsIndexed[it->second.mIndex] = column;
	}
}

S32 LLScrollListCtrl::getScrollPos() const
{
	return mScrollbar->getDocPos();
}

void LLScrollListCtrl::setScrollPos(S32 pos)
{
	mScrollbar->setDocPos(pos);

	onScrollChange(mScrollbar->getDocPos(), mScrollbar, this);
}

void LLScrollListCtrl::scrollToShowSelected()
{
	// Do not scroll automatically when capturing mouse input as that will
	// change what is currently under the mouse cursor
	if (hasMouseCapture())
	{
		return;
	}

	// If user specifies sort, make sure it is maintained, else we end up
	// showing the wrong item line... HB
	if (!mSorted && !mSortColumns.empty())
	{
		sortItems();
	}

	S32 index = getFirstSelectedIndex();
	if (index < 0)
	{
		return;
	}

	LLScrollListItem* item = mItemList[index];
	if (!item) // Paranoia
	{
		return;
	}

	if (index < mScrollLines)
	{
		// Need to scroll to show item
		setScrollPos(index);
	}
	else if (index >= mScrollLines + mPageLines)
	{
		setScrollPos(index - mPageLines + 1);
	}
}

void LLScrollListCtrl::scrollToShowLast()
{
	// Do not scroll automatically when capturing mouse input as that will
	// change what is currently under the mouse cursor
	if (hasMouseCapture())
	{
		return;
	}

	S32 index = mItemList.size() - 1;
	if (index < 0)
	{
		return;
	}

	if (index < mScrollLines)
	{
		// Need to scroll to show item
		setScrollPos(index);
	}
	else if (index >= mScrollLines + mPageLines)
	{
		setScrollPos(index - mPageLines + 1);
	}
}

void LLScrollListCtrl::updateStaticColumnWidth(LLScrollListColumn* col,
											   S32 new_width)
{
	mTotalStaticColumnWidth += llmax(0, new_width) - llmax(0, col->getWidth());
}

//virtual
LLXMLNodePtr LLScrollListCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_SCROLL_LIST_CTRL_TAG);

	// Attributes

	node->createChild("multi_select",
					  true)->setBoolValue(mAllowMultipleSelection);

	node->createChild("draw_border",
					  true)->setBoolValue(mBorder != NULL);

	node->createChild("draw_heading",
					  true)->setBoolValue(mDisplayColumnHeaders);

	node->createChild("background_visible",
					  true)->setBoolValue(mBackgroundVisible);

	node->createChild("draw_stripes", true)->setBoolValue(mDrawStripes);

	node->createChild("column_padding", true)->setIntValue(mColumnPadding);

	addColorXML(node, mBgWriteableColor, "bg_writeable_color",
				"ScrollBgWriteableColor");
	addColorXML(node, mBgReadOnlyColor, "bg_read_only_color",
				"ScrollBgReadOnlyColor");
	addColorXML(node, mBgSelectedColor, "bg_selected_color",
				"ScrollSelectedBGColor");
	addColorXML(node, mBgStripeColor, "bg_stripe_color",
				"ScrollBGStripeColor");
	addColorXML(node, mFgSelectedColor, "fg_selected_color",
				"ScrollSelectedFGColor");
	addColorXML(node, mFgUnselectedColor, "fg_unselected_color",
				"ScrollUnselectedColor");
	addColorXML(node, mFgDisabledColor, "fg_disable_color",
				"ScrollDisabledColor");
	addColorXML(node, mHighlightedColor, "highlighted_color",
				"ScrollHighlightedColor");

	// Contents

	std::vector<const LLScrollListColumn*> sorted_list;
	sorted_list.resize(mColumns.size());
	for (column_map_t::const_iterator it = mColumns.begin(),
									  end = mColumns.end();
		 it != end; ++it)
	{
		sorted_list[it->second.mIndex] = &it->second;
	}

	for (size_t i = 0, count = sorted_list.size(); i < count; ++i)
	{
		LLXMLNodePtr child_node = node->createChild("column", false);
		const LLScrollListColumn* column = sorted_list[i];

		child_node->createChild("name", true)->setStringValue(column->mName);
		child_node->createChild("label", true)->setStringValue(column->mLabel);
		child_node->createChild("width",
								true)->setIntValue(column->getWidth());
	}

	return node;
}

void LLScrollListCtrl::setScrollListParameters(LLXMLNodePtr node)
{
	// James: this is not a good way to do colors. We need a central "UI style"
	// manager that sets the colors for ALL scroll lists, buttons, etc.

	LLColor4 color;
	if (node->hasAttribute("fg_unselected_color"))
	{
		LLUICtrlFactory::getAttributeColor(node,"fg_unselected_color", color);
		setFgUnselectedColor(color);
	}
	if (node->hasAttribute("fg_selected_color"))
	{
		LLUICtrlFactory::getAttributeColor(node,"fg_selected_color", color);
		setFgSelectedColor(color);
	}
	if (node->hasAttribute("bg_selected_color"))
	{
		LLUICtrlFactory::getAttributeColor(node,"bg_selected_color", color);
		setBgSelectedColor(color);
	}
	if (node->hasAttribute("fg_disable_color"))
	{
		LLUICtrlFactory::getAttributeColor(node,"fg_disable_color", color);
		setFgDisableColor(color);
	}
	if (node->hasAttribute("bg_writeable_color"))
	{
		LLUICtrlFactory::getAttributeColor(node,"bg_writeable_color", color);
		setBgWriteableColor(color);
	}
	if (node->hasAttribute("bg_read_only_color"))
	{
		LLUICtrlFactory::getAttributeColor(node,"bg_read_only_color", color);
		setReadOnlyBgColor(color);
	}
	if (LLUICtrlFactory::getAttributeColor(node,"bg_stripe_color", color))
	{
		setBgStripeColor(color);
	}
	if (LLUICtrlFactory::getAttributeColor(node,"highlighted_color", color))
	{
		setHighlightedColor(color);
	}

	if (node->hasAttribute("background_visible"))
	{
		bool background_visible = false;
		node->getAttributeBool("background_visible", background_visible);
		setBackgroundVisible(background_visible);
	}

	if (node->hasAttribute("draw_stripes"))
	{
		bool draw_stripes = false;
		node->getAttributeBool("draw_stripes", draw_stripes);
		setDrawStripes(draw_stripes);
	}

	if (node->hasAttribute("column_padding"))
	{
		S32 column_padding;
		node->getAttributeS32("column_padding", column_padding);
		setColumnPadding(column_padding);
	}
}

//static
LLView* LLScrollListCtrl::fromXML(LLXMLNodePtr node, LLView* parent,
								  LLUICtrlFactory* factory)
{
	std::string name = LL_SCROLL_LIST_CTRL_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	bool multi_select = false;
	node->getAttributeBool("multi_select", multi_select);

	bool draw_border = true;
	node->getAttributeBool("draw_border", draw_border);

	bool draw_heading = false;
	node->getAttributeBool("draw_heading", draw_heading);

	S32 search_column = 0;
	node->getAttributeS32("search_column", search_column);

	S32 sort_column = -1;
	node->getAttributeS32("sort_column", sort_column);

	bool sort_ascending = true;
	node->getAttributeBool("sort_ascending", sort_ascending);

	LLUICtrlCallback callback = NULL;

	LLScrollListCtrl* scroll_list = new LLScrollListCtrl(name, rect, callback,
														 NULL, multi_select,
														 draw_border);

	scroll_list->setDisplayHeading(draw_heading);
	if (node->hasAttribute("heading_height"))
	{
		S32 heading_height;
		node->getAttributeS32("heading_height", heading_height);
		scroll_list->setHeadingHeight(heading_height);
	}

	scroll_list->setScrollListParameters(node);

	scroll_list->initFromXML(node, parent);

	scroll_list->setSearchColumn(search_column);

	LLSD columns;
	S32 index = 0;
	LLXMLNodePtr child;
	std::string labelname, columnname, sortname, imagename, tooltip;
	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		if (child->hasName("column"))
		{
			labelname.clear();
			child->getAttributeString("label", labelname);

			columnname.clear();
			child->getAttributeString("name", columnname);
			if (columnname.empty())
			{
				columnname = labelname;
			}
			else if (labelname.empty())
			{
				labelname = columnname;
			}

			sortname = columnname;
			child->getAttributeString("sort", sortname);

			bool sort_ascending = true;
			child->getAttributeBool("sort_ascending", sort_ascending);

			imagename.clear();
			child->getAttributeString("image", imagename);

			bool columndynamicwidth = false;
			child->getAttributeBool("dynamicwidth", columndynamicwidth);

			S32 columnwidth = -1;
			child->getAttributeS32("width", columnwidth);

			tooltip.clear();
			child->getAttributeString("tool_tip", tooltip);

			F32 columnrelwidth = 0.f;
			child->getAttributeF32("relwidth", columnrelwidth);

			LLFontGL::HAlign h_align = LLFontGL::LEFT;
			h_align = LLView::selectFontHAlign(child);

			columns[index]["name"] = columnname;
			columns[index]["sort"] = sortname;
			columns[index]["sort_ascending"] = sort_ascending;
			columns[index]["image"] = imagename;
			columns[index]["label"] = labelname;
			columns[index]["width"] = columnwidth;
			columns[index]["relwidth"] = columnrelwidth;
			columns[index]["dynamicwidth"] = columndynamicwidth;
			columns[index]["halign"] = (S32)h_align;
			columns[index++]["tool_tip"] = tooltip;
		}
	}
	scroll_list->setColumnHeadings(columns);

	if (sort_column >= 0)
	{
		scroll_list->sortByColumnIndex(sort_column, sort_ascending);
	}

	LLUUID id;
	std::string value, font, font_style;
	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		if (child->hasName("row"))
		{
			child->getAttributeUUID("id", id);

			LLSD row;
			row["id"] = id;

			S32 column_idx = 0;
			for (LLXMLNodePtr row_child = child->getFirstChild();
				 row_child.notNull(); row_child = row_child->getNextSibling())
			{
				if (row_child->hasName("column"))
				{
					value = row_child->getTextContents();

					columnname.clear();
					row_child->getAttributeString("name", columnname);

					font.clear();
					row_child->getAttributeString("font", font);

					font_style.clear();
					row_child->getAttributeString("font-style", font_style);

					row["columns"][column_idx]["column"] = columnname;
					row["columns"][column_idx]["value"] = value;
					row["columns"][column_idx]["font"] = font;
					row["columns"][column_idx++]["font-style"] = font_style;
				}
			}
			scroll_list->addElement(row);
		}
	}

	std::string contents = node->getTextContents();
	if (!contents.empty())
	{
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep("\t\n");
		tokenizer tokens(contents, sep);
		tokenizer::iterator token_iter = tokens.begin();

		while (token_iter != tokens.end())
		{
			const std::string& line = *token_iter;
			scroll_list->addSimpleElement(line);
			++token_iter;
		}
	}

	return scroll_list;
}

// LLEditMenuHandler functions

//virtual
void LLScrollListCtrl::copy()
{
	std::string buffer;

	std::vector<LLScrollListItem*> items = getAllSelected();
	for (size_t i = 0, count = items.size(); i < count; ++i)
	{
		LLScrollListItem* itemp = items[i];
		if (itemp)
		{
			buffer += itemp->getContentsCSV() + "\n";
		}
	}
	gClipboard.copyFromSubstring(utf8str_to_wstring(buffer), 0,
								 buffer.length());
}

//virtual
bool LLScrollListCtrl::canCopy() const
{
	return getFirstSelected() != NULL;
}

//virtual
void LLScrollListCtrl::cut()
{
	copy();
	doDelete();
}

//virtual
bool LLScrollListCtrl::canCut() const
{
	return canCopy() && canDoDelete();
}

//virtual
void LLScrollListCtrl::selectAll()
{
	// Deselects all other items
	for (item_list::iterator iter = mItemList.begin(), end = mItemList.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* itemp = *iter;
		if (itemp && itemp->getEnabled())
		{
			selectItem(itemp, false);
		}
	}

	if (mCommitOnSelectionChange)
	{
		commitIfChanged();
	}
}

//virtual
bool LLScrollListCtrl::canSelectAll() const
{
	return getCanSelect() && mAllowMultipleSelection &&
		   !(mMaxSelectable > 0 && mItemList.size() > mMaxSelectable);
}

//virtual
void LLScrollListCtrl::deselect()
{
	deselectAllItems();
}

void LLScrollListCtrl::addColumn(const LLSD& column, EAddPosition pos)
{
	std::string name = column["name"].asString();
	// if no column name provided, just use ordinal as name
	if (name.empty())
	{
		std::ostringstream new_name;
		new_name << mColumnsIndexed.size();
		name = new_name.str();
	}
	if (mColumns.find(name) == mColumns.end())
	{
		// Add column
		mColumns[name] = LLScrollListColumn(column, this);
		LLScrollListColumn* new_column = &mColumns[name];
		new_column->mParentCtrl = this;
		new_column->mIndex = mColumns.size() - 1;

		// Add button
		if (new_column->getWidth() > 0 || new_column->mRelWidth > 0 ||
			new_column->mDynamicWidth)
		{
			if (getNumColumns() > 0)
			{
				mTotalColumnPadding += mColumnPadding;
			}
			if (new_column->mRelWidth >= 0)
			{
				new_column->setWidth(ll_roundp(new_column->mRelWidth *
											   (mItemListRect.getWidth() -
												mTotalStaticColumnWidth -
												mTotalColumnPadding)));
			}
			else if (new_column->mDynamicWidth)
			{
				++mNumDynamicWidthColumns;
				new_column->setWidth((mItemListRect.getWidth() -
									  mTotalStaticColumnWidth -
									  mTotalColumnPadding) /
									 mNumDynamicWidthColumns);
			}
			S32 top = mItemListRect.mTop;
			S32 left = mItemListRect.mLeft;
			for (column_map_t::iterator it = mColumns.begin(),
										end = mColumns.end();
				 it != end; ++it)
			{
				if (it->second.mIndex < new_column->mIndex &&
					it->second.getWidth() > 0)
				{
					left += it->second.getWidth() + mColumnPadding;
				}
			}
			std::string button_name = "btn_" + name;
			S32 right = left + new_column->getWidth();
			if (new_column->mIndex != (S32)mColumns.size() - 1)
			{
				right += mColumnPadding;
			}
			LLRect temp_rect = LLRect(left, top + mHeadingHeight, right, top);
			new_column->mHeader = new LLColumnHeader(button_name, temp_rect,
													 new_column);
			if (column["image"].asString() != "")
			{
#if 0
				new_column->mHeader->setScaleImage(false);
#endif
				new_column->mHeader->setImage(column["image"].asString());
			}
			else
			{
				new_column->mHeader->setLabel(new_column->mLabel);
			}

			new_column->mHeader->setToolTip(column["tool_tip"].asString());

			// RN: although it might be useful to change sort order with the
			// keyboard, mixing tab stops on child items along with the parent
			// item is not supported yet
			new_column->mHeader->setTabStop(false);
			addChild(new_column->mHeader);
			new_column->mHeader->setVisible(mDisplayColumnHeaders);

			sendChildToFront(mScrollbar);
		}
	}

	dirtyColumns();
}

//static
void LLScrollListCtrl::onClickColumn(void* userdata)
{
	LLScrollListColumn* info = (LLScrollListColumn*)userdata;
	if (!info) return;

	LLScrollListCtrl* parent = info->mParentCtrl;
	if (!parent) return;

	S32 column_index = info->mIndex;

	LLScrollListColumn* column = parent->mColumnsIndexed[info->mIndex];
	bool ascending = column->mSortAscending;
	if (column->mSortingColumn != column->mName &&
		parent->mColumns.find(column->mSortingColumn) != parent->mColumns.end())
	{
		LLScrollListColumn& info_redir =
			parent->mColumns[column->mSortingColumn];
		column_index = info_redir.mIndex;
	}

	// If this column is the primary sort key, reverse the direction
	if (!parent->mSortColumns.empty() &&
		parent->mSortColumns.back().first == column_index)
	{
		ascending = !parent->mSortColumns.back().second;
	}

	parent->sortByColumnIndex(column_index, ascending);

	if (parent->mOnSortChangedCallback)
	{
		parent->mOnSortChangedCallback(parent->getCallbackUserData());
	}
}

std::string LLScrollListCtrl::getSortColumnName()
{
	LLScrollListColumn* column = NULL;
	if (!mSortColumns.empty())
	{
		column = mColumnsIndexed[mSortColumns.back().first];
	}
	return column ? column->mName : "";
}

void LLScrollListCtrl::clearColumns()
{
	for (column_map_t::iterator it = mColumns.begin(), end = mColumns.end();
		 it != end; ++it)
	{
		LLColumnHeader* header = it->second.mHeader;
		if (header)
		{
			removeChild(header);
			delete header;
		}
	}
	mColumns.clear();
	mSortColumns.clear();
	mTotalStaticColumnWidth = 0;
	mTotalColumnPadding = 0;
	dirtyColumns();	// Clears mColumnsIndexed
}

void LLScrollListCtrl::setColumnLabel(const std::string& column,
									  const std::string& label)
{
	column_map_t::iterator it = mColumns.find(column);
	if (it != mColumns.end())
	{
		it->second.mLabel = label;
		if (it->second.mHeader)
		{
			it->second.mHeader->setLabel(label);
		}
	}
}

LLScrollListColumn* LLScrollListCtrl::getColumn(S32 index)
{
	if (index < 0 || index >= (S32)mColumnsIndexed.size())
	{
		return NULL;
	}
	return mColumnsIndexed[index];
}

void LLScrollListCtrl::setColumnHeadings(LLSD headings)
{
	mColumns.clear();
	for (LLSD::array_const_iterator itor = headings.beginArray(),
									end = headings.endArray();
		 itor != end; ++itor)
	{
		addColumn(*itor);
	}
}

LLScrollListItem* LLScrollListCtrl::addElement(const LLSD& value,
											   EAddPosition pos,
											   void* userdata)
{
	LLSD id = value["id"];

	LLScrollListItem* new_item = new LLScrollListItem(id, userdata);
	if (value.has("enabled"))
	{
		new_item->setEnabled(value["enabled"].asBoolean());
	}

	new_item->setNumColumns(mColumns.size());

	static const LLFontGL* default_font = LLFontGL::getFontSansSerifSmall();

	// Add any columns we do not already have
	LLSD columns = value["columns"];
	S32 col_index = 0;
	for (LLSD::array_const_iterator it = columns.beginArray(),
									end = columns.endArray();
		 it != end; ++it)
	{
		if (it->isUndefined())
		{
			// Skip unused columns in item passed in
			continue;
		}
		std::string column = (*it)["column"].asString();

		LLScrollListColumn* columnp = NULL;

		// Empty columns strings index by ordinal
		if (column.empty())
		{
			std::ostringstream new_name;
			new_name << col_index;
			column = new_name.str();
		}

		column_map_t::iterator column_it = mColumns.find(column);
		if (column_it != mColumns.end())
		{
			columnp = &column_it->second;
		}

		// Create new column on demand
		if (!columnp)
		{
			LLSD new_column;
			new_column["name"] = column;
			new_column["label"] = column;
			// If width supplied for column, use it, otherwise use adaptive
			// width
			if (it->has("width"))
			{
				new_column["width"] = (*it)["width"];
			}
			else
			{
				new_column["dynamicwidth"] = true;
			}
			addColumn(new_column);
			columnp = &mColumns[column];
			new_item->setNumColumns(mColumns.size());
		}

		S32 index = columnp->mIndex;
		S32 width = columnp->getWidth();
		LLFontGL::HAlign font_alignment = columnp->mFontAlignment;
		LLColor4 fcolor = LLColor4::black;

		LLSD value = (*it)["value"];
		std::string fontname = (*it)["font"].asString();
		std::string fontstyle = (*it)["font-style"].asString();
		std::string type = (*it)["type"].asString();
		std::string format = (*it)["format"].asString();

		if (it->has("font-color"))
		{
			LLSD sd_color = (*it)["font-color"];
			fcolor.setValue(sd_color);
		}

		bool has_color = it->has("color");
		LLColor4 color = LLColor4((*it)["color"]);
		bool enabled = !it->has("enabled") || (*it)["enabled"].asBoolean();

		const LLFontGL* font = LLFontGL::getFont(fontname);
		if (!font)
		{
			font = default_font;
		}
		U8 font_style = LLFontGL::getStyleFromString(fontstyle);

		if (type == "icon")
		{
			LLScrollListIcon* cell = new LLScrollListIcon(value, width);
			if (has_color)
			{
				cell->setColor(color);
			}
			new_item->setColumn(index, cell);
		}
		else if (type == "checkbox")
		{
			LLCheckBoxCtrl* ctrl =
				new LLCheckBoxCtrl("check", LLRect(0, width, width, 0), " ");
			ctrl->setEnabled(enabled);
			ctrl->setValue(value);
			LLScrollListCheck* cell = new LLScrollListCheck(ctrl, width);
			if (has_color)
			{
				cell->setColor(color);
			}
			new_item->setColumn(index, cell);
		}
		else if (type == "separator")
		{
			LLScrollListSeparator* cell = new LLScrollListSeparator(width);
			if (has_color)
			{
				cell->setColor(color);
			}
			new_item->setColumn(index, cell);
		}
		else if (type == "date")
		{
			LLScrollListDate* cell = new LLScrollListDate(value.asDate(),
														  format, font,
														  width, font_style,
														  font_alignment);
			if (has_color)
			{
				cell->setColor(color);
			}
			new_item->setColumn(index, cell);
			if (columnp->mHeader && !value.asString().empty())
			{
				columnp->mHeader->setHasResizableElement(true);
			}
		}
		else
		{
			LLScrollListText* cell = new LLScrollListText(value.asString(),
														  font, width,
														  font_style,
														  font_alignment,
														  fcolor, true);
			if (has_color)
			{
				cell->setColor(color);
			}
			new_item->setColumn(index, cell);
			if (columnp->mHeader && !value.asString().empty())
			{
				columnp->mHeader->setHasResizableElement(true);
			}
		}

		++col_index;
	}

	// Add dummy cells for missing columns
	for (column_map_t::iterator column_it = mColumns.begin();
		 column_it != mColumns.end(); ++column_it)
	{
		S32 column_idx = column_it->second.mIndex;
		if (!new_item->getColumn(column_idx))
		{
			LLScrollListColumn* column_ptr = &column_it->second;
			new_item->setColumn(column_idx,
								new LLScrollListText(LLStringUtil::null,
													 default_font,
													 column_ptr->getWidth(),
													 LLFontGL::NORMAL));
		}
	}

	addItem(new_item, pos);

	return new_item;
}

LLScrollListItem* LLScrollListCtrl::addSimpleElement(const std::string& value,
													 EAddPosition pos,
													 const LLSD& id)
{
	LLSD entry_id = id;
	if (id.isUndefined())
	{
		entry_id = value;
	}

	LLScrollListItem* new_item = new LLScrollListItem(entry_id);

	static const LLFontGL* font = LLFontGL::getFontSansSerifSmall();

	new_item->addColumn(value, font, getRect().getWidth());

	addItem(new_item, pos);
	return new_item;
}

void LLScrollListCtrl::setValue(const LLSD& value)
{
	for (LLSD::array_const_iterator it = value.beginArray(),
									end = value.endArray();
		 it != end; ++it)
	{
		addElement(*it);
	}
}

LLSD LLScrollListCtrl::getValue() const
{
	LLScrollListItem* item = getFirstSelected();
	return item ? item->getValue() : LLSD();
}

bool LLScrollListCtrl::operateOnSelection(EOperation op)
{
	if (op == OP_DELETE)
	{
		deleteSelectedItems();
		return true;
	}
	else if (op == OP_DESELECT)
	{
		deselectAllItems();
	}
	return false;
}

bool LLScrollListCtrl::operateOnAll(EOperation op)
{
	if (op == OP_DELETE)
	{
		clearRows();
		return true;
	}
	else if (op == OP_DESELECT)
	{
		deselectAllItems();
	}
	else if (op == OP_SELECT)
	{
		selectAll();
	}
	return false;
}

//virtual
void LLScrollListCtrl::setFocus(bool b)
{
	mSearchString.clear();
	// For tabbing into pristine scroll lists (Finder)
	if (!getFirstSelected())
	{
		selectFirstItem();
	}

	if (b)
	{
		grabMenuHandler();
	}
	else
	{
		releaseMenuHandler();
	}

	LLUICtrl::setFocus(b);
}

//virtual
bool LLScrollListCtrl::isDirty() const
{
	return mAllowMultipleSelection ? mDirty
								   : mOriginalSelection != getFirstSelectedIndex();
}

// Clear dirty state
void LLScrollListCtrl::resetDirty()
{
	mDirty = false;
	mOriginalSelection = getFirstSelectedIndex();
}

//virtual
void LLScrollListCtrl::onFocusReceived()
{
	// Forget latent selection changes when getting focus
	mSelectionChanged = false;
	LLUICtrl::onFocusReceived();
}

//virtual
void LLScrollListCtrl::onFocusLost()
{
	if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
	}
	LLUICtrl::onFocusLost();
}

LLColumnHeader::LLColumnHeader(const std::string& label,
							   const LLRect& rect,
							   LLScrollListColumn* column,
							   const LLFontGL* fontp)
:	LLComboBox(label, rect, label, NULL, NULL),
	mColumn(column),
	mOrigLabel(label),
	mShowSortOptions(false),
	mHasResizableElement(false)
{
	mListPosition = LLComboBox::ABOVE;
	setCommitCallback(onSelectSort);
	setCallbackUserData(this);
	mButton->setTabStop(false);
	// Require at least two frames between mouse down and mouse up event to
	// capture intentional "hold" not just bad framerate
	mButton->setHeldDownDelay(LLUI::sColumnHeaderDropDownDelay, 2);
	mButton->setHeldDownCallback(onHeldDown);
	mButton->setClickedCallback(onClick);
	mButton->setMouseDownCallback(onMouseDown);

	mButton->setCallbackUserData(this);
	mButton->setToolTip(label);

	// *TODO: Translate
	mAscendingText = "[LOW]...[HIGH](Ascending)";
	mDescendingText = "[HIGH]...[LOW](Descending)";

	mList->reshape(llmax(mList->getRect().getWidth(), 110,
						 getRect().getWidth()),
				   mList->getRect().getHeight());

	// Resize handles on left and right
	constexpr S32 RESIZE_BAR_THICKNESS = 3;
	mResizeBar = new LLResizeBar("resizebar", this,
								 LLRect(getRect().getWidth() -
										RESIZE_BAR_THICKNESS,
										getRect().getHeight(),
										getRect().getWidth(), 0),
								 MIN_COLUMN_WIDTH, S32_MAX,
								 LLResizeBar::RIGHT);
	addChild(mResizeBar);

	mResizeBar->setEnabled(false);
}

void LLColumnHeader::draw()
{
	static const LLUIImagePtr up_arrow_image =
		LLUI::getUIImage("up_arrow.tga");
	static const LLUIImagePtr down_arrow_image =
		LLUI::getUIImage("down_arrow.tga");

	bool draw_arrow = !mColumn->mLabel.empty() &&
					  mColumn->mParentCtrl->isSorted() &&
					  mColumn->mParentCtrl->getSortColumnName() ==
						mColumn->mSortingColumn;

	bool is_ascending = mColumn->mParentCtrl->getSortAscending();
	mButton->setImageOverlay(is_ascending ? up_arrow_image : down_arrow_image,
							 LLFontGL::RIGHT, draw_arrow ? LLColor4::white
														 : LLColor4::transparent);
	mArrowImage = mButton->getImageOverlay();

#if 0
	bool clip = getRect().mRight > mColumn->mParentCtrl->getItemListRect().getWidth();
	LLGLEnable scissor_test(clip ? GL_SCISSOR_TEST : GL_FALSE);

	LLRect column_header_local_rect(-getRect().mLeft, getRect().getHeight(),
									mColumn->mParentCtrl->getItemListRect().getWidth() -
									getRect().mLeft, 0);
	LLUI::setScissorRegionLocal(column_header_local_rect);
#endif

	// Draw children
	LLComboBox::draw();

	if (mList->getVisible())
	{
		// Sync sort order with list selection every frame
		mColumn->mParentCtrl->sortByColumn(mColumn->mSortingColumn,
										   getCurrentIndex() == 0);
	}
}

bool LLColumnHeader::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (canResize() && mResizeBar->getRect().pointInRect(x, y))
	{
		// Reshape column to max content width
		mColumn->mParentCtrl->calcMaxContentWidth();
		LLRect column_rect = getRect();
		column_rect.mRight = column_rect.mLeft + mColumn->mMaxContentWidth;
		userSetShape(column_rect);
	}
	else
	{
		onClick(this);
	}

	return true;
}

void LLColumnHeader::setImage(const std::string& image_name)
{
	if (mButton)
	{
		mButton->setImageSelected(image_name);
		mButton->setImageUnselected(image_name);
	}
}

//static
void LLColumnHeader::onClick(void* user_data)
{
	LLColumnHeader* headerp = (LLColumnHeader*)user_data;
	if (!headerp) return;

	LLScrollListColumn* column = headerp->mColumn;
	if (!column) return;

	if (headerp->mList->getVisible())
	{
		headerp->hideList();
	}

	LLScrollListCtrl::onClickColumn(column);

	// Propagate new sort order to sort order list
	headerp->mList->selectNthItem(column->mParentCtrl->getSortAscending() ? 0
																		  : 1);
}

//static
void LLColumnHeader::onMouseDown(void* user_data)
{
	// For now, do nothing but block the normal showList() behavior
}

//static
void LLColumnHeader::onHeldDown(void* user_data)
{
	LLColumnHeader* headerp = (LLColumnHeader*)user_data;
	if (headerp)
	{
		headerp->showList();
	}
}

void LLColumnHeader::showList()
{
	if (mShowSortOptions)
	{
		mOrigLabel = mButton->getLabelSelected();

		// Move sort column over to this column and do initial sort
		mColumn->mParentCtrl->sortByColumn(mColumn->mSortingColumn,
										   mColumn->mParentCtrl->getSortAscending());

		std::string low_item_text;
		std::string high_item_text;

		LLScrollListItem* itemp = mColumn->mParentCtrl->getFirstData();
		if (itemp)
		{
			LLScrollListCell* cell = itemp->getColumn(mColumn->mIndex);
			if (cell && cell->isText())
			{
				if (mColumn->mParentCtrl->getSortAscending())
				{
					low_item_text = cell->getValue().asString();
				}
				else
				{
					high_item_text = cell->getValue().asString();
				}
			}
		}

		itemp = mColumn->mParentCtrl->getLastData();
		if (itemp)
		{
			LLScrollListCell* cell = itemp->getColumn(mColumn->mIndex);
			if (cell && cell->isText())
			{
				if (mColumn->mParentCtrl->getSortAscending())
				{
					high_item_text = cell->getValue().asString();
				}
				else
				{
					low_item_text = cell->getValue().asString();
				}
			}
		}

		LLStringUtil::truncate(low_item_text, 3);
		LLStringUtil::truncate(high_item_text, 3);

		std::string ascending_string;
		std::string descending_string;

		if (low_item_text.empty() || high_item_text.empty())
		{
			ascending_string = "Ascending";
			descending_string = "Descending";
		}
		else
		{
			mAscendingText.setArg("[LOW]", low_item_text);
			mAscendingText.setArg("[HIGH]", high_item_text);
			mDescendingText.setArg("[LOW]", low_item_text);
			mDescendingText.setArg("[HIGH]", high_item_text);
			ascending_string = mAscendingText.getString();
			descending_string = mDescendingText.getString();
		}

		static const LLFontGL* font = LLFontGL::getFontSansSerifSmall();

		S32 text_width = font->getWidth(ascending_string);
		text_width = llmax(text_width,
						   font->getWidth(descending_string)) + 10;
		text_width = llmax(text_width, getRect().getWidth() - 30);

		mList->getColumn(0)->setWidth(text_width);
		((LLScrollListText*)mList->getFirstData()->getColumn(0))->setText(ascending_string);
		((LLScrollListText*)mList->getLastData()->getColumn(0))->setText(descending_string);

		mList->reshape(llmax(text_width + 30, 110, getRect().getWidth()),
					   mList->getRect().getHeight());

		LLComboBox::showList();
	}
}

//static
void LLColumnHeader::onSelectSort(LLUICtrl* ctrl, void* user_data)
{
	LLColumnHeader* headerp = (LLColumnHeader*)user_data;
	if (!headerp) return;

	LLScrollListColumn* column = headerp->mColumn;
	if (!column) return;

	LLScrollListCtrl* parent = column->mParentCtrl;
	if (!parent) return;

	if (headerp->getCurrentIndex() == 0)
	{
		// Ascending
		parent->sortByColumn(column->mSortingColumn, true);
	}
	else
	{
		// Descending
		parent->sortByColumn(column->mSortingColumn, false);
	}

	// Restore original column header
	headerp->setLabel(headerp->mOrigLabel);
}

LLView*	LLColumnHeader::findSnapEdge(S32& new_edge_val,
									 const LLCoordGL& mouse_dir,
									 ESnapEdge snap_edge, ESnapType snap_type,
									 S32 threshold, S32 padding)
{
	// This logic assumes dragging on right
	llassert(snap_edge == SNAP_RIGHT);

	// Use higher snap threshold for column headers
	threshold = llmin(threshold, 10);

	LLRect snap_rect = getSnapRect();
	mColumn->mParentCtrl->calcMaxContentWidth();
	S32 snap_delta = mColumn->mMaxContentWidth - snap_rect.getWidth();

	// X coord growing means column growing, so same signs mean we are going in
	// right direction
	if (abs(snap_delta) <= threshold && mouse_dir.mX * snap_delta > 0)
	{
		new_edge_val = snap_rect.mRight + snap_delta;
	}
	else
	{
		LLScrollListColumn* next_column =
			mColumn->mParentCtrl->getColumn(mColumn->mIndex + 1);
		while (next_column)
		{
			if (next_column->mHeader)
			{
				snap_delta = next_column->mHeader->getSnapRect().mRight -
							 next_column->mMaxContentWidth - snap_rect.mRight;
				if (abs(snap_delta) <= threshold &&
					mouse_dir.mX * snap_delta > 0)
				{
					new_edge_val = snap_rect.mRight + snap_delta;
				}
				break;
			}
			next_column =
				mColumn->mParentCtrl->getColumn(next_column->mIndex + 1);
		}
	}

	return this;
}

void LLColumnHeader::userSetShape(const LLRect& new_rect)
{
	S32 new_width = new_rect.getWidth();
#if 0
	S32 delta_width = new_width -
					  (getRect().getWidth() +
					   mColumn->mParentCtrl->getColumnPadding());
#else
	S32 delta_width = new_width - getRect().getWidth();
#endif

	if (delta_width != 0)
	{
		S32 remaining_width = -delta_width;
		S32 col;
		for (col = mColumn->mIndex + 1;
			 col < mColumn->mParentCtrl->getNumColumns(); ++col)
		{
			LLScrollListColumn* columnp = mColumn->mParentCtrl->getColumn(col);
			if (!columnp) continue;

			if (columnp->mHeader && columnp->mHeader->canResize())
			{
				// How many pixels in width can this column afford to give up ?
				S32 resize_buffer_amt =
					llmax(0, columnp->getWidth() - MIN_COLUMN_WIDTH);

				// User shrinking column, need to add width to other columns
				if (delta_width < 0)
				{
					if (columnp->getWidth() > 0)
					{
						// Statically sized column, give all remaining width to
						// this column
						columnp->setWidth(columnp->getWidth() +
										  remaining_width);
						if (columnp->mRelWidth > 0.f)
						{
							columnp->mRelWidth =
								(F32)columnp->getWidth() /
								(F32)mColumn->mParentCtrl->getItemListRect().getWidth();
						}
						// All padding went to this widget, we are done
						break;
					}
				}
				else
				{
					// User growing column, need to take width from other
					// columns
					remaining_width += resize_buffer_amt;

					if (columnp->getWidth() > 0)
					{
						columnp->setWidth(columnp->getWidth() -
										  llmin(columnp->getWidth() -
												MIN_COLUMN_WIDTH,
												delta_width));
						if (columnp->mRelWidth > 0.f)
						{
							columnp->mRelWidth =
								(F32)columnp->getWidth() /
								(F32)mColumn->mParentCtrl->getItemListRect().getWidth();
						}
					}

					if (remaining_width >= 0)
					{
						// Width sucked up from neighboring columns, done
						break;
					}
				}
			}
		}

		// Clamp resize amount to maximum that can be absorbed by other columns
		if (delta_width > 0)
		{
			delta_width += llmin(remaining_width, 0);
		}

		// Propagate constrained delta_width to new width for this column
		new_width = getRect().getWidth() + delta_width -
					mColumn->mParentCtrl->getColumnPadding();

		// Use requested width
		mColumn->setWidth(new_width);

		// Update proportional spacing
		if (mColumn->mRelWidth > 0.f)
		{
			mColumn->mRelWidth =
					(F32)new_width /
					(F32)mColumn->mParentCtrl->getItemListRect().getWidth();
		}

		// Tell scroll list to layout columns again. Do immediate update to get
		// proper feedback to resize handle which needs to know how far the
		// resize actually went.
		mColumn->mParentCtrl->updateColumns(true);
	}
}

void LLColumnHeader::setHasResizableElement(bool resizable)
{
	if (mHasResizableElement != resizable)
	{
		mColumn->mParentCtrl->dirtyColumns();
		mHasResizableElement = resizable;
	}
}

void LLColumnHeader::updateResizeBars()
{
	S32 num_resizable_columns = 0;
	for (S32 col = 0, count = mColumn->mParentCtrl->getNumColumns();
		 col < count; ++col)
	{
		LLScrollListColumn* columnp = mColumn->mParentCtrl->getColumn(col);
		if (!columnp) continue;

		LLColumnHeader* headerp = columnp->mHeader;
		if (headerp && headerp->canResize())
		{
			++num_resizable_columns;
		}
	}

	S32 num_resizers_enabled = 0;

	// Now enable/disable resize handles on resizable columns if we have at
	// least two
	for (S32 col = 0, count = mColumn->mParentCtrl->getNumColumns();
		 col < count; ++col)
	{
		LLScrollListColumn* columnp = mColumn->mParentCtrl->getColumn(col);
		if (!columnp) continue;

		LLColumnHeader* headerp = columnp->mHeader;
		if (!headerp) continue;

		bool enable = num_resizable_columns >= 2 &&
					  num_resizers_enabled < num_resizable_columns - 1 &&
					  headerp->canResize();
		headerp->enableResizeBar(enable);
		if (enable)
		{
			++num_resizers_enabled;
		}
	}
}

void LLColumnHeader::enableResizeBar(bool enable)
{
	mResizeBar->setEnabled(enable);
}

bool LLColumnHeader::canResize()
{
	return getVisible() && (mHasResizableElement || mColumn->mDynamicWidth);
}

void LLScrollListColumn::setWidth(S32 width)
{
	if (!mDynamicWidth && mRelWidth <= 0.f)
	{
		mParentCtrl->updateStaticColumnWidth(this, width);
	}
	mWidth = width;
}

// Default constructor
LLScrollListColumn::LLScrollListColumn()
:	mSortAscending(true),
	mWidth(-1),
	mRelWidth(-1.0),
	mDynamicWidth(false),
	mMaxContentWidth(0),
	mIndex(-1),
	mParentCtrl(NULL),
	mHeader(NULL),
	mFontAlignment(LLFontGL::LEFT)
{
}

LLScrollListColumn::LLScrollListColumn(const LLSD& sd,
									   LLScrollListCtrl* parent)
:	mWidth(0),
	mIndex (-1),
	mParentCtrl(parent),
	mHeader(NULL),
	mMaxContentWidth(0),
	mDynamicWidth(false),
	mSortAscending(true),
	mRelWidth(-1.f)
{
	mName = sd.get("name").asString();
	mSortingColumn = mName;
	if (sd.has("sort"))
	{
		mSortingColumn = sd.get("sort").asString();
	}
	if (sd.has("sort_ascending"))
	{
		mSortAscending = sd.get("sort_ascending").asBoolean();
	}
	mLabel = sd.get("label").asString();
	if (sd.has("relwidth") && (F32)sd.get("relwidth").asReal() > 0)
	{
		mRelWidth = llclamp((F32)sd.get("relwidth").asReal(), 0.f, 1.f);
	}
	else if (sd.has("dynamicwidth") && sd.get("dynamicwidth").asBoolean())
	{
		mDynamicWidth = true;
		mRelWidth = -1;
	}
	else
	{
		setWidth(sd.get("width").asInteger());
	}

	if (sd.has("halign"))
	{
		mFontAlignment =
			(LLFontGL::HAlign)llclamp(sd.get("halign").asInteger(),
									  (S32)LLFontGL::LEFT,
									  (S32)LLFontGL::HCENTER);
	}
	else
	{
		mFontAlignment = LLFontGL::LEFT;
	}
}
