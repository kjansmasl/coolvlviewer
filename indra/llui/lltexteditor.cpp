/**
 * @file lltexteditor.cpp
 * @brief LLTextEditor base class
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

// Text editor widget to let users enter a a multi-line ASCII document.

#include "linden_common.h"

#include <queue>

#include "lltexteditor.h"

#include "llclipboard.h"
#include "llcontrol.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llfontfreetype.h"
#include "llimagegl.h"
#include "llkeyboard.h"
#include "llmenugl.h"
#include "llrender.h"
#include "llscrollbar.h"
#include "llstl.h"
#include "llspellcheck.h"
#include "lltimer.h"
#include "lluictrlfactory.h"
#include "llundo.h"
#include "llviewborder.h"
#include "llwindow.h"

// gcc 13 sees array bound issues where tehre are none... HB
#if defined(GCC_VERSION) && GCC_VERSION >= 130000
# pragma GCC diagnostic ignored "-Warray-bounds"
# pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

static const std::string LL_SIMPLE_TEXT_EDITOR_TAG = "simple_text_editor";
static LLRegisterWidget<LLTextEditor> r27(LL_SIMPLE_TEXT_EDITOR_TAG);

//
// Constants
//
constexpr S32 UI_TEXTEDITOR_BORDER = 1;
constexpr S32 UI_TEXTEDITOR_H_PAD = 4;
constexpr S32 UI_TEXTEDITOR_V_PAD_TOP = 4;
constexpr S32 UI_TEXTEDITOR_LINE_NUMBER_MARGIN = 32;
constexpr S32 UI_TEXTEDITOR_LINE_NUMBER_DIGITS = 4;
constexpr F32 CURSOR_FLASH_DELAY = 1.f;				// In seconds
constexpr S32 CURSOR_THICKNESS = 2;
constexpr S32 SPACES_PER_TAB = 4;

constexpr F32 PREEDIT_MARKER_BRIGHTNESS = 0.4f;
constexpr S32 PREEDIT_MARKER_GAP = 1;
constexpr S32 PREEDIT_MARKER_POSITION = 2;
constexpr S32 PREEDIT_MARKER_THICKNESS = 1;
constexpr F32 PREEDIT_STANDOUT_BRIGHTNESS = 0.6f;
constexpr S32 PREEDIT_STANDOUT_GAP = 1;
constexpr S32 PREEDIT_STANDOUT_POSITION = 2;
constexpr S32 PREEDIT_STANDOUT_THICKNESS = 2;

LLColor4 LLTextEditor::sLinkColor = LLColor4::blue;
void (*LLTextEditor::sURLcallback)(const std::string&) = NULL;
bool (*LLTextEditor::sSecondlifeURLcallback)(const std::string&) = NULL;
bool (*LLTextEditor::sSecondlifeURLcallbackRightClick)(const std::string&) = NULL;

///////////////////////////////////////////////////////////////////

class LLTextEditor::LLTextCmdInsert : public LLTextEditor::LLTextCmd
{
public:
	LLTextCmdInsert(S32 pos, bool group_with_next, const LLWString& ws)
	:	LLTextCmd(pos, group_with_next),
		mWString(ws)
	{
	}

	virtual ~LLTextCmdInsert()
	{
	}

	virtual bool execute(LLTextEditor* editor, S32* delta)
	{
		*delta = insert(editor, getPosition(), mWString);
		LLWStringUtil::truncate(mWString, *delta);
		//mWString = wstring_truncate(mWString, *delta);
		return (*delta != 0);
	}

	virtual S32 undo(LLTextEditor* editor)
	{
		remove(editor, getPosition(), mWString.length());
		return getPosition();
	}

	virtual S32 redo(LLTextEditor* editor)
	{
		insert(editor, getPosition(), mWString);
		return getPosition() + mWString.length();
	}

private:
	LLWString mWString;
};

///////////////////////////////////////////////////////////////////
class LLTextEditor::LLTextCmdAddChar : public LLTextEditor::LLTextCmd
{
public:
	LLTextCmdAddChar(S32 pos, bool group_with_next, llwchar wc)
	:	LLTextCmd(pos, group_with_next),
		mWString(1, wc),
		mBlockExtensions(false)
	{
	}

	virtual void blockExtensions()
	{
		mBlockExtensions = true;
	}

	virtual bool canExtend(S32 pos) const
	{
		return !mBlockExtensions &&
			   pos == getPosition() + (S32)mWString.length();
	}

	virtual bool execute(LLTextEditor* editor, S32* delta)
	{
		*delta = insert(editor, getPosition(), mWString);
		LLWStringUtil::truncate(mWString, *delta);
		//mWString = wstring_truncate(mWString, *delta);
		return *delta != 0;
	}

	virtual bool extendAndExecute(LLTextEditor* editor, S32 pos, llwchar wc,
								  S32* delta)
	{
		LLWString ws;
		ws += wc;

		*delta = insert(editor, pos, ws);
		if (*delta > 0)
		{
			mWString += wc;
		}
		return *delta != 0;
	}

	virtual S32 undo(LLTextEditor* editor)
	{
		remove(editor, getPosition(), mWString.length());
		return getPosition();
	}

	virtual S32 redo(LLTextEditor* editor)
	{
		insert(editor, getPosition(), mWString);
		return getPosition() + mWString.length();
	}

private:
	LLWString	mWString;
	bool		mBlockExtensions;
};

///////////////////////////////////////////////////////////////////

class LLTextEditor::LLTextCmdOverwriteChar : public LLTextEditor::LLTextCmd
{
public:
	LLTextCmdOverwriteChar(S32 pos, bool group_with_next, llwchar wc)
	:	LLTextCmd(pos, group_with_next),
		mChar(wc),
		mOldChar(0)
	{
	}

	virtual bool execute(LLTextEditor* editor, S32* delta)
	{
		mOldChar = editor->getWChar(getPosition());
		overwrite(editor, getPosition(), mChar);
		*delta = 0;
		return true;
	}

	virtual S32 undo(LLTextEditor* editor)
	{
		overwrite(editor, getPosition(), mOldChar);
		return getPosition();
	}

	virtual S32 redo(LLTextEditor* editor)
	{
		overwrite(editor, getPosition(), mChar);
		return getPosition() + 1;
	}

private:
	llwchar		mChar;
	llwchar		mOldChar;
};

///////////////////////////////////////////////////////////////////

class LLTextEditor::LLTextCmdRemove : public LLTextEditor::LLTextCmd
{
public:
	LLTextCmdRemove(S32 pos, bool group_with_next, S32 len)
	:	LLTextCmd(pos, group_with_next),
		mLen(len)
	{
	}

	virtual bool execute(LLTextEditor* editor, S32* delta)
	{
		mWString = editor->getWSubString(getPosition(), mLen);
		*delta = remove(editor, getPosition(), mLen);
		return *delta != 0;
	}

	virtual S32 undo(LLTextEditor* editor)
	{
		insert(editor, getPosition(), mWString);
		return getPosition() + mWString.length();
	}

	virtual S32 redo(LLTextEditor* editor)
	{
		remove(editor, getPosition(), mLen);
		return getPosition();
	}

private:
	LLWString	mWString;
	S32			mLen;
};

///////////////////////////////////////////////////////////////////

LLTextEditor::LLTextEditor(const std::string& name, const LLRect& rect,
						   S32 max_length,	// In bytes
						   const std::string& default_text, LLFontGL* font,
						   bool allow_embedded_items)
:	LLEditMenuHandler(HAS_CONTEXT_MENU | HAS_UNDO_REDO | HAS_CUSTOM),
	LLUICtrl(name, rect, true, NULL, NULL, FOLLOWS_TOP | FOLLOWS_LEFT),
	mTextIsUpToDate(true),
	mMaxTextByteLength(max_length),
	mBaseDocIsPristine(true),
	mPristineCmd(NULL),
	mLastCmd(NULL),
	mCursorPos(0),
	mIsSelecting(false),
	mSelectionStart(0),
	mSelectionEnd(0),
	mScrolledToBottom(true),
	mOnScrollEndCallback(NULL),
	mOnScrollEndData(NULL),
	mKeystrokeCallback(NULL),
	mKeystrokeData(NULL),
	mOnHandleKeyCallback(NULL),
	mOnHandleKeyData(NULL),
	mCursorColor(LLUI::sTextCursorColor),
	mFgColor(LLUI::sTextFgColor),
	mDefaultColor(LLUI::sTextDefaultColor),
	mReadOnlyFgColor(LLUI::sTextFgReadOnlyColor),
	mWriteableBgColor(LLUI::sTextBgWriteableColor),
	mReadOnlyBgColor(LLUI::sTextBgReadOnlyColor),
	mFocusBgColor(LLUI::sTextBgFocusColor),
	mLinkColor(sLinkColor),
	mReadOnly(false),
	mWordWrap(false),
	mShowLineNumbers(false),
	mTabsToNextField(true),
	mCommitOnFocusLost(false),
	mHideScrollbarForShortDocs(false),
	mTrackBottom(false),
	mAllowEmbeddedItems(allow_embedded_items),
	mPreserveSegments(false),
	mHandleEditKeysDirectly(false),
	mMouseDownX(0),
	mMouseDownY(0),
	mLastSelectionX(-1),
	mLastSelectionY(-1),
	mReflowNeeded(false),
	mScrollNeeded(false),
	mParseHTML(false),
	mSpellCheck(true)
{
	// Reset desired x cursor position
	mDesiredXPixel = -1;

	if (font)
	{
		mGLFont = font;
	}
	else
	{
		mGLFont = LLFontGL::getFontSansSerif();
	}

	updateTextRect();

	S32 line_height = ll_roundp(mGLFont->getLineHeight());
	S32 page_size = mTextRect.getHeight() / line_height;

	// Init the scrollbar
	LLRect scroll_rect;
	scroll_rect.setOriginAndSize(getRect().getWidth() - SCROLLBAR_SIZE, 1,
								 SCROLLBAR_SIZE, getRect().getHeight() - 1);
	S32 lines_in_doc = getLineCount();
	mScrollbar = new LLScrollbar("Scrollbar", scroll_rect,
								 LLScrollbar::VERTICAL, lines_in_doc, 0,
								 page_size, NULL, this);
	mScrollbar->setFollowsRight();
	mScrollbar->setFollowsTop();
	mScrollbar->setFollowsBottom();
	mScrollbar->setEnabled(true);
	mScrollbar->setVisible(true);
	mScrollbar->setOnScrollEndCallback(mOnScrollEndCallback, mOnScrollEndData);
	addChild(mScrollbar);

	mBorder = new LLViewBorder("text ed border",
							   LLRect(0, getRect().getHeight(),
									  getRect().getWidth(), 0),
							   LLViewBorder::BEVEL_IN,
							   LLViewBorder::STYLE_LINE, UI_TEXTEDITOR_BORDER);
	addChild(mBorder);

	appendText(default_text, false, false);

	resetDirty();		// Update saved text state

	mHTML.clear();

	mShowMisspelled = LLSpellCheck::getInstance()->getShowMisspelled();
}

LLTextEditor::~LLTextEditor()
{
	gFocusMgr.releaseFocusIfNeeded(this); // calls onCommit()

	// Scrollbar is deleted by LLView
	mHoverSegment = NULL;
	std::for_each(mSegments.begin(), mSegments.end(), DeletePointer());
	mSegments.clear();

	std::for_each(mUndoStack.begin(), mUndoStack.end(), DeletePointer());
	mUndoStack.clear();
}

void LLTextEditor::spellReplace(SpellMenuBind* data)
{
	if (data)
	{
		S32 length = data->mWordPositionEnd - data->mWordPositionStart;
		remove(data->mWordPositionStart, length, true);
		LLWString clean_string = utf8str_to_wstring(data->mWord);
		insert(data->mWordPositionStart, clean_string, false);
		mCursorPos += clean_string.length() - length;
		needsReflow();
	}
}

void LLTextEditor::spellCorrect(void* data)
{
	SpellMenuBind* menu_bind = (SpellMenuBind*)data;
	LLTextEditor* text = menu_bind->mOrigin;
	if (menu_bind && text)
	{
		LL_DEBUGS("SpellCheck") << menu_bind->mMenuItem->getName()
								<< " : " << text->getName()
								<< " : " << menu_bind->mWord << LL_ENDL;
		text->spellReplace(menu_bind);
		// Make it update:
		text->mKeystrokeTimer.reset();
		text->mPrevSpelledText.erase();
	}
}

void LLTextEditor::spellShow(void* data)
{
	SpellMenuBind* menu_bind = (SpellMenuBind*)data;
	LLTextEditor* text = menu_bind->mOrigin;
	if (menu_bind && text)
	{
		text->mShowMisspelled = (menu_bind->mWord == "Show Misspellings");
		// Make it update:
		text->mKeystrokeTimer.reset();
		text->mPrevSpelledText.erase();
	}
}

void LLTextEditor::spellAdd(void* data)
{
	SpellMenuBind* menu_bind = (SpellMenuBind*)data;
	LLTextEditor* text = menu_bind->mOrigin;
	if (menu_bind && text)
	{
		LLSpellCheck::getInstance()->addToCustomDictionary(menu_bind->mWord);
		// Make it update:
		text->mKeystrokeTimer.reset();
		text->mPrevSpelledText.erase();
	}
}

void LLTextEditor::spellIgnore(void* data)
{
	SpellMenuBind* menu_bind = (SpellMenuBind*)data;
	LLTextEditor* text = menu_bind->mOrigin;
	if (menu_bind && text)
	{
		LLSpellCheck::getInstance()->addToIgnoreList(menu_bind->mWord);
		// Make it update:
		text->mKeystrokeTimer.reset();
		text->mPrevSpelledText.erase();
	}
}

std::vector<S32> LLTextEditor::getMisspelledWordsPositions()
{
	std::vector<S32> bad_words_pos;
    const LLWString& text = mWText;
	std::string selected_word;
	S32 word_start = 0;
	S32 word_end = mSpellCheckStart;
	S32 true_end;

	while (word_end < mSpellCheckEnd)
	{
		if (LLWStringUtil::isPartOfLexicalWord(text[word_end]))
		{
			// Select the word under the cursor
			while (word_end > 0 &&
				   LLWStringUtil::isPartOfLexicalWord(text[word_end - 1]))
			{
				--word_end;
			}
			if (text[word_end] == L'\'')
			{
				// Do not count "'" at the start of a word
				++word_end;
			}
			word_start = word_end;
			while (word_end < (S32)text.length() &&
				   LLWStringUtil::isPartOfLexicalWord(text[word_end]))
			{
				++word_end;
			}
			if (text[word_end - 1] == L'\'')
			{
				// Do not count "'" at the end of a word
				true_end = word_end - 1;
			}
			else
			{
				true_end = word_end;
			}
			if (true_end > word_start + 2)	// Do not bother for 2 or less characters words
			{
				std::string part(text.begin(), text.end());
				selected_word = part.substr(word_start, true_end - word_start);

				if (!LLSpellCheck::getInstance()->checkSpelling(selected_word))
				{
					// Misspelled word here
					bad_words_pos.emplace_back(word_start);
					bad_words_pos.push_back(true_end);
				}
			}
		}
		++word_end;
	}

	return bad_words_pos;
}

void LLTextEditor::setTrackColor(const LLColor4& color)
{
	mScrollbar->setTrackColor(color);
}

void LLTextEditor::setThumbColor(const LLColor4& color)
{
	mScrollbar->setThumbColor(color);
}

void LLTextEditor::setHighlightColor(const LLColor4& color)
{
	mScrollbar->setHighlightColor(color);
}

void LLTextEditor::setShadowColor(const LLColor4& color)
{
	mScrollbar->setShadowColor(color);
}

void LLTextEditor::updateLineStartList(S32 startpos)
{
	updateSegments();

	bindEmbeddedChars(mGLFont);

	S32 seg_num = mSegments.size();
	S32 seg_idx = 0;
	S32 seg_offset = 0;

	if (!mLineStartList.empty())
	{
		getSegmentAndOffset(startpos, &seg_idx, &seg_offset);
		line_info t(seg_idx, seg_offset);
		line_list_t::iterator iter = std::upper_bound(mLineStartList.begin(),
													  mLineStartList.end(), t,
													  line_info_compare());
		if (iter != mLineStartList.begin()) --iter;
		seg_idx = iter->mSegment;
		seg_offset = iter->mOffset;
		mLineStartList.erase(iter, mLineStartList.end());
	}

	while (seg_idx < seg_num)
	{
		mLineStartList.emplace_back(seg_idx, seg_offset);
		bool line_ended = false;
		S32 start_x = mShowLineNumbers ? UI_TEXTEDITOR_LINE_NUMBER_MARGIN : 0;
		S32 line_width = start_x;
		while (!line_ended && seg_idx < seg_num)
		{
			LLTextSegment* segment = mSegments[seg_idx];
			S32 start_idx = segment->getStart() + seg_offset;
			S32 end_idx = start_idx;
			while (end_idx < segment->getEnd() && mWText[end_idx] != '\n')
			{
				++end_idx;
			}
			if (start_idx == end_idx)
			{
				if (end_idx >= segment->getEnd())
				{
					// Empty segment
					++seg_idx;
					seg_offset = 0;
				}
				else
				{
					// Empty line
					line_ended = true;
					++seg_offset;
				}
			}
			else
			{
				const llwchar* str = mWText.c_str() + start_idx;
				S32 drawn = mGLFont->maxDrawableChars(str,
													  (F32)abs(mTextRect.getWidth()) - line_width,
													  end_idx - start_idx,
													  mWordWrap,
													  mAllowEmbeddedItems);
				if (drawn == 0 && line_width == start_x)
				{
					// If at the beginning of a line, draw at least one
					// character, even if it does not all fit.
					drawn = 1;
				}
				seg_offset += drawn;
				line_width += mGLFont->getWidth(str, 0, drawn,
												mAllowEmbeddedItems);
				end_idx = segment->getStart() + seg_offset;
				if (end_idx < segment->getEnd())
				{
					line_ended = true;
					if (mWText[end_idx] == '\n')
					{
						++seg_offset; // skip newline
					}
				}
				else
				{
					// Finished with segment
					++seg_idx;
					seg_offset = 0;
				}
			}
		}
	}

	unbindEmbeddedChars(mGLFont);

	mScrollbar->setDocSize(getLineCount());

	if (mHideScrollbarForShortDocs)
	{
		bool short_doc = mScrollbar->getDocSize() <= mScrollbar->getPageSize();
		mScrollbar->setVisible(!short_doc);
	}

	// If scrolled to bottom, stay at bottom unless user is selecting text.
	// Do this after updating page size.
	if (mScrolledToBottom && mTrackBottom && !hasMouseCapture())
	{
		endOfDoc();
	}
}

////////////////////////////////////////////////////////////
// LLTextEditor
// Public methods

bool LLTextEditor::truncate()
{
	bool did_truncate = false;

	// First rough check - if we are less than 1/4th the size, we are OK
	if (mWText.size() >= (size_t)(mMaxTextByteLength / 4))
	{
		// Have to check actual byte size
		S32 utf8_byte_size = wstring_utf8_length(mWText);
		if (utf8_byte_size > mMaxTextByteLength)
		{
			// Truncate safely in UTF-8
			std::string temp_utf8_text = wstring_to_utf8str(mWText);
			temp_utf8_text = utf8str_truncate(temp_utf8_text,
											  mMaxTextByteLength);
			mWText = utf8str_to_wstring(temp_utf8_text);
			mTextIsUpToDate = false;
			did_truncate = true;
		}
	}

	return did_truncate;
}

void LLTextEditor::setText(const std::string& utf8str)
{
	// LLStringUtil::removeCRLF(utf8str);
	mUTF8Text = utf8str_removeCRLF(utf8str);
	// mUTF8Text = utf8str;
	mWText = utf8str_to_wstring(mUTF8Text);
	mTextIsUpToDate = true;

	truncate();
	blockUndo();

	setCursorPos(0);
	deselect();

	needsReflow();

	resetDirty();
}

void LLTextEditor::setWText(const LLWString& wtext)
{
	mWText = wtext;
	mUTF8Text.clear();
	mTextIsUpToDate = false;

	truncate();
	blockUndo();

	setCursorPos(0);
	deselect();

	needsReflow();

	resetDirty();
}

//virtual
void LLTextEditor::setValue(const LLSD& value)
{
	setText(value.asString());
}

const std::string& LLTextEditor::getText() const
{
	if (!mTextIsUpToDate)
	{
		if (mAllowEmbeddedItems)
		{
			llwarns << "getText() called on text with embedded items (not supported)"
					<< llendl;
		}
		mUTF8Text = wstring_to_utf8str(mWText);
		mTextIsUpToDate = true;
	}
	return mUTF8Text;
}

//virtual
LLSD LLTextEditor::getValue() const
{
	return LLSD(getText());
}

void LLTextEditor::setWordWrap(bool b)
{
	mWordWrap = b;

	setCursorPos(0);
	deselect();

	needsReflow();
}

void LLTextEditor::setBorderVisible(bool b)
{
	mBorder->setVisible(b);
}

bool LLTextEditor::isBorderVisible() const
{
	return mBorder->getVisible();
}

void LLTextEditor::setHideScrollbarForShortDocs(bool b)
{
	mHideScrollbarForShortDocs = b;

	if (mHideScrollbarForShortDocs)
	{
		bool short_doc = mScrollbar->getDocSize() <= mScrollbar->getPageSize();
		mScrollbar->setVisible(!short_doc);
	}
}

void LLTextEditor::selectNext(const std::string& search_text_in,
							  bool case_insensitive, bool wrap)
{
	if (search_text_in.empty())
	{
		return;
	}

	LLWString text = getWText();
	LLWString search_text = utf8str_to_wstring(search_text_in);
	if (case_insensitive)
	{
		LLWStringUtil::toLower(text);
		LLWStringUtil::toLower(search_text);
	}

	if (mIsSelecting)
	{
		LLWString selected_text = text.substr(mSelectionEnd,
											  mSelectionStart - mSelectionEnd);

		if (selected_text == search_text)
		{
			// We already have this word selected, we are searching for the next.
			mCursorPos += search_text.size();
		}
	}

	size_t loc = text.find(search_text, mCursorPos);
	// If Maybe we wrapped, search again
	if (wrap && loc == std::string::npos)
	{
		loc = text.find(search_text);
	}

	// If still not found, then search_text just is not found.
    if (loc == std::string::npos)
	{
		mIsSelecting = false;
		mSelectionEnd = 0;
		mSelectionStart = 0;
		return;
	}

	setCursorPos(loc);
	scrollToPos(mCursorPos);

	mIsSelecting = true;
	mSelectionEnd = mCursorPos;
	mSelectionStart = llmin((S32)getLength(),
							(S32)(mCursorPos + search_text.size()));
}

bool LLTextEditor::replaceText(const std::string& search_text_in,
							   const std::string& replace_text,
							   bool case_insensitive, bool wrap)
{
	bool replaced = false;

	if (search_text_in.empty())
	{
		return replaced;
	}

	LLWString search_text = utf8str_to_wstring(search_text_in);
	if (mIsSelecting)
	{
		LLWString text = getWText();
		LLWString selected_text = text.substr(mSelectionEnd,
											  mSelectionStart - mSelectionEnd);

		if (case_insensitive)
		{
			LLWStringUtil::toLower(selected_text);
			LLWStringUtil::toLower(search_text);
		}

		if (selected_text == search_text)
		{
			// *HACK: this is used when replacing SLURLs with names in chat.
			// We invalidate any existing segment at this position then, when
			// the replacement text length does not match the replaced text
			// length, we shift the segments that follow...
			// *TODO: make it a proper text editor feature, and extend segments
			// preservation over text deletion and insertion.
			if (mPreserveSegments)
			{
				S32 offset = utf8str_to_wstring(replace_text).size() -
							 search_text.size();
				if (offset != 0 && !mSegments.empty())
				{
					for (segment_list_t::iterator it = mSegments.begin(),
												  end = mSegments.end();
						 it != end; )
					{
						LLTextSegment* segment = *it;
						S32 seg_start = segment->getStart();
						S32 seg_end = segment->getEnd();

						if (seg_end > mCursorPos)
						{
							if (seg_start > mCursorPos)
							{
								segment->shift(offset);
							}
							else
							{
								// This is the current segment: only change
								// its end position.
								S32 new_end = seg_end + offset;
								if (seg_start >= new_end)
								{
									// If we replaced it with empty text, we
									// need to delete it entirely.
									delete *it;
									it = mSegments.erase(it);
									continue;
								}
								segment->setEnd(new_end);
							}
						}
						++it;
					}
				}
			}

			insertText(replace_text);
			replaced = true;
		}
	}

	selectNext(search_text_in, case_insensitive, wrap);
	return replaced;
}

void LLTextEditor::replaceTextAll(const std::string& search_text,
								  const std::string& replace_text,
								  bool case_insensitive)
{
	S32 cur_pos = mScrollbar->getDocPos();

	setCursorPos(0);
	selectNext(search_text, case_insensitive, false);

	bool replaced = true;
	while (replaced)
	{
		replaced = replaceText(search_text,replace_text, case_insensitive,
							   false);
	}

	mScrollbar->setDocPos(cur_pos);
}

// Picks a new cursor position based on the screen size of text being drawn.
void LLTextEditor::setCursorAtLocalPos(S32 local_x, S32 local_y, bool round)
{
	setCursorPos(getCursorPosFromLocalCoord(local_x, local_y, round));
}

S32 LLTextEditor::prevWordPos(S32 cursorPos) const
{
	const LLWString& wtext = mWText;
	while (cursorPos > 0 && wtext[cursorPos - 1] == ' ')
	{
		--cursorPos;
	}
	while (cursorPos > 0 && LLWStringUtil::isPartOfWord(wtext[cursorPos - 1]))
	{
		--cursorPos;
	}
	return cursorPos;
}

S32 LLTextEditor::nextWordPos(S32 cursorPos) const
{
	const LLWString& wtext = mWText;
	while (cursorPos < getLength() &&
		   LLWStringUtil::isPartOfWord(wtext[cursorPos]))
	{
		++cursorPos;
	}
	while (cursorPos < getLength() && wtext[cursorPos] == ' ')
	{
		++cursorPos;
	}
	return cursorPos;
}

bool LLTextEditor::getWordBoundriesAt(const S32 at, S32* word_begin,
									  S32* word_length) const
{
	S32 pos = at;
	S32 start;
	if (LLWStringUtil::isPartOfLexicalWord(mWText[pos]))
	{
		while (pos > 0 && LLWStringUtil::isPartOfLexicalWord(mWText[pos - 1]))
		{
			--pos;
		}
		if (mWText[pos] == L'\'')
		{
			// Do not count "'" at the start of a word
			++pos;
		}
		start = pos;
		while (pos < getLength() &&
			   LLWStringUtil::isPartOfLexicalWord(mWText[pos]))
		{
			++pos;
		}
		if (mWText[pos - 1] == L'\'')
		{
			// Do not count "'" at the end of a word
			--pos;
		}

		if (start >= pos)
		{
			return false;
		}

		*word_begin = start;
		*word_length = pos - start;

		return true;
	}

	return false;
}

S32 LLTextEditor::getLineStart(S32 line) const
{
	S32 num_lines = getLineCount();
	if (num_lines == 0)
    {
		return 0;
    }

	line = llclamp(line, 0, num_lines - 1);
	S32 segidx = mLineStartList[line].mSegment;
	S32 segoffset = mLineStartList[line].mOffset;
	LLTextSegment* seg = mSegments[segidx];
	S32 res = seg->getStart() + segoffset;
	if (res > seg->getEnd())
	{
		llwarns << "Text length (" << res << ") greater than text end ("
				<< seg->getEnd() << ")." << llendl;
		res = seg->getEnd();
	}
	return res;
}

// Given an offset into text (pos), find the corresponding line (from the start
// of the doc) and an offset into the line.
void LLTextEditor::getLineAndOffset(S32 startpos, S32* linep, S32* offsetp) const
{
	if (mLineStartList.empty())
	{
		*linep = 0;
		*offsetp = startpos;
	}
	else
	{
		S32 seg_idx, seg_offset;
		getSegmentAndOffset(startpos, &seg_idx, &seg_offset);

		line_info tline(seg_idx, seg_offset);
		line_list_t::const_iterator iter = std::upper_bound(mLineStartList.begin(),
															mLineStartList.end(),
															tline,
															line_info_compare());
		if (iter != mLineStartList.begin()) --iter;
		*linep = iter - mLineStartList.begin();
		S32 line_start = mSegments[iter->mSegment]->getStart() + iter->mOffset;
		*offsetp = startpos - line_start;
	}
}

void LLTextEditor::getSegmentAndOffset(S32 startpos, S32* segidxp,
									   S32* offsetp) const
{
	if (mSegments.empty())
	{
		*segidxp = -1;
		*offsetp = startpos;
	}

	LLTextSegment tseg(startpos);
	segment_list_t::const_iterator seg_iter;
	seg_iter = std::upper_bound(mSegments.begin(), mSegments.end(), &tseg,
								LLTextSegment::compare());
	if (seg_iter != mSegments.begin()) --seg_iter;
	*segidxp = seg_iter - mSegments.begin();
	*offsetp = startpos - (*seg_iter)->getStart();
}

const LLTextSegment* LLTextEditor::getPreviousSegment() const
{
	// Find segment index at character to left of cursor (or rightmost edge of
	// selection)
	S32 idx = llmax(0, getSegmentIdxAtOffset(mCursorPos) - 1);
	return idx >= 0 ? mSegments[idx] : NULL;
}

void LLTextEditor::getSelectedSegments(std::vector<const LLTextSegment*>& segments) const
{
	S32 left = hasSelection() ? llmin(mSelectionStart, mSelectionEnd)
							  : mCursorPos;
	S32 right = hasSelection() ? llmax(mSelectionStart, mSelectionEnd)
							   : mCursorPos;
	S32 first_idx = llmax(0, getSegmentIdxAtOffset(left));
	S32 last_idx = llmax(0, first_idx, getSegmentIdxAtOffset(right));

	for (S32 idx = first_idx; idx <= last_idx; ++idx)
	{
		segments.push_back(mSegments[idx]);
	}
}

S32 LLTextEditor::getCursorPosFromLocalCoord(S32 local_x, S32 local_y,
											 bool round) const
{
	if (mShowLineNumbers)
	{
		local_x -= UI_TEXTEDITOR_LINE_NUMBER_MARGIN;
	}

	// If round is true, if the position is on the right half of a character,
	// the cursor will be put to its right.  If round is false, the cursor will
	// always be put to the character's left.

	// Figure out which line we are nearest to.
	S32 total_lines = getLineCount();
	S32 line_height = ll_roundp(mGLFont->getLineHeight());
	S32 max_visible_lines = mTextRect.getHeight() / line_height;
	S32 scroll_lines = mScrollbar->getDocPos();
	// Lines currently visible
	S32 visible_lines = llmin(total_lines - scroll_lines, max_visible_lines);

	//S32 line = S32(0.5f + ((mTextRect.mTop - local_y) / mGLFont->getLineHeight()));
	S32 line = (mTextRect.mTop - 1 - local_y) / line_height;
	if (line >= total_lines)
	{
		return getLength(); // past the end
	}

	line = llclamp(line, 0, visible_lines) + scroll_lines;

	S32 line_start = getLineStart(line);
	S32 next_start = getLineStart(line + 1);
	S32	line_end = (next_start != line_start) ? next_start - 1 : getLength();

	if (line_start == -1)
	{
		return 0;
	}

	S32 line_len = line_end - line_start;
	S32 pos;
	if (mAllowEmbeddedItems)
	{
		// Figure out which character we are nearest to.
		bindEmbeddedChars(mGLFont);
		pos = mGLFont->charFromPixelOffset(mWText.c_str(), line_start,
										   (F32)(local_x - mTextRect.mLeft),
										   (F32)(mTextRect.getWidth()),
										   line_len, round, true);
		unbindEmbeddedChars(mGLFont);
	}
	else
	{
		pos = mGLFont->charFromPixelOffset(mWText.c_str(), line_start,
										   (F32)(local_x - mTextRect.mLeft),
										   (F32)mTextRect.getWidth(),
										   line_len, round);
	}
	return line_start + pos;
}

void LLTextEditor::setCursor(S32 row, S32 column)
{
	// Make sure we are not trying to set the cursor out of boundaries
	if (row < 0)
	{
		row = 0;
	}
	if (column < 0)
	{
		column = 0;
	}

	const llwchar* doc = mWText.c_str();
	while (row--)
	{
		while (*doc && *doc++ != '\n');
	}
	while (column-- && *doc && *doc++ != '\n');
	setCursorPos(doc - mWText.c_str());
}

void LLTextEditor::setCursorPos(S32 offset)
{
	mCursorPos = llclamp(offset, 0, (S32)getLength());
	needsScroll();
	// Reset desired x cursor position
	mDesiredXPixel = -1;
}

void LLTextEditor::deselect()
{
	mSelectionStart = 0;
	mSelectionEnd = 0;
	mIsSelecting = false;
}

void LLTextEditor::startSelection()
{
	if (!mIsSelecting)
	{
		mIsSelecting = true;
		mSelectionStart = mCursorPos;
		mSelectionEnd = mCursorPos;
	}
}

void LLTextEditor::endSelection()
{
	if (mIsSelecting)
	{
		mIsSelecting = false;
		mSelectionEnd = mCursorPos;
	}
}

void LLTextEditor::setSelection(S32 start, S32 end)
{
	setCursorPos(end);
	startSelection();
	setCursorPos(start);
	endSelection();
}

bool LLTextEditor::selectionContainsLineBreaks()
{
	if (hasSelection())
	{
		S32 left = llmin(mSelectionStart, mSelectionEnd);
		S32 right = left + abs(mSelectionStart - mSelectionEnd);

		const LLWString& wtext = mWText;
		for (S32 i = left; i < right; ++i)
		{
			if (wtext[i] == '\n')
			{
				return true;
			}
		}
	}
	return false;
}

// Assumes that pos is at the start of the line spaces may be positive (indent)
// or negative (unindent). Returns the actual number of characters added or
// removed.
S32 LLTextEditor::indentLine(S32 pos, S32 spaces)
{
	llassert(pos >= 0);
	llassert(pos <= getLength());

	S32 delta_spaces = 0;

	if (spaces >= 0)
	{
		// Indent
		for (S32 i = 0; i < spaces; ++i)
		{
			delta_spaces += addChar(pos, ' ');
		}
	}
	else
	{
		// Unindent
		for (S32 i = 0; i < -spaces; ++i)
		{
			const LLWString& wtext = mWText;
			if (wtext[pos] == ' ')
			{
				delta_spaces += remove(pos, 1, false);
			}
 		}
	}

	return delta_spaces;
}

void LLTextEditor::indentSelectedLines(S32 spaces)
{
	if (hasSelection())
	{
		const LLWString& text = mWText;
		S32 left = llmin(mSelectionStart, mSelectionEnd);
		S32 right = left + abs(mSelectionStart - mSelectionEnd);
		bool cursor_on_right = mSelectionEnd > mSelectionStart;
		S32 cur = left;

		// Expand left to start of line
		while (cur > 0 && text[cur] != '\n')
		{
			--cur;
		}
		left = cur;
		if (cur > 0)
		{
			++left;
		}

		// Expand right to end of line
		if (text[right - 1] == '\n')
		{
			--right;
		}
		else
		{
			while (text[right] != '\n' && right <= getLength())
			{
				++right;
			}
		}

		// Find each start-of-line and indent it
		do
		{
			if (text[cur] == '\n')
			{
				++cur;
			}

			S32 delta_spaces = indentLine(cur, spaces);
			if (delta_spaces > 0)
			{
				cur += delta_spaces;
			}
			right += delta_spaces;

			//text = mWText;

			// Find the next new line
			while (cur < right && text[cur] != '\n')
			{
				++cur;
			}
		}
		while (cur < right);

		if (right < getLength() && text[right] == '\n')
		{
			++right;
		}

		// Set the selection and cursor
		if (cursor_on_right)
		{
			mSelectionStart = left;
			mSelectionEnd = right;
		}
		else
		{
			mSelectionStart = right;
			mSelectionEnd = left;
		}
		mCursorPos = mSelectionEnd;
	}
}

//virtual
void LLTextEditor::selectAll()
{
	mSelectionStart = getLength();
	mSelectionEnd = 0;
	mCursorPos = mSelectionEnd;
}

bool LLTextEditor::handleToolTip(S32 x, S32 y, std::string& msg,
								 LLRect* sticky_rect_screen)
{
	for (child_list_const_iter_t child_it = getChildList()->begin(),
								 end = getChildList()->end();
		 child_it != end; ++child_it)
	{
		LLView* viewp = *child_it;
		S32 local_x = x - viewp->getRect().mLeft;
		S32 local_y = y - viewp->getRect().mBottom;
		if (viewp->handleToolTip(local_x, local_y, msg, sticky_rect_screen))
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
		bool has_tool_tip = cur_segment->getToolTip(msg);
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

bool LLTextEditor::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	// Pretend the mouse is over the scrollbar
	return mScrollbar->handleScrollWheel(0, 0, clicks);
}

bool LLTextEditor::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	// Key presses are not being passed to the Popup menu.
	// A proper fix is non-trivial so instead just close the menu.
	LLMenuGL* menu = getContextMenu();
	if (menu && menu->isOpen())
	{
		LLMenuGL::sMenuContainer->hideMenus();
	}

	// Let scrollbar have first dibs
	handled = LLView::childrenHandleMouseDown(x, y, mask) != NULL;

	if (!handled)
	{
		if (!(mask & MASK_SHIFT))
		{
			deselect();
		}

		// If we are not scrolling (handled by child), then we are selecting
		if (mask & MASK_SHIFT)
		{
			S32 old_cursor_pos = mCursorPos;
			setCursorAtLocalPos(x, y, true);

			if (hasSelection())
			{
#if 0				// Mac-like behavior - extend selection towards the cursor
					if (mCursorPos < mSelectionStart &&
						mCursorPos < mSelectionEnd)
					{
						// ...left of selection
						mSelectionStart = llmax(mSelectionStart,
												mSelectionEnd);
						mSelectionEnd = mCursorPos;
					}
					else if (mCursorPos > mSelectionStart &&
							 mCursorPos > mSelectionEnd)
					{
						// ...right of selection
						mSelectionStart = llmin(mSelectionStart,
												mSelectionEnd);
						mSelectionEnd = mCursorPos;
					}
					else
					{
						mSelectionEnd = mCursorPos;
					}
#else
					// Windows behavior
					mSelectionEnd = mCursorPos;
#endif
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

bool LLTextEditor::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
	setFocus(true);
	if (canPastePrimary())
	{
		setCursorAtLocalPos(x, y, true);
		pastePrimary();
	}
	return true;
}

bool LLTextEditor::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	setFocus(true);

	S32 word_start = 0;
	S32 word_len = 0;
	S32 pos = getCursorPosFromLocalCoord(x, y, true);

	// If the context menu has not yet been created for this editor, this call
	// will create it now. HB
	LLMenuGL* menu = createContextMenu();
	if (menu)
	{
		SpellMenuBind* menu_bind;
		LLMenuItemCallGL* menu_item;

		// Remove old suggestions
		for (S32 i = 0, count = mSuggestionMenuItems.size();
			 i < count; ++i)
		{
			menu_bind = mSuggestionMenuItems[i];
			if (menu_bind)
			{
				menu_item = menu_bind->mMenuItem;
				menu->remove(menu_item);
				menu_item->die();
#if 0
				delete menu_bind->mMenuItem;
				menu_bind->mMenuItem = NULL;
#endif
				delete menu_bind;
			}
		}
		mSuggestionMenuItems.clear();

		// Not read-only, spell_check="true" in xui and spell checking enabled
		bool spell_check = !mReadOnly && mSpellCheck &&
							LLSpellCheck::getInstance()->getSpellCheck();
		menu->setItemVisible("spell_sep", spell_check);
		if (spell_check)
		{
			// Search for word matches
			bool is_word_part = getWordBoundriesAt(pos, &word_start,
												   &word_len);
			if (is_word_part)
			{
				const LLWString& text = mWText;
				std::string part(text.begin(), text.end());
				std::string selected_word = part.substr(word_start, word_len);
				if (!LLSpellCheck::getInstance()->checkSpelling(selected_word))
				{
					// Misspelled word here
					std::vector<std::string> suggestions;
					S32 count =
						LLSpellCheck::getInstance()->getSuggestions(selected_word,
																	suggestions);
					for (S32 i = 0; i < count; ++i)
					{
						menu_bind = new SpellMenuBind;
						menu_bind->mOrigin = this;
						menu_bind->mWord = suggestions[i];
						menu_bind->mWordPositionEnd = word_start + word_len;
						menu_bind->mWordPositionStart = word_start;
						menu_item = new LLMenuItemCallGL(menu_bind->mWord,
														 spellCorrect,
														 NULL, menu_bind);
						menu_bind->mMenuItem = menu_item;
						mSuggestionMenuItems.push_back(menu_bind);
						menu->append(menu_item);
					}
					menu_bind = new SpellMenuBind;
					menu_bind->mOrigin = this;
					menu_bind->mWord = selected_word;
					menu_bind->mWordPositionEnd = word_start + word_len;
					menu_bind->mWordPositionStart = word_start;
					menu_item = new LLMenuItemCallGL("Add word", spellAdd,
													 NULL, menu_bind);
					menu_bind->mMenuItem = menu_item;
					mSuggestionMenuItems.push_back(menu_bind);
					menu->append(menu_item);

					menu_bind = new SpellMenuBind;
					menu_bind->mOrigin = this;
					menu_bind->mWord = selected_word;
					menu_bind->mWordPositionEnd = word_start + word_len;
					menu_bind->mWordPositionStart = word_start;
					menu_item = new LLMenuItemCallGL("Ignore word",
													 spellIgnore, NULL,
													 menu_bind);
					menu_bind->mMenuItem = menu_item;
					mSuggestionMenuItems.push_back(menu_bind);
					menu->append(menu_item);
				}
			}

			menu_bind = new SpellMenuBind;
			menu_bind->mOrigin = this;
			if (mShowMisspelled)
			{
				menu_bind->mWord = "Hide misspellings";
			}
			else
			{
				menu_bind->mWord = "Show misspellings";
			}
			menu_item = new LLMenuItemCallGL(menu_bind->mWord, spellShow,
											 NULL, menu_bind);
			menu_bind->mMenuItem = menu_item;
			mSuggestionMenuItems.push_back(menu_bind);
			menu->append(menu_item);
		}

		menu->buildDrawLabels();
		menu->updateParent(LLMenuGL::sMenuContainer);
		LLMenuGL::showPopup(this, menu, x, y);
	}

	return true;
}

bool LLTextEditor::handleHover(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	mHoverSegment = NULL;
	if (hasMouseCapture())
	{
		if (mIsSelecting)
		{
			if (x != mLastSelectionX || y != mLastSelectionY)
			{
				mLastSelectionX = x;
				mLastSelectionY = y;
			}

			if (y > mTextRect.mTop)
			{
				mScrollbar->setDocPos(mScrollbar->getDocPos() - 1);
			}
			else if (y < mTextRect.mBottom)
			{
				mScrollbar->setDocPos(mScrollbar->getDocPos() + 1);
			}

			setCursorAtLocalPos(x, y, true);
			mSelectionEnd = mCursorPos;
		}

		LL_DEBUGS("UserInput") << "hover handled by " << getName()
							   << " (active)" << LL_ENDL;
		gWindowp->setCursor(UI_CURSOR_IBEAM);
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
					LL_DEBUGS("UserInput") << "hover handled by " << getName()
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

	if (mOnScrollEndCallback && mOnScrollEndData &&
		mScrollbar->getDocPos() == mScrollbar->getDocPosMax())
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}

	return handled;
}

bool LLTextEditor::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	// let scrollbar have first dibs
	handled = LLView::childrenHandleMouseUp(x, y, mask) != NULL;

	if (!handled)
	{
		if (mIsSelecting)
		{
			// Finish selection
			if (y > mTextRect.mTop)
			{
				mScrollbar->setDocPos(mScrollbar->getDocPos() - 1);
			}
			else if (y < mTextRect.mBottom)
			{
				mScrollbar->setDocPos(mScrollbar->getDocPos() + 1);
			}

			setCursorAtLocalPos(x, y, true);
			endSelection();
		}

		if (!hasSelection())
		{
			handleMouseUpOverSegment(x, y, mask);
		}

		// take selection to 'primary' clipboard
		updatePrimary();

		handled = true;
	}

	// Delay cursor flashing
	resetKeystrokeTimer();

	if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);

		handled = true;
	}

	return handled;
}

bool LLTextEditor::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	// Let scrollbar have first dibs
	handled = LLView::childrenHandleDoubleClick(x, y, mask) != NULL;

	if (!handled)
	{
		setCursorAtLocalPos(x, y, false);
		deselect();

		const LLWString& text = mWText;

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

		// We do not want handleMouseUp() to "finish" the selection (and
		// thereby set mSelectionEnd to where the mouse is), so we finish the
		// selectionhere.
		mIsSelecting = false;

		// Delay cursor flashing
		resetKeystrokeTimer();

		// Take selection to 'primary' clipboard
		updatePrimary();

		handled = true;
	}

	return handled;
}

// Allow calling cards to be dropped onto text fields.  Append the name and
// a carriage return.
//virtual
bool LLTextEditor::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data,  EAcceptance* accept,
									 std::string& tooltip_msg)
{
	*accept = ACCEPT_NO;
	return true;
}

// Returns change in number of characters in mText
S32 LLTextEditor::execute(LLTextCmd* cmd)
{
	S32 delta = 0;
	if (cmd->execute(this, &delta))
	{
		// Delete top of undo stack
		undo_stack_t::iterator enditer = std::find(mUndoStack.begin(),
												   mUndoStack.end(), mLastCmd);
		if (enditer != mUndoStack.begin())
		{
			--enditer;
			std::for_each(mUndoStack.begin(), enditer, DeletePointer());
			mUndoStack.erase(mUndoStack.begin(), enditer);
		}
		// Push the new command is now on the top (front) of the undo stack.
		mUndoStack.push_front(cmd);
		mLastCmd = cmd;

		if (mKeystrokeCallback)
		{
			mKeystrokeCallback(this, mKeystrokeData);
		}
	}
	else
	{
		// Operation failed, so do not put it on the undo stack.
		delete cmd;
	}

	return delta;
}

S32 LLTextEditor::insert(S32 pos, const LLWString& wstr,
						 bool group_with_next_op)
{
	return execute(new LLTextCmdInsert(pos, group_with_next_op, wstr));
}

S32 LLTextEditor::remove(S32 pos, S32 length, bool group_with_next_op)
{
	return execute(new LLTextCmdRemove(pos, group_with_next_op, length));
}

S32 LLTextEditor::append(const LLWString& wstr, bool group_with_next_op)
{
	return insert(mWText.length(), wstr, group_with_next_op);
}

S32 LLTextEditor::overwriteChar(S32 pos, llwchar wc)
{
	if ((S32)mWText.length() == pos)
	{
		return addChar(pos, wc);
	}
	else
	{
		return execute(new LLTextCmdOverwriteChar(pos, false, wc));
	}
}

// Removes a single character from the text. Tries to remove a pseudo-tab (up
// to four spaces in a row)
void LLTextEditor::removeCharOrTab()
{
	if (!getEnabled())
	{
		return;
	}

	if (mCursorPos > 0)
	{
		S32 chars_to_remove = 1;

		const LLWString& text = mWText;
		if (text[mCursorPos - 1] == ' ')
		{
			// Try to remove a "tab"
			S32 line, offset;
			getLineAndOffset(mCursorPos, &line, &offset);
			if (offset > 0)
			{
				chars_to_remove = offset % SPACES_PER_TAB;
				if (chars_to_remove == 0)
				{
					chars_to_remove = SPACES_PER_TAB;
				}

				for (S32 i = 0; i < chars_to_remove; ++i)
				{
					if (text[ mCursorPos - i - 1] != ' ')
					{
						// Fewer than a full tab's worth of spaces, so just
						// delete a single character.
						chars_to_remove = 1;
						break;
					}
				}
			}
		}

		for (S32 i = 0; i < chars_to_remove; ++i)
		{
			setCursorPos(mCursorPos - 1);
			remove(mCursorPos, 1, false);
		}
	}
	else
	{
		reportBadKeystroke();
	}
}

// Remove a single character from the text
S32 LLTextEditor::removeChar(S32 pos)
{
	if (mKeystrokeCallback)
	{
		mKeystrokeCallback(this, mKeystrokeData);
	}
	return remove(pos, 1, false);
}

void LLTextEditor::removeChar()
{
	if (!getEnabled())
	{
		return;
	}
	if (mCursorPos > 0)
	{
		setCursorPos(mCursorPos - 1);
		removeChar(mCursorPos);
	}
	else
	{
		reportBadKeystroke();
	}
}

// Add a single character to the text
S32 LLTextEditor::addChar(S32 pos, llwchar wc)
{
	if (wstring_utf8_length(mWText) +
		wchar_utf8_length(wc) >= mMaxTextByteLength)
	{
		make_ui_sound("UISndBadKeystroke");
		return 0;
	}

	if (mKeystrokeCallback)
	{
		mKeystrokeCallback(this, mKeystrokeData);
	}

	if (mLastCmd && mLastCmd->canExtend(pos))
	{
		S32 delta = 0;
		mLastCmd->extendAndExecute(this, pos, wc, &delta);
		return delta;
	}
	else
	{
		return execute(new LLTextCmdAddChar(pos, false, wc));
	}
}

void LLTextEditor::addChar(llwchar wc)
{
	if (!getEnabled())
	{
		return;
	}
	if (hasSelection())
	{
		deleteSelection(true);
	}
	else if (gKeyboardp && gKeyboardp->getInsertMode() == LL_KIM_OVERWRITE)
	{
		removeChar(mCursorPos);
	}

	setCursorPos(mCursorPos + addChar(mCursorPos, wc));
}

bool LLTextEditor::handleSelectionKey(KEY key, MASK mask)
{
	bool handled = false;

	if (mask & MASK_SHIFT)
	{
		handled = true;

		switch (key)
		{
		case KEY_LEFT:
			if (0 < mCursorPos)
			{
				startSelection();
				--mCursorPos;
				if (mask & MASK_CONTROL)
				{
					mCursorPos = prevWordPos(mCursorPos);
				}
				mSelectionEnd = mCursorPos;
			}
			break;

		case KEY_RIGHT:
			if (mCursorPos < getLength())
			{
				startSelection();
				++mCursorPos;
				if (mask & MASK_CONTROL)
				{
					mCursorPos = nextWordPos(mCursorPos);
				}
				mSelectionEnd = mCursorPos;
			}
			break;

		case KEY_UP:
			startSelection();
			changeLine(-1);
			mSelectionEnd = mCursorPos;
			break;

		case KEY_PAGE_UP:
			startSelection();
			changePage(-1);
			mSelectionEnd = mCursorPos;
			break;

		case KEY_HOME:
			startSelection();
			if (mask & MASK_CONTROL)
			{
				mCursorPos = 0;
			}
			else
			{
				startOfLine();
			}
			mSelectionEnd = mCursorPos;
			break;

		case KEY_DOWN:
			startSelection();
			changeLine(1);
			mSelectionEnd = mCursorPos;
			break;

		case KEY_PAGE_DOWN:
			startSelection();
			changePage(1);
			mSelectionEnd = mCursorPos;
			break;

		case KEY_END:
			startSelection();
			if (mask & MASK_CONTROL)
			{
				mCursorPos = getLength();
			}
			else
			{
				endOfLine();
			}
			mSelectionEnd = mCursorPos;
			break;

		default:
			handled = false;
		}
	}

	if (!handled && mHandleEditKeysDirectly)
	{
		if ((MASK_CONTROL & mask) && key == 'A')
		{
			if (canSelectAll())
			{
				selectAll();
			}
			else
			{
				reportBadKeystroke();
			}
			handled = true;
		}
	}

	if (handled)
	{
		// Take selection to 'primary' clipboard
		updatePrimary();
	}

	return handled;
}

bool LLTextEditor::handleNavigationKey(KEY key, MASK mask)
{
	bool handled = false;

	// Ignore capslock key
	if (MASK_NONE == mask)
	{
		handled = true;
		switch (key)
		{
		case KEY_UP:
			if (mReadOnly)
			{
				mScrollbar->setDocPos(mScrollbar->getDocPos() - 1);
			}
			else
			{
				changeLine(-1);
			}
			break;

		case KEY_PAGE_UP:
			changePage(-1);
			break;

		case KEY_HOME:
			if (mReadOnly)
			{
				mScrollbar->setDocPos(0);
			}
			else
			{
				startOfLine();
			}
			break;

		case KEY_DOWN:
			if (mReadOnly)
			{
				mScrollbar->setDocPos(mScrollbar->getDocPos() + 1);
			}
			else
			{
				changeLine(1);
			}
			break;

		case KEY_PAGE_DOWN:
			changePage(1);
			break;

		case KEY_END:
			if (mReadOnly)
			{
				mScrollbar->setDocPos(mScrollbar->getDocPosMax());
			}
			else
			{
				endOfLine();
			}
			break;

		case KEY_LEFT:
			if (mReadOnly)
			{
				break;
			}
			if (hasSelection())
			{
				setCursorPos(llmin(mCursorPos - 1, mSelectionStart,
								   mSelectionEnd));
			}
			else
			{
				if (0 < mCursorPos)
				{
					setCursorPos(mCursorPos - 1);
				}
				else
				{
					reportBadKeystroke();
				}
			}
			break;

		case KEY_RIGHT:
			if (mReadOnly)
			{
				break;
			}
			if (hasSelection())
			{
				setCursorPos(llmax(mCursorPos + 1, mSelectionStart,
								   mSelectionEnd));
			}
			else
			{
				if (mCursorPos < getLength())
				{
					setCursorPos(mCursorPos + 1);
				}
				else
				{
					reportBadKeystroke();
				}
			}
			break;

		default:
			handled = false;
		}
	}

	if (mOnScrollEndCallback && mOnScrollEndData &&
		mScrollbar->getDocPos() == mScrollbar->getDocPosMax())
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}

	return handled;
}

void LLTextEditor::deleteSelection(bool group_with_next_op)
{
	if (getEnabled() && hasSelection())
	{
		S32 pos = llmin(mSelectionStart, mSelectionEnd);
		S32 length = abs(mSelectionStart - mSelectionEnd);

		remove(pos, length, group_with_next_op);

		deselect();
		setCursorPos(pos);
	}
}

//virtual
bool LLTextEditor::canCut() const
{
	return !mReadOnly && hasSelection();
}

// Cuts selection to clipboard
void LLTextEditor::cut()
{
	if (!canCut())
	{
		return;
	}
	S32 left_pos = llmin(mSelectionStart, mSelectionEnd);
	S32 length = abs(mSelectionStart - mSelectionEnd);
	gClipboard.copyFromSubstring(mWText, left_pos, length);
	deleteSelection(false);

	needsReflow();

	if (mKeystrokeCallback)
	{
		mKeystrokeCallback(this, mKeystrokeData);
	}

	// Force spell-check update:
	mKeystrokeTimer.reset();
	mPrevSpelledText.erase();
}

bool LLTextEditor::canCopy() const
{
	return hasSelection();
}

// Copies selection to clipboard
void LLTextEditor::copy()
{
	if (!canCopy())
	{
		return;
	}
	S32 left_pos = llmin(mSelectionStart, mSelectionEnd);
	S32 length = abs(mSelectionStart - mSelectionEnd);
	gClipboard.copyFromSubstring(mWText, left_pos, length);

	// Force spell-check update:
	mKeystrokeTimer.reset();
	mPrevSpelledText.erase();
}

bool LLTextEditor::canPaste() const
{
	return !mReadOnly && gClipboard.canPasteString();
}

// Pastes from clipboard
void LLTextEditor::paste()
{
	pasteHelper(false);
}

// Pastes from primary
void LLTextEditor::pastePrimary()
{
	pasteHelper(true);
}

// Pastes from primary (is_primary == true) or clipboard (is_primary == false)
void LLTextEditor::pasteHelper(bool is_primary)
{
	bool can_paste_it;
	if (is_primary)
	{
		can_paste_it = canPastePrimary();
	}
	else
	{
		can_paste_it = canPaste();
	}

	if (!can_paste_it)
	{
		return;
	}

	LLWString paste;
	if (is_primary)
	{
		paste = gClipboard.getPastePrimaryWString();
	}
	else
	{
		paste = gClipboard.getPasteWString();
	}

	if (paste.empty())
	{
		return;
	}

	// Delete any selected characters (the paste replaces them)
	if (!is_primary && hasSelection())
	{
		deleteSelection(true);
	}

	// Clean up string (replace tabs and remove characters that our fonts do
	// not support).
	LLWString clean_string(paste);
	LLWStringUtil::replaceTabsWithSpaces(clean_string, SPACES_PER_TAB);
	if (mAllowEmbeddedItems)
	{
		constexpr llwchar LF = 10;
		S32 len = clean_string.length();
		for (S32 i = 0; i < len; ++i)
		{
			llwchar wc = clean_string[i];
			if (wc < LLFontFreetype::FIRST_CHAR && wc != LF)
			{
				clean_string[i] = LL_UNKNOWN_CHAR;
			}
			else if (wc >= FIRST_EMBEDDED_CHAR && wc <= LAST_EMBEDDED_CHAR)
			{
				clean_string[i] = pasteEmbeddedItem(wc);
			}
		}
	}

	// Insert the new text into the existing text.
	setCursorPos(mCursorPos + insert(mCursorPos, clean_string, false));
	deselect();

	needsReflow();

	if (mKeystrokeCallback)
	{
		mKeystrokeCallback(this, mKeystrokeData);
	}

	// Force spell-check update:
	mKeystrokeTimer.reset();
	mPrevSpelledText.erase();
}

// Copies selection to primary
void LLTextEditor::copyPrimary()
{
	if (!canCopy())
	{
		return;
	}
	S32 left_pos = llmin(mSelectionStart, mSelectionEnd);
	S32 length = abs(mSelectionStart - mSelectionEnd);
	gClipboard.copyFromPrimarySubstring(mWText, left_pos, length);

	// Force spell-check update:
	mKeystrokeTimer.reset();
	mPrevSpelledText.erase();
}

bool LLTextEditor::canPastePrimary() const
{
	return !mReadOnly && gClipboard.canPastePrimaryString();
}

void LLTextEditor::updatePrimary()
{
	if (canCopy())
	{
		copyPrimary();
	}
}

bool LLTextEditor::handleControlKey(KEY key, MASK mask)
{
	bool handled = false;

	if (mask & MASK_CONTROL)
	{
		handled = true;

		switch (key)
		{
		case KEY_HOME:
			if (mask & MASK_SHIFT)
			{
				startSelection();
				mCursorPos = 0;
				mSelectionEnd = mCursorPos;
			}
			else
			{
				// Ctrl-Home, Ctrl-Left, Ctrl-Right, Ctrl-Down
				// all move the cursor as if clicking, so should deselect.
				deselect();
				setCursorPos(0);
			}
			break;

		case KEY_END:
			{
				if (mask & MASK_SHIFT)
				{
					startSelection();
				}
				else
				{
					// Ctrl-Home, Ctrl-Left, Ctrl-Right, Ctrl-Down
					// all move the cursor as if clicking, so should deselect.
					deselect();
				}
				endOfDoc();
				if (mask & MASK_SHIFT)
				{
					mSelectionEnd = mCursorPos;
				}
				break;
			}

		case KEY_RIGHT:
			if (mCursorPos < getLength())
			{
				// Ctrl-Home, Ctrl-Left, Ctrl-Right, Ctrl-Down
				// all move the cursor as if clicking, so should deselect.
				deselect();

				setCursorPos(nextWordPos(mCursorPos + 1));
			}
			break;

		case KEY_LEFT:
			if (mCursorPos > 0)
			{
				// Ctrl-Home, Ctrl-Left, Ctrl-Right, Ctrl-Down
				// all move the cursor as if clicking, so should deselect.
				deselect();

				setCursorPos(prevWordPos(mCursorPos - 1));
			}
			break;

		default:
			handled = false;
		}
	}

	if (handled)
	{
		updatePrimary();
	}

	return handled;
}

bool LLTextEditor::handleEditKey(KEY key, MASK mask)
{
	bool handled = false;

	// Standard edit keys (Ctrl-X, Delete, etc,) are handled here instead of
	// routed by the menu system.
	if (KEY_DELETE == key)
	{
		if (canDoDelete())
		{
			doDelete();
		}
		else
		{
			reportBadKeystroke();
		}
		handled = true;
	}
	else if (MASK_CONTROL & mask)
	{
		if ('C' == key)
		{
			if (canCopy())
			{
				copy();
			}
			else
			{
				reportBadKeystroke();
			}
			handled = true;
		}
		else if ('V' == key)
		{
			if (canPaste())
			{
				paste();
			}
			else
			{
				reportBadKeystroke();
			}
			handled = true;
		}
		else if ('X' == key)
		{
			if (canCut())
			{
				cut();
			}
			else
			{
				reportBadKeystroke();
			}
			handled = true;
		}
	}

	if (handled)
	{
		// Take selection to 'primary' clipboard
		updatePrimary();
	}

	return handled;
}


bool LLTextEditor::handleSpecialKey(KEY key, MASK mask, bool* return_key_hit)
{
	*return_key_hit = false;
	bool handled = true;

	switch (key)
	{
	case KEY_INSERT:
		if (mask == MASK_NONE && gKeyboardp)
		{
			gKeyboardp->toggleInsertMode();
		}
		break;

	case KEY_BACKSPACE:
		if (hasSelection())
		{
			deleteSelection(false);
		}
		else if (0 < mCursorPos)
		{
			removeCharOrTab();
		}
		else
		{
			reportBadKeystroke();
		}
		break;

	case KEY_RETURN:
		if (mask == MASK_NONE)
		{
			if (hasSelection())
			{
				deleteSelection(false);
			}
			autoIndent(); // *TODO: make this optional
		}
		else
		{
			handled = false;
			break;
		}
		break;

	case KEY_TAB:
		if (mask & (MASK_CONTROL | MASK_ALT))
		{
			handled = false;
			break;
		}
		if (hasSelection() && selectionContainsLineBreaks())
		{
			indentSelectedLines((mask & MASK_SHIFT) ? -SPACES_PER_TAB
													: SPACES_PER_TAB);
		}
		else
		{
			if (hasSelection())
			{
				deleteSelection(false);
			}

			S32 line, offset;
			getLineAndOffset(mCursorPos, &line, &offset);

			S32 spaces_needed = SPACES_PER_TAB - (offset % SPACES_PER_TAB);
			for (S32 i = 0; i < spaces_needed; ++i)
			{
				addChar(' ');
			}
		}
		break;

	default:
		handled = false;
	}

	return handled;
}

void LLTextEditor::unindentLineBeforeCloseBrace()
{
	if (mCursorPos >= 1)
	{
		const LLWString& text = mWText;
		if (' ' == text[mCursorPos - 1])
		{
			removeCharOrTab();
		}
	}
}

bool LLTextEditor::handleKeyHere(KEY key, MASK mask)
{
	bool handled = false;
	bool selection_modified = false;
	bool return_key_hit = false;
	bool text_may_have_changed = true;

	// Key presses are not being passed to the Popup menu.
	// A proper fix is non-trivial so instead just close the menu.
	LLMenuGL* menu = getContextMenu();
	if (menu && menu->isOpen())
	{
		LLMenuGL::sMenuContainer->hideMenus();
	}

	if (gFocusMgr.getKeyboardFocus() == this)
	{
		// Special case for TAB. If want to move to next field, report not
		// handled and let the parent take care of field movement.
		if (KEY_TAB == key && mTabsToNextField)
		{
			return false;
		}

		handled = handleNavigationKey(key, mask);
		if (handled)
		{
			text_may_have_changed = false;
		}

		if (!handled)
		{
			handled = handleSelectionKey(key, mask);
			if (handled)
			{
				selection_modified = true;
			}
		}

		if (!handled)
		{
			handled = handleControlKey(key, mask);
			if (handled)
			{
				selection_modified = true;
			}
		}

		if (!handled && mHandleEditKeysDirectly)
		{
			handled = handleEditKey(key, mask);
			if (handled)
			{
				selection_modified = true;
				text_may_have_changed = true;
			}
		}

		// Key presses are not being passed to the Popup menu.
		// A proper fix is non-trivial so instead just close the menu.
		LLMenuGL* menu = getContextMenu();
		if (menu && menu->isOpen())
		{
			LLMenuGL::sMenuContainer->hideMenus();
		}

		// Handle most keys only if the text editor is writeable.
		if (!mReadOnly)
		{
			if (!handled && mOnHandleKeyCallback)
			{
				handled = mOnHandleKeyCallback(key, mask, this,
											   mOnHandleKeyData);
			}
			if (!handled)
			{
				handled = handleSpecialKey(key, mask, &return_key_hit);
				if (handled)
				{
					selection_modified = true;
					text_may_have_changed = true;
				}
			}
		}

		if (handled)
		{
			resetKeystrokeTimer();

			// Most keystrokes will make the selection box go away, but not all will.
			if (!selection_modified && KEY_SHIFT != key && KEY_TAB != key &&
				KEY_CONTROL != key && KEY_ALT != key && KEY_CAPSLOCK)
			{
				deselect();
			}

			if (text_may_have_changed)
			{
				needsReflow();
			}
			needsScroll();
		}
	}

	return handled;
}

bool LLTextEditor::handleUnicodeCharHere(llwchar uni_char)
{
	if (uni_char < 0x20 || uni_char == 0x7F) // Control character or DEL
	{
		return false;
	}

	bool handled = false;

	if (gFocusMgr.getKeyboardFocus() == this)
	{
		// Handle most keys only if the text editor is writeable.
		if (!mReadOnly)
		{
			if ('}' == uni_char)
			{
				unindentLineBeforeCloseBrace();
			}

			// TODO: KLW Add auto show of tool tip on (
			addChar(uni_char);

			// Keys that add characters temporarily hide the cursor
			gWindowp->hideCursorUntilMouseMove();

			handled = true;
		}

		if (handled)
		{
			resetKeystrokeTimer();

			// Most keystrokes will make the selection box go away, but not all
			// will.
			deselect();

			needsReflow();
		}
	}

	return handled;
}

//virtual
bool LLTextEditor::canDoDelete() const
{
	return !mReadOnly && (hasSelection() || (mCursorPos < getLength()));
}

void LLTextEditor::doDelete()
{
	if (!canDoDelete())
	{
		return;
	}
	if (hasSelection())
	{
		deleteSelection(false);
	}
	else if (mCursorPos < getLength())
	{
		S32 i;
		S32 chars_to_remove = 1;
		const LLWString& text = mWText;
		if (text[ mCursorPos ] == ' ' &&
			mCursorPos + SPACES_PER_TAB < getLength())
		{
			// Try to remove a full tab's worth of spaces
			S32 line, offset;
			getLineAndOffset(mCursorPos, &line, &offset);
			chars_to_remove = SPACES_PER_TAB - (offset % SPACES_PER_TAB);
			if (chars_to_remove == 0)
			{
				chars_to_remove = SPACES_PER_TAB;
			}

			for (i = 0; i < chars_to_remove; ++i)
			{
				if (text[mCursorPos + i] != ' ')
				{
					chars_to_remove = 1;
					break;
				}
			}
		}

		for (i = 0; i < chars_to_remove; ++i)
		{
			setCursorPos(mCursorPos + 1);
			removeChar();
		}
	}

	needsReflow();

	if (mKeystrokeCallback)
	{
		mKeystrokeCallback(this, mKeystrokeData);
	}

	// Force spell-check update:
	mKeystrokeTimer.reset();
	mPrevSpelledText.erase();
}

void LLTextEditor::blockUndo()
{
	mBaseDocIsPristine = false;
	mLastCmd = NULL;
	std::for_each(mUndoStack.begin(), mUndoStack.end(), DeletePointer());
	mUndoStack.clear();

	// Force spell-check update:
	mKeystrokeTimer.reset();
	mPrevSpelledText.erase();
}

//virtual
bool LLTextEditor::canUndo() const
{
	return !mReadOnly && mLastCmd != NULL;
}

//virtual
void LLTextEditor::undo()
{
	if (!canUndo())
	{
		return;
	}

	S32 pos = 0;

	deselect();

	do
	{
		pos = mLastCmd->undo(this);
		undo_stack_t::iterator iter = std::find(mUndoStack.begin(),
												mUndoStack.end(), mLastCmd);
		if (iter != mUndoStack.end())
		{
			++iter;
		}
		if (iter != mUndoStack.end())
		{
			mLastCmd = *iter;
		}
		else
		{
			mLastCmd = NULL;
		}

	}
	while (mLastCmd && mLastCmd->groupWithNext());

	setCursorPos(pos);
	needsReflow();

	// Force spell-check update:
	mKeystrokeTimer.reset();
	mPrevSpelledText.erase();
}

//virtual
bool LLTextEditor::canRedo() const
{
	return !mReadOnly && mUndoStack.size() > 0 &&
		   mLastCmd != mUndoStack.front();
}

//virtual
void LLTextEditor::redo()
{
	if (!canRedo())
	{
		return;
	}

	S32 pos = 0;

	deselect();

	do
	{
		if (!mLastCmd)
		{
			mLastCmd = mUndoStack.back();
		}
		else
		{
			undo_stack_t::iterator iter = std::find(mUndoStack.begin(),
													mUndoStack.end(),
													mLastCmd);
			if (iter != mUndoStack.begin())
			{
				mLastCmd = *(--iter);
			}
			else
			{
				mLastCmd = NULL;
			}
		}

		if (mLastCmd)
		{
			pos = mLastCmd->redo(this);
		}
	}
	while (mLastCmd && mLastCmd->groupWithNext() &&
		   mLastCmd != mUndoStack.front());

	setCursorPos(pos);
	needsReflow();

	// Force spell-check update:
	mKeystrokeTimer.reset();
	mPrevSpelledText.erase();
}

void LLTextEditor::onFocusReceived()
{
	grabMenuHandler();
	LLUICtrl::onFocusReceived();
	updateAllowingLanguageInput();
}

// virtual, from LLView
void LLTextEditor::onFocusLost()
{
	updateAllowingLanguageInput();

	// Route menu back to the default
	releaseMenuHandler();

	if (mCommitOnFocusLost)
	{
		onCommit();
	}

	// Make sure cursor is shown again
	gWindowp->showCursorFromMouseMove();

	LLUICtrl::onFocusLost();
}

void LLTextEditor::setEnabled(bool enabled)
{
	// just treat enabled as read-only flag
	bool read_only = !enabled;
	if (read_only != mReadOnly)
	{
		mReadOnly = read_only;
		updateSegments();
		updateAllowingLanguageInput();
	}
}

void LLTextEditor::drawBackground()
{
	S32 left = 0;
	S32 top = getRect().getHeight();
	S32 right = getRect().getWidth();
	S32 bottom = 0;

	LLColor4 bg_color =
		mReadOnly ? mReadOnlyBgColor
				  : gFocusMgr.getKeyboardFocus() == this ? mFocusBgColor
														 : mWriteableBgColor;
	if (mShowLineNumbers)
	{
		gl_rect_2d(left, top, UI_TEXTEDITOR_LINE_NUMBER_MARGIN, bottom,
				   mReadOnlyBgColor); // line number area always read-only
		gl_rect_2d(UI_TEXTEDITOR_LINE_NUMBER_MARGIN, top, right, bottom,
				   bg_color); // body text area to the right of line numbers
		gl_rect_2d(UI_TEXTEDITOR_LINE_NUMBER_MARGIN, top,
				   UI_TEXTEDITOR_LINE_NUMBER_MARGIN - 1, bottom,
				   LLColor4::grey3); // separator
	}
	else
	{
		gl_rect_2d(left, top, right, bottom, bg_color); // body text area
	}

	LLView::draw();
}

// Draws the black box behind the selected text
void LLTextEditor::drawSelectionBackground()
{
	// Draw selection even if we do not have keyboard focus for search/replace
	if (hasSelection())
	{
		const LLWString& text = mWText;
		const S32 text_len = getLength();
		std::queue<S32> line_endings;

		S32 line_height = ll_roundp(mGLFont->getLineHeight());

		S32 selection_left = llmin(mSelectionStart, mSelectionEnd);
		S32 selection_right = llmax(mSelectionStart, mSelectionEnd);
		S32 selection_left_x = mTextRect.mLeft;
		S32 selection_left_y = mTextRect.mTop - line_height;
		S32 selection_right_x = mTextRect.mRight;
		S32 selection_right_y = mTextRect.mBottom;

		bool selection_right_visible = false;

		// Skip through the lines we are not drawing.
		S32 cur_line = mScrollbar->getDocPos();

		S32 left_line_num = cur_line;
		S32 num_lines = getLineCount();

		S32 line_start = -1;
		if (cur_line >= num_lines)
		{
			return;
		}

		line_start = getLineStart(cur_line);

		S32 left_visible_pos	= line_start;
		S32 right_visible_pos	= line_start;

		S32 text_y = mTextRect.mTop - line_height;

		// Find the coordinates of the selected area
		while (cur_line < num_lines)
		{
			S32 next_line = -1;
			S32 line_end = text_len;

			if (cur_line + 1 < num_lines)
			{
				next_line = getLineStart(cur_line + 1);
				line_end = next_line;

				line_end = line_end - line_start == 0 ||
						   text[next_line - 1] == '\n' ||
						   text[next_line - 1] == '\0' ||
						   text[next_line - 1] == ' ' ||
						   text[next_line - 1] == '\t' ? next_line - 1
													   : next_line;
			}

			const llwchar* line = text.c_str() + line_start;

			if (line_start <= selection_left && selection_left <= line_end)
			{
				left_line_num = cur_line;
				selection_left_x =
					mTextRect.mLeft +
					mGLFont->getWidth(line, 0, selection_left - line_start,
									   mAllowEmbeddedItems);
				selection_left_y = text_y;
			}
			if (line_start <= selection_right && selection_right <= line_end)
			{
				selection_right_visible = true;
				selection_right_x =
					mTextRect.mLeft +
					mGLFont->getWidth(line, 0, selection_right - line_start,
									  mAllowEmbeddedItems);
#if 0
				if (selection_right == line_end)
				{
					// Add empty space for "newline"
					selection_right_x += mGLFont->getWidth("n");
				}
#endif
				selection_right_y = text_y;
			}

			// If selection spans end of current line...
			if (selection_left <= line_end && line_end < selection_right &&
				selection_left != selection_right)
			{
				// Extend selection slightly beyond end of line to indicate
				// selection of newline character (use "n" character to
				// determine width)
				const LLWString nstr(utf8str_to_wstring("n"));
				line_endings.push(mTextRect.mLeft +
								  mGLFont->getWidth(line, 0,
													line_end - line_start,
													mAllowEmbeddedItems) +
								  mGLFont->getWidth(nstr.c_str()));
			}

			// Move down one line
			text_y -= line_height;

			right_visible_pos = line_end;
			line_start = next_line;
			++cur_line;

			if (selection_right_visible)
			{
				break;
			}
		}

		// Draw the selection box (we are using a box instead of reversing the
		// colors on the selected text).
		bool selection_visible = left_visible_pos <= selection_right &&
								 selection_left <= right_visible_pos;
		if (selection_visible)
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			const LLColor4& color = mReadOnly ? mReadOnlyBgColor
											  : mWriteableBgColor;
			F32 alpha = hasFocus() ? 1.f : 0.5f;
			gGL.color4f(1.f - color.mV[0], 1.f - color.mV[1],
						1.f - color.mV[2], alpha);
			S32 margin_offset =
				mShowLineNumbers ? UI_TEXTEDITOR_LINE_NUMBER_MARGIN : 0;

			if (selection_left_y == selection_right_y)
			{
				// Draw from selection start to selection end
				gl_rect_2d(selection_left_x + margin_offset,
						   selection_left_y + line_height + 1,
						   selection_right_x + margin_offset,
						   selection_right_y);
			}
			else
			{
				// Draw from selection start to the end of the first line
				if (mTextRect.mRight == selection_left_x)
				{
					selection_left_x -= CURSOR_THICKNESS;
				}

				S32 line_end = line_endings.front();
				line_endings.pop();
				gl_rect_2d(selection_left_x + margin_offset,
						   selection_left_y + line_height + 1,
						   line_end + margin_offset, selection_left_y);

				S32 line_num = left_line_num + 1;
				while (line_endings.size())
				{
					S32 vert_offset = -(line_num - left_line_num) * line_height;
					// Draw the block between the two lines
					gl_rect_2d(mTextRect.mLeft + margin_offset,
							   selection_left_y + vert_offset + line_height + 1,
							   line_endings.front() + margin_offset,
							   selection_left_y + vert_offset);
					line_endings.pop();
					++line_num;
				}

				// Draw from the start of the last line to selection end
				if (mTextRect.mLeft == selection_right_x)
				{
					selection_right_x += CURSOR_THICKNESS;
				}
				gl_rect_2d(mTextRect.mLeft + margin_offset,
						   selection_right_y + line_height + 1,
						   selection_right_x + margin_offset,
						   selection_right_y);
			}
		}
	}
}

void LLTextEditor::drawMisspelled()
{
	LL_FAST_TIMER(FTM_RENDER_SPELLCHECK);

	// Do not bother checking if the text did not change in a while, and
	// fire a spell checking every second while typing only when the text
	// is under 1024 characters large.
	S32 elapsed = (S32)mSpellTimer.getElapsedTimeF32();
	S32 keystroke = (S32)mKeystrokeTimer.getElapsedTimeF32();
	if (keystroke < 2 &&
		((getLength() < 1024 && (elapsed & 1)) || keystroke > 0))
	{
		S32 new_start_spell = getLineStart(mScrollbar->getDocPos());
		S32 new_end_spell =
			getLineStart(mScrollbar->getDocPos() + 1 +
						 mScrollbar->getDocSize()-mScrollbar->getDocPosMax());
		if (mScrollbar->getDocPos() == mScrollbar->getDocPosMax())
		{
			new_end_spell = (S32)mWText.length();
		}

		if (new_start_spell != mSpellCheckStart ||
			new_end_spell != mSpellCheckEnd || isSpellDirty())
		{
			mSpellCheckStart = new_start_spell;
			mSpellCheckEnd = new_end_spell;
			resetSpellDirty();
			mMisspellLocations = getMisspelledWordsPositions();
		}
	}

	if (mShowMisspelled)
	{
		const LLWString& text = mWText;
		const S32 text_len = getLength();
		const S32 num_lines = getLineCount();
		const F32 line_height = mGLFont->getLineHeight();
		const S32 start_search_pos = mScrollbar->getDocPos();
		// Skip through the lines we are not drawing.
		if (start_search_pos >= num_lines)
		{
			return;
		}
		const S32 start_line_start = getLineStart(start_search_pos);
		const F32 start_text_y = (F32)mTextRect.mTop - line_height;

		const S32 misspells = (S32)mMisspellLocations.size();
		bool found_first_visible = false;
		bool visible;

		for (S32 i = 0; i < misspells; ++i)
		{
			S32 wstart = mMisspellLocations[i++];
			S32 wend = mMisspellLocations[i];

			S32 search_pos = start_search_pos;
			S32 line_start = start_line_start;
			F32 text_y = start_text_y;

			F32 word_left = 0.f;
			F32 word_right = 0.f;

			S32 line_end = 0;
			// Determine if the word is visible and if so at what coordinates
			while (mTextRect.mBottom <= ll_round(text_y) &&
				   search_pos < num_lines)
			{
				line_end = text_len + 1;
				S32 next_line = -1;
				visible = false;

				if (search_pos + 1 < num_lines)
				{
					next_line = getLineStart(search_pos + 1);
					line_end = next_line - 1;
				}
				const llwchar* line = text.c_str() + line_start;
				// Find the cursor and selection bounds
				if (line_start <= wstart && wend <= line_end)
				{
					visible = true;
					word_left = (F32)mTextRect.mLeft - 1.f +
								mGLFont->getWidthF32(line, 0,
													 wstart - line_start,
													 mAllowEmbeddedItems);
					word_right = (F32)mTextRect.mLeft + 1.f +
								 mGLFont->getWidthF32(line, 0,
													  wend - line_start,
													  mAllowEmbeddedItems);
					// Draw the zig zag line
					gGL.color4ub(255, 0, 0, 200);
					while (word_left < word_right)
					{
						gl_line_2d((S32)word_left, (S32)text_y - 2,
								   (S32)word_left + 3, (S32)text_y + 1);
						gl_line_2d((S32)word_left + 3, (S32)text_y + 1,
								   (S32)word_left + 6, (S32)text_y - 2);
						word_left += 6;
					}
					break;
				}
				if (visible && !found_first_visible)
				{
					found_first_visible = true;
				}
				else if (!visible && found_first_visible)
				{
					// We found the last visible misspelled word. Stop now.
					return;
				}
				// move down one line
				text_y -= line_height;
				line_start = next_line;
				++search_pos;
			}
			if (mShowLineNumbers)
			{
				word_left += UI_TEXTEDITOR_LINE_NUMBER_MARGIN;
				word_right += UI_TEXTEDITOR_LINE_NUMBER_MARGIN;
			}
		}
	}
}

void LLTextEditor::drawCursor()
{
	if (gFocusMgr.getKeyboardFocus() == this && gShowTextEditCursor &&
		!mReadOnly)
	{
		const LLWString& text = mWText;
		const S32 text_len = getLength();

		// Skip through the lines we are not drawing.
		S32 cur_pos = mScrollbar->getDocPos();

		S32 num_lines = getLineCount();
		if (cur_pos >= num_lines)
		{
			return;
		}
		S32 line_start = getLineStart(cur_pos);

		F32 line_height = mGLFont->getLineHeight();
		F32 text_y = (F32)(mTextRect.mTop) - line_height;

		F32 cursor_left = 0.f;
		F32 next_char_left = 0.f;
		F32 cursor_bottom = 0.f;
		bool cursor_visible = false;

		S32 line_end = 0;
		// Determine if the cursor is visible and if so at what coordinates
		while (mTextRect.mBottom <= ll_round(text_y) && cur_pos < num_lines)
		{
			line_end = text_len + 1;
			S32 next_line = -1;

			if (cur_pos + 1 < num_lines)
			{
				next_line = getLineStart(cur_pos + 1);
				line_end = next_line - 1;
			}

			const llwchar* line = text.c_str() + line_start;

			// Find the cursor and selection bounds
			if (line_start <= mCursorPos && mCursorPos <= line_end)
			{
				cursor_visible = true;
				next_char_left = (F32)mTextRect.mLeft +
								 mGLFont->getWidthF32(line, 0,
													  mCursorPos - line_start,
													  mAllowEmbeddedItems);
				cursor_left = next_char_left - 1.f;
				cursor_bottom = text_y;
				break;
			}

			// Move down one line
			text_y -= line_height;
			line_start = next_line;
			++cur_pos;
		}

		if (mShowLineNumbers)
		{
			cursor_left += UI_TEXTEDITOR_LINE_NUMBER_MARGIN;
		}

		// Draw the cursor
		if (cursor_visible)
		{
			// Flash the cursor every half second starting a fixed time after
			// the last keystroke
			F32 elapsed = mKeystrokeTimer.getElapsedTimeF32();
			if (elapsed < CURSOR_FLASH_DELAY || (S32(elapsed * 2) & 1))
			{
				F32 cursor_top = cursor_bottom + line_height + 1.f;
				F32 cursor_right = cursor_left + (F32)CURSOR_THICKNESS;
				if (gKeyboardp &&
					gKeyboardp->getInsertMode() == LL_KIM_OVERWRITE &&
					!hasSelection())
				{
					cursor_left += CURSOR_THICKNESS;
					const LLWString space(utf8str_to_wstring(" "));
					F32 spacew = mGLFont->getWidthF32(space.c_str());
					if (mCursorPos == line_end)
					{
						cursor_right = cursor_left + spacew;
					}
					else
					{
						F32 width = mGLFont->getWidthF32(text.c_str(),
														 mCursorPos, 1,
														 mAllowEmbeddedItems);
						cursor_right = cursor_left + llmax(spacew, width);
					}
				}

				gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

				gGL.color4fv(mCursorColor.mV);

				gl_rect_2d(llfloor(cursor_left), llfloor(cursor_top),
						   llfloor(cursor_right), llfloor(cursor_bottom));

				if (gKeyboardp &&
					gKeyboardp->getInsertMode() == LL_KIM_OVERWRITE &&
					!hasSelection() && text[mCursorPos] != '\n')
				{
					const LLTextSegment* segmentp =
						getSegmentAtOffset(mCursorPos);
					LLColor4 text_color;
					if (segmentp)
					{
						text_color = segmentp->getColor();
					}
					else if (mReadOnly)
					{
						text_color = mReadOnlyFgColor;
					}
					else
					{
						text_color = mFgColor;
					}
					mGLFont->render(text, mCursorPos, next_char_left,
									cursor_bottom + line_height,
									LLColor4(1.f - text_color.mV[VRED],
											 1.f - text_color.mV[VGREEN],
											 1.f - text_color.mV[VBLUE], 1.f),
									LLFontGL::LEFT, LLFontGL::TOP,
									LLFontGL::NORMAL, 1);
				}

				// Make sure the IME is in the right place
				LLRect screen_pos = getScreenRect();
				LLCoordGL ime_pos(screen_pos.mLeft + llfloor(cursor_left),
								  screen_pos.mBottom + llfloor(cursor_top));

				ime_pos.mX = (S32) (ime_pos.mX * LLUI::sGLScaleFactor.mV[VX]);
				ime_pos.mY = (S32) (ime_pos.mY * LLUI::sGLScaleFactor.mV[VY]);
				gWindowp->setLanguageTextInput(ime_pos);
			}
		}
	}
}

void LLTextEditor::drawPreeditMarker()
{
	if (!hasPreeditString())
	{
		return;
	}

	const llwchar* text = mWText.c_str();
	const S32 text_len = getLength();
	const S32 num_lines = getLineCount();

	S32 cur_line = mScrollbar->getDocPos();
	if (cur_line >= num_lines)
	{
		return;
	}

	const S32 line_height = ll_roundp(mGLFont->getLineHeight());

	S32 line_start = getLineStart(cur_line);
	S32 line_y = mTextRect.mTop - line_height;
	while (mTextRect.mBottom <= line_y && num_lines > cur_line)
	{
		S32 next_start = -1;
		S32 line_end = text_len;

		if (cur_line + 1 < num_lines)
		{
			next_start = getLineStart(cur_line + 1);
			line_end = next_start;
		}
		if (text[line_end-1] == '\n')
		{
			--line_end;
		}

		// Does this line contain preedits?
		if (line_start >= mPreeditPositions.back())
		{
			// We have passed the preedits.
			break;
		}
		if (line_end > mPreeditPositions.front())
		{
			for (U32 i = 0, count = mPreeditStandouts.size(); i < count; ++i)
			{
				S32 left = mPreeditPositions[i];
				S32 right = mPreeditPositions[i + 1];
				if (right <= line_start || left >= line_end)
				{
					continue;
				}

				S32 preedit_left = mTextRect.mLeft;
				if (left > line_start)
				{
					preedit_left += mGLFont->getWidth(text, line_start,
													  left - line_start,
													  mAllowEmbeddedItems);
				}
				S32 preedit_right = mTextRect.mLeft;
				if (right < line_end)
				{
					preedit_right += mGLFont->getWidth(text, line_start,
													   right - line_start,
													   mAllowEmbeddedItems);
				}
				else
				{
					preedit_right += mGLFont->getWidth(text, line_start,
													   line_end - line_start,
													   mAllowEmbeddedItems);
				}

				if (mPreeditStandouts[i])
				{
					gl_rect_2d(preedit_left + PREEDIT_STANDOUT_GAP,
							   line_y + PREEDIT_STANDOUT_POSITION,
							   preedit_right - PREEDIT_STANDOUT_GAP - 1,
							   line_y + PREEDIT_STANDOUT_POSITION -
							   PREEDIT_STANDOUT_THICKNESS,
							   (mCursorColor * PREEDIT_STANDOUT_BRIGHTNESS +
								mWriteableBgColor *
								(1 - PREEDIT_STANDOUT_BRIGHTNESS)).setAlpha(1.f));
				}
				else
				{
					gl_rect_2d(preedit_left + PREEDIT_MARKER_GAP,
							   line_y + PREEDIT_MARKER_POSITION,
							   preedit_right - PREEDIT_MARKER_GAP - 1,
							   line_y + PREEDIT_MARKER_POSITION -
							   PREEDIT_MARKER_THICKNESS,
							   (mCursorColor * PREEDIT_MARKER_BRIGHTNESS +
								mWriteableBgColor *
								(1 - PREEDIT_MARKER_BRIGHTNESS)).setAlpha(1.f));
				}
			}
		}

		// Move down one line
		line_y -= line_height;
		line_start = next_start;
		++cur_line;
	}
}

void LLTextEditor::drawText()
{
	const LLWString& text = mWText;
	const S32 text_len = getLength();
	if (text_len <= 0) return;
	S32 selection_left = -1;
	S32 selection_right = -1;
	// Draw selection even if we do not have keyboard focus for search/replace
	if (hasSelection())
	{
		selection_left = llmin(mSelectionStart, mSelectionEnd);
		selection_right = llmax(mSelectionStart, mSelectionEnd);
	}

	LLGLSUIDefault gls_ui;

	// There are several concepts that are important for understanding the
	// following drawing code.
	// The document is logically a sequence of characters (stored in a
	// LLWString).
	// Variables below with "start" or "end" in their names refer to positions
	// or offsets into it.
	// Next there are two kinds of "line" variables to understand. Newline
	// characters in the character sequence represent logical lines. These are
	// what get numbered and so variables representing this kind of line have
	// "num" in their names.
	// The others represent line fragments or displayed lines which the
	// scrollbar deals with.
	// When the "show line numbers" property is turned on, we draw line numbers
	// to the left of the beginning of each logical line and not in front of
	// wrapped "continuation" display lines. -MG

	// Scrollbar counts each wrap as a new line.
	S32 cur_line = mScrollbar->getDocPos();
	S32 num_lines = getLineCount();
	if (cur_line >= num_lines) return;

	S32 line_start = getLineStart(cur_line);
	S32 prev_start = getLineStart(cur_line - 1);

	// Does not count wraps. i.e. only counts newlines.
	S32 cur_line_num  = getLineForPosition(line_start);

	S32 prev_line_num = getLineForPosition(prev_start);

	bool cur_line_is_continuation = cur_line_num > 0 &&
									cur_line_num == prev_line_num;
	bool line_wraps = false;

	LLTextSegment t(line_start);
	segment_list_t::iterator seg_iter;
	seg_iter = std::upper_bound(mSegments.begin(), mSegments.end(), &t,
								LLTextSegment::compare());
	if (seg_iter == mSegments.end() || (*seg_iter)->getStart() > line_start)
	{
		--seg_iter;
	}
	LLTextSegment* cur_segment = *seg_iter;

	S32 line_height = ll_roundp(mGLFont->getLineHeight());
	F32 text_y = (F32)(mTextRect.mTop - line_height);
	while (mTextRect.mBottom <= text_y && cur_line < num_lines)
	{
		S32 next_start = -1;
		S32 line_end = text_len;

		if (cur_line + 1 < num_lines)
		{
			next_start = getLineStart(cur_line + 1);
			line_end = next_start;
		}
		line_wraps = text[line_end - 1] != '\n';
		if (!line_wraps)
		{
			--line_end; // do not attempt to draw the newline char.
		}

		F32 text_start = (F32)mTextRect.mLeft;
		F32 text_x = text_start +
					 (mShowLineNumbers ? UI_TEXTEDITOR_LINE_NUMBER_MARGIN : 0);

		// Draw the line numbers
		if (mShowLineNumbers && !cur_line_is_continuation)
		{
			const LLFontGL* num_font = LLFontGL::getFontMonospace();
			F32 y_top = text_y +
						(F32)ll_roundp(num_font->getLineHeight()) * 0.5f;
			const LLWString ltext =
				utf8str_to_wstring(llformat("%*d",
											UI_TEXTEDITOR_LINE_NUMBER_DIGITS,
											cur_line_num));
			bool is_cur_line = getCurrentLine() == cur_line_num;
			const U8 style = is_cur_line ? LLFontGL::BOLD : LLFontGL::NORMAL;
			const LLColor4 fg_color = is_cur_line ? mCursorColor
												  : mReadOnlyFgColor;
			num_font->render(ltext, // string to draw
							 0, // begin offset
							 3., // x
							 y_top, // y
							 fg_color,
							 LLFontGL::LEFT, // horizontal alignment
							 LLFontGL::VCENTER, // vertical alignment
							 style,
							 S32_MAX, // max chars
							 UI_TEXTEDITOR_LINE_NUMBER_MARGIN); // max pixels
		}

		S32 seg_start = line_start;
		while (seg_start < line_end)
		{
			while (cur_segment->getEnd() <= seg_start)
			{
				++seg_iter;
				if (seg_iter == mSegments.end())
				{
					llwarns << "Ran off the segmentation end !" << llendl;
					return;
				}
				cur_segment = *seg_iter;
			}

			// Draw a segment within the line
			S32 clipped_end	= llmin(line_end, cur_segment->getEnd());
			S32 clipped_len = clipped_end - seg_start;
			if (clipped_len > 0)
			{
				LLStyleSP style = cur_segment->getStyle();
				if (style->isImage() && cur_segment->getStart() >= seg_start &&
					cur_segment->getStart() <= clipped_end)
				{
					S32 style_image_height = style->mImageHeight;
					S32 style_image_width = style->mImageWidth;
					LLUIImagePtr image = style->getImage();
					image->draw(ll_round(text_x),
								ll_round(text_y) +
								line_height-style_image_height,
								style_image_width, style_image_height);
				}

				bool is_embedded = cur_segment == mHoverSegment &&
								   style->getIsEmbeddedItem();
				if (is_embedded)
				{
					style->mUnderline = true;
					is_embedded = true;
				}

				S32 left_pos = llmin(mSelectionStart, mSelectionEnd);

				if (!is_embedded && mParseHTML && left_pos > seg_start &&
					left_pos < clipped_end && mIsSelecting &&
					mSelectionStart == mSelectionEnd)
				{
					mHTML = style->getLinkHREF();
				}

				drawClippedSegment(text, seg_start, clipped_end, text_x,
								   text_y, selection_left, selection_right,
								   style, &text_x);

				if (text_x == text_start && mShowLineNumbers)
				{
					text_x += UI_TEXTEDITOR_LINE_NUMBER_MARGIN;
				}

				// Note: text_x is incremented by drawClippedSegment()
				seg_start += clipped_len;
			}
		}

		// Move down one line
		text_y -= (F32)line_height;

		if (line_wraps)
		{
			--cur_line_num;
		}
		// So as to not not number the continuation lines
		cur_line_is_continuation = line_wraps;

		line_start = next_start;
		++cur_line;
		++cur_line_num;
	}
}

// Draws a single text segment, reversing the color for selection if needed.
void LLTextEditor::drawClippedSegment(const LLWString& text, S32 seg_start,
									  S32 seg_end, F32 x, F32 y,
									  S32 selection_left, S32 selection_right,
									  const LLStyleSP& style, F32* right_x)
{
	if (!style->isVisible())
	{
		return;
	}

	const LLFontGL* font = mGLFont;

	LLColor4 color = style->getColor();

	if (style->getFontString()[0])
	{
		font = LLFontGL::getFont(style->getFontID());
	}

	U8 font_flags = LLFontGL::NORMAL;

	if (style->mBold)
	{
		font_flags |= LLFontGL::BOLD;
	}
	if (style->mItalic)
	{
		font_flags |= LLFontGL::ITALIC;
	}
	if (style->mUnderline)
	{
		font_flags |= LLFontGL::UNDERLINE;
	}

	if (style->getIsEmbeddedItem())
	{
		color = mReadOnly ? LLUI::sTextEmbeddedItemReadOnlyColor
						  : LLUI::sTextEmbeddedItemColor;
	}

	F32 y_top = y + (F32)ll_roundp(font->getLineHeight());

 	bool use_embedded = mAllowEmbeddedItems && style->getIsEmbeddedItem();
 	if (selection_left > seg_start)
	{
		// Draw normally
		S32 start = seg_start;
		S32 end = llmin(selection_left, seg_end);
		S32 length =  end - start;
		font->render(text, start, x, y_top, color, LLFontGL::LEFT,
					 LLFontGL::TOP, font_flags, length, S32_MAX,
					 right_x, use_embedded);
	}
	x = *right_x;

	if (selection_left < seg_end && selection_right > seg_start)
	{
		// Draw reversed
		S32 start = llmax(selection_left, seg_start);
		S32 end = llmin(selection_right, seg_end);
		S32 length = end - start;

		font->render(text, start, x, y_top,
					 LLColor4(1.f - color.mV[0],
							  1.f - color.mV[1],
							  1.f - color.mV[2], 1.f),
					 LLFontGL::LEFT, LLFontGL::TOP, font_flags, length,
					 S32_MAX, right_x, use_embedded);
	}
	x = *right_x;
	if (selection_right < seg_end)
	{
		// Draw normally
		S32 start = llmax(selection_right, seg_start);
		S32 end = seg_end;
		S32 length = end - start;
		font->render(text, start, x, y_top, color, LLFontGL::LEFT,
					 LLFontGL::TOP, font_flags, length, S32_MAX, right_x,
					 use_embedded);
	}
}

void LLTextEditor::draw()
{
	// do on-demand reflow
	if (mReflowNeeded)
	{
		updateLineStartList();
		mReflowNeeded = false;
	}

	// then update scroll position, as cursor may have moved
	if (mScrollNeeded)
	{
		updateScrollFromCursor();
		mScrollNeeded = false;
	}

	{
		LLLocalClipRect clip(LLRect(0, getRect().getHeight(),
							 getRect().getWidth() -
							 (mScrollbar->getVisible() ? SCROLLBAR_SIZE : 0),
							 0));

		bindEmbeddedChars(mGLFont);

		drawBackground();
		drawSelectionBackground();
		drawPreeditMarker();
		drawText();
		drawCursor();
		if (!mReadOnly && mSpellCheck && hasFocus() &&
			LLSpellCheck::getInstance()->getSpellCheck())
		{
			drawMisspelled();
		}

		unbindEmbeddedChars(mGLFont);

		// RN: the decision was made to always show the orange border for
		// keyboard focus but do not put an insertion caret when in readonly
		// mode
		mBorder->setKeyboardFocusHighlight(gFocusMgr.getKeyboardFocus() == this
										   /*&& !mReadOnly*/);
	}

	LLView::draw();  // Draw children (scrollbar and border)

	// remember if we are supposed to be at the bottom of the buffer
	mScrolledToBottom = isScrolledToBottom();
}

void LLTextEditor::onTabInto()
{
#if 0
	// Selecting all on tabInto causes users to hit tab twice and replace their
	// text with a tab character theoretically, one could selectAll if
	// mTabsToNextField is true, but we could not think of a use case where
	// you would want to select all anyway preserve insertion point when
	// returning to the editor
	selectAll();
#endif
}

//virtual
void LLTextEditor::clear()
{
	setText(LLStringUtil::null);
	std::for_each(mSegments.begin(), mSegments.end(), DeletePointer());
	mSegments.clear();
}

// Start or stop the editor from accepting text-editing keystrokes
// see also LLLineEditor
void LLTextEditor::setFocus(bool new_state)
{
	bool old_state = hasFocus();

	// Do not change anything if the focus state did not change
	if (new_state == old_state) return;

	// Notify early if we are losing focus.
	if (!new_state)
	{
		gWindowp->allowLanguageTextInput(this, false);
	}

	LLUICtrl::setFocus(new_state);

	if (new_state)
	{
		// Route menu to this class
		grabMenuHandler();

		// Do not start the cursor flashing right away
		resetKeystrokeTimer();
	}
	else
	{
		// Route menu back to the default
		releaseMenuHandler();

		endSelection();
	}
}

// Given a line (from the start of the doc) and an offset into the line, find
// the offset (pos) into text.
S32 LLTextEditor::getPos(S32 line, S32 offset)
{
	S32 line_start = getLineStart(line);
	S32 next_start = getLineStart(line+1);
	if (next_start == line_start)
	{
		next_start = getLength() + 1;
	}
	S32 line_length = next_start - line_start - 1;
	line_length = llmax(line_length, 0);
	return line_start + llmin(offset, line_length);
}

void LLTextEditor::changePage(S32 delta)
{
	S32 line, offset;
	getLineAndOffset(mCursorPos, &line, &offset);

	// Get desired x position to remember previous position
	S32 desired_x_pixel = mDesiredXPixel;

	// Allow one line overlap
	S32 page_size = mScrollbar->getPageSize() - 1;
	if (delta == -1)
	{
		line = llmax(line - page_size, 0);
		setCursorPos(getPos(line, offset));
		mScrollbar->setDocPos(mScrollbar->getDocPos() - page_size);
	}
	else
	if (delta == 1)
	{
		setCursorPos(getPos(line + page_size, offset));
		mScrollbar->setDocPos(mScrollbar->getDocPos() + page_size);
	}

	// Put desired position into remember-buffer after setCursorPos()
	mDesiredXPixel = desired_x_pixel;

	if (mOnScrollEndCallback && mOnScrollEndData &&
		mScrollbar->getDocPos() == mScrollbar->getDocPosMax())
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}
}

void LLTextEditor::changeLine(S32 delta)
{
	bindEmbeddedChars(mGLFont);

	S32 line, offset;
	getLineAndOffset(mCursorPos, &line, &offset);

	S32  line_start = getLineStart(line);

	// Set desired x position to remembered previous position
	S32 desired_x_pixel = mDesiredXPixel;
	// If remembered position was reset (thus -1), calculate new one here
	if (desired_x_pixel == -1)
	{
		desired_x_pixel = mGLFont->getWidth(mWText.c_str(), line_start, offset,
											mAllowEmbeddedItems);
	}

	S32 new_line = 0;
	if (delta < 0 && line > 0)
	{
		new_line = line - 1;
	}
	else
	if (delta > 0 && line < getLineCount() - 1)
	{
		new_line = line + 1;
	}
	else
	{
		unbindEmbeddedChars(mGLFont);
		return;
	}

	S32 num_lines = getLineCount();
	S32 new_line_start = getLineStart(new_line);
	S32 new_line_end = getLength();
	if (new_line + 1 < num_lines)
	{
		new_line_end = getLineStart(new_line + 1) - 1;
	}

	S32 new_line_len = new_line_end - new_line_start;

	S32 new_offset =
		mGLFont->charFromPixelOffset(mWText.c_str(), new_line_start,
									 (F32)desired_x_pixel,
									 (F32)mTextRect.getWidth(),
									 new_line_len, mAllowEmbeddedItems);

	setCursorPos (getPos(new_line, new_offset));

	// Put desired position into remember-buffer after setCursorPos()
	mDesiredXPixel = desired_x_pixel;
	unbindEmbeddedChars(mGLFont);
}

bool LLTextEditor::isScrolledToTop()
{
	return mScrollbar->isAtBeginning();
}

bool LLTextEditor::isScrolledToBottom()
{
	return mScrollbar->isAtEnd();
}

void LLTextEditor::startOfLine()
{
	S32 line, offset;
	getLineAndOffset(mCursorPos, &line, &offset);
	setCursorPos(mCursorPos - offset);
}

// public
void LLTextEditor::setCursorAndScrollToEnd()
{
	deselect();
	endOfDoc();
	needsScroll();
}

void LLTextEditor::scrollToPos(S32 pos)
{
	mScrollbar->setDocSize(getLineCount());

	S32 line, offset;
	getLineAndOffset(pos, &line, &offset);

	S32 page_size = mScrollbar->getPageSize();

	if (line < mScrollbar->getDocPos())
	{
		// Scroll so that the cursor is at the top of the page
		mScrollbar->setDocPos(line);
	}
	else if (line >= mScrollbar->getDocPos() + page_size - 1)
	{
		S32 new_pos = 0;
		if (line < mScrollbar->getDocSize() - 1)
		{
			// Scroll so that the cursor is one line above the bottom of the
			// page
			new_pos = line - page_size + 1;
		}
		else
		{
			// If there is less than a page of text remaining, scroll so that
			// the cursor is at the bottom
			new_pos = mScrollbar->getDocPosMax();
		}
		mScrollbar->setDocPos(new_pos);
	}

	// Check if we have scrolled to bottom for callback if asked for callback
	if (mOnScrollEndCallback && mOnScrollEndData &&
		mScrollbar->getDocPos() == mScrollbar->getDocPosMax())
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}
}

void LLTextEditor::getLineAndColumnForPosition(S32 position, S32* line,
											   S32* col, bool include_wordwrap)
{
	if (include_wordwrap)
	{
		getLineAndOffset(mCursorPos, line, col);
	}
	else
	{
		const LLWString& text = mWText;
		S32 line_count = 0;
		S32 line_start = 0;
		S32 i;
		for (i = 0; text[i] && i < position; ++i)
		{
			if ('\n' == text[i])
			{
				line_start = i + 1;
				++line_count;
			}
		}
		*line = line_count;
		*col = i - line_start;
	}
}

void LLTextEditor::getCurrentLineAndColumn(S32* line, S32* col,
										   bool include_wordwrap)
{
	getLineAndColumnForPosition(mCursorPos, line, col, include_wordwrap);
}

S32 LLTextEditor::getCurrentLine()
{
	return getLineForPosition(mCursorPos);
}

S32 LLTextEditor::getLineForPosition(S32 position)
{
	S32 line, col;
	getLineAndColumnForPosition(position, &line, &col, false);
	return line;
}

void LLTextEditor::endOfLine()
{
	S32 line, offset;
	getLineAndOffset(mCursorPos, &line, &offset);
	S32 num_lines = getLineCount();
	if (line + 1 >= num_lines)
	{
		setCursorPos(getLength());
	}
	else
	{
		setCursorPos(getLineStart(line + 1) - 1);
	}
}

void LLTextEditor::endOfDoc()
{
	mScrollbar->setDocPos(mScrollbar->getDocPosMax());
	mScrolledToBottom = true;

	S32 len = getLength();
	if (len)
	{
		setCursorPos(len);
	}
	if (mOnScrollEndCallback && mOnScrollEndData &&
		mScrollbar->getDocPos() == mScrollbar->getDocPosMax())
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}
}

// Sets the scrollbar from the cursor position
void LLTextEditor::updateScrollFromCursor()
{
	if (mReadOnly)
	{
		// No cursor in read only mode
		return;
	}
	scrollToPos(mCursorPos);
}

void LLTextEditor::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLView::reshape(width, height, called_from_parent);

	// Do this first after reshape, because other things depend on up-to-date
	// mTextRect
	updateTextRect();

	needsReflow();

	// Propagate shape information to scrollbar
	mScrollbar->setDocSize(getLineCount());

	S32 line_height = ll_roundp(mGLFont->getLineHeight());
	S32 page_lines = mTextRect.getHeight() / line_height;
	mScrollbar->setPageSize(page_lines);
}

void LLTextEditor::autoIndent()
{
	// Count the number of spaces in the current line
	S32 line, offset;
	getLineAndOffset(mCursorPos, &line, &offset);
	S32 line_start = getLineStart(line);
	S32 space_count = 0;
	S32 i;

	const LLWString& text = mWText;
	while (' ' == text[line_start])
	{
		++space_count;
		++line_start;
	}

	// If we are starting a braced section, indent one level.
	if (mCursorPos > 0 && text[mCursorPos - 1] == '{')
	{
		space_count += SPACES_PER_TAB;
	}

	// Insert that number of spaces on the new line
	addChar('\n');
	for (i = 0; i < space_count; ++i)
	{
		addChar(' ');
	}
}

// Inserts new text at the cursor position
void LLTextEditor::insertText(const std::string& new_text)
{
	bool enabled = getEnabled();
	setEnabled(true);

	// Delete any selected characters (the insertion replaces them)
	if (hasSelection())
	{
		deleteSelection(true);
	}

	setCursorPos(mCursorPos + insert(mCursorPos, utf8str_to_wstring(new_text),
				 false));

	needsReflow();

	setEnabled(enabled);
}

void LLTextEditor::appendColoredText(const std::string& new_text,
									 bool allow_undo, bool prepend_newline,
									 const LLColor4& color,
									 const std::string& font_name)
{
	LLStyleSP style(new LLStyle);
	style->setVisible(true);
	style->setColor(color);
	style->setFontName(font_name);
	appendStyledText(new_text, allow_undo, prepend_newline, style);
}

void LLTextEditor::appendStyledText(const std::string& new_text,
									 bool allow_undo,
									 bool prepend_newline,
									 LLStyleSP stylep)
{
	S32 part = (S32)WHOLE;
	if (mParseHTML)
	{
		S32 start = 0, end = 0;
		std::string text = new_text;
		while (findHTML(text, &start, &end))
		{
			LLStyleSP html(new LLStyle);
			html->setVisible(true);
			html->setColor(mLinkColor);
			if (stylep)
			{
				html->setFontName(stylep->getFontString());
			}
			html->mUnderline = true;

			if (start > 0)
			{
				if (part == (S32)WHOLE || part == (S32)START)
				{
					part = (S32)START;
				}
				else
				{
					part = (S32)MIDDLE;
				}
				std::string subtext = text.substr(0, start);
				appendText(subtext, allow_undo, prepend_newline, stylep);
			}

			html->setLinkHREF(text.substr(start, end - start));
			appendText(text.substr(start, end - start), allow_undo,
					   prepend_newline, html);
			if (end < (S32)text.length())
			{
				text = text.substr(end, text.length() - end);
				end = 0;
				part = (S32)END;
			}
			else
			{
				break;
			}
		}
		if (part != (S32)WHOLE)
		{
			part = (S32)END;
		}
		if (end < (S32)text.length())
		{
			appendText(text, allow_undo, prepend_newline, stylep);
		}
	}
	else
	{
		appendText(new_text, allow_undo, prepend_newline, stylep);
	}
}

// Appends new text to end of document
void LLTextEditor::appendText(const std::string& new_text, bool allow_undo,
							  bool prepend_newline, const LLStyleSP stylep)
{
	// Save old state
	bool was_scrolled_to_bottom =
		mScrollbar->getDocPos() == mScrollbar->getDocPosMax();
	S32 selection_start = mSelectionStart;
	S32 selection_end = mSelectionEnd;
	bool was_selecting = mIsSelecting;
	S32 cursor_pos = mCursorPos;
	S32 old_length = getLength();
	bool cursor_was_at_end = mCursorPos == old_length;

	deselect();

	setCursorPos(old_length);

	// Add carriage return if not first line
	if (getLength() != 0 && prepend_newline)
	{
		std::string final_text = "\n";
		final_text += new_text;
		append(utf8str_to_wstring(final_text), true);
	}
	else
	{
		append(utf8str_to_wstring(new_text), true);
	}

	if (stylep)
	{
		S32 segment_start = old_length;
		S32 segment_end = getLength();
		LLTextSegment* segment = new LLTextSegment(stylep, segment_start,
												   segment_end);
		mSegments.push_back(segment);
	}

	needsReflow();

	// Set the cursor and scroll position
	// Maintain the scroll position unless the scroll was at the end of the doc
	// (in which case, move it to the new end of the doc) or unless the user
	// was doing actively selecting
	if (was_scrolled_to_bottom && !was_selecting)
	{
		if (selection_start != selection_end)
		{
			// maintain an existing non-active selection
			mSelectionStart = selection_start;
			mSelectionEnd = selection_end;
		}
		endOfDoc();
	}
	else if (selection_start != selection_end)
	{
		mSelectionStart = selection_start;
		mSelectionEnd = selection_end;
		mIsSelecting = was_selecting;
		setCursorPos(cursor_pos);
	}
	else if (cursor_was_at_end)
	{
		setCursorPos(getLength());
	}
	else
	{
		setCursorPos(cursor_pos);
	}

	if (!allow_undo)
	{
		blockUndo();
	}
}

S32 LLTextEditor::removeFirstLine()
{
	S32 num_lines = getLineCount();
	if (!num_lines)
	{
		return 0;
	}
	S32 length = getLineStart(1) - 1;
	if (length <= 0)
	{
		length = getLength();
	}
	deselect();
	removeStringNoUndo(0, length);
	pruneSegments();
	updateLineStartList();
	needsScroll();
	return length;
}

void LLTextEditor::removeTextFromEnd(S32 num_chars)
{
	if (num_chars <= 0) return;

	num_chars = llclamp(num_chars, 0, getLength());
	remove(getLength() - num_chars, num_chars, false);

	S32 len = getLength();
	mCursorPos = llclamp(mCursorPos, 0, len);
	mSelectionStart = llclamp(mSelectionStart, 0, len);
	mSelectionEnd = llclamp(mSelectionEnd, 0, len);

	pruneSegments();

	// pruneSegments will invalidate mLineStartList.
	updateLineStartList();
	needsScroll();
}

///////////////////////////////////////////////////////////////////
// Returns change in number of characters in mWText

S32 LLTextEditor::insertStringNoUndo(S32 pos, const LLWString& wstr)
{
	S32 old_len = mWText.length();		// length() returns character length
	S32 insert_len = wstr.length();

	mWText.insert(pos, wstr);
	mTextIsUpToDate = false;

	if (truncate())
	{
		// The user's not getting everything he's hoping for
		make_ui_sound("UISndBadKeystroke");
		insert_len = mWText.length() - old_len;
	}

	return insert_len;
}

S32 LLTextEditor::removeStringNoUndo(S32 pos, S32 length)
{
	mWText.erase(pos, length);
	mTextIsUpToDate = false;
	// This will be wrong if someone calls removeStringNoUndo with an excessive
	// length
	return -length;
}

S32 LLTextEditor::overwriteCharNoUndo(S32 pos, llwchar wc)
{
	if (pos > (S32)mWText.length())
	{
		return 0;
	}
	mWText[pos] = wc;
	mTextIsUpToDate = false;
	return 1;
}

void LLTextEditor::makePristine()
{
	mPristineCmd = mLastCmd;
	mBaseDocIsPristine = !mLastCmd;

	// Create a clean partition in the undo stack. We do not want a single
	// command to extend from the "pre-pristine" state to the "post-pristine"
	// state.
	if (mLastCmd)
	{
		mLastCmd->blockExtensions();
	}
}

bool LLTextEditor::isPristine() const
{
	if (mPristineCmd)
	{
		return mPristineCmd == mLastCmd;
	}

	// No undo stack, so check if the version before and commands were done was
	// the original version
	return !mLastCmd && mBaseDocIsPristine;
}

bool LLTextEditor::tryToRevertToPristineState()
{
	if (!isPristine())
	{
		deselect();
		S32 i = 0;
		while (!isPristine() && canUndo())
		{
			undo();
			--i;
		}

		while (!isPristine() && canRedo())
		{
			redo();
			++i;
		}

		if (!isPristine())
		{
			// failed, so go back to where we started
			while (i > 0)
			{
				undo();
				--i;
			}
		}

		needsReflow();
	}

	return isPristine(); // true => success
}

void LLTextEditor::updateTextRect()
{
	mTextRect.setOriginAndSize(UI_TEXTEDITOR_BORDER + UI_TEXTEDITOR_H_PAD,
							   UI_TEXTEDITOR_BORDER,
							   getRect().getWidth() - SCROLLBAR_SIZE -
							   2 * (UI_TEXTEDITOR_BORDER +
							   UI_TEXTEDITOR_H_PAD),
							   getRect().getHeight() -
							   2 * UI_TEXTEDITOR_BORDER -
							   UI_TEXTEDITOR_V_PAD_TOP);
}

void LLTextEditor::loadKeywords(const std::string& filename,
								const std::vector<std::string>& funcs,
								const std::vector<std::string>& tooltips,
								const LLColor3& color)
{
	if (mKeywords.loadFromFile(filename))
	{
		S32 count = llmin(funcs.size(), tooltips.size());
		for (S32 i = 0; i < count; ++i)
		{
			std::string name = utf8str_trim(funcs[i]);
			mKeywords.addToken(LLKeywordToken::WORD, name, color, tooltips[i]);
		}

		mKeywords.findSegments(&mSegments, mWText, mDefaultColor);

		llassert(mSegments.front()->getStart() == 0 &&
				 mSegments.back()->getEnd() == getLength());
	}
}

void LLTextEditor::updateSegments()
{
	// For now, we allow keywords-based syntax highlighting (e.g. for the
	// script editor), or embedded items (e.g. for notecards and group notices
	// with inventory offers), or styled text (with colors, links, etc), the
	// latter staying untouched by updateSegments(). It is however not possible
	// to mix up the three types of text editors...
	if (mKeywords.isLoaded())
	{
		// *HACK: no non-ASCII keywords for now
		mKeywords.findSegments(&mSegments, mWText, mDefaultColor);
	}
	else if (mAllowEmbeddedItems)
	{
		findEmbeddedItemSegments();
	}

	// Make sure we have at least one segment
	if (mSegments.size() == 1 && mSegments[0]->getIsDefault())
	{
		delete mSegments[0];
		mSegments.clear(); // create default segment
	}
	if (mSegments.empty())
	{
		LLColor4& text_color = mReadOnly ? mReadOnlyFgColor : mFgColor;
		LLTextSegment* default_segment = new LLTextSegment(text_color, 0,
														   mWText.length());
		default_segment->setIsDefault(true);
		mSegments.push_back(default_segment);
	}
}

// Only effective if text was removed from the end of the editor
// *NOTE: Using this will invalidate references to mSegments from
// mLineStartList.
void LLTextEditor::pruneSegments()
{
	S32 len = mWText.length();
	// Find and update the first valid segment
	segment_list_t::iterator segments_begin = mSegments.begin();
	segment_list_t::iterator iter = mSegments.end();
	while (iter != segments_begin)
	{
		LLTextSegment* seg = *(--iter);
		if (seg->getStart() < len)
		{
			// valid segment
			if (seg->getEnd() > len)
			{
				seg->setEnd(len);
			}
			break; // done
		}
	}
	if (iter != mSegments.end())
	{
		// erase invalid segments
		++iter;
		std::for_each(iter, mSegments.end(), DeletePointer());
		mSegments.erase(iter, mSegments.end());
	}
#if 0	// We do not care, and it can happen legally for
		// removeTextFromEnd(text->getMaxLength())
	else
	{
		llwarns << "Tried to erase end of empty LLTextEditor" << llendl;
	}
#endif
}

void LLTextEditor::findEmbeddedItemSegments()
{
	mHoverSegment = NULL;
	std::for_each(mSegments.begin(), mSegments.end(), DeletePointer());
	mSegments.clear();

	bool found_embedded_items = false;
	const LLWString& text = mWText;
	S32 idx = 0;
	while (text[idx])
	{
		if (text[idx] >= FIRST_EMBEDDED_CHAR &&
			text[idx] <= LAST_EMBEDDED_CHAR)
 		{
			found_embedded_items = true;
			break;
		}
		++idx;
	}

	if (!found_embedded_items)
	{
		return;
	}

	S32 text_len = text.length();
	LLColor4& text_color = mReadOnly ? mReadOnlyFgColor : mFgColor;
	bool in_text = false;
	if (idx > 0)
	{
		// text
		mSegments.push_back(new LLTextSegment(text_color, 0, text_len));
		in_text = true;
	}

	LLStyleSP embedded_style(new LLStyle);
	embedded_style->setIsEmbeddedItem(true);

	// Start with i just after the first embedded item
	while (text[idx])
	{
		if (text[idx] >= FIRST_EMBEDDED_CHAR &&
			text[idx] <= LAST_EMBEDDED_CHAR)
		{
			if (in_text)
			{
				mSegments.back()->setEnd(idx);
			}
			// item
			mSegments.push_back(new LLTextSegment(embedded_style, idx,
												  idx + 1));
			in_text = false;
		}
		else if (!in_text)
		{
			// text
			mSegments.push_back(new LLTextSegment(text_color, idx, text_len));
			in_text = true;
		}
		++idx;
	}
}

bool LLTextEditor::handleMouseUpOverSegment(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		// This mouse up was part of a click. Regardless of where the cursor
		// is, see if we recently touched a link and launch it if we did.
		if (mParseHTML && mHTML.length() > 0)
		{
			// Special handling for slurls
			if (sSecondlifeURLcallback && !(*sSecondlifeURLcallback)(mHTML))
			{
				if (sURLcallback)
				{
					(*sURLcallback)(mHTML);
				}
			}
			mHTML.clear();
		}
	}

	return false;
}

// Finds the text segment (if any) at the give local screen position
const LLTextSegment* LLTextEditor::getSegmentAtLocalPos(S32 x, S32 y) const
{
	// Find the cursor position at the requested local screen position
	S32 offset = getCursorPosFromLocalCoord(x, y, false);
	S32 idx = getSegmentIdxAtOffset(offset);
	return idx >= 0 ? mSegments[idx] : NULL;
}

const LLTextSegment* LLTextEditor::getSegmentAtOffset(S32 offset) const
{
	S32 idx = getSegmentIdxAtOffset(offset);
	return idx >= 0 ? mSegments[idx] : NULL;
}

S32 LLTextEditor::getSegmentIdxAtOffset(S32 offset) const
{
	if (mSegments.empty() || offset < 0 || offset >= getLength())
	{
		return -1;
	}
	else
	{
		S32 segidx, segoff;
		getSegmentAndOffset(offset, &segidx, &segoff);
		return segidx;
	}
}

void LLTextEditor::onMouseCaptureLost()
{
	endSelection();
}

void LLTextEditor::setOnScrollEndCallback(void (*callback)(void*),
										  void* userdata)
{
	mOnScrollEndCallback = callback;
	mOnScrollEndData = userdata;
	mScrollbar->setOnScrollEndCallback(callback, userdata);
}

void LLTextEditor::setKeystrokeCallback(void (*callback)(LLTextEditor*, void*),
										void* userdata)
{
	mKeystrokeCallback = callback;
	mKeystrokeData = userdata;
}

void LLTextEditor::setOnHandleKeyCallback(bool (*callback)(KEY, MASK,
														   LLTextEditor*,
														   void*),
										  void* userdata)
{
	mOnHandleKeyCallback = callback;
	mOnHandleKeyData = userdata;
}

///////////////////////////////////////////////////////////////////
// Hack for Notecards

bool LLTextEditor::importBuffer(const char* buffer, S32 length)
{
	std::istringstream instream(buffer);

	// Version 1 format:
	//		Linden text version 1\n
	//		{\n
	//			<EmbeddedItemList chunk>
	//			Text length <bytes without \0>\n
	//			<text without \0> (text may contain ext_char_values)
	//		}\n

	char tbuf[MAX_STRING];

	S32 version = 0;
	instream.getline(tbuf, MAX_STRING);
	if (1 != sscanf(tbuf, "Linden text version %d", &version))
	{
		llwarns << "Invalid Linden text file header " << llendl;
		return false;
	}

	if (version != 1)
	{
		llwarns << "Invalid Linden text file version: " << version << llendl;
		return false;
	}

	instream.getline(tbuf, MAX_STRING);
	if (sscanf(tbuf, "{"))
	{
		llwarns << "Invalid Linden text file format" << llendl;
		return false;
	}

	S32 text_len = 0;
	instream.getline(tbuf, MAX_STRING);
	if (sscanf(tbuf, "Text length %d", &text_len) != 1)
	{
		llwarns << "Invalid Linden text length field" << llendl;
		return false;
	}

	if (text_len > mMaxTextByteLength)
	{
		llwarns << "Invalid Linden text length: " << text_len << llendl;
		return false;
	}

	char* text = new char[text_len + 1];
	if (!text)
	{
		llerrs << "Memory allocation failure." << llendl;
		return false;
	}

	bool success = true;
	instream.get(text, text_len + 1, '\0');
	text[text_len] = '\0';
	if (text_len != (S32)strlen(text))
	{
		llwarns << llformat("Invalid text length: %d != %d ",
							strlen(text), text_len) << llendl;
		success = false;
	}

	instream.getline(tbuf, MAX_STRING);
	if (success && sscanf(tbuf, "}"))
	{
		llwarns << "Invalid Linden text file format: missing terminal }"
				<< llendl;
		success = false;
	}

	if (success)
	{
		// Actually set the text
		setText(text);
	}

	delete[] text;

	setCursorPos(mCursorPos);
	deselect();

	needsReflow();
	return success;
}

bool LLTextEditor::exportBuffer(std::string& buffer)
{
	std::ostringstream outstream(buffer);

	outstream << "Linden text version 1\n";
	outstream << "{\n";

	outstream << llformat("Text length %d\n", mWText.length());
	outstream << getText();
	outstream << "}\n";

	return true;
}

//////////////////////////////////////////////////////////////////////////
// LLTextSegment

LLTextSegment::LLTextSegment(S32 start)
:	mStart(start),
	mEnd(0),
	mToken(NULL),
	mIsDefault(false)
{
}

LLTextSegment::LLTextSegment(const LLStyleSP& style, S32 start, S32 end)
:	mStyle(style),
	mStart(start),
	mEnd(end),
	mToken(NULL),
	mIsDefault(false)
{
}

LLTextSegment::LLTextSegment(const LLColor4& color, S32 start, S32 end,
							 bool is_visible)
:	mStyle(new LLStyle(is_visible, color, LLStringUtil::null)),
	mStart(start),
	mEnd(end),
	mToken(NULL),
	mIsDefault(false)
{
}

LLTextSegment::LLTextSegment(const LLColor4& color, S32 start, S32 end)
:	mStyle(new LLStyle(true, color, LLStringUtil::null)),
	mStart(start),
	mEnd(end),
	mToken(NULL),
	mIsDefault(false)
{
}

LLTextSegment::LLTextSegment(const LLColor3& color, S32 start, S32 end)
:	mStyle(new LLStyle(true, color, LLStringUtil::null)),
	mStart(start),
	mEnd(end),
	mToken(NULL),
	mIsDefault(false)
{
}

bool LLTextSegment::getToolTip(std::string& msg) const
{
	if (mToken && !mToken->getToolTip().empty())
	{
		const LLWString& wmsg = mToken->getToolTip();
		msg = wstring_to_utf8str(wmsg);
		return true;
	}
	return false;
}

void LLTextSegment::dump() const
{
	llinfos << "Segment "
#if 0
			<< "(color: " << mColor.mV[VX] << ", " << mColor.mV[VY] << ", "
			<< mColor.mV[VZ] << ")"
#endif
			<< "[" << mStart << ", " << getEnd() << "]" << llendl;

}

//virtual
LLXMLNodePtr LLTextEditor::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_SIMPLE_TEXT_EDITOR_TAG);

	// Attributes

	node->createChild("max_length", true)->setIntValue(getMaxLength());
	node->createChild("embedded_items",
					  true)->setBoolValue(mAllowEmbeddedItems);
	node->createChild("font",
					  true)->setStringValue(LLFontGL::nameFromFont(mGLFont));
	node->createChild("word_wrap", true)->setBoolValue(mWordWrap);
	node->createChild("hide_scrollbar",
					  true)->setBoolValue(mHideScrollbarForShortDocs);

	addColorXML(node, mCursorColor, "cursor_color", "TextCursorColor");
	addColorXML(node, mFgColor, "text_color", "TextFgColor");
	addColorXML(node, mDefaultColor, "text_default_color", "TextDefaultColor");
	addColorXML(node, mReadOnlyFgColor, "text_readonly_color",
				"TextFgReadOnlyColor");
	addColorXML(node, mReadOnlyBgColor, "bg_readonly_color",
				"TextBgReadOnlyColor");
	addColorXML(node, mWriteableBgColor, "bg_writeable_color",
				"TextBgWriteableColor");
	addColorXML(node, mFocusBgColor, "bg_focus_color", "TextBgFocusColor");

	// Contents
 	node->setStringValue(getText());

	return node;
}

//static
LLView* LLTextEditor::fromXML(LLXMLNodePtr node, LLView* parent,
							  LLUICtrlFactory* factory)
{
	std::string name = "text_editor";
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	U32 max_text_length = 255;
	node->getAttributeU32("max_length", max_text_length);

	bool allow_embedded_items = false;
	node->getAttributeBool("embedded_items", allow_embedded_items);

	LLFontGL* font = LLView::selectFont(node);

	std::string text = node->getTextContents().substr(0, max_text_length - 1);

	LLTextEditor* text_editor = new LLTextEditor(name, rect, max_text_length,
												 text, font,
												 allow_embedded_items);

	text_editor->setTextEditorParameters(node);

	bool hide_scrollbar = false;
	node->getAttributeBool("hide_scrollbar",hide_scrollbar);
	text_editor->setHideScrollbarForShortDocs(hide_scrollbar);

	text_editor->initFromXML(node, parent);

	return text_editor;
}

void LLTextEditor::setTextEditorParameters(LLXMLNodePtr node)
{
	bool word_wrap = false;
	node->getAttributeBool("word_wrap", word_wrap);
	setWordWrap(word_wrap);

	node->getAttributeBool("show_line_numbers", mShowLineNumbers);

	node->getAttributeBool("track_bottom", mTrackBottom);

	// By default, spell check is enabled for text editors
	if (node->hasAttribute("spell_check"))
	{
		node->getAttributeBool("spell_check", mSpellCheck);
	}

	LLColor4 color;
	if (LLUICtrlFactory::getAttributeColor(node, "cursor_color", color))
	{
		setCursorColor(color);
	}
	if (LLUICtrlFactory::getAttributeColor(node, "text_color", color))
	{
		setFgColor(color);
	}
	if (LLUICtrlFactory::getAttributeColor(node, "text_readonly_color", color))
	{
		setReadOnlyFgColor(color);
	}
	if (LLUICtrlFactory::getAttributeColor(node, "bg_readonly_color", color))
	{
		setReadOnlyBgColor(color);
	}
	if (LLUICtrlFactory::getAttributeColor(node, "bg_writeable_color", color))
	{
		setWriteableBgColor(color);
	}
}

// Refactoring note: we may eventually want to replace this with std::regex
// or std::tokenizer capabilities since we have already fixed at least two
// JIRAs concerning logic issues associated with this function.
S32 LLTextEditor::findHTMLToken(const std::string& line, S32 pos,
								bool reverse) const
{
	static const std::string openers = " \t\n('\"[{<>";
	static const std::string closers = " \t\n)'\"]}><;";

	if (reverse)
	{
		for (S32 index = pos; index >= 0; --index)
		{
			char c = line[index];
			S32 m2 = openers.find(c);
			if (m2 >= 0)
			{
				return index + 1;
			}
		}
		return 0; // index is -1, we do not want to return that.
	}

	// Adjust the search slightly, to allow matching parenthesis inside the URL
	S32 len = line.length();
	S32 paren_count = 0;
	for (S32 index = pos; index < len; ++index)
	{
		char c = line[index];
		if (c == '(')
		{
			++paren_count;
		}
		else if (c == ')')
		{
			if (paren_count <= 0)
			{
				return index;
			}
			else
			{
				--paren_count;
			}
		}
		else
		{
			S32 m2 = closers.find(c);
			if (m2 >= 0)
			{
				return index;
			}
		}
	}
	return len;
}

bool LLTextEditor::findHTML(const std::string& line, S32* begin,
							S32* end) const
{
	static const std::string badneighbors =
			".,<>?';\"][}{=-+_)(*&^%$#@!~`\t\r\n\\";

	bool matched = false;

	size_t m1 = line.find("://", *end);
	if (m1 != std::string::npos) // Easy match.
	{
		*begin = findHTMLToken(line, m1, true);
		*end = findHTMLToken(line, m1, false);

		// Load_url only handles http and https so do not hilite ftp, smb, etc
		size_t m2 = line.substr(*begin, (m1 - *begin)).find("http");
		size_t m3 = line.substr(*begin, (m1 - *begin)).find("secondlife");

		if ((m2 != std::string::npos || m3 != std::string::npos) &&
			badneighbors.find(line.substr(m1 + 3, 1)) == std::string::npos)
		{
			matched = true;
		}
	}
#if 0
	// Matches things like secondlife.com (no http://) needs a whitelist to
	// really be effective.
	else	// Harder match.
	{
		m1 = line.find(".", *end);
		if (m1 != std::string::npos)
		{
			*end = findHTMLToken(line, m1, false);
			*begin = findHTMLToken(line, m1, true);

			m1 = line.rfind(".", *end);
			if ((*end - m1) > 2 && m1 > *begin)
			{
				size_t m2 = badneighbors.find(line.substr(m1 + 1, 1));
				size_t m3 = badneighbors.find(line.substr(m1 - 1, 1));
				if (m3 == std::string::npos && m2 == std::string::npos)
				{
					matched = true;
				}
			}
		}
	}
#endif

	if (matched)
	{
		std::string url = line.substr(*begin, *end - *begin);
		std::string slurl_id = "slurl.com/secondlife/";
		size_t strpos = url.find(slurl_id);
		if (strpos == std::string::npos)
		{
			slurl_id = "maps.secondlife.com/secondlife/";
			strpos = url.find(slurl_id);
			if (strpos == std::string::npos)
			{
				slurl_id = "secondlife://";
				strpos = url.find(slurl_id);
				if (strpos == std::string::npos)
				{
					slurl_id = "sl://";
					strpos = url.find(slurl_id);
				}
			}
		}
		if (strpos != std::string::npos)
		{
			strpos += slurl_id.length();

			while (url.find("/", strpos) == std::string::npos)
			{
				if ((size_t)(*end + 2) >= line.length() ||
					line.substr(*end, 1) != " ")
				{
					matched = false;
					break;
				}

				strpos = (*end + 1) - *begin;

				*end = findHTMLToken(line,(*begin + strpos), false);
				url = line.substr(*begin, *end - *begin);
			}
		}

	}

	if (!matched)
	{
		*begin = *end = 0;
	}

	return matched;
}

void LLTextEditor::updateAllowingLanguageInput()
{
	if (hasFocus() && !mReadOnly)
	{
		gWindowp->allowLanguageTextInput(this, true);
	}
	else
	{
		gWindowp->allowLanguageTextInput(this, false);
	}
}

// Preedit is managed off the undo/redo command stack.

bool LLTextEditor::hasPreeditString() const
{
	return mPreeditPositions.size() > 1;
}

void LLTextEditor::resetPreedit()
{
	if (hasPreeditString())
	{
		if (hasSelection())
		{
			llwarns << "Preedit and selection !" << llendl;
			deselect();
		}

		mCursorPos = mPreeditPositions.front();
		removeStringNoUndo(mCursorPos, mPreeditPositions.back() - mCursorPos);
		insertStringNoUndo(mCursorPos, mPreeditOverwrittenWString);

		mPreeditWString.clear();
		mPreeditOverwrittenWString.clear();
		mPreeditPositions.clear();

		// A call to updatePreedit should soon follow under a normal course of
		// operation, so we do not need to maintain internal variables such as
		// line start positions now.
	}
}

void LLTextEditor::updatePreedit(const LLWString& preedit_string,
								 const segment_lengths_t& preedit_segment_lengths,
								 const standouts_t& preedit_standouts,
								 S32 caret_position)
{
	// Just in case.
	if (mReadOnly)
	{
		return;
	}

	gWindowp->hideCursorUntilMouseMove();

	S32 insert_preedit_at = mCursorPos;

	mPreeditWString = preedit_string;
	mPreeditPositions.resize(preedit_segment_lengths.size() + 1);
	S32 position = insert_preedit_at;
	for (segment_lengths_t::size_type i = 0,
									  size = preedit_segment_lengths.size();
		 i < size; ++i)
	{
		mPreeditPositions[i] = position;
		position += preedit_segment_lengths[i];
	}
	mPreeditPositions.back() = position;

	if (gKeyboardp && gKeyboardp->getInsertMode() == LL_KIM_OVERWRITE)
	{
		mPreeditOverwrittenWString = getWSubString(insert_preedit_at,
												   mPreeditWString.length());
		removeStringNoUndo(insert_preedit_at, mPreeditWString.length());
	}
	else
	{
		mPreeditOverwrittenWString.clear();
	}
	insertStringNoUndo(insert_preedit_at, mPreeditWString);

	mPreeditStandouts = preedit_standouts;

	needsReflow();
	setCursorPos(insert_preedit_at + caret_position);

	// Update of the preedit should be caused by some key strokes.
	mKeystrokeTimer.reset();
}

bool LLTextEditor::getPreeditLocation(S32 query_offset, LLCoordGL* coord,
									  LLRect* bounds, LLRect* control) const
{
	if (control)
	{
		LLRect control_rect_screen;
		localRectToScreen(mTextRect, &control_rect_screen);
		LLUI::screenRectToGL(control_rect_screen, control);
	}

	S32 preedit_left_position, preedit_right_position;
	if (hasPreeditString())
	{
		preedit_left_position = mPreeditPositions.front();
		preedit_right_position = mPreeditPositions.back();
	}
	else
	{
		preedit_left_position = preedit_right_position = mCursorPos;
	}

	const S32 query = (query_offset >= 0 ? preedit_left_position + query_offset
										 : mCursorPos);
	if (query < preedit_left_position || query > preedit_right_position)
	{
		return false;
	}

	const S32 first_visible_line = mScrollbar->getDocPos();
	if (query < getLineStart(first_visible_line))
	{
		return false;
	}

	S32 current_line = first_visible_line;
	S32 current_line_start, current_line_end;
	while (true)
	{
		current_line_start = getLineStart(current_line);
		current_line_end = getLineStart(current_line + 1);
		if (query >= current_line_start && query < current_line_end)
		{
			break;
		}
		if (current_line_start == current_line_end)
		{
			// We have reached on the last line. The query position must be
			// here.
			break;
		}
		++current_line;
	}

	const llwchar* const text = mWText.c_str();
	const S32 line_height = ll_roundp(mGLFont->getLineHeight());

	if (coord)
	{
		const S32 query_x = mTextRect.mLeft +
							mGLFont->getWidth(text, current_line_start,
											  query - current_line_start,
											  mAllowEmbeddedItems);
		const S32 query_y = mTextRect.mTop -
							(current_line - first_visible_line) * line_height -
							line_height / 2;
		S32 query_screen_x, query_screen_y;
		localPointToScreen(query_x, query_y, &query_screen_x, &query_screen_y);
		LLUI::screenPointToGL(query_screen_x, query_screen_y, &coord->mX,
							  &coord->mY);
	}

	if (bounds)
	{
		S32 preedit_left = mTextRect.mLeft;
		if (preedit_left_position > current_line_start)
		{
			preedit_left += mGLFont->getWidth(text, current_line_start,
											  preedit_left_position -
											  current_line_start,
											  mAllowEmbeddedItems);
		}

		S32 preedit_right = mTextRect.mLeft;
		if (preedit_right_position < current_line_end)
		{
			preedit_right += mGLFont->getWidth(text, current_line_start,
											   preedit_right_position -
											   current_line_start,
											   mAllowEmbeddedItems);
		}
		else
		{
			preedit_right += mGLFont->getWidth(text, current_line_start,
											   current_line_end -
											   current_line_start,
											   mAllowEmbeddedItems);
		}

		const S32 preedit_top = mTextRect.mTop -
								(current_line - first_visible_line) *
								line_height;
		const S32 preedit_bottom = preedit_top - line_height;

		const LLRect preedit_rect_local(preedit_left, preedit_top,
										preedit_right, preedit_bottom);
		LLRect preedit_rect_screen;
		localRectToScreen(preedit_rect_local, &preedit_rect_screen);
		LLUI::screenRectToGL(preedit_rect_screen, bounds);
	}

	return true;
}

void LLTextEditor::getSelectionRange(S32* position, S32* length) const
{
	if (hasSelection())
	{
		*position = llmin(mSelectionStart, mSelectionEnd);
		*length = abs(mSelectionStart - mSelectionEnd);
	}
	else
	{
		*position = mCursorPos;
		*length = 0;
	}
}

void LLTextEditor::getPreeditRange(S32* position, S32* length) const
{
	if (hasPreeditString())
	{
		*position = mPreeditPositions.front();
		*length = mPreeditPositions.back() - mPreeditPositions.front();
	}
	else
	{
		*position = mCursorPos;
		*length = 0;
	}
}

void LLTextEditor::markAsPreedit(S32 position, S32 length)
{
	deselect();
	setCursorPos(position);
	if (hasPreeditString())
	{
		llwarns << "markAsPreedit invoked when hasPreeditString is true."
				<< llendl;
	}
	mPreeditWString = LLWString(mWText, position, length);
	if (length > 0)
	{
		mPreeditPositions.resize(2);
		mPreeditPositions[0] = position;
		mPreeditPositions[1] = position + length;
		mPreeditStandouts.resize(1);
		mPreeditStandouts[0] = false;
	}
	else
	{
		mPreeditPositions.clear();
		mPreeditStandouts.clear();
	}
	if (gKeyboardp && gKeyboardp->getInsertMode() == LL_KIM_OVERWRITE)
	{
		mPreeditOverwrittenWString = mPreeditWString;
	}
	else
	{
		mPreeditOverwrittenWString.clear();
	}
}

S32 LLTextEditor::getPreeditFontSize() const
{
	return ll_roundp(mGLFont->getLineHeight() * LLUI::sGLScaleFactor.mV[VY]);
}
