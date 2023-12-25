/**
 * @file lllineeditor.cpp
 * @brief LLLineEditor base class
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

// Text editor widget to let users enter a single line.

#include "linden_common.h"

#include "lllineeditor.h"

#include "llbutton.h"
#include "llclipboard.h"
#include "llcontrol.h"
#include "llfasttimer.h"
#include "llgl.h"
#include "llkeyboard.h"
#include "lllocale.h"
#include "llmenugl.h"
#include "llrect.h"
#include "llspellcheck.h"
#include "llstring.h"
#include "lltimer.h"
#include "lluictrlfactory.h"
#include "llwindow.h"

// gcc 13 sees array bound issues where tehre are none... HB
#if defined(GCC_VERSION) && GCC_VERSION >= 130000
# pragma GCC diagnostic ignored "-Warray-bounds"
# pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

//
// Constants
//

constexpr S32 UI_LINEEDITOR_CURSOR_THICKNESS = 2;
constexpr S32 UI_LINEEDITOR_H_PAD = 2;
constexpr S32 UI_LINEEDITOR_V_PAD = 1;
constexpr F32 CURSOR_FLASH_DELAY = 1.f;	// In seconds
constexpr S32 SCROLL_INCREMENT_ADD = 0;	// Make space for typing
constexpr S32 SCROLL_INCREMENT_DEL = 4;	// Make space for baskspacing
constexpr F32 AUTO_SCROLL_TIME = 0.05f;

constexpr F32 MARKER_BRIGHTNESS = 0.4f;
constexpr F32 STANDOUT_BRIGHTNESS = 0.6f;
constexpr S32 PREEDIT_BORDER = 1;

///////////////////////////////////////////////////////////////////////////////
// LLLineEditor class
///////////////////////////////////////////////////////////////////////////////

static const std::string LL_LINE_EDITOR_TAG = "line_editor";
static LLRegisterWidget<LLLineEditor> r06(LL_LINE_EDITOR_TAG);

//static
LLUIImagePtr LLLineEditor::sImage;

LLLineEditor::LLLineEditor(const std::string& name, const LLRect& rect,
						   const std::string& default_text,
						   const LLFontGL* font, S32 max_length_bytes,
						   void (*commit_callback)(LLUICtrl* caller,
												   void* user_data),
						   void (*keystroke_callback)(LLLineEditor* caller,
													  void* user_data),
						   void (*focus_lost_callback)(LLFocusableElement* caller,
													   void* user_data),
						   void* userdata,
						   LLLinePrevalidateFunc prevalidate_func,
						   LLViewBorder::EBevel border_bevel,
						   LLViewBorder::EStyle border_style,
						   S32 border_thickness)
:	LLEditMenuHandler(HAS_CONTEXT_MENU | HAS_CUSTOM),
	LLUICtrl(name, rect, true, commit_callback, userdata,
			 FOLLOWS_TOP | FOLLOWS_LEFT),
	mMaxLengthBytes(max_length_bytes),
	mCursorPos(0),
	mScrollHPos(0),
	mTextPadLeft(0),
	mTextPadRight(0),
	mCommitOnFocusLost(true),
	mRevertOnEsc(true),
	mKeystrokeCallback(keystroke_callback),
	mOnHandleKeyCallback(NULL),
	mOnHandleKeyData(NULL),
	mScrolledCallback(NULL),
	mScrolledCallbackData(NULL),
	mIsSelecting(false),
	mSelectionStart(0),
	mSelectionEnd(0),
	mLastSelectionX(-1),
	mLastSelectionY(-1),
	mLastSelectionStart(-1),
	mLastSelectionEnd(-1),
	mPrevalidateFunc(prevalidate_func),
	mCursorColor(LLUI::sTextCursorColor),
	mFgColor(LLUI::sTextFgColor),
	mReadOnlyFgColor(LLUI::sTextFgReadOnlyColor),
	mTentativeFgColor(LLUI::sTextFgTentativeColor),
	mWriteableBgColor(LLUI::sTextBgWriteableColor),
	mReadOnlyBgColor(LLUI::sTextBgReadOnlyColor),
	mFocusBgColor(LLUI::sTextBgFocusColor),
	mBorderThickness(border_thickness),
	mIgnoreArrowKeys(false),
	mIgnoreTab(true),
	mDrawAsterixes(false),
	mHandleEditKeysDirectly(false),
	mSelectAllonFocusReceived(false),
	mPassDelete(false),
	mReadOnly(false),
	mHaveHistory(false),
	mImage(sImage),
	mReplaceNewlinesWithSpaces(true),
	mSpellCheck(false)
{
	llassert(max_length_bytes > 0);

	// Initialize current history line iterator
	mCurrentHistoryLine = mLineHistory.begin();

	if (font)
	{
		mGLFont = font;
	}
	else
	{
		mGLFont = LLFontGL::getFontSansSerifSmall();
	}

	setFocusLostCallback(focus_lost_callback);

	setTextPadding(0, 0);

	mScrollTimer.reset();

	setText(default_text);

	setCursor(mText.length());

	// Scalable UI somehow made these rectangles off-by-one.
	// I don't know why. JC
	LLRect border_rect(0, getRect().getHeight() - 1,
					   getRect().getWidth() - 1, 0);
	mBorder = new LLViewBorder("line ed border", border_rect, border_bevel,
							   border_style, mBorderThickness);
	addChild(mBorder);
	mBorder->setFollows(FOLLOWS_LEFT | FOLLOWS_RIGHT | FOLLOWS_TOP |
						FOLLOWS_BOTTOM);

	if (!sImage)
	{
		sImage = LLUI::getUIImage("sm_rounded_corners_simple.tga");
	}
	mImage = sImage;

	mShowMisspelled = LLSpellCheck::getInstance()->getShowMisspelled();
}

LLLineEditor::~LLLineEditor()
{
	mCommitOnFocusLost = false;
	gFocusMgr.releaseFocusIfNeeded(this);
}

void LLLineEditor::onFocusReceived()
{
	grabMenuHandler();
	LLUICtrl::onFocusReceived();
	updateAllowingLanguageInput();
}

void LLLineEditor::onFocusLost()
{
	// The call to updateAllowLanguageInput() when loosing the keyboard focus
	// *may* indirectly invoke handleUnicodeCharHere(), so it must be called
	// before onCommit.
	updateAllowingLanguageInput();

	if (mCommitOnFocusLost && mText.getString() != mPrevText)
	{
		onCommit();
	}

	releaseMenuHandler();

	gWindowp->showCursorFromMouseMove();

	LLUICtrl::onFocusLost();
}

void LLLineEditor::onCommit()
{
	// Put current line into the line history
	updateHistory();

	setControlValue(getValue());

	LLUICtrl::onCommit();
	resetDirty();

	selectAll();
}

// line history support
void LLLineEditor::updateHistory()
{
	// On history enabled line editors, remember committed line and reset
	// current history line number. Be sure only to remember lines that are not
	// empty and that are different from the last on the list.
	if (mHaveHistory && getLength())
	{
		if (!mLineHistory.empty())
		{
			// When not empty, last line of history should always be blank.
			if (mLineHistory.back().empty())
			{
				// discard the empty line
				mLineHistory.pop_back();
			}
			else
			{
				llwarns << "Last line of history was not blank." << llendl;
			}
		}

		// Add text to history, ignoring duplicates
		if (mLineHistory.empty() || getText() != mLineHistory.back())
		{
			mLineHistory.emplace_back(getText());
		}

		// Restore the blank line and set mCurrentHistoryLine to point at it
		mLineHistory.emplace_back("");
		mCurrentHistoryLine = mLineHistory.end() - 1;
	}
}

void LLLineEditor::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLUICtrl::reshape(width, height, called_from_parent);

	// For clamping side-effect:
	setTextPadding(mTextPadLeft, mTextPadRight);
	setCursor(mCursorPos);
}

void LLLineEditor::setEnabled(bool enabled)
{
	mReadOnly = !enabled;
	setTabStop(!mReadOnly);
	updateAllowingLanguageInput();
}

void LLLineEditor::setMaxTextLength(S32 max_text_length)
{
	mMaxLengthBytes = llmax(0, max_text_length);
}

void LLLineEditor::setTextPadding(S32 left, S32 right)
{
	mTextPadLeft = llclamp(left, 0, getRect().getWidth());
	mTextPadRight = llclamp(right, 0, getRect().getWidth());
	mMinHPixels = UI_LINEEDITOR_H_PAD + mTextPadLeft;
	mMaxHPixels = getRect().getWidth() - mMinHPixels - mTextPadRight;
}

void LLLineEditor::setText(const std::string& new_text)
{
	// If new text is identical, do not copy and do not move insertion point
	if (mText.getString() == new_text)
	{
		return;
	}

	// Check to see if entire field is selected.
	S32 len = mText.length();
	bool all_selected = len > 0 &&
						((mSelectionStart == 0 && mSelectionEnd == len) ||
						 (mSelectionStart == len && mSelectionEnd == 0));

	// Do safe truncation so we do not split multi-byte characters. Also
	// consider entire string selected when mSelectAllonFocusReceived is set on
	// an empty, focused line editor.
	all_selected = all_selected ||
				   (len == 0 && hasFocus() && mSelectAllonFocusReceived);

	std::string truncated_utf8 = new_text;
	if (truncated_utf8.size() > (U32)mMaxLengthBytes)
	{
		truncated_utf8 = utf8str_truncate(new_text, mMaxLengthBytes);
	}
	mText.assign(truncated_utf8);

	if (all_selected)
	{
		// ...keep whole thing selected
		selectAll();
	}
	else
	{
		// Try to preserve insertion point, but deselect text
		deselect();
	}
	setCursor(llmin((S32)mText.length(), mCursorPos));

	// Set current history line to end of history.
	mCurrentHistoryLine = mLineHistory.end() - 1;

	mPrevText = mText;
}

// Picks a new cursor position based on the actual screen size of text being
// drawn.
S32 LLLineEditor::calculateCursorFromMouse(S32 local_mouse_x)
{
	const llwchar* wtext = mText.getWString().c_str();
	LLWString asterix_text;
	if (mDrawAsterixes)
	{
		for (S32 i = 0, len = mText.length(); i < len; ++i)
		{
			asterix_text += (llwchar)0x2022L;
		}
		wtext = asterix_text.c_str();
	}

	return mScrollHPos +
		   mGLFont->charFromPixelOffset(wtext, mScrollHPos,
										(F32)(local_mouse_x - mMinHPixels),
										// min-max range is inclusive
										(F32)(mMaxHPixels - mMinHPixels + 1));

}

void LLLineEditor::setCursorAtLocalPos(S32 local_mouse_x)
{
	setCursor(calculateCursorFromMouse(local_mouse_x));
}

void LLLineEditor::setCursor(S32 pos)
{
	S32 old_cursor_pos = mCursorPos;
	S32 old_scroll_pos = mScrollHPos;
	mCursorPos = llclamp(pos, 0, mText.length());

	S32 pixels_after_scroll = findPixelNearestPos();
	if (pixels_after_scroll > mMaxHPixels)
	{
		const llwchar* wtext = mText.getWString().c_str();
		LLWString asterix_text;
		if (mDrawAsterixes)
		{
			for (S32 i = 0, len = mText.length(); i < len; ++i)
			{
				asterix_text += (llwchar)0x2022L;
			}
			wtext = asterix_text.c_str();
		}
		std::string saved_text;
		if (mDrawAsterixes)
		{
			saved_text = mText.getString();
			std::string text;
			for (S32 i = 0, len = mText.length(); i < len; ++i)
			{
				text += '*';
			}
			mText = text;
		}

		S32 width_chars_to_left = mGLFont->getWidth(wtext, 0, mScrollHPos);
		S32 last_visible_char =
			mGLFont->maxDrawableChars(wtext,
									  llmax(0.f, (F32)(mMaxHPixels -
													   mMinHPixels +
													   width_chars_to_left)));
		S32 min_scroll =
			mGLFont->firstDrawableChar(wtext,
									   (F32)(mMaxHPixels - mMinHPixels -
											 UI_LINEEDITOR_CURSOR_THICKNESS -
											 UI_LINEEDITOR_H_PAD),
									   mText.length(), mCursorPos);
		if (old_cursor_pos == last_visible_char)
		{
			mScrollHPos = llmin(mText.length(),
								llmax(min_scroll,
									  mScrollHPos + SCROLL_INCREMENT_ADD));
		}
		else
		{
			mScrollHPos = min_scroll;
		}
	}
	else if (mCursorPos < mScrollHPos)
	{
		if (old_cursor_pos == mScrollHPos)
		{
			mScrollHPos = llmax(0,
								llmin(mCursorPos,
									  mScrollHPos - SCROLL_INCREMENT_DEL));
		}
		else
		{
			mScrollHPos = mCursorPos;
		}
	}

	if (old_scroll_pos == 0 && mScrollHPos != 0 && mScrolledCallback)
	{
		mScrolledCallback(this, mScrolledCallbackData);
	}
}

void LLLineEditor::setCursorToEnd()
{
	setCursor(mText.length());
	deselect();
}

void LLLineEditor::resetScrollPosition()
{
	mScrollHPos = 0;
	// Make sure cursor says in visible range
	setCursor(getCursor());
}


void LLLineEditor::deselect()
{
	mSelectionStart = 0;
	mSelectionEnd = 0;
	mIsSelecting = false;
}

void LLLineEditor::startSelection()
{
	mIsSelecting = true;
	mSelectionStart = mCursorPos;
	mSelectionEnd = mCursorPos;
}

void LLLineEditor::endSelection()
{
	if (mIsSelecting)
	{
		mIsSelecting = false;
		mSelectionEnd = mCursorPos;
	}
}

void LLLineEditor::selectAll()
{
	mSelectionStart = mText.length();
	mSelectionEnd = 0;
	setCursor(mSelectionEnd);
	mIsSelecting = true;
}

void LLLineEditor::spellCorrect(void* data)
{
	SpellMenuBind* menu_bind = (SpellMenuBind*)data;
	LLLineEditor* line = menu_bind->mOrigin;
	if (menu_bind && line)
	{
		LL_DEBUGS("SpellCheck") << menu_bind->mMenuItem->getName()
								<< " : " << menu_bind->mOrigin->getName()
								<< " : " << menu_bind->mWord << LL_ENDL;
		line->spellReplace(menu_bind);
		// Make it update:
		line->mKeystrokeTimer.reset();
		line->mPrevSpelledText.erase();
	}
}

void LLLineEditor::spellShow(void* data)
{
	SpellMenuBind* menu_bind = (SpellMenuBind*)data;
	LLLineEditor* line = menu_bind->mOrigin;
	if (menu_bind && line)
	{
		line->mShowMisspelled = (menu_bind->mWord == "Show Misspellings");
		// Make it update:
		line->mKeystrokeTimer.reset();
		line->mPrevSpelledText.erase();
	}
}

void LLLineEditor::spellAdd(void* data)
{
	SpellMenuBind* menu_bind = (SpellMenuBind*)data;
	LLLineEditor* line = menu_bind->mOrigin;
	if (menu_bind && line)
	{
		LLSpellCheck::getInstance()->addToCustomDictionary(menu_bind->mWord);
		// Make it update:
		line->mKeystrokeTimer.reset();
		line->mPrevSpelledText.erase();
	}
}

void LLLineEditor::spellIgnore(void* data)
{
	SpellMenuBind* menu_bind = (SpellMenuBind*)data;
	LLLineEditor* line = menu_bind->mOrigin;
	if (menu_bind && line)
	{
		LLSpellCheck::getInstance()->addToIgnoreList(menu_bind->mWord);
		// Make it update:
		line->mKeystrokeTimer.reset();
		line->mPrevSpelledText.erase();
	}
}

std::vector<S32> LLLineEditor::getMisspelledWordsPositions()
{
	std::vector<S32> bad_words_pos;
    const LLWString& text = mText.getWString();
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
			// Do not bother for 2 or less characters words
			if (true_end > word_start + 2)
			{
				std::string part(text.begin(), text.end());
				selected_word = part.substr(word_start, true_end - word_start);

				if (!LLSpellCheck::getInstance()->checkSpelling(selected_word))
				{
					// Misspelled word here
					bad_words_pos.emplace_back(word_start);
					bad_words_pos.emplace_back(true_end);
				}
			}
		}
		++word_end;
	}

	return bad_words_pos;
}

bool LLLineEditor::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	setFocus(true);

	if (mSelectionEnd == 0 && mSelectionStart == mText.length())
	{
		// If everything is selected, handle this as a normal click to change
		// insertion point
		handleMouseDown(x, y, mask);
	}
	else
	{
		const LLWString& wtext = mText.getWString();

		bool do_select_all = true;

		// Select the word we're on
		if (LLWStringUtil::isPartOfWord(wtext[mCursorPos]))
		{
			S32 old_selection_start = mLastSelectionStart;
			S32 old_selection_end = mLastSelectionEnd;

			// Select word the cursor is over
			while (mCursorPos > 0 &&
				   LLWStringUtil::isPartOfWord(wtext[mCursorPos - 1]))
			{	// Find the start of the word
				--mCursorPos;
			}
			startSelection();

			while (mCursorPos < (S32)wtext.length() &&
				   LLWStringUtil::isPartOfWord(wtext[mCursorPos]))
			{	// Find the end of the word
				++mCursorPos;
			}
			mSelectionEnd = mCursorPos;

			// If nothing changed, then the word was already selected. Select
			// the whole line.
			do_select_all = old_selection_start == mSelectionStart &&
							old_selection_end == mSelectionEnd;
		}

		if (do_select_all)
		{
			selectAll();
		}
	}

	// We do not want handleMouseUp() to "finish" the selection (and thereby
	// set mSelectionEnd to where the mouse is), so we finish the selection
	// here.
	mIsSelecting = false;

	// Delay cursor flashing
	mKeystrokeTimer.reset();

	// Take selection to 'primary' clipboard
	updatePrimary();

	return true;
}

bool LLLineEditor::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Check first whether the "clear search" button wants to deal with this.
	if (childrenHandleMouseDown(x, y, mask))
	{
		return true;
	}
	if (mSelectAllonFocusReceived && gFocusMgr.getKeyboardFocus() != this)
	{
		setFocus(true);
	}
	else
	{
		mLastSelectionStart = -1;

		setFocus(true);

		if (mask & MASK_SHIFT)
		{
			// Handle selection extension
			S32 old_cursor_pos = mCursorPos;
			setCursorAtLocalPos(x);

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
			// Save selection for word/line selecting on double-click
			mLastSelectionStart = mSelectionStart;
			mLastSelectionEnd = mSelectionEnd;

			// Move cursor and deselect for regular click
			setCursorAtLocalPos(x);
			deselect();
			startSelection();
		}

		gFocusMgr.setMouseCapture(this);
	}

	// delay cursor flashing
	mKeystrokeTimer.reset();

	return true;
}

bool LLLineEditor::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
	setFocus(true);
	if (canPastePrimary())
	{
		setCursorAtLocalPos(x);
		pastePrimary();
	}
	return true;
}

bool LLLineEditor::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	setFocus(true);

	S32 word_start = 0;
	S32 word_len = 0;
	S32 pos = calculateCursorFromMouse(x);

	// If the context menu has not yet been created for this editor, this call
	// will create it now. HB
	LLMenuGL* menu = createContextMenu();
	if (menu)
	{
		SpellMenuBind* menu_bind;
		LLMenuItemCallGL* menu_item;

		// Remove old suggestions
		for (S32 i = 0, count = mSuggestionMenuItems.size(); i < count; ++i)
		{
			menu_bind = mSuggestionMenuItems[i];
			if (menu_bind)
			{
				menu_item = menu_bind->mMenuItem;
				menu->remove(menu_item);
				menu_item->die();
				//delete menu_bind->mMenuItem;
				//menu_bind->mMenuItem = NULL;
				delete menu_bind;
			}
		}
		mSuggestionMenuItems.clear();

		LLSpellCheck* checker = NULL;
		// Not read-only, spell_check="true" in XUI and spell checking enabled
		bool spell_check = !mReadOnly && mSpellCheck;
		if (spell_check)
		{
			checker = LLSpellCheck::getInstance();
			spell_check = checker->getSpellCheck();
		}
		menu->setItemVisible("spell_sep", spell_check);
		if (spell_check)
		{
			// search for word matches
			bool is_word_part = getWordBoundriesAt(pos, &word_start,
												   &word_len);
			if (is_word_part)
			{
				const LLWString& text = mText.getWString();
				std::string part(text.begin(), text.end());
				std::string selected_word = part.substr(word_start, word_len);
				if (!checker->checkSpelling(selected_word))
				{
					// misspelled word here
					std::vector<std::string> suggestions;
					S32 count = checker->getSuggestions(selected_word,
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
													 spellIgnore,
													 NULL, menu_bind);
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

bool LLLineEditor::handleHover(S32 x, S32 y, MASK mask)
{
	// Check first whether the "clear search" button wants to deal with this.
	if (!hasMouseCapture())
	{
		if (childrenHandleHover(x, y, mask) != NULL)
		{
			return true;
		}
	}

	bool handled = false;

	if (hasMouseCapture() && mIsSelecting)
	{
		if (x != mLastSelectionX || y != mLastSelectionY)
		{
			mLastSelectionX = x;
			mLastSelectionY = y;
		}
		// Scroll if mouse cursor outside of bounds
		if (mScrollTimer.hasExpired())
		{
			S32 increment = ll_roundp(mScrollTimer.getElapsedTimeF32() /
									  AUTO_SCROLL_TIME);
			mScrollTimer.reset();
			mScrollTimer.setTimerExpirySec(AUTO_SCROLL_TIME);
			if (x < mMinHPixels && mScrollHPos > 0)
			{
				// Scroll to the left
				mScrollHPos = llclamp(mScrollHPos - increment, 0,
									  mText.length());
			}
			else if (x > mMaxHPixels && mCursorPos < (S32)mText.length())
			{
				// If scrolling one pixel would make a difference...
				S32 pixels_after_scrolling_one_char = findPixelNearestPos(1);
				if (pixels_after_scrolling_one_char >= mMaxHPixels)
				{
					// ...scroll to the right
					mScrollHPos = llclamp(mScrollHPos + increment, 0,
										  mText.length());
				}
			}
		}

		setCursorAtLocalPos(x);
		mSelectionEnd = mCursorPos;

		// Delay cursor flashing
		mKeystrokeTimer.reset();

		gWindowp->setCursor(UI_CURSOR_IBEAM);
		LL_DEBUGS("UserInput") << "hover handled by " << getName()
							   << " (active)" << LL_ENDL;
		handled = true;
	}

	if (!handled)
	{
		gWindowp->setCursor(UI_CURSOR_IBEAM);
		LL_DEBUGS("UserInput") << "hover handled by " << getName()
							   << " (inactive)" << LL_ENDL;
		handled = true;
	}

	return handled;
}

bool LLLineEditor::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
		handled = true;
	}

	// Check first whether the "clear search" button wants to deal with this.
	if (!handled && childrenHandleMouseUp(x, y, mask) != NULL)
	{
		return true;
	}

	if (mIsSelecting)
	{
		setCursorAtLocalPos(x);
		mSelectionEnd = mCursorPos;

		handled = true;
	}

	if (handled)
	{
		// Delay cursor flashing
		mKeystrokeTimer.reset();

		// Take selection to 'primary' clipboard
		updatePrimary();
	}

	return handled;
}

// Remove a single character from the text
void LLLineEditor::removeChar()
{
	if (mCursorPos > 0)
	{
		mText.erase(mCursorPos - 1, 1);
		setCursor(mCursorPos - 1);
	}
	else
	{
		reportBadKeystroke();
	}
}

void LLLineEditor::addChar(llwchar uni_char)
{
	llwchar new_c = uni_char;
	if (hasSelection())
	{
		deleteSelection();
	}
	else if (gKeyboardp && gKeyboardp->getInsertMode() == LL_KIM_OVERWRITE)
	{
		mText.erase(mCursorPos, 1);
	}

	S32 cur_bytes = mText.getString().size();
	S32 new_bytes = wchar_utf8_length(new_c);

	// Check byte length limit
	if (new_bytes + cur_bytes <= mMaxLengthBytes)
	{
		// Will we need to scroll ?
		LLWString w_buf;
		w_buf.assign(1, new_c);

		mText.insert(mCursorPos, w_buf);
		setCursor(mCursorPos + 1);
	}
	else
	{
		reportBadKeystroke();
	}

	gWindowp->hideCursorUntilMouseMove();
}

// Extends the selection box to the new cursor position
void LLLineEditor::extendSelection(S32 new_cursor_pos)
{
	if (!mIsSelecting)
	{
		startSelection();
	}

	setCursor(new_cursor_pos);
	mSelectionEnd = mCursorPos;
}

void LLLineEditor::setSelection(S32 start, S32 end)
{
	// JC, yes, this seems odd, but I think you have to presume a selection
	// dragged from the end towards the start.
	S32 len = mText.length();
	mSelectionStart = llclamp(end, 0, len);
	mSelectionEnd = llclamp(start, 0, len);
	mIsSelecting = true;
	setCursor(start);
}

void LLLineEditor::setDrawAsterixes(bool b)
{
	mDrawAsterixes = b;
	updateAllowingLanguageInput();
}

S32 LLLineEditor::prevWordPos(S32 cursor_pos) const
{
	const LLWString& wtext = mText.getWString();
	while (cursor_pos > 0 && wtext[cursor_pos - 1] == ' ')
	{
		--cursor_pos;
	}
	while (cursor_pos > 0 &&
		   LLWStringUtil::isPartOfWord(wtext[cursor_pos - 1]))
	{
		--cursor_pos;
	}
	return cursor_pos;
}

S32 LLLineEditor::nextWordPos(S32 cursor_pos) const
{
	const LLWString& wtext = mText.getWString();
	while (cursor_pos < getLength() &&
		   LLWStringUtil::isPartOfWord(wtext[cursor_pos]))
	{
		++cursor_pos;
	}
	while (cursor_pos < getLength() && wtext[cursor_pos] == ' ')
	{
		++cursor_pos;
	}
	return cursor_pos;
}

bool LLLineEditor::getWordBoundriesAt(S32 at, S32* word_begin,
									  S32* word_length) const
{
	const LLWString& wtext = mText.getWString();
	S32 pos = at;
	S32 start;
	if (LLWStringUtil::isPartOfLexicalWord(wtext[pos]))
	{
		while (pos > 0 && LLWStringUtil::isPartOfLexicalWord(wtext[pos - 1]))
		{
			--pos;
		}
		if (wtext[pos] == L'\'')
		{
			// Do not count "'" at the start of a word
			++pos;
		}
		start = pos;
		while (pos < (S32)wtext.length() &&
			   LLWStringUtil::isPartOfLexicalWord(wtext[pos]))
		{
			++pos;
		}
		if (wtext[pos - 1] == L'\'')
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

void LLLineEditor::spellReplace(SpellMenuBind* data)
{
	if (data)
	{
		S32 length = data->mWordPositionEnd - data->mWordPositionStart;
		mText.erase(data->mWordPositionStart, length);
		insert(data->mWord, data->mWordPositionStart);
		mCursorPos += data->mWord.length() - length;
	}
}

void LLLineEditor::insert(std::string what, S32 where)
{
	LLLineEditorRollback rollback(this);
	LLWString clean_string(utf8str_to_wstring(what));
	LLWStringUtil::replaceTabsWithSpaces(clean_string, 4);
	mText.insert(where, clean_string);
	// See if we should move over the cursor acordingly. Validate new string
	// and rollback if needed.
	if (mPrevalidateFunc && !mPrevalidateFunc(mText.getWString()))
	{
		rollback.doRollback(this);
		reportBadKeystroke();
	}
	else if (mKeystrokeCallback)
	{
		mKeystrokeCallback(this, mCallbackUserData);
	}
}

bool LLLineEditor::handleSelectionKey(KEY key, MASK mask)
{
	bool handled = false;

	if (mask & MASK_SHIFT)
	{
		handled = true;

		switch (key)
		{
			case KEY_LEFT:
				if (mCursorPos > 0)
				{
					S32 cursor_pos = mCursorPos - 1;
					if (mask & MASK_CONTROL)
					{
						cursor_pos = prevWordPos(cursor_pos);
					}
					extendSelection(cursor_pos);
				}
				else
				{
					reportBadKeystroke();
				}
				break;

			case KEY_RIGHT:
				if (mCursorPos < mText.length())
				{
					S32 cursor_pos = mCursorPos + 1;
					if (mask & MASK_CONTROL)
					{
						cursor_pos = nextWordPos(cursor_pos);
					}
					extendSelection(cursor_pos);
				}
				else
				{
					reportBadKeystroke();
				}
				break;

			case KEY_PAGE_UP:
			case KEY_HOME:
				extendSelection(0);
				break;

			case KEY_PAGE_DOWN:
			case KEY_END:
			{
				S32 len = mText.length();
				if (len)
				{
					extendSelection(len);
				}
				break;
			}

			default:
				handled = false;
		}
	}

	if (!handled && mHandleEditKeysDirectly && (MASK_CONTROL & mask) &&
		key == 'A')
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

	if (handled)
	{
		// take selection to 'primary' clipboard
		updatePrimary();
	}

	return handled;
}

void LLLineEditor::deleteSelection()
{
	if (!mReadOnly && hasSelection())
	{
		S32 left_pos = llmin(mSelectionStart, mSelectionEnd);
		S32 selection_length = abs(mSelectionStart - mSelectionEnd);

		mText.erase(left_pos, selection_length);
		deselect();
		setCursor(left_pos);

		// Force spell-check update:
		mKeystrokeTimer.reset();
		mPrevSpelledText.erase();
	}
}

bool LLLineEditor::canCut() const
{
	return !mReadOnly && !mDrawAsterixes && hasSelection();
}

// Cut selection to clipboard
void LLLineEditor::cut()
{
	if (canCut())
	{
		// Prepare for possible rollback
		LLLineEditorRollback rollback(this);

		S32 left_pos = llmin(mSelectionStart, mSelectionEnd);
		S32 length = abs(mSelectionStart - mSelectionEnd);
		gClipboard.copyFromSubstring(mText.getWString(), left_pos, length);
		deleteSelection();

		// Validate new string and rollback the if needed.
		bool need_to_rollback = mPrevalidateFunc &&
								!mPrevalidateFunc(mText.getWString());
		if (need_to_rollback)
		{
			rollback.doRollback(this);
			reportBadKeystroke();
		}
		else if (mKeystrokeCallback)
		{
			mKeystrokeCallback(this, mCallbackUserData);
		}

		// Force spell-check update:
		mKeystrokeTimer.reset();
		mPrevSpelledText.erase();
	}
}

bool LLLineEditor::canCopy() const
{
	return !mDrawAsterixes && hasSelection();
}

// Copy selection to clipboard
void LLLineEditor::copy()
{
	if (canCopy())
	{
		S32 left_pos = llmin(mSelectionStart, mSelectionEnd);
		S32 length = abs(mSelectionStart - mSelectionEnd);
		gClipboard.copyFromSubstring(mText.getWString(), left_pos, length);

		// Force spell-check update:
		mKeystrokeTimer.reset();
		mPrevSpelledText.erase();
	}
}

bool LLLineEditor::canPaste() const
{
	return !mReadOnly && gClipboard.canPasteString();
}

void LLLineEditor::paste()
{
	bool is_primary = false;
	pasteHelper(is_primary);
}

void LLLineEditor::pastePrimary()
{
	bool is_primary = true;
	pasteHelper(is_primary);
}

// Paste from primary (is_primary==true) or clipboard (is_primary==false)
void LLLineEditor::pasteHelper(bool is_primary)
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

	if (can_paste_it)
	{
		LLWString paste;
		if (is_primary)
		{
			paste = gClipboard.getPastePrimaryWString();
		}
		else
		{
			paste = gClipboard.getPasteWString();
		}

		if (!paste.empty())
		{
			// Prepare for possible rollback
			LLLineEditorRollback rollback(this);

			// Delete any selected characters
			if (!is_primary && hasSelection())
			{
				deleteSelection();
			}

			// Clean up string (replace tabs and returns and remove characters
			// that our fonts do not support)
			LLWString clean_string(paste);
			LLWStringUtil::replaceTabsWithSpaces(clean_string, 1);
			// Note: character 182 is the paragraph character
			LLWStringUtil::replaceChar(clean_string, '\n',
									   mReplaceNewlinesWithSpaces ? ' '
																  : 182);

			// Insert the string

			// Check to see that the size is not going to be larger than the
			// max number of bytes
			U32 available_bytes = mMaxLengthBytes - wstring_utf8_length(mText);

			if (available_bytes < (U32)wstring_utf8_length(clean_string))
			{
				// Does not all fit
				llwchar current_symbol = clean_string[0];
				U32 wchars_that_fit = 0;
				U32 total_bytes = wchar_utf8_length(current_symbol);

				// Loop over the "wide" characters (symbols) and check to see
				// how large (in bytes) each symbol is.
				while (total_bytes <= available_bytes)
				{
					// While we still have available bytes "accept" the current
					// symbol and check the size of the next one
					current_symbol = clean_string[++wchars_that_fit];
					total_bytes += wchar_utf8_length(current_symbol);
				}
				// Truncate the clean string at the limit of what will fit
				clean_string = clean_string.substr(0, wchars_that_fit);
				reportBadKeystroke();
			}

			mText.insert(mCursorPos, clean_string);
			setCursor(mCursorPos + (S32)clean_string.length());
			deselect();

			// Validate new string and rollback if needed.
			if (mPrevalidateFunc && !mPrevalidateFunc(mText.getWString()))
			{
				rollback.doRollback(this);
				reportBadKeystroke();
			}
			else if (mKeystrokeCallback)
			{
				mKeystrokeCallback(this, mCallbackUserData);
			}
		}

		// Force spell-check update:
		mKeystrokeTimer.reset();
		mPrevSpelledText.erase();
	}
}

// Copy selection to primary
void LLLineEditor::copyPrimary()
{
	if (canCopy())
	{
		S32 left_pos = llmin(mSelectionStart, mSelectionEnd);
		S32 length = abs(mSelectionStart - mSelectionEnd);
		gClipboard.copyFromPrimarySubstring(mText.getWString(), left_pos,
											length);

		// Force spell-check update:
		mKeystrokeTimer.reset();
		mPrevSpelledText.erase();
	}
}

bool LLLineEditor::canPastePrimary() const
{
	return !mReadOnly && gClipboard.canPastePrimaryString();
}

void LLLineEditor::updatePrimary()
{
	if (canCopy())
	{
		copyPrimary();
	}
}

bool LLLineEditor::handleSpecialKey(KEY key, MASK mask)
{
	bool handled = false;

	switch (key)
	{
		case KEY_INSERT:
			if (mask == MASK_NONE && gKeyboardp)
			{
				gKeyboardp->toggleInsertMode();
			}
			handled = true;
			break;

		case KEY_BACKSPACE:
			if (!mReadOnly)
			{
				if (hasSelection())
				{
					deleteSelection();
				}
				else if (mCursorPos > 0)
				{
					removeChar();
				}
				else
				{
					reportBadKeystroke();
				}
			}
			handled = true;
			break;

		case KEY_PAGE_UP:
		case KEY_HOME:
			if (!mIgnoreArrowKeys)
			{
				setCursor(0);
				handled = true;
			}
			break;

		case KEY_PAGE_DOWN:
		case KEY_END:
			if (!mIgnoreArrowKeys)
			{
				S32 len = mText.length();
				if (len)
				{
					setCursor(len);
				}
				handled = true;
			}
			break;

		case KEY_LEFT:
			if (mIgnoreArrowKeys && mask == MASK_NONE)
			{
				break;
			}
			if ((mask & MASK_ALT) == 0)
			{
				if (hasSelection())
				{
					setCursor(llmin(mCursorPos - 1, mSelectionStart,
									mSelectionEnd));
				}
				else if (mCursorPos > 0)
				{
					S32 cursor_pos = mCursorPos - 1;
					if (mask & MASK_CONTROL)
					{
						cursor_pos = prevWordPos(cursor_pos);
					}
					setCursor(cursor_pos);
				}
				else
				{
					reportBadKeystroke();
				}
				handled = true;
			}
			break;

		case KEY_RIGHT:
			if (mIgnoreArrowKeys && mask == MASK_NONE)
			{
				break;
			}
			if ((mask & MASK_ALT) == 0)
			{
				if (hasSelection())
				{
					setCursor(llmax(mCursorPos + 1, mSelectionStart,
									mSelectionEnd));
				}
				else if (mCursorPos < mText.length())
				{
					S32 cursor_pos = mCursorPos + 1;
					if (mask & MASK_CONTROL)
					{
						cursor_pos = nextWordPos(cursor_pos);
					}
					setCursor(cursor_pos);
				}
				else
				{
					reportBadKeystroke();
				}
				handled = true;
			}
			break;

			// handle ctrl-uparrow if we have a history enabled line editor.
		case KEY_UP:
			if (mHaveHistory && mask == MASK_CONTROL)
			{
				if (mCurrentHistoryLine > mLineHistory.begin())
				{
					mText.assign(*(--mCurrentHistoryLine));
					setCursor(llmin((S32)mText.length(), mCursorPos));
				}
				else
				{
					reportBadKeystroke();
				}
				handled = true;
			}
			break;

		// handle ctrl-downarrow if we have a history enabled line editor
		case KEY_DOWN:
			if (mHaveHistory  && mask == MASK_CONTROL)
			{
				if (!mLineHistory.empty() &&
					mCurrentHistoryLine < mLineHistory.end() - 1)
				{
					mText.assign(*(++mCurrentHistoryLine));
					setCursor(llmin((S32)mText.length(), mCursorPos));
				}
				else
				{
					reportBadKeystroke();
				}
				handled = true;
			}
			break;

		case KEY_RETURN:
			// store sent line in history
			updateHistory();
			break;

		case KEY_ESCAPE:
	    	if (mask == MASK_NONE && mRevertOnEsc &&
				mText.getString() != mPrevText)
			{
				setText(mPrevText);
				// Note, do not set handled, still want to loose focus (would
				// not commit because text is now unchanged)
			}
			break;

		default:
			break;
	}

	if (!handled && mHandleEditKeysDirectly)
	{
		// Standard edit keys (Ctrl-X, Delete, etc,) are handled here instead
		// of routed by the menu system.
		if (key == KEY_DELETE)
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
			if (key == 'C')
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
			else if (key == 'V')
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
			else if (key == 'X')
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
	}

	return handled;
}

bool LLLineEditor::handleKeyHere(KEY key, MASK mask)
{
	bool handled = false;

	// Key presses are not being passed to the Popup menu. A proper fix is
	// non-trivial so instead just close the menu.
	LLMenuGL* menu = getContextMenu();
	if (menu && menu->isOpen())
	{
		LLMenuGL::sMenuContainer->hideMenus();
	}

	if (gFocusMgr.getKeyboardFocus() == this)
	{
		LLLineEditorRollback rollback(this);

		bool selection_modified = false;
		if (!handled)
		{
			handled = handleSelectionKey(key, mask);
			selection_modified = handled;
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
				handled = handleSpecialKey(key, mask);
			}
		}

		if (handled)
		{
			mKeystrokeTimer.reset();

			// Most keystrokes will make the selection box go away, but not all
			// will.
			if (!selection_modified && KEY_SHIFT != key &&
				KEY_CONTROL != key && KEY_ALT != key && KEY_CAPSLOCK)
			{
				deselect();
			}

			// If read-only, don't allow changes
			bool need_to_rollback = mReadOnly &&
									mText.getString() == rollback.getText();
			if (!need_to_rollback)
			{
				// Validate new string and rollback the keystroke if needed.
				need_to_rollback = mPrevalidateFunc &&
								   !mPrevalidateFunc(mText.getWString());
			}
			if (need_to_rollback)
			{
				rollback.doRollback(this);

				reportBadKeystroke();
			}

			// Notify owner if requested
			if (!need_to_rollback && handled)
			{
				if (mKeystrokeCallback)
				{
					mKeystrokeCallback(this, mCallbackUserData);
				}
			}
		}
	}

	return handled;
}

bool LLLineEditor::handleUnicodeCharHere(llwchar uni_char)
{
	if (uni_char < 0x20 || uni_char == 0x7F) // Control character or DEL
	{
		return false;
	}

	bool handled = false;

	if (gFocusMgr.getKeyboardFocus() == this && getVisible() && !mReadOnly)
	{
		// Key presses are not being passed to the pop-up menu.
		// A proper fix is non-trivial so instead just close the menu.
		LLMenuGL* menu = getContextMenu();
		if (menu && menu->isOpen())
		{
			LLMenuGL::sMenuContainer->hideMenus();
		}

		handled = true;

		LLLineEditorRollback rollback(this);

		addChar(uni_char);

		mKeystrokeTimer.reset();

		deselect();

		// Validate new string and rollback the keystroke if needed.
		bool need_to_rollback = mPrevalidateFunc &&
								!mPrevalidateFunc(mText.getWString());
		if (need_to_rollback)
		{
			rollback.doRollback(this);

			reportBadKeystroke();
		}

		// Notify owner if requested
		if (!need_to_rollback && handled && mKeystrokeCallback)
		{
			// *HACK: the only usage of this callback does not do anything with
			// the character. We will have to do something about this if
			// something ever changes - Doug
			mKeystrokeCallback(this, mCallbackUserData);
		}
	}
	return handled;
}

bool LLLineEditor::canDoDelete() const
{
	return !mReadOnly &&
			(!mPassDelete || hasSelection() || mCursorPos < mText.length());
}

void LLLineEditor::doDelete()
{
	if (canDoDelete() && !mText.empty())
	{
		// Prepare for possible rollback
		LLLineEditorRollback rollback(this);

		if (hasSelection())
		{
			deleteSelection();
		}
		else if (mCursorPos < mText.length())
		{
			setCursor(mCursorPos + 1);
			removeChar();
		}

		// Validate new string and rollback the if needed.
		if (mPrevalidateFunc && !mPrevalidateFunc(mText.getWString()))
		{
			rollback.doRollback(this);
			reportBadKeystroke();
		}
		else
		{
			if (mKeystrokeCallback)
			{
				mKeystrokeCallback(this, mCallbackUserData);
			}
		}

		// Force spell-check update:
		mKeystrokeTimer.reset();
		mPrevSpelledText.erase();
	}
}

void LLLineEditor::drawMisspelled(LLRect background)
{
	LL_FAST_TIMER(FTM_RENDER_SPELLCHECK);

	S32 elapsed = (S32)mSpellTimer.getElapsedTimeF32();
	S32 keystroke = (S32)mKeystrokeTimer.getElapsedTimeF32();
	// Do not bother checking if the text did not change in a while and fire a
	// spell checking only once a second while typing.
	if (keystroke < 2 && (elapsed & 1))
	{
		S32 new_start_spell = mScrollHPos;
		S32 cursorloc = calculateCursorFromMouse(mMaxHPixels);
		S32 length = (S32)mText.length();
		S32 new_end_spell = length > cursorloc ? cursorloc : length;
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
		const S32 bottom = background.mBottom;
		const S32 maxw = getRect().getWidth();
		for (S32 i = 0, count = mMisspellLocations.size(); i < count; ++i)
		{
			S32 wstart =
				findPixelNearestPos(mMisspellLocations[i++] - mCursorPos);
			if (wstart > maxw)
			{
				wstart = maxw;
			}
			S32 wend = findPixelNearestPos(mMisspellLocations[i] - mCursorPos);
			if (wend > maxw)
			{
				wend = maxw;
			}
			// Draw the zig zag line
			gGL.color4ub(255, 0, 0, 200);
			while (wstart < wend)
			{
				gl_line_2d(wstart, bottom - 1, wstart + 3, bottom + 2);
				gl_line_2d(wstart + 3, bottom + 2, wstart + 6, bottom - 1);
				wstart += 6;
			}
		}
	}
}

void LLLineEditor::draw()
{
	S32 text_len = mText.length();

	std::string saved_text;
	if (mDrawAsterixes)
	{
		saved_text = mText.getString();
		std::string text;
		for (S32 i = 0, len = mText.length(); i < len; ++i)
		{
			text += '*';
		}
		mText = text;
	}

	// Draw rectangle for the background
	LLRect background(0, getRect().getHeight(), getRect().getWidth(), 0);
	background.stretch(-mBorderThickness);

	LLColor4 bg_color = mReadOnlyBgColor;

	// Drawing solids requires texturing be disabled
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		// Draw background for text
		if (!mReadOnly)
		{
			if (gFocusMgr.getKeyboardFocus() == this)
			{
				bg_color = mFocusBgColor;
			}
			else
			{
				bg_color = mWriteableBgColor;
			}
		}
		gl_rect_2d(background, bg_color);
	}

	// Draw text

	S32 cursor_bottom = background.mBottom + 1;
	S32 cursor_top = background.mTop - 1;

	LLColor4 text_color;
	if (!mReadOnly)
	{
		if (!getTentative())
		{
			text_color = mFgColor;
		}
		else
		{
			text_color = mTentativeFgColor;
		}
	}
	else
	{
		text_color = mReadOnlyFgColor;
	}
	LLColor4 label_color = mTentativeFgColor;

	if (hasPreeditString())
	{
		// Draw preedit markers. This needs to be before drawing letters.
		for (U32 i = 0, size = mPreeditStandouts.size(); i < size; ++i)
		{
			const S32 preedit_left = mPreeditPositions[i];
			const S32 preedit_right = mPreeditPositions[i + 1];
			if (preedit_right > mScrollHPos)
			{
				S32 preedit_pixels_left =
					findPixelNearestPos(llmax(preedit_left,
											  mScrollHPos) - mCursorPos);
				S32 preedit_pixels_right =
					llmin(findPixelNearestPos(preedit_right - mCursorPos),
											  background.mRight);
				if (preedit_pixels_left >= background.mRight)
				{
					break;
				}
				LLColor4 color;
				if (mPreeditStandouts[i])
				{
					color = (text_color * STANDOUT_BRIGHTNESS +
							 bg_color *
							 (1.f - STANDOUT_BRIGHTNESS)).setAlpha(1.f);
				}
				else
				{
					color = (text_color * MARKER_BRIGHTNESS +
							 bg_color *
							 (1.f - MARKER_BRIGHTNESS)).setAlpha(1.f);
				}
				gl_rect_2d(preedit_pixels_left + PREEDIT_BORDER,
						   background.mBottom + PREEDIT_BORDER,
						   preedit_pixels_right - PREEDIT_BORDER,
						   background.mBottom, color);
			}
		}
	}

	S32 rendered_text = 0;
	F32 rendered_pixels_right = (F32)mMinHPixels;
	F32 text_bottom = (F32)background.mBottom + (F32)UI_LINEEDITOR_V_PAD;

	if (gFocusMgr.getKeyboardFocus() == this && hasSelection())
	{
		S32 select_left;
		S32 select_right;
		if (mSelectionStart < mCursorPos)
		{
			select_left = mSelectionStart;
			select_right = mCursorPos;
		}
		else
		{
			select_left = mCursorPos;
			select_right = mSelectionStart;
		}

		if (select_left > mScrollHPos)
		{
			// Unselected, left side
			rendered_text = mGLFont->render(mText, mScrollHPos,
											rendered_pixels_right, text_bottom,
											text_color, LLFontGL::LEFT,
											LLFontGL::BOTTOM, LLFontGL::NORMAL,
											select_left - mScrollHPos,
											mMaxHPixels - ll_round(rendered_pixels_right),
											&rendered_pixels_right);
		}

		if (rendered_pixels_right < (F32)mMaxHPixels &&
			rendered_text < text_len)
		{
			LLColor4 color(1.f - bg_color.mV[0], 1.f - bg_color.mV[1],
						   1.f - bg_color.mV[2], 1.f);
			// Selected middle
			S32 width = mGLFont->getWidth(mText.getWString().c_str(),
										  mScrollHPos + rendered_text,
										  select_right - mScrollHPos - rendered_text);
			S32 right_delta = ll_round(rendered_pixels_right);
			width = llmin(width, mMaxHPixels - right_delta);
			gl_rect_2d(right_delta, cursor_top, right_delta + width,
					   cursor_bottom, color);

			rendered_text += mGLFont->render(mText, mScrollHPos + rendered_text,
											 rendered_pixels_right, text_bottom,
											 LLColor4(1.f - text_color.mV[0],
													  1.f - text_color.mV[1],
													  1.f - text_color.mV[2],
													  1),
											 LLFontGL::LEFT, LLFontGL::BOTTOM,
											 LLFontGL::NORMAL,
											 select_right - mScrollHPos - rendered_text,
											 mMaxHPixels - right_delta,
											 &rendered_pixels_right);
		}

		if (rendered_pixels_right < (F32)mMaxHPixels &&
			rendered_text < text_len)
		{
			// Unselected, right side
			mGLFont->render(mText, mScrollHPos + rendered_text,
							rendered_pixels_right, text_bottom, text_color,
							LLFontGL::LEFT, LLFontGL::BOTTOM, LLFontGL::NORMAL,
							S32_MAX,
							mMaxHPixels - ll_round(rendered_pixels_right),
							&rendered_pixels_right);
		}
	}
	else
	{
		mGLFont->render(mText, mScrollHPos, rendered_pixels_right, text_bottom,
						text_color, LLFontGL::LEFT, LLFontGL::BOTTOM,
						LLFontGL::NORMAL, S32_MAX,
						mMaxHPixels - ll_round(rendered_pixels_right),
						&rendered_pixels_right);
	}

	if (!mReadOnly && mSpellCheck && hasFocus() &&
		LLSpellCheck::getInstance()->getSpellCheck())
	{
		drawMisspelled(background);
	}

	// If we are editing...
	if (gFocusMgr.getKeyboardFocus() == this)
	{
		//mBorder->setVisible(true); // ok, programmer art just this once.
		// (Flash the cursor every half second)
		if (gShowTextEditCursor && !mReadOnly)
		{
			F32 elapsed = mKeystrokeTimer.getElapsedTimeF32();
			if (elapsed < CURSOR_FLASH_DELAY || (S32(elapsed * 2) & 1))
			{
				S32 cursor_left = findPixelNearestPos();
				cursor_left -= UI_LINEEDITOR_CURSOR_THICKNESS / 2;
				S32 cursor_right = cursor_left + UI_LINEEDITOR_CURSOR_THICKNESS;
				bool ins_mode = !hasSelection() && gKeyboardp &&
								gKeyboardp->getInsertMode() == LL_KIM_OVERWRITE;
				if (ins_mode)
				{
					static const LLWString space(utf8str_to_wstring(" "));
					S32 wswidth = mGLFont->getWidth(space.c_str());
					S32 width = mGLFont->getWidth(mText.getWString().c_str(),
												  mCursorPos, 1) + 1;
					cursor_right = cursor_left + llmax(wswidth, width);
				}
				// Use same color as text for the Cursor
				gl_rect_2d(cursor_left, cursor_top, cursor_right,
						   cursor_bottom, text_color);
				if (ins_mode)
				{
					mGLFont->render(mText, mCursorPos,
									(F32)(cursor_left +
										  UI_LINEEDITOR_CURSOR_THICKNESS / 2),
									text_bottom,
									LLColor4(1.f - text_color.mV[0],
											 1.f - text_color.mV[1],
											 1.f - text_color.mV[2], 1),
									LLFontGL::LEFT, LLFontGL::BOTTOM,
									LLFontGL::NORMAL, 1);
				}

				// Make sure the IME is in the right place
				// RCalculcate for IME position
				S32 pixels_after_scroll = findPixelNearestPos();
				LLRect screen_pos = getScreenRect();
				LLCoordGL ime_pos(screen_pos.mLeft + pixels_after_scroll,
								  screen_pos.mTop - UI_LINEEDITOR_V_PAD);

				ime_pos.mX = (S32)(ime_pos.mX * LLUI::sGLScaleFactor.mV[VX]);
				ime_pos.mY = (S32)(ime_pos.mY * LLUI::sGLScaleFactor.mV[VY]);
				gWindowp->setLanguageTextInput(ime_pos);
			}
		}

		// Draw label if no text is provided but we should draw it in a
		// different color to give indication that it is not text you typed in
		if (mText.empty() && mReadOnly)
		{
			mGLFont->render(mLabel.getWString(), 0, mMinHPixels,
							(F32)text_bottom, label_color, LLFontGL::LEFT,
							LLFontGL::BOTTOM, LLFontGL::NORMAL, S32_MAX,
							mMaxHPixels - ll_round(rendered_pixels_right),
							&rendered_pixels_right, false);
		}

		// Draw children (border)
		mBorder->setKeyboardFocusHighlight(true);
		LLView::draw();
		mBorder->setKeyboardFocusHighlight(false);
	}
	else // Does not have keyboard input
	{
		// Draw label if no text provided
		if (mText.empty())
		{
			mGLFont->render(mLabel.getWString(), 0, mMinHPixels,
							(F32)text_bottom, label_color,
							LLFontGL::LEFT, LLFontGL::BOTTOM, LLFontGL::NORMAL,
							S32_MAX,
							mMaxHPixels - ll_round(rendered_pixels_right),
							&rendered_pixels_right, false);
		}
		// Draw children (border)
		LLView::draw();
	}

	if (mDrawAsterixes)
	{
		mText = saved_text;
	}
}

// Returns the local screen space X coordinate associated with the text cursor
// position.
S32 LLLineEditor::findPixelNearestPos(S32 cursor_offset) const
{
	S32 dpos = mCursorPos - mScrollHPos + cursor_offset;
	S32 width;
	if (mDrawAsterixes)
	{
		LLWString asterix;
		for (S32 i = 0, len = mText.length(); i < len; ++i)
		{
			asterix += llwchar('*');
		}
		width = mGLFont->getWidth(asterix.c_str(), mScrollHPos, dpos);
	}
	else
	{
		width = mGLFont->getWidth(mText.getWString().c_str(), mScrollHPos,
								  dpos);
	}
	return mMinHPixels + width;
}

void LLLineEditor::reportBadKeystroke()
{
	make_ui_sound("UISndBadKeystroke");
}

//virtual
void LLLineEditor::clear()
{
	mText.clear();
	setCursor(0);
}

//virtual
void LLLineEditor::onTabInto()
{
	selectAll();
}

// Start or stop the editor from accepting text-editing keystrokes
void LLLineEditor::setFocus(bool new_state)
{
	bool old_state = hasFocus();

	if (!new_state)
	{
		gWindowp->allowLanguageTextInput(this, false);
	}

	// Getting focus when we did not have it before, and we want to select all
	if (!old_state && new_state && mSelectAllonFocusReceived)
	{
		selectAll();
		// We do not want handleMouseUp() to "finish" the selection (and
		// thereby set mSelectionEnd to where the mouse is), so we finish the
		// selection here.
		mIsSelecting = false;
	}

	if (new_state)
	{
		grabMenuHandler();

		// Do not start the cursor flashing right away
		mKeystrokeTimer.reset();
	}
	else
	{
		// Not really needed, since loss of keyboard focus should take care of
		// this, but limited paranoia is ok.
		releaseMenuHandler();

		endSelection();
	}

	LLUICtrl::setFocus(new_state);

	if (new_state)
	{
		// Allow Language Text Input only when this LineEditor has no
		// prevalidate function attached. This criterion works fine for now,
		// since all prevalidate func reject any non-ASCII characters. I am not
		// sure for future versions, however.
		gWindowp->allowLanguageTextInput(this, mPrevalidateFunc == NULL);
	}
}

//virtual
void LLLineEditor::setRect(const LLRect& rect)
{
	LLUICtrl::setRect(rect);
	if (mBorder)
	{
		LLRect border_rect = mBorder->getRect();
		// Scalable UI somehow made these rectangles off-by-one.
		// I don't know why. JC
		border_rect.setOriginAndSize(border_rect.mLeft, border_rect.mBottom,
									 rect.getWidth()-1, rect.getHeight() - 1);
		mBorder->setRect(border_rect);
	}
}

void LLLineEditor::setPrevalidate(bool (*func)(const LLWString&))
{
	mPrevalidateFunc = func;
	updateAllowingLanguageInput();
}

// Limits what characters can be used to [1234567890.-] with [-] only valid in
// the first position. Does NOT ensure that the string is a well-formed number
// (that's the job of post-validation) for the simple reasons that intermediate
// states may be invalid even if the final result is valid.
//
//static
bool LLLineEditor::prevalidateFloat(const LLWString& str)
{
	LLLocale locale(LLLocale::USER_LOCALE);

	bool success = true;
	LLWString trimmed = str;
	LLWStringUtil::trim(trimmed);
	S32 len = trimmed.length();
	if (len > 0)
	{
		// May be a comma or period, depending on the locale
		llwchar decimal_point = (llwchar)LLLocale::getDecimalPoint();

		S32 i = 0;

		// First character can be a negative sign
		if (trimmed[0] == '-')
		{
			++i;
		}

		for ( ; i < len; ++i)
		{
			if (decimal_point != trimmed[i] &&
				!LLStringOps::isDigit(trimmed[i]))
			{
				success = false;
				break;
			}
		}
	}

	return success;
}

//static
bool LLLineEditor::postvalidateFloat(const std::string& str)
{
	LLLocale locale(LLLocale::USER_LOCALE);

    bool success = true;
    bool has_decimal = false;
    bool has_digit = false;

	LLWString trimmed = utf8str_to_wstring(str);
	LLWStringUtil::trim(trimmed);
	S32 len = trimmed.length();
	if (0 < len)
	{
		S32 i = 0;

		// First character can be a negative sign
		if ('-' == trimmed[0])
		{
			++i;
		}

		// May be a comma or period, depending on the locale
		llwchar decimal_point = (llwchar)LLLocale::getDecimalPoint();

		for ( ; i < len; ++i)
		{
			if (decimal_point == trimmed[i])
			{
				if (has_decimal)
				{
					// can't have two
					success = false;
					break;
				}
				else
				{
					has_decimal = true;
				}
			}
			else
			if (LLStringOps::isDigit(trimmed[i]))
			{
				has_digit = true;
			}
			else
			{
				success = false;
				break;
			}
		}
	}

	// Gotta have at least one
	success = has_digit;

	return success;
}

// Limits what characters can be used to [1234567890-] with [-] only valid in
// the first position. Does NOT ensure that the string is a well-formed number
// (that's the job of post-validation) for the simple reasons that intermediate
// states may be invalid even if the final result is valid.
//static
bool LLLineEditor::prevalidateInt(const LLWString& str)
{
	LLLocale locale(LLLocale::USER_LOCALE);

	bool success = true;
	LLWString trimmed = str;
	LLWStringUtil::trim(trimmed);
	S32 len = trimmed.length();
	if (0 < len)
	{
		S32 i = 0;

		// First character can be a negative sign
		if ('-' == trimmed[0])
		{
			++i;
		}

		for ( ; i < len; ++i)
		{
			if (!LLStringOps::isDigit(trimmed[i]))
			{
				success = false;
				break;
			}
		}
	}

	return success;
}

//static
bool LLLineEditor::prevalidatePositiveS32(const LLWString& str)
{
	LLLocale locale(LLLocale::USER_LOCALE);

	LLWString trimmed = str;
	LLWStringUtil::trim(trimmed);
	S32 len = trimmed.length();
	bool success = true;
	if (0 < len)
	{
		if ('-' == trimmed[0] || '0' == trimmed[0])
		{
			success = false;
		}
		S32 i = 0;
		while (success && (i < len))
		{
			if (!LLStringOps::isDigit(trimmed[i++]))
			{
				success = false;
			}
		}
	}
	if (success)
	{
		S32 val = strtol(wstring_to_utf8str(trimmed).c_str(), NULL, 10);
		if (val <= 0)
		{
			success = false;
		}
	}
	return success;
}

bool LLLineEditor::prevalidateNonNegativeS32(const LLWString& str)
{
	LLLocale locale(LLLocale::USER_LOCALE);

	LLWString trimmed = str;
	LLWStringUtil::trim(trimmed);
	S32 len = trimmed.length();
	bool success = true;
	if (0 < len)
	{
		if ('-' == trimmed[0])
		{
			success = false;
		}
		S32 i = 0;
		while (success && (i < len))
		{
			if (!LLStringOps::isDigit(trimmed[i++]))
			{
				success = false;
			}
		}
	}
	if (success)
	{
		S32 val = strtol(wstring_to_utf8str(trimmed).c_str(), NULL, 10);
		if (val < 0)
		{
			success = false;
		}
	}
	return success;
}

bool LLLineEditor::prevalidateAlphaNum(const LLWString& str)
{
	LLLocale locale(LLLocale::USER_LOCALE);

	bool rv = true;
	S32 len = str.length();
	if (len == 0) return rv;
	while (len--)
	{
		if (!LLStringOps::isAlnum((char)str[len]))
		{
			rv = false;
			break;
		}
	}
	return rv;
}

//static
bool LLLineEditor::prevalidateAlphaNumSpace(const LLWString& str)
{
	LLLocale locale(LLLocale::USER_LOCALE);

	bool rv = true;
	S32 len = str.length();
	if (len == 0) return rv;
	while (len--)
	{
		if (!(LLStringOps::isAlnum((char)str[len]) || ' ' == str[len]))
		{
			rv = false;
			break;
		}
	}
	return rv;
}

//static
bool LLLineEditor::prevalidatePrintableNotPipe(const LLWString& str)
{
	bool rv = true;
	S32 len = str.length();
	if (len == 0) return rv;
	while (len--)
	{
		if ('|' == str[len])
		{
			rv = false;
			break;
		}
		if (!(' ' == str[len] || LLStringOps::isAlnum((char)str[len]) ||
			LLStringOps::isPunct((char)str[len])))
		{
			rv = false;
			break;
		}
	}
	return rv;
}

//static
bool LLLineEditor::prevalidatePrintableNoSpace(const LLWString& str)
{
	bool rv = true;
	S32 len = str.length();
	if (len == 0) return rv;
	while (len--)
	{
		if (LLStringOps::isSpace(str[len]))
		{
			rv = false;
			break;
		}
		if (!(LLStringOps::isAlnum((char)str[len]) ||
		      LLStringOps::isPunct((char)str[len])))
		{
			rv = false;
			break;
		}
	}
	return rv;
}

//static
bool LLLineEditor::prevalidateASCII(const LLWString& str)
{
	bool rv = true;
	S32 len = str.length();
	while (len--)
	{
		if (str[len] < 0x20 || str[len] > 0x7f)
		{
			rv = false;
			break;
		}
	}
	return rv;
}

void LLLineEditor::onMouseCaptureLost()
{
	endSelection();
}

void LLLineEditor::setSelectAllonFocusReceived(bool b)
{
	mSelectAllonFocusReceived = b;
}

void LLLineEditor::setKeystrokeCallback(void (*keystroke_callback)(LLLineEditor*,
																   void*))
{
	mKeystrokeCallback = keystroke_callback;
}

void LLLineEditor::setOnHandleKeyCallback(bool (*callback)(KEY, MASK,
														   LLLineEditor*,
														   void*),
										  void* userdata)
{
	mOnHandleKeyCallback = callback;
	mOnHandleKeyData = userdata;
}

void LLLineEditor::setScrolledCallback(void (*scrolled_callback)(LLLineEditor* caller,
									 							 void* user_data),
									   void* userdata)
{
	mScrolledCallback = scrolled_callback;
	mScrolledCallbackData = userdata;
}

//virtual
LLXMLNodePtr LLLineEditor::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_LINE_EDITOR_TAG);

	node->createChild("max_length", true)->setIntValue(mMaxLengthBytes);

	node->createChild("font",
					  true)->setStringValue(LLFontGL::nameFromFont(mGLFont));

	if (mBorder)
	{
		std::string bevel;
		switch (mBorder->getBevel())
		{
			case LLViewBorder::BEVEL_IN:
				bevel = "in";
				break;

			case LLViewBorder::BEVEL_OUT:
				bevel = "out";
				break;

			case LLViewBorder::BEVEL_BRIGHT:
				bevel = "bright";
				break;

			case LLViewBorder::BEVEL_NONE:
			default:
				bevel = "none";
		}
		node->createChild("bevel_style", true)->setStringValue(bevel);

		std::string style;
		if (mBorder->getStyle() == LLViewBorder::STYLE_TEXTURE)
		{
			style = "texture";
		}
		else
		{
			style = "line";
		}
		node->createChild("border_style", true)->setStringValue(style);

		node->createChild("border_thickness",
						  true)->setIntValue(mBorder->getBorderWidth());
	}

	if (!mLabel.empty())
	{
		node->createChild("label", true)->setStringValue(mLabel.getString());
	}

	node->createChild("select_all_on_focus_received",
					  true)->setBoolValue(mSelectAllonFocusReceived);

	node->createChild("handle_edit_keys_directly",
					  true)->setBoolValue(mHandleEditKeysDirectly);

	addColorXML(node, mCursorColor, "cursor_color", "TextCursorColor");
	addColorXML(node, mFgColor, "text_color", "TextFgColor");
	addColorXML(node, mReadOnlyFgColor, "text_readonly_color",
				"TextFgReadOnlyColor");
	addColorXML(node, mTentativeFgColor, "text_tentative_color",
				"TextFgTentativeColor");
	addColorXML(node, mReadOnlyBgColor, "bg_readonly_color",
				"TextBgReadOnlyColor");
	addColorXML(node, mWriteableBgColor, "bg_writeable_color",
				"TextBgWriteableColor");
	addColorXML(node, mFocusBgColor, "bg_focus_color", "TextBgFocusColor");

	node->createChild("select_on_focus",
					  true)->setBoolValue(mSelectAllonFocusReceived);

	return node;
}

//static
LLView* LLLineEditor::fromXML(LLXMLNodePtr node, LLView* parent,
							  LLUICtrlFactory* factory)
{
	std::string name = LL_LINE_EDITOR_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	S32 max_text_length = 128;
	node->getAttributeS32("max_length", max_text_length);

	LLFontGL* font = LLView::selectFont(node);

	std::string text = node->getTextContents().substr(0, max_text_length - 1);

	LLViewBorder::EBevel bevel_style = LLViewBorder::BEVEL_IN;
	LLViewBorder::getBevelFromAttribute(node, bevel_style);

	LLViewBorder::EStyle border_style = LLViewBorder::STYLE_LINE;
	std::string border_string;
	node->getAttributeString("border_style", border_string);
	LLStringUtil::toLower(border_string);

	if (border_string == "texture")
	{
		border_style = LLViewBorder::STYLE_TEXTURE;
	}

	S32 border_thickness = 1;
	node->getAttributeS32("border_thickness", border_thickness);

	LLLineEditor* line_editor = new LLLineEditor(name, rect, text, font,
												 max_text_length,
												 NULL, NULL, NULL, NULL, NULL,
												 bevel_style, border_style,
												 border_thickness);

	std::string label;
	if (node->getAttributeString("label", label))
	{
		line_editor->setLabel(label);
	}
	bool select_all_on_focus_received = false;
	if (node->getAttributeBool("select_all_on_focus_received",
							   select_all_on_focus_received))
	{
		line_editor->setSelectAllonFocusReceived(select_all_on_focus_received);
	}
	bool handle_edit_keys_directly = false;
	if (node->getAttributeBool("handle_edit_keys_directly",
							   handle_edit_keys_directly))
	{
		line_editor->setHandleEditKeysDirectly(handle_edit_keys_directly);
	}
	bool commit_on_focus_lost = true;
	if (node->getAttributeBool("commit_on_focus_lost",
							   commit_on_focus_lost))
	{
		line_editor->setCommitOnFocusLost(commit_on_focus_lost);
	}
	bool spell_check = false;
	if (node->getAttributeBool("spell_check", spell_check))
	{
		line_editor->setSpellCheck(spell_check);
	}

	line_editor->setColorParameters(node);

	if (node->hasAttribute("select_on_focus"))
	{
		bool selectall = false;
		node->getAttributeBool("select_on_focus", selectall);
		line_editor->setSelectAllonFocusReceived(selectall);
	}

	std::string prevalidate;
	if (node->getAttributeString("prevalidate", prevalidate))
	{
		LLStringUtil::toLower(prevalidate);

		if (prevalidate == "ascii")
		{
			line_editor->setPrevalidate(prevalidateASCII);
		}
		else if (prevalidate == "float")
		{
			line_editor->setPrevalidate(prevalidateFloat);
		}
		else if (prevalidate == "int")
		{
			line_editor->setPrevalidate(prevalidateInt);
		}
		else if (prevalidate == "positive_s32")
		{
			line_editor->setPrevalidate(prevalidatePositiveS32);
		}
		else if (prevalidate == "non_negative_s32")
		{
			line_editor->setPrevalidate(prevalidateNonNegativeS32);
		}
		else if (prevalidate == "alpha_num")
		{
			line_editor->setPrevalidate(prevalidateAlphaNum);
		}
		else if (prevalidate == "alpha_num_space")
		{
			line_editor->setPrevalidate(prevalidateAlphaNumSpace);
		}
		else if (prevalidate == "printable_not_pipe")
		{
			line_editor->setPrevalidate(prevalidatePrintableNotPipe);
		}
		else if (prevalidate == "printable_no_space")
		{
			line_editor->setPrevalidate(prevalidatePrintableNoSpace);
		}
	}

	line_editor->initFromXML(node, parent);

	return line_editor;
}

//static
void LLLineEditor::cleanupLineEditor()
{
	sImage = NULL;
}

//static
LLUIImagePtr LLLineEditor::parseImage(std::string name, LLXMLNodePtr from,
									  LLUIImagePtr def)
{
	std::string xml_name;
	if (from->hasAttribute(name.c_str()))
	{
		from->getAttributeString(name.c_str(), xml_name);
	}

	if (xml_name.empty())
	{
		return def;
	}

	LLUIImagePtr image = LLUI::getUIImage(xml_name);
	return image.isNull() ? def : image;
}

void LLLineEditor::setColorParameters(LLXMLNodePtr node)
{
	// overrides default image if supplied.
	mImage = parseImage("image", node, mImage);

	LLColor4 color;
	if (LLUICtrlFactory::getAttributeColor(node,"cursor_color", color))
	{
		setCursorColor(color);
	}
	if (node->hasAttribute("text_color"))
	{
		LLUICtrlFactory::getAttributeColor(node,"text_color", color);
		setFgColor(color);
	}
	if (node->hasAttribute("text_readonly_color"))
	{
		LLUICtrlFactory::getAttributeColor(node,"text_readonly_color", color);
		setReadOnlyFgColor(color);
	}
	if (LLUICtrlFactory::getAttributeColor(node,"text_tentative_color", color))
	{
		setTentativeFgColor(color);
	}
	if (node->hasAttribute("bg_readonly_color"))
	{
		LLUICtrlFactory::getAttributeColor(node,"bg_readonly_color", color);
		setReadOnlyBgColor(color);
	}
	if (node->hasAttribute("bg_writeable_color"))
	{
		LLUICtrlFactory::getAttributeColor(node,"bg_writeable_color", color);
		setWriteableBgColor(color);
	}
}

void LLLineEditor::updateAllowingLanguageInput()
{
	// Allow Language Text Input only when this LineEditor has no prevalidate
	// function attached (as long as other criteria common to LLTextEditor).
	// This criterion works fine on 1.15.0.2, since all prevalidate func
	// reject any non-ASCII characters. I'm not sure on future versions,
	// however...
	if (hasFocus() && !mReadOnly && !mDrawAsterixes && !mPrevalidateFunc)
	{
		gWindowp->allowLanguageTextInput(this, true);
	}
	else
	{
		gWindowp->allowLanguageTextInput(this, false);
	}
}

bool LLLineEditor::hasPreeditString() const
{
	return mPreeditPositions.size() > 1;
}

void LLLineEditor::resetPreedit()
{
	if (hasPreeditString())
	{
		if (hasSelection())
		{
			llwarns << "Preedit and selection !  Deselecting." << llendl;
			deselect();
		}

		const S32 preedit_pos = mPreeditPositions.front();
		mText.erase(preedit_pos, mPreeditPositions.back() - preedit_pos);
		mText.insert(preedit_pos, mPreeditOverwrittenWString);
		setCursor(preedit_pos);

		mPreeditWString.clear();
		mPreeditOverwrittenWString.clear();
		mPreeditPositions.clear();

		// Do not reset keystroke timer nor invoke keystroke callback, because
		// a call to updatePreedit should be follow soon in normal course of
		// operation, and timer and callback will be maintained there. Doing so
		// here made an odd sound (VWR-3410).
	}
}

void LLLineEditor::updatePreedit(const LLWString& preedit_string,
								 const segment_lengths_t& preedit_segment_lengths,
								 const standouts_t& preedit_standouts,
								 S32 caret_position)
{
	// Just in case.
	if (mReadOnly)
	{
		return;
	}

	// Note that call to updatePreedit is always preceeded by resetPreedit,
	// so we have no existing selection/preedit.

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
		mPreeditOverwrittenWString.assign(LLWString(mText, insert_preedit_at,
										  mPreeditWString.length()));
		mText.erase(insert_preedit_at, mPreeditWString.length());
	}
	else
	{
		mPreeditOverwrittenWString.clear();
	}
	mText.insert(insert_preedit_at, mPreeditWString);

	mPreeditStandouts = preedit_standouts;

	setCursor(position);
	setCursor(mPreeditPositions.front() + caret_position);

	// Update of the preedit should be caused by some key strokes.
	mKeystrokeTimer.reset();
	if (mKeystrokeCallback)
	{
		mKeystrokeCallback(this, mCallbackUserData);
	}
}

bool LLLineEditor::getPreeditLocation(S32 query_offset, LLCoordGL* coord,
									  LLRect* bounds, LLRect* control) const
{
	if (control)
	{
		LLRect control_rect_screen;
		localRectToScreen(getRect(), &control_rect_screen);
		LLUI::screenRectToGL(control_rect_screen, control);
	}

	S32 preedit_left_column, preedit_right_column;
	if (hasPreeditString())
	{
		preedit_left_column = mPreeditPositions.front();
		preedit_right_column = mPreeditPositions.back();
	}
	else
	{
		preedit_left_column = preedit_right_column = mCursorPos;
	}
	if (preedit_right_column < mScrollHPos)
	{
		// This should not occur...
		return false;
	}

	const S32 query = query_offset >= 0 ? preedit_left_column + query_offset
										: mCursorPos;
	if (query < mScrollHPos || query < preedit_left_column ||
		query > preedit_right_column)
	{
		return false;
	}

	if (coord)
	{
		S32 query_local = findPixelNearestPos(query - mCursorPos);
		S32 query_screen_x, query_screen_y;
		localPointToScreen(query_local, getRect().getHeight() / 2,
						   &query_screen_x, &query_screen_y);
		LLUI::screenPointToGL(query_screen_x, query_screen_y, &coord->mX,
							  &coord->mY);
	}

	if (bounds)
	{
		S32 preedit_left_local = findPixelNearestPos(llmax(preedit_left_column,
														   mScrollHPos) -
													 mCursorPos);
		S32 preedit_right_local = llmin(findPixelNearestPos(preedit_right_column -
															mCursorPos),
										getRect().getWidth() - mBorderThickness);
		if (preedit_left_local > preedit_right_local)
		{
			// Is this condition possible ?
			preedit_right_local = preedit_left_local;
		}

		LLRect preedit_rect_local(preedit_left_local, getRect().getHeight(),
								  preedit_right_local, 0);
		LLRect preedit_rect_screen;
		localRectToScreen(preedit_rect_local, &preedit_rect_screen);
		LLUI::screenRectToGL(preedit_rect_screen, bounds);
	}

	return true;
}

void LLLineEditor::getPreeditRange(S32* position, S32* length) const
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

void LLLineEditor::getSelectionRange(S32* position, S32* length) const
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

void LLLineEditor::markAsPreedit(S32 position, S32 length)
{
	deselect();
	setCursor(position);
	if (hasPreeditString())
	{
		llwarns << "markAsPreedit invoked when hasPreeditString is true."
				<< llendl;
	}
	mPreeditWString.assign(LLWString(mText.getWString(), position, length));
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

S32 LLLineEditor::getPreeditFontSize() const
{
	return ll_roundp(mGLFont->getLineHeight() * LLUI::sGLScaleFactor.mV[VY]);
}

LLWString LLLineEditor::getConvertedText() const
{
	LLWString text = getWText();
	LLWStringUtil::trim(text);
	if (!mReplaceNewlinesWithSpaces)
	{
		// Convert paragraph symbols back into newlines.
		LLWStringUtil::replaceChar(text, 182, '\n');
	}
	return text;
}

///////////////////////////////////////////////////////////////////////////////
// LLSearchEditor class
///////////////////////////////////////////////////////////////////////////////

static const std::string LL_SEARCH_EDITOR_TAG = "search_editor";
static LLRegisterWidget<LLSearchEditor> r07(LL_SEARCH_EDITOR_TAG);

LLSearchEditor::LLSearchEditor(const std::string& name, const LLRect& rect,
							   S32 max_length)
:	LLUICtrl(name, rect, true, NULL, NULL),
	mSearchCallback(NULL),
	mLineCommitCallback(NULL),
	mCommitCallbackUserData(NULL)
{
	LLRect line_edit_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
	mSearchLineEditor = new LLLineEditor(name + "_line_editor", line_edit_rect,
										 LLStringUtil::null, NULL, max_length,
										 NULL, NULL, NULL, this);
	mSearchLineEditor->setFollowsAll();
	mSearchLineEditor->setSelectAllonFocusReceived(true);

	addChild(mSearchLineEditor);

	 // Button is square, and as tall as search editor
	S32 btn_width = rect.getHeight();

	LLRect clear_btn_rect(rect.getWidth() - btn_width, rect.getHeight(),
						  rect.getWidth(), 0);
	mClearSearchButton = new LLButton(name + "_clear_button", clear_btn_rect,
									  "icn_clear_lineeditor.tga",
									  "UIImgBtnCloseInactiveUUID",
									  NULL, onClearSearch, this,
									  NULL, LLStringUtil::null);
	mClearSearchButton->setFollowsRight();
	mClearSearchButton->setFollowsTop();
	mClearSearchButton->setImageColor(LLUI::sTextFgTentativeColor);
	mClearSearchButton->setTabStop(false);
	mSearchLineEditor->addChild(mClearSearchButton);

	mSearchLineEditor->setTextPadding(0, btn_width);
}

//virtual
void LLSearchEditor::clear()
{
	if (mSearchLineEditor)
	{
		mSearchLineEditor->clear();
	}
}

//virtual
void LLSearchEditor::draw()
{
	mClearSearchButton->setVisible(!mSearchLineEditor->getWText().empty());
	LLUICtrl::draw();
}

void LLSearchEditor::setCommitCallback(void (*cb)(LLUICtrl*, void*))
{
	mLineCommitCallback = cb;
	if (mLineCommitCallback)
	{
		mSearchLineEditor->setCommitCallback(onSearchEditCommit);
	}
	else
	{
		mSearchLineEditor->setCommitCallback(NULL);
	}
}

void LLSearchEditor::setSearchCallback(void (*cb)(const std::string&, void*),
									   void* userdata)
{
	mSearchCallback = cb;
	if (mSearchCallback)
	{
		mSearchLineEditor->setKeystrokeCallback(onSearchEditKeystroke);
	}
	else
	{
		mSearchLineEditor->setKeystrokeCallback(NULL);
	}
	mCallbackUserData = userdata;
}

//static
void LLSearchEditor::onSearchEditCommit(LLUICtrl* ctrl, void* data)
{
	LLSearchEditor* self = (LLSearchEditor*)data;
	if (self && self->mLineCommitCallback)
	{
		self->mLineCommitCallback(ctrl, self->mCommitCallbackUserData);
	}
}

//static
void LLSearchEditor::onSearchEditKeystroke(LLLineEditor* caller, void* data)
{
	LLSearchEditor* self = (LLSearchEditor*)data;
	if (caller && self && self->mSearchCallback)
	{
		self->mSearchCallback(caller->getText(), self->mCallbackUserData);
	}
}

//static
void LLSearchEditor::onClearSearch(void* data)
{
	LLSearchEditor* self = (LLSearchEditor*)data;
	if (!self) return;

	self->setText(LLStringUtil::null);
	if (self->mSearchCallback)
	{
		self->mSearchCallback(LLStringUtil::null, self->mCallbackUserData);
	}
}

//virtual
LLXMLNodePtr LLSearchEditor::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_SEARCH_EDITOR_TAG);

	return node;
}

//static
LLView* LLSearchEditor::fromXML(LLXMLNodePtr node, LLView* parent,
								LLUICtrlFactory* factory)
{
	std::string name = LL_SEARCH_EDITOR_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	S32 max_text_length = 128;
	node->getAttributeS32("max_length", max_text_length);

	std::string text = node->getValue().substr(0, max_text_length - 1);

	LLSearchEditor* self = new LLSearchEditor(name, rect, max_text_length);

	std::string label;
	if (node->getAttributeString("label", label))
	{
		self->mSearchLineEditor->setLabel(label);
	}

	self->setText(text);

	self->initFromXML(node, parent);

	return self;
}
