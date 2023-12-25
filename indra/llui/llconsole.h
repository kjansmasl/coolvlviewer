/**
 * @file llconsole.h
 * @brief a simple console-style output device
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

#ifndef LL_LLCONSOLE_H
#define LL_LLCONSOLE_H

#include <deque>

#include "llerrorcontrol.h" // For LLLineBuffer
#include "llmutex.h"
#include "llview.h"
#include "llcolor4.h"

// Let enough room for the Lua side bar
constexpr S32 CONSOLE_PADDING_LEFT = 48;
constexpr S32 CONSOLE_PADDING_RIGHT = 48;

class LLFontGL;
class LLSD;

class LLConsole final : public LLLineBuffer, public LLView
{
public:
	// font_size_index: -1 = monospace, 0 small, 1 big
	LLConsole(const std::string& name, const LLRect& rect,
			  S32 font_size_index, U32 max_lines, F32 persist_time);
	~LLConsole() override;

	// Overrides
	void draw() override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;
	// From LLLineBuffer
	void clear() override;
	void addLine(const std::string& utf8line) override;

	// Maximum number of lines displayed in the console
	void setMaxLines(U32 lines)				{ mMaxLines = llmax(lines, 5U); }

	// Each line lasts this long after being added
	void setLinePersistTime(F32 seconds);

	// -1 = monospace, 0 means small, font size = 1 means big
	void setFontSize(S32 size_index);

	void addConsoleLine(const std::string& utf8line, const LLColor4& color);
	void addConsoleLine(const LLWString& wline, const LLColor4& color);

	void replaceAllText(const std::string& search_txt,
						const std::string& replace_txt,
						bool case_insensitive);

	// A paragraph color segment defines the color of text in a line of text
	// that was received for console display. It has no notion of line wraps,
	// screen position, or the text it contains.
	// It is only the number of characters that are a color and the color.
	struct ParagraphColorSegment
	{
		ParagraphColorSegment(S32 num_chars, const LLColor4& color)
		:	mNumChars(num_chars),
			mColor(color)
		{
		}

		S32			mNumChars;
		LLColor4	mColor;
	};

	// A line color segment is a chunk of text, the color associated with it,
	// and the X Position it was calculated to begin at on the screen. X
	// positions are re-calculated if the screen changes size.
	class LineColorSegment
	{
		public:
			LineColorSegment(LLWString text, LLColor4 color, F32 xpos)
			:	mText(text),
				mColor(color),
				mXPosition(xpos)
			{
			}

		public:
			LLWString mText;
			LLColor4  mColor;
			F32		  mXPosition;
	};

	typedef std::list<LineColorSegment> line_color_segments_t;
	typedef std::list<line_color_segments_t> lines_t;
	typedef std::list<ParagraphColorSegment> paragraph_color_segments_t;

	// A paragraph is a processed element containing the entire text of the
	// message (used for recalculating positions on screen resize), the time
	// this message was added to the console output, the visual screen width
	// of the longest line in this block and a list of one or more lines which
	// are used to display this message.
	class Paragraph
	{
		public:
			Paragraph(LLWString str, const LLColor4& color, F32 add_time);
			void updateLines(F32 screen_width, LLFontGL* font,
							 bool force_resize = false);
		public:
			// The entire text of the paragraph:
			LLWString					mParagraphText;
			paragraph_color_segments_t	mParagraphColorSegments;
			// Time this paragraph was added to the display:
			F32							mAddTime;
			// Width of the widest line of text in this paragraph:
			F32							mMaxWidth;
			lines_t						mLines;
	};

	static void setBackground(const LLColor4& color, F32 opacity);

	LL_INLINE static const LLColor4& getBackground()
	{
		return sConsoleBackground;
	}

private:
	void replaceParaText(Paragraph* para, const LLWString& search_text,
						 const LLWString& replace_text, bool case_insensitive,
						 bool new_paragraph = false);

public:
	// The console contains a deque of paragraphs which represent the
	// individual messages.
	typedef std::deque<Paragraph*> paragraph_t;
	paragraph_t			mParagraphs;
	paragraph_t			mNewParagraphs;

private:
	LLFontGL*			mFont;
	S32					mFontSize;
	F32					mLineHeight;
	U32					mMaxLines;
	F32					mLinePersistTime;	// Age at which to stop drawing.
	F32					mFadeTime;			// Age at which to start fading
	S32					mConsoleWidth;
	S32					mConsoleHeight;
	LLMutex 			mQueueMutex;
	LLTimer				mTimer;

	static LLColor4		sConsoleBackground;
};

// To be used for the main (chat) console only !
extern LLConsole* gConsolep;

#endif
