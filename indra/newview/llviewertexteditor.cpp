/**
 * @file llviewertexteditor.cpp
 * @brief Text editor widget to let users enter a multi-line document.
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

#include "llviewerprecompiledheaders.h"

#include <utility>

#include "llviewertexteditor.h"

#include "llaudioengine.h"
#include "llnotecard.h"
#include "llmemorystream.h"
#include "llnotifications.h"
#include "llscrollbar.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llwindow.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llfloaterchat.h"
#include "llfloaterworldmap.h"
#include "llgridmanager.h"
#include "llinventoryactions.h"
#include "llinventorybridge.h"
#include "lllogchat.h"
#include "llpreviewlandmark.h"
#include "llpreviewnotecard.h"
#include "llpreviewtexture.h"
#include "lltooldraganddrop.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewertexturelist.h"

static const std::string LL_TEXT_EDITOR_TAG = "text_editor";
static LLRegisterWidget<LLViewerTextEditor> r(LL_TEXT_EDITOR_TAG);

///----------------------------------------------------------------------------
/// Class LLEmbeddedNotecardOpener
///----------------------------------------------------------------------------
class LLEmbeddedNotecardOpener : public LLInventoryCallback
{
protected:
	LOG_CLASS(LLEmbeddedNotecardOpener);

	LLViewerTextEditor* mTextEditor;

public:
	LLEmbeddedNotecardOpener()
	:	mTextEditor(NULL)
	{
	}

	void setEditor(LLViewerTextEditor* editp)	{ mTextEditor = editp; }

	void fire(const LLUUID& inv_item) override
	{
		if (!mTextEditor)
		{
			// The parent text editor may have vanished by now. In that case
			// just quit.
			llwarns << "Copy from notecard callback fired but parent notecard closed. Item ID: "
					<< inv_item << llendl;
			return;
		}

		LLInventoryItem* item = gInventory.getItem(inv_item);
		if (!item)
		{
			llwarns << "Item add reported, but not found in inventory. Item ID: "
					<< inv_item << llendl;
			return;
		}

		LL_DEBUGS("CopyFromNotecard") << "Copy from notecard callback fired for item ID: "
									  << inv_item << LL_ENDL;
		// See if we can bring an existing preview to the front
		if (!LLPreview::show(item->getUUID(), true))
		{
			// There is not one, so make a new preview
			S32 left, top;
			gFloaterViewp->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("NotecardEditorRect");
			rect.translate(left - rect.mLeft, top - rect.mTop);
			LLPreviewNotecard* preview =
				new LLPreviewNotecard("preview notecard", rect,
									  "Embedded Note: " + item->getName(),
									  item->getUUID(), LLUUID::null,
									  item->getAssetUUID(), true,
									  (LLViewerInventoryItem*)item);
			preview->setFocus(true);

			// Force to be entirely onscreen.
			gFloaterViewp->adjustToFitScreen(preview);
		}
	}
};

///////////////////////////////////////////////////////////////////////////////
// LLEmbeddedItems
//
// Embedded items are stored as:
// * A global map of llwchar to LLInventoryItem
// * This is unique for each item embedded in any notecard to support
//	 copy/paste across notecards
// * A per-notecard set of embeded llwchars for easy removal from the global
//   list
// * A per-notecard vector of embedded lwchars for mapping from old style 0x80
//	 + item format notechards

class LLEmbeddedItems
{
protected:
	LOG_CLASS(LLEmbeddedItems);

public:
	LLEmbeddedItems(const LLViewerTextEditor* editor);
	~LLEmbeddedItems();
	void clear();

	// Returns true if there are no embedded items.
	bool empty();

	void bindEmbeddedChars(LLFontGL* font) const;
	void unbindEmbeddedChars(LLFontGL* font) const;

	bool insertEmbeddedItem(LLInventoryItem* item, llwchar* value, bool is_new);
	bool removeEmbeddedItem(llwchar ext_char);

	// Returns true if /this/ editor has an entry for this item
	bool hasEmbeddedItem(llwchar ext_char);

	void getEmbeddedItemList(std::vector<LLPointer<LLInventoryItem> >& items);
	void addItems(const std::vector<LLPointer<LLInventoryItem> >& items);

	llwchar getEmbeddedCharFromIndex(S32 index);

	void  removeUnusedChars();
	void copyUsedCharsToIndexed();
	S32 getIndexFromEmbeddedChar(llwchar wch);

	void markSaved();

	// returns item from static list
	static LLInventoryItem* getEmbeddedItem(llwchar ext_char);
	// returns whether item from static list is saved
	static bool getEmbeddedItemSaved(llwchar ext_char);

private:

	struct embedded_info_t
	{
		LLPointer<LLInventoryItem> mItem;
		bool mSaved;
	};
	typedef std::map<llwchar, embedded_info_t> item_map_t;
	static item_map_t sEntries;
	static std::stack<llwchar> sFreeEntries;

	std::set<llwchar> mEmbeddedUsedChars;	 // list of used llwchars
	// index -> wchar for 0x80 + index format
	std::vector<llwchar> mEmbeddedIndexedChars;
	const LLViewerTextEditor* mEditor;
};

//statics
LLEmbeddedItems::item_map_t LLEmbeddedItems::sEntries;
std::stack<llwchar> LLEmbeddedItems::sFreeEntries;

LLEmbeddedItems::LLEmbeddedItems(const LLViewerTextEditor* editor)
:	mEditor(editor)
{
}

LLEmbeddedItems::~LLEmbeddedItems()
{
	clear();
}

void LLEmbeddedItems::clear()
{
	// Remove entries for this editor from static list
	for (std::set<llwchar>::iterator iter = mEmbeddedUsedChars.begin(),
									 end = mEmbeddedUsedChars.end();
		 iter != end; )
	{
		std::set<llwchar>::iterator curiter = iter++;
		removeEmbeddedItem(*curiter);
	}
	mEmbeddedUsedChars.clear();
	mEmbeddedIndexedChars.clear();
}

bool LLEmbeddedItems::empty()
{
	removeUnusedChars();
	return mEmbeddedUsedChars.empty();
}

// Inserts a new unique entry
bool LLEmbeddedItems::insertEmbeddedItem(LLInventoryItem* item,
										 llwchar* ext_char, bool is_new)
{
	// Now insert a new one
	llwchar wc_emb;
	if (!sFreeEntries.empty())
	{
		wc_emb = sFreeEntries.top();
		sFreeEntries.pop();
	}
	else if (sEntries.empty())
	{
		wc_emb = FIRST_EMBEDDED_CHAR;
	}
	else
	{
		item_map_t::iterator last = sEntries.end();
		--last;
		wc_emb = last->first;
		if (wc_emb >= LAST_EMBEDDED_CHAR)
		{
			return false;
		}
		++wc_emb;
	}

	sEntries[wc_emb].mItem = item;
	sEntries[wc_emb].mSaved = !is_new;
	*ext_char = wc_emb;
	mEmbeddedUsedChars.insert(wc_emb);
	return true;
}

// Removes an entry (all entries are unique)
bool LLEmbeddedItems::removeEmbeddedItem(llwchar ext_char)
{
	mEmbeddedUsedChars.erase(ext_char);
	item_map_t::iterator iter = sEntries.find(ext_char);
	if (iter != sEntries.end())
	{
		sEntries.erase(ext_char);
		sFreeEntries.push(ext_char);
		return true;
	}
	return false;
}

//static
LLInventoryItem* LLEmbeddedItems::getEmbeddedItem(llwchar ext_char)
{
	if (ext_char >= FIRST_EMBEDDED_CHAR && ext_char <= LAST_EMBEDDED_CHAR)
	{
		item_map_t::iterator iter = sEntries.find(ext_char);
		if (iter != sEntries.end())
		{
			return iter->second.mItem;
		}
	}
	return NULL;
}

//static
bool LLEmbeddedItems::getEmbeddedItemSaved(llwchar ext_char)
{
	if (ext_char >= FIRST_EMBEDDED_CHAR && ext_char <= LAST_EMBEDDED_CHAR)
	{
		item_map_t::iterator iter = sEntries.find(ext_char);
		if (iter != sEntries.end())
		{
			return iter->second.mSaved;
		}
	}
	return false;
}

llwchar	LLEmbeddedItems::getEmbeddedCharFromIndex(S32 index)
{
	if (index >= (S32)mEmbeddedIndexedChars.size())
	{
		llwarns << "No item for embedded char " << index
				<< " using LL_UNKNOWN_CHAR" << llendl;
		return LL_UNKNOWN_CHAR;
	}
	return mEmbeddedIndexedChars[index];
}

void LLEmbeddedItems::removeUnusedChars()
{
	std::set<llwchar> used = mEmbeddedUsedChars;
	const LLWString& wtext = mEditor->getWText();
	for (S32 i = 0, count = wtext.size(); i < count; ++i)
	{
		llwchar wc = wtext[i];
		if (wc >= FIRST_EMBEDDED_CHAR && wc <= LAST_EMBEDDED_CHAR)
		{
			used.erase(wc);
		}
	}
	// Remove chars not actually used
	for (std::set<llwchar>::iterator iter = used.begin(), end = used.end();
		 iter != end; ++iter)
	{
		removeEmbeddedItem(*iter);
	}
}

void LLEmbeddedItems::copyUsedCharsToIndexed()
{
	// Prune unused items
	removeUnusedChars();

	// Copy all used llwchars to mEmbeddedIndexedChars
	mEmbeddedIndexedChars.clear();
	for (std::set<llwchar>::iterator iter = mEmbeddedUsedChars.begin(),
									 end = mEmbeddedUsedChars.end();
		 iter != end; ++iter)
	{
		mEmbeddedIndexedChars.emplace_back(*iter);
	}
}

S32 LLEmbeddedItems::getIndexFromEmbeddedChar(llwchar wch)
{
	S32 idx = 0;
	for (std::vector<llwchar>::iterator iter = mEmbeddedIndexedChars.begin(),
										end = mEmbeddedIndexedChars.end();
		 iter != end; ++iter)
	{
		if (wch == *iter)
		{
			break;
		}
		++idx;
	}
	if (idx < (S32)mEmbeddedIndexedChars.size())
	{
		return idx;
	}
	else
	{
		llwarns << "Embedded char " << wch << " not found, using 0" << llendl;
		return 0;
	}
}

bool LLEmbeddedItems::hasEmbeddedItem(llwchar ext_char)
{
	std::set<llwchar>::iterator iter = mEmbeddedUsedChars.find(ext_char);
	if (iter != mEmbeddedUsedChars.end())
	{
		return true;
	}
	return false;
}

void LLEmbeddedItems::bindEmbeddedChars(LLFontGL* font) const
{
	if (sEntries.empty())
	{
		return;
	}

	for (std::set<llwchar>::const_iterator iter1 = mEmbeddedUsedChars.begin(),
										   end1 = mEmbeddedUsedChars.end();
		 iter1 != end1; ++iter1)
	{
		llwchar wch = *iter1;
		item_map_t::iterator iter2 = sEntries.find(wch);
		if (iter2 == sEntries.end())
		{
			continue;
		}
		LLInventoryItem* item = iter2->second.mItem;
		if (!item)
		{
			continue;
		}
		const char* img_name;
		switch (item->getType())
		{
			case LLAssetType::AT_TEXTURE:
				if (item->getInventoryType() == LLInventoryType::IT_SNAPSHOT)
				{
					img_name = "inv_item_snapshot.tga";
				}
				else
				{
					img_name = "inv_item_texture.tga";
				}
				break;

			case LLAssetType::AT_SOUND:
				img_name = "inv_item_sound.tga";
				break;

			case LLAssetType::AT_CALLINGCARD:
				img_name = "inv_item_callingcard_offline.tga";
				break;

			case LLAssetType::AT_LANDMARK:
				if (item->getFlags() &
					LLInventoryItem::II_FLAGS_LANDMARK_VISITED)
				{
					img_name = "inv_item_landmark_visited.tga";
				}
				else
				{
					img_name = "inv_item_landmark.tga";
				}
				break;

			case LLAssetType::AT_CLOTHING:
				img_name = "inv_item_clothing.tga";
				break;

			case LLAssetType::AT_OBJECT:
				if (item->getFlags() &
					LLInventoryItem::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS)
				{
					img_name = "inv_item_object_multi.tga";
				}
				else
				{
					img_name = "inv_item_object.tga";
				}
				break;

			case LLAssetType::AT_NOTECARD:
				img_name = "inv_item_notecard.tga";
				break;

			case LLAssetType::AT_LSL_TEXT:
				img_name = "inv_item_script.tga";
				break;

			case LLAssetType::AT_BODYPART:
				img_name = "inv_item_skin.tga";
				break;

			case LLAssetType::AT_ANIMATION:
				img_name = "inv_item_animation.tga";
				break;

			case LLAssetType::AT_GESTURE:
				img_name = "inv_item_gesture.tga";
				break;

			case LLAssetType::AT_SETTINGS:
				img_name = "inv_item_settings.tga";
				break;

			case LLAssetType::AT_MATERIAL:
				img_name = "inv_item_material.tga";
				break;

			default:
				llwarns << "Unknown/unsupported embedded item, type: "
						<< item->getType() << llendl;
				img_name = "inv_item_invalid.tga";
		}

		LLUIImagePtr image = LLUI::getUIImage(img_name);
		if (image.notNull())
		{
			font->addEmbeddedChar(wch, image->getImage(), item->getName());
		}
		else
		{
			llwarns << "Missing image: " << img_name << llendl;
			llassert(false);
		}
	}
}

void LLEmbeddedItems::unbindEmbeddedChars(LLFontGL* font) const
{
	if (sEntries.empty())
	{
		return;
	}

	for (std::set<llwchar>::const_iterator iter = mEmbeddedUsedChars.begin(),
										   end = mEmbeddedUsedChars.end();
		 iter != end; ++iter)
	{
		font->removeEmbeddedChar(*iter);
	}
}

void LLEmbeddedItems::addItems(const std::vector<LLPointer<LLInventoryItem> >& items)
{
	for (std::vector<LLPointer<LLInventoryItem> >::const_iterator
			iter = items.begin(), end = items.end();
		 iter != end; ++iter)
	{
		LLInventoryItem* item = *iter;
		if (item)
		{
			llwchar wc;
			if (!insertEmbeddedItem(item, &wc, false))
			{
				break;
			}
			mEmbeddedIndexedChars.emplace_back(wc);
		}
	}
}

void LLEmbeddedItems::getEmbeddedItemList(std::vector<LLPointer<LLInventoryItem> >& items)
{
	for (std::set<llwchar>::iterator iter = mEmbeddedUsedChars.begin(),
									 end = mEmbeddedUsedChars.end();
		 iter != end; ++iter)
	{
		llwchar wc = *iter;
		LLPointer<LLInventoryItem> item = getEmbeddedItem(wc);
		if (item)
		{
			items.emplace_back(std::move(item));
		}
	}
}

void LLEmbeddedItems::markSaved()
{
	for (std::set<llwchar>::iterator iter = mEmbeddedUsedChars.begin(),
									 end = mEmbeddedUsedChars.end();
		 iter != end; ++iter)
	{
		llwchar wc = *iter;
		sEntries[wc].mSaved = true;
	}
}

///////////////////////////////////////////////////////////////////////////////

class LLViewerTextEditor::LLTextCmdInsertEmbeddedItem final
:	public LLTextEditor::LLTextCmd
{
public:
	LLTextCmdInsertEmbeddedItem(S32 pos, LLInventoryItem* item)
	:	LLTextCmd(pos, false),
		mExtCharValue(0)
	{
		mItem = item;
	}

	bool execute(LLTextEditor* editor, S32* delta) override
	{
		LLViewerTextEditor* viewer_editor = (LLViewerTextEditor*)editor;
		// Take this opportunity to remove any unused embedded items from this
		// editor
		viewer_editor->mEmbeddedItemList->removeUnusedChars();
		if (viewer_editor->mEmbeddedItemList->insertEmbeddedItem(mItem,
																 &mExtCharValue,
																 true))
		{
			LLWString ws;
			ws.assign(1, mExtCharValue);
			*delta = insert(editor, getPosition(), ws);
			return *delta != 0;
		}
		return false;
	}

	S32 undo(LLTextEditor* editor) override
	{
		remove(editor, getPosition(), 1);
		return getPosition();
	}

	S32 redo(LLTextEditor* editor) override
	{
		LLWString ws;
		ws += mExtCharValue;
		insert(editor, getPosition(), ws);
		return getPosition() + 1;
	}

	bool hasExtCharValue(llwchar value) const override
	{
		return value == mExtCharValue;
	}

private:
	LLPointer<LLInventoryItem> mItem;
	llwchar mExtCharValue;
};

struct LLNotecardCopyInfo
{
	LLNotecardCopyInfo(LLViewerTextEditor* ed, LLInventoryItem* item)
	:	mTextEd(ed)
	{
		mItem = item;
	}

	LLViewerTextEditor* mTextEd;
	// need to make this be a copy (not a * here) because it isn't stable.
	// I wish we had passed LLPointers all the way down, but we didn't
	LLPointer<LLInventoryItem> mItem;
};

//----------------------------------------------------------------------------
// LLViewerTextEditor class proper
//----------------------------------------------------------------------------

LLViewerTextEditor::LLViewerTextEditor(const std::string& name,
									   const LLRect& rect, S32 max_length,
									   const std::string& default_text,
									   LLFontGL* font,
									   bool allow_embedded_items)
:	LLTextEditor(name, rect, max_length, default_text, font,
				 allow_embedded_items),
	mDragItemChar(0),
	mDragItemSaved(false),
	mInventoryCallback(new LLEmbeddedNotecardOpener)
{
	mEmbeddedItemList = new LLEmbeddedItems(this);
	mInventoryCallback->setEditor(this);
}

LLViewerTextEditor::~LLViewerTextEditor()
{
	delete mEmbeddedItemList;

	// The inventory callback may still be in use by gInventoryCallbackManager
	// so set its reference to this to null.
	mInventoryCallback->setEditor(NULL);
}

//virtual
void LLViewerTextEditor::makePristine()
{
	mEmbeddedItemList->markSaved();
	LLTextEditor::makePristine();
}

bool LLViewerTextEditor::handleToolTip(S32 x, S32 y, std::string& msg,
									   LLRect* sticky_rect_screen)
{
	for (child_list_const_iter_t child_iter = getChildList()->begin();
		 child_iter != getChildList()->end(); ++child_iter)
	{
		LLView* viewp = *child_iter;
		S32 local_x = x - viewp->getRect().mLeft;
		S32 local_y = y - viewp->getRect().mBottom;
		if (viewp->getVisible() && viewp->getEnabled() &&
			viewp->pointInView(local_x, local_y) &&
			viewp->handleToolTip(local_x, local_y, msg, sticky_rect_screen))
		{
			return true;
		}
	}

	if (mSegments.empty())
	{
		return true;
	}

	const LLTextSegment* cur_segment = getSegmentAtLocalPos(x, y);
	if (cur_segment)
	{
		bool has_tool_tip = false;
		if (cur_segment->getStyle()->getIsEmbeddedItem())
		{
			LLWString wtip;
			has_tool_tip = getEmbeddedItemToolTipAtPos(cur_segment->getStart(),
													   wtip);
			msg = wstring_to_utf8str(wtip);
		}
		else
		{
			has_tool_tip = cur_segment->getToolTip(msg);
		}
		if (has_tool_tip)
		{
			// Just use a slop area around the cursor
			// Convert rect local to screen coordinates
			S32 SLOP = 8;
			localPointToScreen(x - SLOP, y - SLOP,
							   &(sticky_rect_screen->mLeft),
							   &(sticky_rect_screen->mBottom));
			sticky_rect_screen->mRight = sticky_rect_screen->mLeft + 2 * SLOP;
			sticky_rect_screen->mTop = sticky_rect_screen->mBottom + 2 * SLOP;
		}
	}
	return true;
}

bool LLViewerTextEditor::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Let scrollbar have first dibs
	bool handled = LLView::childrenHandleMouseDown(x, y, mask) != NULL;

	// Enable I Agree checkbox if the user scrolled through entire text
	if (mOnScrollEndCallback &&
		mScrollbar->getDocPos() == mScrollbar->getDocPosMax())
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}

	if (!handled)
	{
		if (!(mask & MASK_SHIFT))
		{
			deselect();
		}

		bool start_select = true;
		if (allowsEmbeddedItems())
		{
			setCursorAtLocalPos(x, y, false);
			llwchar wc = 0;
			if (mCursorPos < getLength())
			{
				wc = getWChar(mCursorPos);
			}
			LLInventoryItem* item_at_pos = LLEmbeddedItems::getEmbeddedItem(wc);
			if (item_at_pos)
			{
				mDragItem = item_at_pos;
				mDragItemChar = wc;
				mDragItemSaved = LLEmbeddedItems::getEmbeddedItemSaved(wc);
				gFocusMgr.setMouseCapture(this);
				mMouseDownX = x;
				mMouseDownY = y;
				S32 screen_x;
				S32 screen_y;
				localPointToScreen(x, y, &screen_x, &screen_y);
				gToolDragAndDrop.setDragStart(screen_x, screen_y);

				start_select = false;
			}
			else
			{
				mDragItem = NULL;
			}
		}

		if (start_select)
		{
			// If we are not scrolling (handled by child) then we are selecting
			if (mask & MASK_SHIFT)
			{
				S32 old_cursor_pos = mCursorPos;
				setCursorAtLocalPos(x, y, true);

				if (hasSelection())
				{
					mSelectionEnd = mCursorPos;
				}
				else
				{
					mSelectionStart = old_cursor_pos;
					mSelectionEnd = mCursorPos;
				}
				// Assume we are starting a drag select
				mIsSelecting = true;
			}
			else
			{
				setCursorAtLocalPos(x, y, true);
				startSelection();
			}
			gFocusMgr.setMouseCapture(this);
		}

		handled = true;
	}

	if (hasTabStop())
	{
		setFocus(true);
		handled = true;
	}

	// Delay cursor flashing
	resetKeystrokeTimer();

	return handled;
}

bool LLViewerTextEditor::handleHover(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	if (!mDragItem)
	{
		// Leave hover segment active during drag and drop
		mHoverSegment = NULL;
	}
	if (hasMouseCapture())
	{
		if (mIsSelecting)
		{
			if (x != mLastSelectionX || y != mLastSelectionY)
			{
				mLastSelectionX = x;
				mLastSelectionY = y;
			}

			if (y > getTextRect().mTop)
			{
				mScrollbar->setDocPos(mScrollbar->getDocPos() - 1);
			}
			else if (y < getTextRect().mBottom)
			{
				mScrollbar->setDocPos(mScrollbar->getDocPos() + 1);
			}

			setCursorAtLocalPos(x, y, true);
			mSelectionEnd = mCursorPos;

			updateScrollFromCursor();
			gWindowp->setCursor(UI_CURSOR_IBEAM);
		}
		else if (mDragItem)
		{
			S32 screen_x;
			S32 screen_y;
			localPointToScreen(x, y, &screen_x, &screen_y);

			if (gToolDragAndDrop.isOverThreshold(screen_x, screen_y))
			{
				const LLUUID& src_id = isPristine() ? mSourceID : LLUUID::null;
				gToolDragAndDrop.beginDrag(
					LLAssetType::lookupDragAndDropType(mDragItem->getType()),
					mDragItem->getUUID(), LLToolDragAndDrop::SOURCE_NOTECARD,
					src_id, mObjectID);

				return gToolDragAndDrop.handleHover(x, y, mask);
			}

			gWindowp->setCursor(UI_CURSOR_HAND);
		}

		LL_DEBUGS("UserInput") << "hover handled by " << getName()
							   << " (active)" << LL_ENDL;
		handled = true;
	}

	if (!handled)
	{
		// Pass to children
		handled = LLView::childrenHandleHover(x, y, mask) != NULL;
	}

	if (handled)
	{
		// Delay cursor flashing
		resetKeystrokeTimer();
	}

	// Opaque
	if (!handled)
	{
		// Check to see if we are over an HTML-style link
		if (!mSegments.empty())
		{
			const LLTextSegment* cur_segment = getSegmentAtLocalPos(x, y);
			if (cur_segment)
			{
				if (cur_segment->getStyle()->isLink())
				{
					LL_DEBUGS("UserInput") << "hover handled by " << getName()
										   << " (over link, inactive)"
										   << LL_ENDL;
					gWindowp->setCursor(UI_CURSOR_HAND);
					handled = true;
				}
				else if (cur_segment->getStyle()->getIsEmbeddedItem())
				{
					LL_DEBUGS("UserInput") << "hover handled by "
										   << getName()
										   << " (over embedded item, inactive)"
										   << LL_ENDL;
					gWindowp->setCursor(UI_CURSOR_HAND);
					handled = true;
				}
				mHoverSegment = cur_segment;
			}
		}

		if (!handled)
		{
			LL_DEBUGS("UserInput") << "hover handled by " << getName()
								   << " (inactive)" << LL_ENDL;
			if (!mScrollbar->getVisible() ||
				x < getRect().getWidth() - SCROLLBAR_SIZE)
			{
				gWindowp->setCursor(UI_CURSOR_IBEAM);
			}
			else
			{
				gWindowp->setCursor(UI_CURSOR_ARROW);
			}
			handled = true;
		}
	}

	return handled;
}

bool LLViewerTextEditor::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		if (mDragItem)
		{
			// mouse down was on an item
			S32 dx = x - mMouseDownX;
			S32 dy = y - mMouseDownY;
			if (-2 < dx && dx < 2 && -2 < dy && dy < 2)
			{
				if (mDragItemSaved)
				{
					openEmbeddedItem(mDragItem, mDragItemChar);
				}
				else
				{
					showUnsavedAlertDialog(mDragItem);
				}
			}
		}
		mDragItem = NULL;
	}

	bool handled = LLTextEditor::handleMouseUp(x,y,mask);

	// Used to enable I Agree checkbox if the user scrolled through entire text
	if (mOnScrollEndCallback &&
		mScrollbar->getDocPos() == mScrollbar->getDocPosMax())
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}

	return handled;
}

bool LLViewerTextEditor::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = childrenHandleRightMouseDown(x, y, mask) != NULL;
	if (!handled)
	{
		handled = LLTextEditor::handleRightMouseDown(x, y, mask);
	}

	return handled;
}

bool LLViewerTextEditor::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = childrenHandleMiddleMouseDown(x, y, mask) != NULL;
	if (!handled)
	{
		handled = LLTextEditor::handleMiddleMouseDown(x, y, mask);
	}
	return handled;
}

bool LLViewerTextEditor::handleMiddleMouseUp(S32 x, S32 y, MASK mask)
{
	return childrenHandleMiddleMouseUp(x, y, mask) != NULL;
}

bool LLViewerTextEditor::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	// let scrollbar have first dibs
	bool handled = LLView::childrenHandleDoubleClick(x, y, mask) != NULL;
	if (!handled)
	{
		if (allowsEmbeddedItems())
		{
			const LLTextSegment* cur_segment = getSegmentAtLocalPos(x, y);
			if (cur_segment && cur_segment->getStyle()->getIsEmbeddedItem())
			{
				if (openEmbeddedItemAtPos(cur_segment->getStart()))
				{
					deselect();
					setFocus(false);
					return true;
				}
			}
		}

		setCursorAtLocalPos(x, y, false);
		deselect();

		const LLWString &text = getWText();

		if (LLWStringUtil::isPartOfWord(text[mCursorPos]))
		{
			// Select word the cursor is over
			while (mCursorPos > 0 &&
				   LLWStringUtil::isPartOfWord(text[mCursorPos - 1]))
			{
				--mCursorPos;
			}
			startSelection();

			while (mCursorPos < (S32)text.length() &&
				   LLWStringUtil::isPartOfWord(text[mCursorPos]))
			{
				++mCursorPos;
			}

			mSelectionEnd = mCursorPos;
		}
		else if (mCursorPos < (S32)text.length() &&
				 !iswspace(text[mCursorPos]))
		{
			// Select the character the cursor is over
			startSelection();
			mSelectionEnd = ++mCursorPos;
		}

		// We do not want handleMouseUp() to "finish" the selection and thereby
		// set mSelectionEnd to where the mouse is, so we finish the selection
		// here.
		mIsSelecting = false;

		// delay cursor flashing
		resetKeystrokeTimer();

		// take selection to 'primary' clipboard
		updatePrimary();

		handled = true;
	}

	return handled;
}

// Allows calling cards to be dropped onto text fields. Appends the name and
// a carriage return.
//virtual
bool LLViewerTextEditor::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
										   EDragAndDropType cargo_type,
										   void* cargo_data,
										   EAcceptance* accept,
										   std::string& tooltip_msg)
{
	LLToolDragAndDrop::ESource source = gToolDragAndDrop.getSource();
	if (source == LLToolDragAndDrop::SOURCE_NOTECARD)
	{
		// We currently do not handle dragging items from one notecard to
		// another since items in a notecard must be in Inventory to be
		// verified. See DEV-2891.
		LL_DEBUGS("DragAndDrop") << "Cannot drag from another notecard."
								 << LL_ENDL;
		return false;
	}

	LL_DEBUGS("UserInput") << "dragAndDrop handled by LLViewerTextEditor "
						   << getName() << LL_ENDL;

	if (!getEnabled() || !acceptsTextInput() || !allowsEmbeddedItems() ||
		!cargo_data)
	{
		// Not enabled/allowed/valid
		*accept = ACCEPT_NO;
		// Handled nonetheless
		return true;
	}

	bool supported;
	switch (cargo_type)
	{
		case DAD_CALLINGCARD:
		case DAD_TEXTURE:
		case DAD_SOUND:
		case DAD_LANDMARK:
		case DAD_SCRIPT:
		case DAD_CLOTHING:
		case DAD_OBJECT:
		case DAD_NOTECARD:
		case DAD_BODYPART:
		case DAD_ANIMATION:
		case DAD_GESTURE:
#if LL_MESH_ASSET_SUPPORT
		case DAD_MESH:
#endif
			supported = true;
			break;
		case DAD_MATERIAL:
			supported = gAgent.hasInventoryMaterial();
		case DAD_SETTINGS:
			supported = gAgent.hasExtendedEnvironment();
			break;
		default:
			supported = false;
	}
	if (!supported)
	{
		LL_DEBUGS("DragAndDrop") << "Unsupported item type " << cargo_type
								 << "for embedding" << LL_ENDL;
		*accept = ACCEPT_NO;
		// Handled nonetheless
		return true;
	}

	LLInventoryItem* item = (LLInventoryItem*)cargo_data;
	U32 mask_next = item->getPermissions().getMaskNextOwner();
	if ((mask_next & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED)
	{
		if (drop)
		{
			deselect();
			S32 old_cursor = mCursorPos;
			setCursorAtLocalPos(x, y, true);
			S32 insert_pos = mCursorPos;
			setCursorPos(old_cursor);
			bool inserted = insertEmbeddedItem(insert_pos, item);
			if (inserted && old_cursor > mCursorPos)
			{
				setCursorPos(mCursorPos + 1);
			}
				updateLineStartList();
		}
		*accept = ACCEPT_YES_COPY_MULTI;
	}
	else
	{
		*accept = ACCEPT_NO;
		LL_DEBUGS("DragAndDrop") << "Insufficient item permissions"
								 << LL_ENDL;
		if (tooltip_msg.empty())
		{
			tooltip_msg.assign("Only items with unrestricted\n"
								"'next owner' permissions \n"
								"can be attached to notecards.");
		}
	}

	return true;
}

void LLViewerTextEditor::setASCIIEmbeddedText(const std::string& instr)
{
	LLWString wtext;
	const U8* buffer = (U8*)(instr.c_str());
	while (*buffer)
	{
		llwchar wch;
		U8 c = *buffer++;
		if (c >= 0x80)
		{
			S32 index = (S32)(c - 0x80);
			wch = mEmbeddedItemList->getEmbeddedCharFromIndex(index);
		}
		else
		{
			wch = (llwchar)c;
		}
		wtext.push_back(wch);
	}
	setWText(wtext);
}

void LLViewerTextEditor::setEmbeddedText(const std::string& instr)
{
	LLWString wtext = utf8str_to_wstring(instr);
	for (S32 i = 0, count = wtext.size(); i < count; ++i)
	{
		llwchar wch = wtext[i];
		if (wch >= FIRST_EMBEDDED_CHAR && wch <= LAST_EMBEDDED_CHAR)
		{
			S32 index = wch - FIRST_EMBEDDED_CHAR;
			wtext[i] = mEmbeddedItemList->getEmbeddedCharFromIndex(index);
		}
	}
	setWText(wtext);
}

std::string LLViewerTextEditor::getEmbeddedText()
{
	std::string outtext;
	LLWString outtextw;

	mEmbeddedItemList->copyUsedCharsToIndexed();

	for (S32 i = 0, count = getWText().size(); i < count; ++i)
	{
		llwchar wch = getWChar(i);
		if (wch >= FIRST_EMBEDDED_CHAR && wch <= LAST_EMBEDDED_CHAR)
		{
			S32 index = mEmbeddedItemList->getIndexFromEmbeddedChar(wch);
			wch = FIRST_EMBEDDED_CHAR + index;
		}
		outtextw.push_back(wch);
	}
	outtext = wstring_to_utf8str(outtextw);

	return outtext;
}

std::string LLViewerTextEditor::appendTime(bool prepend_newline)
{
	std::string text = LLLogChat::timestamp(true) + " ";
	appendColoredText(text, false, prepend_newline, LLColor4::grey);
	return text;
}

llwchar LLViewerTextEditor::pasteEmbeddedItem(llwchar ext_char)
{
	if (mEmbeddedItemList->hasEmbeddedItem(ext_char))
	{
		return ext_char; // already exists in my list
	}
	LLInventoryItem* item = LLEmbeddedItems::getEmbeddedItem(ext_char);
	if (item)
	{
		// Add item to my list and return new llwchar associated with it
		llwchar new_wc;
		if (mEmbeddedItemList->insertEmbeddedItem(item, &new_wc, true))
		{
			return new_wc;
		}
	}
	return LL_UNKNOWN_CHAR; // Item not found or list full
}

void LLViewerTextEditor::bindEmbeddedChars(LLFontGL* font) const
{
	mEmbeddedItemList->bindEmbeddedChars(font);
}

void LLViewerTextEditor::unbindEmbeddedChars(LLFontGL* font) const
{
	mEmbeddedItemList->unbindEmbeddedChars(font);
}

bool LLViewerTextEditor::getEmbeddedItemToolTipAtPos(S32 pos,
													 LLWString &msg) const
{
	if (pos < getLength())
	{
		LLInventoryItem* item = LLEmbeddedItems::getEmbeddedItem(getWChar(pos));
		if (item)
		{
			msg = utf8str_to_wstring(item->getName());
			msg += '\n';
			msg += utf8str_to_wstring(item->getDescription());
			return true;
		}
	}
	return false;
}

bool LLViewerTextEditor::openEmbeddedItemAtPos(S32 pos)
{
	if (pos < getLength())
	{
		llwchar wc = getWChar(pos);
		LLInventoryItem* item = LLEmbeddedItems::getEmbeddedItem(wc);
		if (item)
		{
			bool saved = LLEmbeddedItems::getEmbeddedItemSaved(wc);
			if (saved)
			{
				return openEmbeddedItem(item, wc);
			}
			else
			{
				showUnsavedAlertDialog(item);
			}
		}
	}
	return false;
}

bool LLViewerTextEditor::openEmbeddedItem(LLInventoryItem* item, llwchar wc)
{

	switch (item->getType())
	{
		case LLAssetType::AT_TEXTURE:
	  		openEmbeddedTexture(item, wc);
			return true;

		case LLAssetType::AT_SOUND:
			openEmbeddedSound(item, wc);
			return true;

		case LLAssetType::AT_NOTECARD:
			openEmbeddedNotecard(item, wc);
			return true;

		case LLAssetType::AT_LANDMARK:
			openEmbeddedLandmark(item, wc);
			return true;

		case LLAssetType::AT_CALLINGCARD:
			openEmbeddedCallingcard(item, wc);
			return true;

		case LLAssetType::AT_LSL_TEXT:
		case LLAssetType::AT_CLOTHING:
		case LLAssetType::AT_OBJECT:
		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_ANIMATION:
		case LLAssetType::AT_GESTURE:
		case LLAssetType::AT_SETTINGS:
		case LLAssetType::AT_MATERIAL:
			showCopyToInvDialog(item, wc);
			return true;

		default:
			return false;
	}
}

void LLViewerTextEditor::openEmbeddedTexture(LLInventoryItem* item, llwchar wc)
{
	// See if we can bring an existing preview to the front.
	// *NOTE: Just for embedded texture, we should use getAssetUUID(),
	// not getUUID(), because LLPreviewTexture passes AssetUUID into
	// LLPreview constructor ItemUUID parameter.

	if (!LLPreview::show(item->getAssetUUID()))
	{
		// There isn't one, so make a new preview
		if (item)
		{
			S32 left, top;
			gFloaterViewp->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
			rect.translate(left - rect.mLeft, top - rect.mTop);

			LLPreviewTexture* preview;
			preview = new LLPreviewTexture("preview texture", rect,
										   item->getName(),
										   item->getAssetUUID(), true);
			preview->setAuxItem(item);
			preview->setNotecardInfo(mNotecardInventoryID, mObjectID);
		}
	}
}

void LLViewerTextEditor::openEmbeddedSound(LLInventoryItem* item, llwchar wc)
{
	// Play sound locally
	LLVector3d lpos_global = gAgent.getPositionGlobal();
	constexpr F32 SOUND_GAIN = 1.f;
	if (gAudiop)
	{
		gAudiop->triggerSound(item->getAssetUUID(), gAgentID, SOUND_GAIN,
							  LLAudioEngine::AUDIO_TYPE_UI, lpos_global);
	}
	showCopyToInvDialog(item, wc);
}

void LLViewerTextEditor::openEmbeddedLandmark(LLInventoryItem* item,
											  llwchar wc)
{
	std::string title = LLTrans::getString("Landmark") + ": " +
						item->getName();
	open_landmark((LLViewerInventoryItem*)item, title);
}

void LLViewerTextEditor::openEmbeddedCallingcard(LLInventoryItem* item,
												 llwchar)
{
	open_callingcard((LLViewerInventoryItem*)item);
}

void LLViewerTextEditor::openEmbeddedNotecard(LLInventoryItem* item,
											  llwchar wc)
{
	copyInventory(item, gInventoryCallbacks.registerCB(mInventoryCallback));
}

void LLViewerTextEditor::showUnsavedAlertDialog(LLInventoryItem* item)
{
	LLSD payload;
	payload["item_id"] = item->getUUID();
	payload["notecard_id"] = mNotecardInventoryID;
	gNotifications.add("ConfirmNotecardSave", LLSD(), payload,
					   LLViewerTextEditor::onNotecardDialog);
}

//static
bool LLViewerTextEditor::onNotecardDialog(const LLSD& notification,
										  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		// itemptr is deleted by LLPreview::save
		LLPointer<LLInventoryItem>* itemptr =
			new LLPointer<LLInventoryItem>(gInventory.getItem(notification["payload"]["item_id"].asUUID()));
		LLPreview::save(notification["payload"]["notecard_id"].asUUID(),
						itemptr);
	}
	return false;
}

void LLViewerTextEditor::showCopyToInvDialog(LLInventoryItem* item, llwchar wc)
{
	LLSD payload;
	LLUUID item_id = item->getUUID();
	payload["item_id"] = item_id;
	payload["item_wc"] = LLSD::Integer(wc);
	gNotifications.add("ConfirmItemCopy", LLSD(), payload,
					   boost::bind(&LLViewerTextEditor::onCopyToInvDialog,
								   this, _1, _2));
}

bool LLViewerTextEditor::onCopyToInvDialog(const LLSD& notification,
										   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		llwchar wc = llwchar(notification["payload"]["item_wc"].asInteger());
		LLInventoryItem* itemp = LLEmbeddedItems::getEmbeddedItem(wc);
		if (itemp)
		{
			copyInventory(itemp);
		}
	}
	return false;
}

// Returns change in number of characters in mWText
S32 LLViewerTextEditor::insertEmbeddedItem(S32 pos, LLInventoryItem* item)
{
	return execute(new LLTextCmdInsertEmbeddedItem(pos, item));
}

bool LLViewerTextEditor::importStream(std::istream& str)
{
	LLNotecard nc(LLNotecard::MAX_SIZE);
	bool success = nc.importStream(str);
	if (success)
	{
		mEmbeddedItemList->clear();
		const std::vector<LLPointer<LLInventoryItem> >& items = nc.getItems();
		mEmbeddedItemList->addItems(items);
		// Actually set the text
		if (allowsEmbeddedItems())
		{
			if (nc.getVersion() == 1)
			{
				setASCIIEmbeddedText(nc.getText());
			}
			else
			{
				setEmbeddedText(nc.getText());
			}
		}
		else
		{
			setText(nc.getText());
		}
	}
	return success;
}

bool LLViewerTextEditor::importBuffer(const char* buffer, S32 length)
{
	LLMemoryStream str((U8*)buffer, length);
	return importStream(str);
}

bool LLViewerTextEditor::exportBuffer(std::string& buffer)
{
	LLNotecard nc(LLNotecard::MAX_SIZE);

	// Get the embedded text and update the item list to just be the used items
	nc.setText(getEmbeddedText());

	// Now get the used items and copy the list to the notecard
	std::vector<LLPointer<LLInventoryItem> > embedded_items;
	mEmbeddedItemList->getEmbeddedItemList(embedded_items);
	nc.setItems(embedded_items);

	std::stringstream out_stream;
	nc.exportStream(out_stream);

	buffer = out_stream.str();

	return true;
}

void LLViewerTextEditor::copyInventory(const LLInventoryItem* item,
									   U32 callback_id)
{
	copy_inventory_from_notecard(mObjectID, mNotecardInventoryID, item,
								 callback_id);
}

bool LLViewerTextEditor::hasEmbeddedInventory()
{
	return !mEmbeddedItemList->empty();
}

//virtual
LLXMLNodePtr LLViewerTextEditor::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLTextEditor::getXML();

	node->setName(LL_TEXT_EDITOR_TAG);

	return node;
}

LLView* LLViewerTextEditor::fromXML(LLXMLNodePtr node, LLView* parent,
									LLUICtrlFactory* factory)
{
	std::string name = LL_TEXT_EDITOR_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	U32 max_text_length = 255;
	node->getAttributeU32("max_length", max_text_length);

	bool allow_embedded_items = false;
	node->getAttributeBool("embedded_items", allow_embedded_items);

	LLFontGL* font = LLView::selectFont(node);

	// std::string text = node->getValue();
	std::string text = node->getTextContents().substr(0, max_text_length - 1);

	if (text.size() > max_text_length)
	{
		// Erase everything from max_text_length on.
		text.erase(max_text_length);
	}

	LLViewerTextEditor* self = new LLViewerTextEditor(name, rect,
													  max_text_length,
													  LLStringUtil::null, font,
													  allow_embedded_items);

	bool ignore_tabs = self->tabsToNextField();
	node->getAttributeBool("ignore_tab", ignore_tabs);
	self->setTabsToNextField(ignore_tabs);

	self->setTextEditorParameters(node);

	bool hide_scrollbar = false;
	node->getAttributeBool("hide_scrollbar", hide_scrollbar);
	self->setHideScrollbarForShortDocs(hide_scrollbar);

	bool hide_border = !self->isBorderVisible();
	node->getAttributeBool("hide_border", hide_border);
	self->setBorderVisible(!hide_border);

	bool parse_html = self->mParseHTML;
	node->getAttributeBool("allow_html", parse_html);
	self->setParseHTML(parse_html);

	self->initFromXML(node, parent);

	// Add text after all parameters have been set
	self->appendStyledText(text, false, false);

	return self;
}
