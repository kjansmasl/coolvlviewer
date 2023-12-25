/**
 * @file lltexteditor.h
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

// Text editor widget to let users enter a a multi-line ASCII document//

#ifndef LL_LLTEXTEDITOR_H
#define LL_LLTEXTEDITOR_H

#include "lleditmenuhandler.h"
#include "llframetimer.h"
#include "llkeywords.h"
#include "llpreeditor.h"
#include "llpreprocessor.h"
#include "llstyle.h"
#include "llrect.h"
#include "lluictrl.h"

#include "llpreeditor.h"

class LLFontGL;
class LLScrollbar;
class LLViewBorder;
class LLKeywordToken;
class LLMenuItemCallGL;
class LLTextCmd;
class LLUICtrlFactory;

constexpr llwchar FIRST_EMBEDDED_CHAR = 0x100000;
constexpr llwchar LAST_EMBEDDED_CHAR =  0x10ffff;
constexpr S32 MAX_EMBEDDED_ITEMS = LAST_EMBEDDED_CHAR -
								   FIRST_EMBEDDED_CHAR + 1;

class LLTextEditor : public LLUICtrl, public LLEditMenuHandler,
					 protected LLPreeditor
{
protected:
	LOG_CLASS(LLTextEditor);

public:
	// Constants
	enum HighlightPosition	{ WHOLE, START, MIDDLE, END };

	LLTextEditor(const std::string& name, const LLRect& rect,
				 S32 max_length, const std::string& default_text,
				 LLFontGL* glfont = NULL,
				 bool allow_embedded_items = false);

	~LLTextEditor() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView*	fromXML(LLXMLNodePtr node, LLView* parent,
							class LLUICtrlFactory* factory);
	void setTextEditorParameters(LLXMLNodePtr node);

	LL_INLINE void setParseHTML(bool parsing)					{ mParseHTML = parsing; }

	// Mouse handler overrides
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleMiddleMouseDown(S32 x,S32 y,MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;

	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleUnicodeCharHere(llwchar uni_char) override;

	bool handleToolTip(S32 x, S32 y, std::string& msg, LLRect* rect) override;
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type,
						   void* cargo_data, EAcceptance* accept,
						   std::string& tooltip_msg) override;
	void onMouseCaptureLost() override;

	// LLView overrides
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;
	void draw() override;
	void onFocusReceived() override;
	void onFocusLost() override;
	void setEnabled(bool enabled) override;

	// LLUICtrl overrides
	void onTabInto() override;
	void clear() override;
	void setFocus(bool b) override;

	LL_INLINE bool acceptsTextInput() const override			{ return !mReadOnly; }

	LL_INLINE bool isDirty() const override						{ return mLastCmd || (mPristineCmd && mPristineCmd != mLastCmd); }

	// Returns true if user changed value at all
	LL_INLINE virtual bool isSpellDirty() const					{ return mWText != mPrevSpelledText; }
	// Clear dirty state
	LL_INLINE virtual void resetSpellDirty()					{ mPrevSpelledText = mWText; }

	struct SpellMenuBind
	{
		LLTextEditor*		mOrigin;
		LLMenuItemCallGL*	mMenuItem;
		std::string			mWord;
		S32					mWordPositionStart;
		S32					mWordPositionEnd;
		S32					mWordY;
	};

	std::vector<S32> getMisspelledWordsPositions();
	virtual void spellReplace(SpellMenuBind* data);

	bool getWordBoundriesAt(const S32 at, S32* word_begin,
							S32* word_length) const;

	// LLEditMenuHandler interface

	void undo() override;
	bool canUndo() const override;

	void redo() override;
	bool canRedo() const override;

	void cut() override;
	bool canCut() const override;

	void copy() override;
	bool canCopy() const override;

	void paste() override;
	bool canPaste() const override;

	void doDelete() override;
	bool canDoDelete() const override;

	void selectAll() override;
	LL_INLINE bool canSelectAll() const override				{ return true; }

	virtual void deselect() override;
	LL_INLINE virtual bool canDeselect() const override			{ return hasSelection(); }

	// New methods
	virtual void updatePrimary();
	virtual void copyPrimary();
	virtual void pastePrimary();
	virtual bool canPastePrimary() const;

	void selectNext(const std::string& search_text_in, bool case_insensitive,
					bool wrap = true);
	bool replaceText(const std::string& search_text,
					 const std::string& replace_text, bool case_insensitive,
					 bool wrap = true);
	void replaceTextAll(const std::string& search_text,
						const std::string& replace_text,
						bool case_insensitive);

	// Undo/redo stack
	void blockUndo();

	// Text editing
	virtual void makePristine();
	bool isPristine() const;
	LL_INLINE bool allowsEmbeddedItems() const					{ return mAllowEmbeddedItems; }

	LL_INLINE void setPreserveSegments(bool b)					{ mPreserveSegments = b; }
	// Inserts text at cursor
	void insertText(const std::string& text);
	// Appends text at end
	void appendText(const std::string& wtext, bool allow_undo,
					bool prepend_newline, const LLStyleSP stylep = NULL);

	void appendColoredText(const std::string& wtext, bool allow_undo,
						   bool prepend_newline, const LLColor4& color,
						   const std::string& font_name = LLStringUtil::null);
	// If styled text starts a line, you need to prepend a newline.
	void appendStyledText(const std::string& new_text, bool allow_undo,
						  bool prepend_newline, LLStyleSP stylep = NULL);

	// Tries and removes the first line. Returns the number of removed
	// characters (0 when nothing could be removed).
	S32 removeFirstLine();

	// Removes text from the end of document
	// Does not change highlight or cursor position.
	void removeTextFromEnd(S32 num_chars);

	bool tryToRevertToPristineState();

	void setCursor(S32 row, S32 column);
	void setCursorPos(S32 offset);
	void setCursorAndScrollToEnd();
	void scrollToPos(S32 pos);

	LL_INLINE S32 getCursorPos()								{ return mCursorPos; }

	void getLineAndColumnForPosition(S32 position, S32* line, S32* col,
									 bool include_wordwrap);
	void getCurrentLineAndColumn(S32* line, S32* col, bool include_wordwrap);
	S32 getLineForPosition(S32 position);
	S32 getCurrentLine();

	void loadKeywords(const std::string& filename,
					  const std::vector<std::string>& funcs,
					  const std::vector<std::string>& tooltips,
					  const LLColor3& func_color);
	LL_INLINE LLKeywords::keyword_iterator_t keywordsBegin()	{ return mKeywords.begin(); }
	LL_INLINE LLKeywords::keyword_iterator_t keywordsEnd()		{ return mKeywords.end(); }

	// Color support
	LL_INLINE void setCursorColor(const LLColor4& c)			{ mCursorColor = c; }
	LL_INLINE void setFgColor(const LLColor4& c)				{ mFgColor = c; }
	LL_INLINE void setTextDefaultColor(const LLColor4& c)		{ mDefaultColor = c; }
	LL_INLINE void setReadOnlyFgColor(const LLColor4& c)		{ mReadOnlyFgColor = c; }
	LL_INLINE void setWriteableBgColor(const LLColor4& c)		{ mWriteableBgColor = c; }
	LL_INLINE void setReadOnlyBgColor(const LLColor4& c)		{ mReadOnlyBgColor = c; }
	void setTrackColor(const LLColor4& color);
	void setThumbColor(const LLColor4& color);
	void setHighlightColor(const LLColor4& color);
	void setShadowColor(const LLColor4& color);
	LL_INLINE LLColor4 getReadOnlyFgColor()						{ return mReadOnlyFgColor; }
	LL_INLINE void setSpellCheck(bool b)						{ mSpellCheck = b; }
	LL_INLINE bool getSpellCheck()								{ return mSpellCheck; }

	// Hacky methods to make it into a word-wrapping, potentially scrolling,
	// read-only text box.
	void setBorderVisible(bool b);
	bool isBorderVisible() const;
	void setHideScrollbarForShortDocs(bool b);

	void setWordWrap(bool b);
	LL_INLINE void setTabsToNextField(bool b)					{ mTabsToNextField = b; }
	LL_INLINE bool tabsToNextField() const						{ return mTabsToNextField; }
	LL_INLINE void setCommitOnFocusLost(bool b)					{ mCommitOnFocusLost = b; }

	// Hack to handle Notecards
	virtual bool importBuffer(const char* buffer, S32 length);
	virtual bool exportBuffer(std::string& buffer);

	LL_INLINE void setHandleEditKeysDirectly(bool b)			{ mHandleEditKeysDirectly = b; }

	LL_INLINE void setLinkColor(LLColor4 color)					{ mLinkColor = color; }

	LL_INLINE static void setLinksColor(const LLColor4& color)	{ sLinkColor = color; }
	LL_INLINE static LLColor4& getLinksColor()					{ return sLinkColor; }

	// Callbacks
	static void setURLCallbacks(void (*callback1)(const std::string& url),
								bool (*callback2)(const std::string& url),
								bool (*callback3)(const std::string& url))
	{
		sURLcallback = callback1;
		sSecondlifeURLcallback = callback2;
		sSecondlifeURLcallbackRightClick = callback3;
	}

	void setOnScrollEndCallback(void (*callback)(void*), void* userdata);
	void setKeystrokeCallback(void (*callback)(LLTextEditor*, void*),
							  void* userdata);
	void setOnHandleKeyCallback(bool (*callback)(KEY, MASK, LLTextEditor*,
												 void*),
								void* userdata);

	// New methods
	void setValue(const LLSD& value) override;
	LLSD getValue() const override;

 	const std::string& getText() const;

	// Non-undoable
	void setText(const std::string& utf8str);
	void setWText(const LLWString& wtext);

	void setSelection(S32 start, S32 end);

	// Returns byte length limit
	LL_INLINE S32 getMaxLength() const 							{ return mMaxTextByteLength; }

	// Change cursor
	void startOfLine();
	void endOfLine();
	void endOfDoc();

	bool isScrolledToTop();
	bool isScrolledToBottom();

	LL_INLINE const LLWString& getWText() const override		{ return mWText; }
	LL_INLINE llwchar getWChar(S32 pos) const					{ return mWText[pos]; }
	LL_INLINE LLWString getWSubString(S32 pos, S32 len) const	{ return mWText.substr(pos, len); }
	LL_INLINE S32 getLength() const								{ return mWText.length(); }

	LL_INLINE const LLTextSegment* getCurrentSegment() const
	{
		return getSegmentAtOffset(mCursorPos);
	}

	const LLTextSegment* getPreviousSegment() const;
	void getSelectedSegments(std::vector<const LLTextSegment*>& segments) const;

	LL_INLINE bool isReadOnly()									{ return mReadOnly; }

	LL_INLINE void setFont(LLFontGL* fontp)						{ mGLFont = fontp; }

protected:
	void getSegmentAndOffset(S32 startpos, S32* segidxp, S32* offsetp) const;
	void drawPreeditMarker();

	void updateLineStartList(S32 startpos = 0);
	void updateScrollFromCursor();
	void updateTextRect();
	LL_INLINE const LLRect& getTextRect() const					{ return mTextRect; }

	void assignEmbedded(const std::string& s);
	bool truncate();	// Returns true if truncation occurs

	void removeCharOrTab();
	void setCursorAtLocalPos(S32 x, S32 y, bool round);
	S32 getCursorPosFromLocalCoord(S32 local_x, S32 local_y, bool round) const;

	void indentSelectedLines(S32 spaces);
	S32 indentLine(S32 pos, S32 spaces);
	void unindentLineBeforeCloseBrace();

	S32 getSegmentIdxAtOffset(S32 offset) const;
	const LLTextSegment* getSegmentAtLocalPos(S32 x, S32 y) const;
	const LLTextSegment* getSegmentAtOffset(S32 offset) const;

	LL_INLINE void reportBadKeystroke()							{ make_ui_sound("UISndBadKeystroke"); }

	bool handleNavigationKey(KEY key, MASK mask);
	bool handleSpecialKey(KEY key, MASK mask, bool* ret_key_hit);
	bool handleSelectionKey(KEY key, MASK mask);
	bool handleControlKey(KEY key, MASK mask);
	bool handleEditKey(KEY key, MASK mask);
	
	LL_INLINE bool hasSelection() const							{ return mSelectionStart != mSelectionEnd; }
	bool selectionContainsLineBreaks();
	void startSelection();
	void endSelection();
	void deleteSelection(bool transient_operation);

	S32 prevWordPos(S32 cursorPos) const;
	S32 nextWordPos(S32 cursorPos) const;

	LL_INLINE S32 getLineCount() const							{ return mLineStartList.size(); }
	S32 getLineStart(S32 line) const;
	void getLineAndOffset(S32 pos, S32* linep, S32* offsetp) const;
	S32 getPos(S32 line, S32 offset);

	void changePage(S32 delta);
	void changeLine(S32 delta);

	void autoIndent();

	void findEmbeddedItemSegments();

	virtual bool handleMouseUpOverSegment(S32 x, S32 y, MASK mask);

	LL_INLINE virtual llwchar pasteEmbeddedItem(llwchar chr)	{ return chr; }
	virtual void bindEmbeddedChars(LLFontGL*) const				{}
	virtual void unbindEmbeddedChars(LLFontGL*) const			{}

	S32 findHTMLToken(const std::string& line, S32 pos, bool reverse) const;
	bool findHTML(const std::string& line, S32* begin, S32* end) const;

	// Abstract inner base class representing an undoable editor command.
	// Concrete sub-classes can be defined for operations such as insert,
	// remove, etc. Used as arguments to the execute() method below.
	class LLTextCmd
	{
	public:
		LLTextCmd(S32 pos, bool group_with_next)
		:	mPos(pos),
		mGroupWithNext(group_with_next)
		{
		}

		virtual ~LLTextCmd() = default;
		virtual bool execute(LLTextEditor* editor, S32* delta) = 0;
		virtual S32 undo(LLTextEditor* editor) = 0;
		virtual S32 redo(LLTextEditor* editor) = 0;
		LL_INLINE virtual bool canExtend(S32 pos) const			{ return false; }
		virtual void blockExtensions()							{}

		virtual bool extendAndExecute(LLTextEditor* editor, S32 pos, llwchar c,
									  S32* delta)
		{
			llassert(false);
			return 0;
		}

		LL_INLINE virtual bool hasExtCharValue(llwchar value) const
		{
			return false;
		}

		// Defined here so they can access protected LLTextEditor editing
		// methods
		LL_INLINE S32 insert(LLTextEditor* editor, S32 pos,
							 const LLWString& wstr)
		{
			return editor->insertStringNoUndo(pos, wstr);
		}

		LL_INLINE S32 remove(LLTextEditor* editor, S32 pos, S32 length)
		{
			return editor->removeStringNoUndo(pos, length);
		}

		LL_INLINE S32	overwrite(LLTextEditor* editor, S32 pos, llwchar wc)
		{
			return editor->overwriteCharNoUndo(pos, wc);
		}

		LL_INLINE S32 getPosition() const						{ return mPos; }
		LL_INLINE bool groupWithNext() const					{ return mGroupWithNext; }

	private:
		const S32	mPos;
		bool		mGroupWithNext;
	};
	// Takes and applies text commands.
	S32 execute(LLTextCmd* cmd);

	// Undoable operations
	void addChar(llwchar c); // at mCursorPos
	S32 addChar(S32 pos, llwchar wc);
	S32 overwriteChar(S32 pos, llwchar wc);
	void removeChar();
	S32 removeChar(S32 pos);
	S32 insert(S32 pos, const LLWString& wstr, bool with_next_op);
	S32 remove(S32 pos, S32 length, bool group_with_next_op);
	S32 append(const LLWString& wstr, bool group_with_next_op);

	// Direct operations

	// Returns number of characters actually inserted
	S32 insertStringNoUndo(S32 pos, const LLWString& wstr);
	S32 removeStringNoUndo(S32 pos, S32 length);
	S32 overwriteCharNoUndo(S32 pos, llwchar wc);

	LL_INLINE void resetKeystrokeTimer()						{ mKeystrokeTimer.reset(); }

	void updateAllowingLanguageInput();
	bool hasPreeditString() const;

	// Overrides LLPreeditor
	void resetPreedit() override;
	void updatePreedit(const LLWString& preedit_string,
					   const segment_lengths_t& preedit_segment_lengths,
					   const standouts_t& preedit_standouts,
					   S32 caret_position) override;
	void markAsPreedit(S32 position, S32 length) override;
	void getPreeditRange(S32* position, S32* length) const override;
	void getSelectionRange(S32* position, S32* length) const override;
	bool getPreeditLocation(S32 query_offset, LLCoordGL* coord,
							LLRect* bounds, LLRect* control) const override;
	S32 getPreeditFontSize() const override;

	// Context menu actions
	static void spellCorrect(void* data);
	static void spellShow(void* data);
	static void spellAdd(void* data);
	static void spellIgnore(void* data);

private:
	void pasteHelper(bool is_primary);

	void updateSegments();
	void pruneSegments();

	void drawBackground();
	void drawSelectionBackground();
	void drawCursor();
	void drawMisspelled();
	void drawText();
	void drawClippedSegment(const LLWString& wtext, S32 seg_start, S32 seg_end,
							F32 x, F32 y, S32 sel_left, S32 sel_right,
							const LLStyleSP& color, F32* right_x);

	LL_INLINE void	needsReflow()
	{
		mReflowNeeded = true;
		// Cursor might have moved, need to scroll
		mScrollNeeded = true;
	}

	LL_INLINE void needsScroll()								{ mScrollNeeded = true; }

private:
	LLKeywords		mKeywords;
	static LLColor4	sLinkColor;
	static void		(*sURLcallback)(const std::string& url);
	static bool		(*sSecondlifeURLcallback)(const std::string& url);
	static bool		(*sSecondlifeURLcallbackRightClick)(const std::string& url);

	// List of offsets and segment index of the start of each line. Always has
	// at least one node (0).
	struct line_info
	{
		line_info(S32 segment, S32 offset)
		:	mSegment(segment),
			mOffset(offset)
		{
		}

		S32 mSegment;
		S32 mOffset;
	};

	struct line_info_compare
	{
		LL_INLINE bool operator()(const line_info& a, const line_info& b) const
		{
			if (a.mSegment < b.mSegment)
			{
				return true;
			}
			else if (a.mSegment > b.mSegment)
			{
				return false;
			}
			else
			{
				return a.mOffset < b.mOffset;
			}
		}
	};

	// Concrete LLTextCmd sub-classes used by the LLTextEditor base class
	class LLTextCmdInsert;
	class LLTextCmdAddChar;
	class LLTextCmdOverwriteChar;
	class LLTextCmdRemove;

protected:
	// Scrollbar data
	class LLScrollbar*			mScrollbar;
	void						(*mOnScrollEndCallback)(void*);
	void*						mOnScrollEndData;

	void						(*mKeystrokeCallback)(LLTextEditor*, void*);
	void*						mKeystrokeData;

	bool						(*mOnHandleKeyCallback)(KEY, MASK,
														LLTextEditor*, void*);
	void*						mOnHandleKeyData;

	// I-beam is just after the mCursorPos-th character.
	S32							mCursorPos;

	// Use these to determine if a click on an embedded item is a drag or not.
	S32							mMouseDownX;
	S32							mMouseDownY;

	// Are we in the middle of a drag-select?  To figure out if there is a current
	// selection, call hasSelection().
	S32							mSelectionStart;
	S32							mSelectionEnd;
	S32							mLastSelectionX;
	S32							mLastSelectionY;
	std::string					mHTML;

	typedef std::vector<LLTextSegment*> segment_list_t;
	segment_list_t				mSegments;
	const LLTextSegment*		mHoverSegment;

	LLWString					mPreeditWString;
	LLWString					mPreeditOverwrittenWString;
	std::vector<S32> 			mPreeditPositions;
	LLPreeditor::standouts_t	mPreeditStandouts;

	bool						mIsSelecting;
	bool						mParseHTML;

	// Scrollbar
	bool						mHideScrollbarForShortDocs;

private:
	mutable std::string			mUTF8Text;
	LLWString					mWText;

	// Spell checking
	LLWString					mPrevSpelledText;		// saved string so we know whether to respell or not
	S32							mSpellCheckStart;
	S32							mSpellCheckEnd;
	std::vector<S32>			mMisspellLocations;		// where all the misspelled words are
	LLFrameTimer				mSpellTimer;
	std::vector<SpellMenuBind*>	mSuggestionMenuItems;	// to keep track of what we have to remove before rebuilding the context menu
	bool						mSpellCheck;			// set in xui as "spell_check". Default value for a field
	bool						mShowMisspelled;		// whether to highlight misspelled words or not

	S32							mMaxTextByteLength;		// Maximum length mText is allowed to be in bytes

	LLFontGL*					mGLFont;

	class LLViewBorder*			mBorder;

	LLTextCmd*					mPristineCmd;

	LLTextCmd*					mLastCmd;

	typedef std::deque<LLTextCmd*> undo_stack_t;
	undo_stack_t				mUndoStack;

	// X pixel position where the user wants the cursor to be
	S32							mDesiredXPixel;

	// The rect in which text is drawn. Excludes borders.
	LLRect						mTextRect;

	typedef std::vector<line_info> line_list_t;
	line_list_t					mLineStartList;

	LLFrameTimer				mKeystrokeTimer;

	LLColor4					mCursorColor;
	LLColor4					mFgColor;
	LLColor4					mDefaultColor;
	LLColor4					mReadOnlyFgColor;
	LLColor4					mWriteableBgColor;
	LLColor4					mReadOnlyBgColor;
	LLColor4					mFocusBgColor;
	LLColor4					mLinkColor;

	// Last position of the IME editor
	LLCoordGL					mLastIMEPosition;

	bool						mBaseDocIsPristine;

	bool						mReflowNeeded;
	bool						mScrollNeeded;

	bool						mReadOnly;
	bool						mWordWrap;
	bool						mShowLineNumbers;

	// if true, tab moves focus to next field, else inserts spaces
	bool						mTabsToNextField;

	bool						mCommitOnFocusLost;

	// if true, keeps scroll position at bottom during resize
	bool						mTrackBottom;

	bool						mScrolledToBottom;

	bool						mAllowEmbeddedItems;

	// HACK: used to preserve segments on replaceText[All]() in chat
	bool						mPreserveSegments;

	// If true, the standard edit keys (Ctrl-X, Delete, etc,) are handled here
	// instead of routed by the menu system
	bool						mHandleEditKeysDirectly;

	mutable bool				mTextIsUpToDate;
};

class LLTextSegment
{
protected:
	LOG_CLASS(LLTextSegment);

public:
	// For creating a compare value
	LLTextSegment(S32 start);
	LLTextSegment(const LLStyleSP& style, S32 start, S32 end);
	LLTextSegment(const LLColor4& color, S32 start, S32 end, bool is_visible);
	LLTextSegment(const LLColor4& color, S32 start, S32 end);
	LLTextSegment(const LLColor3& color, S32 start, S32 end);

	LL_INLINE S32 getStart() const								{ return mStart; }
	LL_INLINE S32 getEnd() const								{ return mEnd; }
	LL_INLINE void setEnd(S32 end)								{ mEnd = end; }
	LL_INLINE const LLColor4& getColor() const					{ return mStyle->getColor(); }
	LL_INLINE void setColor(const LLColor4& c)					{ mStyle->setColor(c); }
	LL_INLINE const LLStyleSP& getStyle() const					{ return mStyle; }
	LL_INLINE void setStyle(const LLStyleSP& s)					{ mStyle = s; }
	LL_INLINE void setIsDefault(bool b)							{ mIsDefault = b; }
	LL_INLINE bool getIsDefault() const							{ return mIsDefault; }
	LL_INLINE void setToken(LLKeywordToken* t)					{ mToken = t; }
	LL_INLINE LLKeywordToken* getToken() const					{ return mToken; }
	bool getToolTip(std::string& msg) const;

	void dump() const;

	LL_INLINE void shift(S32 offset)
	{
		if (offset + mEnd >= 0 && offset + mStart >= 0)
		{
			mEnd += offset;
			mStart += offset;
		}
	}

	struct compare
	{
		LL_INLINE bool operator()(const LLTextSegment* a,
								  const LLTextSegment* b) const
		{
			return a->mStart < b->mStart;
		}
	};

private:
	LLKeywordToken*	mToken;
	S32				mStart;
	S32				mEnd;
	LLStyleSP		mStyle;
	bool			mIsDefault;
};

#endif  // LL_TEXTEDITOR_
