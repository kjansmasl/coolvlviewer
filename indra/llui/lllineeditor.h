/**
 * @file lllineeditor.h
 * @brief Text editor widget to let users enter/edit a single line.
 *
 * Features:
 *		Text entry of a single line (text, delete, left and right arrow, insert, return).
 *		Callbacks either on every keystroke or just on the return key.
 *		Focus (allow multiple text entry widgets)
 *		Clipboard (cut, copy, and paste)
 *		Horizontal scrolling to allow strings longer than widget size allows
 *		Pre-validation (limit which keys can be used)
 *		Optional line history so previous entries can be recalled by CTRL UP/DOWN
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

#ifndef LL_LLLINEEDITOR_H
#define LL_LLLINEEDITOR_H

#include "lleditmenuhandler.h"
#include "llframetimer.h"
#include "llpreeditor.h"
#include "llpreprocessor.h"
#include "lluictrl.h"
#include "lluistring.h"
#include "llviewborder.h"
#include "llcolor4.h"

class LLButton;
class LLLineEditorRollback;
class LLFontGL;
class LLMenuItemCallGL;

typedef bool (*LLLinePrevalidateFunc)(const LLWString& wstr);

class LLLineEditor : public LLUICtrl, public LLEditMenuHandler,
					 protected LLPreeditor
{
protected:
	LOG_CLASS(LLLineEditor);

public:
	LLLineEditor(const std::string& name, const LLRect& rect,
				 const std::string& default_text = LLStringUtil::null,
				 const LLFontGL* glfont = NULL, S32 max_length_bytes = 254,
				 void (*commit_callback)(LLUICtrl*, void*) = NULL,
				 void (*keystroke_callback)(LLLineEditor*, void*) = NULL,
				 void (*focus_lost_callback)(LLFocusableElement*,
											 void*) = NULL,
				 void* userdata = NULL,
				 LLLinePrevalidateFunc prevalidate_func = NULL,
				 LLViewBorder::EBevel border_bevel = LLViewBorder::BEVEL_IN,
				 LLViewBorder::EStyle border_style = LLViewBorder::STYLE_LINE,
				 S32 border_thickness = 1);

	~LLLineEditor() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;

	void setColorParameters(LLXMLNodePtr node);
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);
	static void cleanupLineEditor();

	// Mouse handler overrides
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x,S32 y,MASK mask) override;
	bool handleMiddleMouseDown(S32 x,S32 y,MASK mask) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleUnicodeCharHere(llwchar uni_char) override;
	void onMouseCaptureLost() override;

	// Returns true if user changed value at all
    LL_INLINE virtual bool isSpellDirty() const				{ return mText.getString() != mPrevSpelledText; }
	// Clear dirty state
    LL_INLINE virtual void resetSpellDirty()				{ mPrevSpelledText = mText.getString(); }

    struct SpellMenuBind
    {
        LLLineEditor*		mOrigin;
        LLMenuItemCallGL*	mMenuItem;
        std::string			mWord;
        S32					mWordPositionStart;
        S32					mWordPositionEnd;
    };

    std::vector<S32> getMisspelledWordsPositions();
    virtual void spellReplace(SpellMenuBind* data);
	virtual void insert(std::string what, S32 where);

	// LLEditMenuHandler overrides

	void cut() override;
	bool canCut() const override;

	void copy() override;
	bool canCopy() const override;

	void paste() override;
	bool canPaste() const override;

	void doDelete() override;
	bool canDoDelete() const override;

	void selectAll() override;
	LL_INLINE bool canSelectAll() const override			{ return true; }

	void deselect() override;
	LL_INLINE bool canDeselect() const override				{ return hasSelection(); }

	// New methods
	virtual void updatePrimary();
	virtual void copyPrimary();
 	virtual void pastePrimary();
	virtual bool canPastePrimary() const;

	// LLView overrides

	void draw() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;
	void onFocusReceived() override;
	void onFocusLost() override;
	void setEnabled(bool enabled) override;

	LL_INLINE bool setTextArg(const std::string& key,
							  const std::string& text) override
	{
		mText.setArg(key, text);
		return true;
	}

	LL_INLINE bool setLabelArg(const std::string& key,
							   const std::string& text) override
	{
		mLabel.setArg(key, text);
		return true;
	}

	// LLUICtrl overrides

	void clear() override;
	void onTabInto() override;
	void setFocus(bool b) override;
	void setRect(const LLRect& rect) override;

	LL_INLINE bool acceptsTextInput() const override		{ return true; }

	void onCommit() override;

	// Returns true if user changed value at all
	LL_INLINE bool isDirty() const override					{ return mText.getString() != mPrevText; }
	// Clear dirty state
	LL_INLINE void resetDirty() override					{ mPrevText = mText.getString(); }

	// Assumes UTF8 text
	LL_INLINE void setValue(const LLSD& value) override		{ setText(value.asString()); }

	LL_INLINE LLSD getValue() const override				{ return LLSD(getText()); }

	// New methods

	LL_INLINE void setLabel(const std::string& label)		{ mLabel = label; }
	void setText(const std::string& new_text);

	LL_INLINE const std::string& getText() const			{ return mText.getString(); }
	LL_INLINE const LLWString& getWText() const override	{ return mText.getWString(); }
	// trimmed text with paragraphs converted to newlines
	LLWString getConvertedText() const;

	LL_INLINE S32 getLength() const							{ return mText.length(); }

	LL_INLINE S32 getCursor()	const						{ return mCursorPos; }
	void setCursor(S32 pos);
	void setCursorToEnd();

	void resetScrollPosition();

	// Selects characters 'start' to 'end'.
	void setSelection(S32 start, S32 end);

	LL_INLINE void setCommitOnFocusLost(bool b)				{ mCommitOnFocusLost = b; }
	LL_INLINE void setRevertOnEsc(bool b)					{ mRevertOnEsc = b; }

	LL_INLINE void setCursorColor(const LLColor4& c)		{ mCursorColor = c; }
	LL_INLINE const	LLColor4& getCursorColor() const		{ return mCursorColor; }

	LL_INLINE void setFgColor(const LLColor4& c)			{ mFgColor = c; }
	LL_INLINE void setReadOnlyFgColor(const LLColor4& c)	{ mReadOnlyFgColor = c; }
	LL_INLINE void setTentativeFgColor(const LLColor4& c)	{ mTentativeFgColor = c; }
	LL_INLINE void setWriteableBgColor(const LLColor4& c)	{ mWriteableBgColor = c; }
	LL_INLINE void setReadOnlyBgColor(const LLColor4& c)	{ mReadOnlyBgColor = c; }
	LL_INLINE void setFocusBgColor(const LLColor4& c)		{ mFocusBgColor = c; }
	LL_INLINE void setSpellCheck(bool b)					{ mSpellCheck = b; }

	LL_INLINE const LLColor4& getFgColor() const			{ return mFgColor; }
	LL_INLINE const LLColor4& getReadOnlyFgColor() const	{ return mReadOnlyFgColor; }
	LL_INLINE const LLColor4& getTentativeFgColor() const	{ return mTentativeFgColor; }
	LL_INLINE const LLColor4& getWriteableBgColor() const	{ return mWriteableBgColor; }
	LL_INLINE const LLColor4& getReadOnlyBgColor() const	{ return mReadOnlyBgColor; }
	LL_INLINE const LLColor4& getFocusBgColor() const		{ return mFocusBgColor; }
	LL_INLINE bool getSpellCheck()							{ return mSpellCheck; }

	LL_INLINE void setIgnoreArrowKeys(bool b)				{ mIgnoreArrowKeys = b; }
	LL_INLINE void setIgnoreTab(bool b)						{ mIgnoreTab = b; }
	LL_INLINE void setPassDelete(bool b)					{ mPassDelete = b; }
	void setDrawAsterixes(bool b);
	LL_INLINE bool getDrawAsterixes()						{ return mDrawAsterixes; }

	// Get the cursor position of the beginning/end of the prev/next word in
	// the text
	S32 prevWordPos(S32 cursor_pos) const;
	S32 nextWordPos(S32 cursor_pos) const;
	bool getWordBoundriesAt(S32 at, S32* word_begin, S32* word_length) const;

	LL_INLINE bool hasSelection() const						{ return mSelectionStart != mSelectionEnd; }
	void startSelection();
	void endSelection();
	void extendSelection(S32 new_cursor_pos);
	void deleteSelection();

	LL_INLINE void setHandleEditKeysDirectly(bool b)		{ mHandleEditKeysDirectly = b; }
	void setSelectAllonFocusReceived(bool b);

	void setKeystrokeCallback(void (*keystroke_callback)(LLLineEditor*,
														 void*));
	void setScrolledCallback(void (*scrolled_callback)(LLLineEditor*, void*),
													   void* userdata);
	void setOnHandleKeyCallback(bool (*callback)(KEY, MASK, LLLineEditor*,
												void*),
								void* userdata);

	void setMaxTextLength(S32 max_text_length);
	// Used to specify room for children before or after text.
	void setTextPadding(S32 left, S32 right);

	// Prevalidation controls which keystrokes can affect the editor
	void setPrevalidate(bool (*func)(const LLWString&));
	static bool prevalidateFloat(const LLWString& str);
	static bool prevalidateInt(const LLWString& str);
	static bool prevalidatePositiveS32(const LLWString& str);
	static bool prevalidateNonNegativeS32(const LLWString& str);
	static bool prevalidateAlphaNum(const LLWString& str);
	static bool prevalidateAlphaNumSpace(const LLWString& str);
	static bool prevalidatePrintableNotPipe(const LLWString& str);
	static bool prevalidatePrintableNoSpace(const LLWString& str);
	static bool prevalidateASCII(const LLWString& str);

	static bool postvalidateFloat(const std::string& str);

	// Line history support:

	// Switches line history on or off
	LL_INLINE void setEnableLineHistory(bool enabled) 		{ mHaveHistory = enabled; }
	// Stores current line in history
	void updateHistory();

	LL_INLINE void setReplaceNewlinesWithSpaces(bool b)		{ mReplaceNewlinesWithSpaces = b; }

private:
	void pasteHelper(bool is_primary);

	void removeChar();
	void addChar(llwchar c);
	S32 calculateCursorFromMouse(S32 local_mouse_x);
	void setCursorAtLocalPos(S32 local_mouse_x);
	S32 findPixelNearestPos(S32 cursor_offset = 0) const;
	void reportBadKeystroke();
	bool handleSpecialKey(KEY key, MASK mask);
	bool handleSelectionKey(KEY key, MASK mask);
	bool handleControlKey(KEY key, MASK mask);
	S32 handleCommitKey(KEY key, MASK mask);

	void updateAllowingLanguageInput();
	bool hasPreeditString() const;

	// Implementation (overrides) of LLPreeditor
	void resetPreedit() override;
	void updatePreedit(const LLWString& preedit_string,
					   const segment_lengths_t& preedit_segment_lengths,
					   const standouts_t& preedit_standouts,
					   S32 caret_position) override;
	void markAsPreedit(S32 position, S32 length) override;
	void getPreeditRange(S32* position, S32* length) const override;
	void getSelectionRange(S32* position, S32* length) const override;
	bool getPreeditLocation(S32 query_position, LLCoordGL* coord,
							LLRect* bounds, LLRect* control) const override;
	S32 getPreeditFontSize() const override;

	// Private helper class
	class LLLineEditorRollback
	{
	public:
		LLLineEditorRollback(LLLineEditor* ed)
		:	mCursorPos(ed->mCursorPos),
			mScrollHPos(ed->mScrollHPos),
			mIsSelecting(ed->mIsSelecting),
			mSelectionStart(ed->mSelectionStart),
			mSelectionEnd(ed->mSelectionEnd)
		{
			mText = ed->getText();
		}

		void doRollback(LLLineEditor* ed)
		{
			ed->mCursorPos = mCursorPos;
			ed->mScrollHPos = mScrollHPos;
			ed->mIsSelecting = mIsSelecting;
			ed->mSelectionStart = mSelectionStart;
			ed->mSelectionEnd = mSelectionEnd;
			ed->mText = mText;
			ed->mPrevText = mText;
		}

		LL_INLINE std::string getText()   					{ return mText; }

	private:
		std::string mText;
		S32		mCursorPos;
		S32		mScrollHPos;
		bool	mIsSelecting;
		S32		mSelectionStart;
		S32		mSelectionEnd;
	};

	// Utility on top of LLUI::getUIImage, looks up a named image in a given
	// XML node and returns it if possible or returns a given default image if
	// anything in the process fails.
	static LLUIImagePtr parseImage(std::string name, LLXMLNodePtr from,
								   LLUIImagePtr def);

	// Context menu actions
	static void spellCorrect(void* data);
	static void spellShow(void* data);
	static void spellAdd(void* data);
	static void spellIgnore(void* data);

    void drawMisspelled(LLRect background);

protected:
	LLUIString					mText;		// The string being edited.
	std::string					mPrevText;	// Saved string for 'ESC' revert
	// Text label that is visible when no user text provided
	LLUIString					mLabel;

	// Spell checking

	// Saved string so we know whether to respell or not
    std::string					mPrevSpelledText;
	// Where all the misspelled words are
    std::vector<S32>			mMisspellLocations;
	// The position of the first character, stored so we know when to update
    S32							mSpellCheckStart;
	// The location of the last character
    S32							mSpellCheckEnd;
	// Set in xui as "spell_check". Default value for a field
	bool						mSpellCheck;
	// Whether to highlight misspelled words or not
	bool						mShowMisspelled;
    LLFrameTimer				mSpellTimer;
	// To keep track of what we have to remove before rebuilding the context
	// menu
	std::vector<SpellMenuBind*>	mSuggestionMenuItems;

	// Line history support:
	typedef	std::vector<std::string> line_history_t;
	line_history_t				mLineHistory;			// Line history storage
	// Currently browsed history line
	line_history_t::iterator	mCurrentHistoryLine;
	// Flag for enabled line history
	bool						mHaveHistory;

	// Selection for clipboard operations
	bool						mIsSelecting;

	S32							mSelectionStart;
	S32							mSelectionEnd;
	S32							mLastSelectionX;
	S32							mLastSelectionY;
	S32							mLastSelectionStart;
	S32							mLastSelectionEnd;

	LLViewBorder*				mBorder;
	const LLFontGL*				mGLFont;

	// Max length of the UTF8 string in bytes
	S32							mMaxLengthBytes;
	// I-beam is just after the mCursorPos-th character.
	S32							mCursorPos;
	// Horizontal offset from the start of mText. Used for scrolling.
	S32							mScrollHPos;
	LLFrameTimer mScrollTimer;
	// Used to reserve space before the beginning of the text for children
	S32							mTextPadLeft;
	// Used to reserve space after the end of the text for children
	S32							mTextPadRight;
	S32							mMinHPixels;
	S32							mMaxHPixels;

	bool						mCommitOnFocusLost;
	bool						mRevertOnEsc;

	void						(*mKeystrokeCallback)(LLLineEditor* caller,
													  void* userdata);

	void						(*mScrolledCallback)(LLLineEditor* caller,
													 void* userdata);
	void*						mScrolledCallbackData;

	bool						(*mOnHandleKeyCallback)(KEY, MASK,
														LLLineEditor*, void*);
	void*						mOnHandleKeyData;

	bool						(*mPrevalidateFunc)(const LLWString& str);

	LLFrameTimer				mKeystrokeTimer;

	LLColor4					mCursorColor;

	LLColor4					mFgColor;
	LLColor4					mReadOnlyFgColor;
	LLColor4					mTentativeFgColor;
	LLColor4					mWriteableBgColor;
	LLColor4					mReadOnlyBgColor;
	LLColor4					mFocusBgColor;

	S32							mBorderThickness;

	LLWString					mPreeditWString;
	LLWString					mPreeditOverwrittenWString;
	std::vector<S32>			mPreeditPositions;
	LLPreeditor::standouts_t	mPreeditStandouts;

	bool						mIgnoreArrowKeys;
	bool						mIgnoreTab;
	bool						mDrawAsterixes;

	// If true, the standard edit keys (Ctrl-X, Delete, etc) are handled here
	// instead of routed by the menu system:
	bool						mHandleEditKeysDirectly;

	bool						mSelectAllonFocusReceived;
	bool						mPassDelete;

	bool						mReadOnly;

private:
	// If false, will replace pasted newlines with paragraph symbol.
	bool						mReplaceNewlinesWithSpaces;

	// Instances that by default point to the statics but can be overidden in
	// XML.
	LLUIImagePtr				mImage;

	// Global instance used as default for member instance above.
	static LLUIImagePtr			sImage;
};

// A line editor with a button to clear it and a callback to call on every edit
// event.
class LLSearchEditor final : public LLUICtrl, public LLEditMenuHandler
{
public:
	LLSearchEditor(const std::string& name, const LLRect& rect,
				   S32 max_length);

	void draw() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;

	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	LL_INLINE void setText(const std::string& new_text)		{ mSearchLineEditor->setText(new_text); }
	LL_INLINE const std::string& getText() const			{ return mSearchLineEditor->getText(); }

	void setSearchCallback(void (*cb)(const std::string&, void*),
						   void* userdata);

	// LLUICtrl overrides since we want the callback to apply to the input line
	void setCommitCallback(void (*cb)(LLUICtrl*, void*));
	void setCallbackUserData(void* data)
	{
		mCommitCallbackUserData = data;
	}

	LL_INLINE void setValue(const LLSD& value) override		{ mSearchLineEditor->setValue(value); }
	LL_INLINE LLSD getValue() const override				{ return mSearchLineEditor->getValue(); }

	LL_INLINE bool setTextArg(const std::string& key,
							  const std::string& text) override
	{
		return mSearchLineEditor->setTextArg(key, text);
	}

	LL_INLINE bool setLabelArg(const std::string& key,
							   const std::string& text) override
	{
		return mSearchLineEditor->setLabelArg(key, text);
	}

	void clear() override;

private:
	// These are wrappers around the LLSearchEditor commits
	static void onSearchEditCommit(LLUICtrl* ctrl, void* data);
	static void onSearchEditKeystroke(LLLineEditor* caller, void* data);
	static void onClearSearch(void* user_data);

private:
	LLLineEditor*	mSearchLineEditor;
	LLButton*		mClearSearchButton;

	void			(*mLineCommitCallback)(LLUICtrl* ctrl, void* user_data);
	void			(*mSearchCallback)(const std::string& search_string,
									   void* user_data);
	void*			mCommitCallbackUserData;
};

#endif  // LL_LINEEDITOR_
