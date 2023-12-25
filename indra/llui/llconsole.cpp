/**
 * @file llconsole.cpp
 * @brief a scrolling console output device
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

#include "llconsole.h"

#include "llgl.h"
#include "llstring.h"
#include "llstl.h"				// For DeletePointer()

// HACK: Defined in newview/llviewerwindow.cpp
extern S32 viewer_window_width();
extern S32 viewer_window_height();

// To be used for the main (chat) console only !
LLConsole* gConsolep = NULL;

constexpr F32 FADE_DURATION = 2.f;
constexpr S32 MIN_CONSOLE_WIDTH = 50;

// Why do not these match ?
constexpr S32 CONSOLE_GUTTER_LEFT = 14;
constexpr S32 CONSOLE_GUTTER_RIGHT = 15;

// Static variables
LLColor4 LLConsole::sConsoleBackground;

LLConsole::LLConsole(const std::string& name, const LLRect& rect,
					 S32 font_size_index, U32 max_lines, F32 persist_time)
 :	LLLineBuffer(),
	LLView(name, rect, false),
	mMaxLines(max_lines),
	mLinePersistTime(persist_time),
	mFadeTime(persist_time - FADE_DURATION),
	mFont(LLFontGL::getFontSansSerif()),
	mConsoleWidth(0),
	mConsoleHeight(0)
{
	mTimer.reset();
	setFontSize(font_size_index);
}

LLConsole::~LLConsole()
{
	clear();
}

//static
void LLConsole::setBackground(const LLColor4& color, F32 opacity)
{
	sConsoleBackground = color;
	sConsoleBackground.mV[VW] *= llclamp(opacity, 0.f, 1.f);
}

void LLConsole::setLinePersistTime(F32 seconds)
{
	mLinePersistTime = seconds;
	mFadeTime = mLinePersistTime - FADE_DURATION;
}

void LLConsole::reshape(S32 width, S32 height, bool called_from_parent)
{
	S32 new_width = llmax(MIN_CONSOLE_WIDTH,
						  llmin(width, viewer_window_width()));
    S32 new_height = llclamp((S32)mLineHeight + 15, getRect().getHeight(),
							 viewer_window_height());

	if (mConsoleWidth == new_width)
	{
		return;
	}

	mConsoleWidth = new_width;
	mConsoleHeight= new_height;

	LLView::reshape(new_width, new_height, called_from_parent);

	for (paragraph_t::iterator it = mParagraphs.begin(),
							   end = mParagraphs.end();
		 it != end; ++it)
	{
		(*it)->updateLines((F32)getRect().getWidth(), mFont, true);
	}
}

void LLConsole::setFontSize(S32 size_index)
{
	mFontSize = size_index;
	if (size_index == -1)
	{
		mFont = LLFontGL::getFontMonospace();
	}
	else if (size_index == 0)
	{
		mFont = LLFontGL::getFontSansSerif();
	}
	else if (size_index == 1)
	{
		mFont = LLFontGL::getFontSansSerifBig();
	}
	else
	{
		mFont = LLFontGL::getFontSansSerifHuge();
	}

	mLineHeight = mFont->getLineHeight();

	for (paragraph_t::iterator it = mParagraphs.begin(),
							   end = mParagraphs.end();
		 it != end; ++it)
	{
		(*it)->updateLines((F32)getRect().getWidth(), mFont, true);
	}
}

void LLConsole::draw()
{
	mQueueMutex.lock();

	for (paragraph_t::iterator it = mNewParagraphs.begin(),
							   end = mNewParagraphs.end();
		 it != end; ++it)
	{
		Paragraph* paragraph = *it;
		if (paragraph)
		{
			mParagraphs.push_back(paragraph);
			paragraph->updateLines((F32)getRect().getWidth(), mFont);
		}
	}
	mNewParagraphs.clear();

	if (mParagraphs.empty()) // No text to draw.
	{
		mQueueMutex.unlock();
		return;
	}

	// Skip lines added more than mLinePersistTime ago
	F32 cur_time = mTimer.getElapsedTimeF32();

	F32 skip_time = cur_time - mLinePersistTime;
	F32 fade_time = cur_time - mFadeTime;

	U32 num_lines = 0;

	paragraph_t::reverse_iterator rit;
	rit = mParagraphs.rbegin();
	U32 paragraph_num = mParagraphs.size();

	while (!mParagraphs.empty() && rit != mParagraphs.rend())
	{
		Paragraph* para = *rit;
		num_lines += para->mLines.size();
		if (num_lines > mMaxLines ||
			(mLinePersistTime > 0.f &&
			 (para->mAddTime - skip_time) /
			  (mLinePersistTime - mFadeTime) <= 0.f))
		{
			// All lines above here are done. Lose them.
			for (U32 i = 0; i < paragraph_num; ++i)
			{
				if (!mParagraphs.empty())
				{
					Paragraph* paragraph = mParagraphs.front();
					mParagraphs.pop_front();
					delete paragraph;
				}
			}
			break;
		}
		--paragraph_num;
		++rit;
	}

	if (mParagraphs.empty())
	{
		mQueueMutex.unlock();
		return;
	}

	// Draw remaining lines
	F32 y_pos = 0.f;

	S32 message_spacing = 4;
	S32 target_height = 0;
	S32 target_width = 0;

	LLGLSUIDefault gls_ui;

	if (!LLUI::sConsoleBoxPerMessage)
	{
		// This section makes a single huge black box behind all the text.
		S32 bkg_height = 4;
		if (LLUI::sDisableMessagesSpacing)
		{
			message_spacing = 0;
			bkg_height = 8;
		}
		S32 bkg_width = 0;
		for (paragraph_t::reverse_iterator rit = mParagraphs.rbegin(),
										   rend = mParagraphs.rend();
			 rit != rend; ++rit)
		{
			Paragraph* para = *rit;

			target_height = llfloor(para->mLines.size() *
									mLineHeight + message_spacing);
			target_width = llfloor(para->mMaxWidth + CONSOLE_GUTTER_RIGHT);

			bkg_height += target_height;
			if (target_width > bkg_width)
			{
				bkg_width = target_width;
			}

			// Why is this not using llfloor as above ?
			y_pos += para->mLines.size() * mLineHeight;
			y_pos += message_spacing;  // Extra spacing between messages.
		}
		LLUIImage::sRoundedSquare->drawSolid(-CONSOLE_GUTTER_LEFT,
											 (S32)(y_pos + mLineHeight -
												   bkg_height -
												   message_spacing),
												   bkg_width, bkg_height,
												   sConsoleBackground);
	}
	y_pos = 0.f;

	for (paragraph_t::reverse_iterator rit = mParagraphs.rbegin(),
									   rend = mParagraphs.rend();
		 rit != rend; ++rit)
	{
		Paragraph* para = *rit;
		target_width = llfloor(para->mMaxWidth + CONSOLE_GUTTER_RIGHT);
		y_pos += para->mLines.size() * mLineHeight;

		if (LLUI::sConsoleBoxPerMessage)
		{
			// Per-message block boxes
			target_height = llfloor(para->mLines.size() * mLineHeight + 8);
			LLUIImage::sRoundedSquare->drawSolid(-CONSOLE_GUTTER_LEFT,
												 (S32)(y_pos + mLineHeight -
													   target_height),
													   target_width,
													   target_height,
													   sConsoleBackground);
		}

		F32 y_off = 0;

		F32 alpha;
		if (mLinePersistTime > 0.f && para->mAddTime < fade_time)
		{
			alpha = (para->mAddTime - skip_time) /
					(mLinePersistTime - mFadeTime);
		}
		else
		{
			alpha = 1.f;
		}

		if (alpha > 0.f)
		{
			for (lines_t::iterator line_it = para->mLines.begin(),
								   end = para->mLines.end();
				 line_it != end; ++line_it)
			{
				for (line_color_segments_t::iterator seg_it = line_it->begin(),
													 end2 = line_it->end();
					 seg_it != end2; ++seg_it)
				{
					const LLColor4& scolor = seg_it->mColor;
					LLColor4 color(scolor.mV[VX], scolor.mV[VY], scolor.mV[VZ],
								   scolor.mV[VW] * alpha);
					mFont->render(seg_it->mText, 0,
								  seg_it->mXPosition - 8, y_pos -  y_off,
								  color, LLFontGL::LEFT, LLFontGL::BASELINE,
								  LLFontGL::DROP_SHADOW, S32_MAX,
								  target_width);
				}
				y_off += mLineHeight;
			}
		}
		y_pos += message_spacing;  // Extra spacing between messages.
	}

	mQueueMutex.unlock();
}

//virtual
void LLConsole::clear()
{
	mTimer.reset();
	mQueueMutex.lock();
	std::for_each(mParagraphs.begin(), mParagraphs.end(), DeletePointer());
	mParagraphs.clear();
	std::for_each(mNewParagraphs.begin(), mNewParagraphs.end(),
				  DeletePointer());
	mNewParagraphs.clear();
	mQueueMutex.unlock();
}

//virtual
void LLConsole::addLine(const std::string& utf8line)
{
	addConsoleLine(utf8line, LLColor4::white);
}

void LLConsole::addConsoleLine(const std::string& utf8line,
							   const LLColor4& color)
{
	LLWString wline = utf8str_to_wstring(utf8line);
	addConsoleLine(wline, color);
}

void LLConsole::addConsoleLine(const LLWString& wline, const LLColor4& color)
{
	Paragraph* paragraph = new Paragraph(wline, color,
										 mTimer.getElapsedTimeF32());
	mQueueMutex.lock();
	mNewParagraphs.push_back(paragraph);
	mQueueMutex.unlock();
}

void LLConsole::replaceParaText(Paragraph* para, const LLWString& search_text,
								const LLWString& replace_text,
								bool case_insensitive, bool new_paragraph)
{
	if (!para) return;

	size_t search_length = search_text.size();
	size_t replace_length = replace_text.size();
	S32 offset = replace_length - search_length;

	LLWString final_text, para_text;
	final_text = para_text = para->mParagraphText;
	if (case_insensitive)
	{
		LLWStringUtil::toLower(para_text);
	}

	bool replaced = false;
	size_t pos;
	while ((pos = para_text.find(search_text)) != std::string::npos)
	{
		replaced = true;
		if (pos == 0)
		{
			para_text.clear();
		}
		else
		{
			para_text = final_text.substr(0, pos);
		}
		para_text += replace_text;
		if (pos + search_length < final_text.size())
		{
			para_text += final_text.substr(pos + search_length);
		}
		final_text = para_text;
		if (case_insensitive)
		{
			LLWStringUtil::toLower(para_text);
		}

		// Update the corresponding segment length when the search and replace
		// texts lengths differ.
		if (offset != 0)
		{
			S32 seg_pos = 0;
			for (paragraph_color_segments_t::iterator
					seg_it = para->mParagraphColorSegments.begin(),
					seg_end = para->mParagraphColorSegments.end();
				 seg_it != seg_end; ++seg_it)
			{
				ParagraphColorSegment segment = *seg_it;
				if (seg_pos >= (S32)pos)
				{
					segment.mNumChars += offset;
					if (segment.mNumChars <= 0) // Empty replacement text ?
					{
						para->mParagraphColorSegments.erase(seg_it);
					}
					break;
				}
				seg_pos += segment.mNumChars;
			}
		}
	}
	if (replaced)
	{
		para->mParagraphText = final_text;
		if (offset != 0 && !new_paragraph)
		{
			para->updateLines((F32)getRect().getWidth(), mFont, true);
		}
	}
}

void LLConsole::replaceAllText(const std::string& search_txt,
							   const std::string& replace_txt,
							   bool case_insensitive)
{
	LLWString search_text = utf8str_to_wstring(search_txt);
	if (case_insensitive)
	{
		LLWStringUtil::toLower(search_text);
	}

	LLWString replace_text = utf8str_to_wstring(replace_txt);

	for (paragraph_t::iterator it = mParagraphs.begin(),
							   end = mParagraphs.end();
		 it != end; ++it)
	{
		replaceParaText(*it, search_text, replace_text, case_insensitive);
	}

	mQueueMutex.lock();
	for (paragraph_t::iterator it = mNewParagraphs.begin(),
							   end = mNewParagraphs.end();
		 it != end; ++it)
	{
		replaceParaText(*it, search_text, replace_text, case_insensitive,
						true);
	}
	mQueueMutex.unlock();
}

// Called when a paragraph is added to the console or window is resized.
void LLConsole::Paragraph::updateLines(F32 screen_width, LLFontGL* font,
									   bool force_resize)
{
	if (!force_resize && mMaxWidth >= 0.f && mMaxWidth < screen_width)
	{
		return;			// No resize required.
	}

	if (mParagraphText.empty() || mParagraphColorSegments.empty() || !font)
	{
		return;			// Not enough info to complete.
	}

	screen_width -= 30;	// Margin for small windows.

	mLines.clear();		// Chuck everything.
	mMaxWidth = 0.f;

	paragraph_color_segments_t::iterator current_color =
		mParagraphColorSegments.begin();
	paragraph_color_segments_t::iterator end_segments =
		mParagraphColorSegments.end();
	U32 current_color_length = current_color->mNumChars;

	// Wrap lines that are longer than the view is wide.
	S32 paragraph_text_length = mParagraphText.size();
	S32 paragraph_offset = 0;	// Offset into the paragraph text.
	while (paragraph_offset < paragraph_text_length)
	{
		bool found_newline = false; // skip '\n'
		// Figure out if a word-wrapped line fits here.
		LLWString::size_type line_end =
			mParagraphText.find_first_of(llwchar('\n'), paragraph_offset);
		if (line_end != LLWString::npos)
		{
			found_newline = true; // skip '\n'
		}
		else
		{
			line_end = paragraph_text_length;
		}

		U32 drawable = font->maxDrawableChars(mParagraphText.c_str() +
											  paragraph_offset,
											  screen_width,
											  line_end - paragraph_offset,
											  true);

		if (drawable != 0 || found_newline)
		{
			F32 x_position = 0;	// Screen X position of text.

			mMaxWidth =
				llmax(mMaxWidth,
					  (F32)font->getWidth(mParagraphText.substr(paragraph_offset,
																drawable).c_str()));
			line_color_segments_t line;

			U32 left_to_draw = drawable;
			U32 drawn = 0;

			while (left_to_draw >= current_color_length &&
				   current_color != end_segments)
			{
				LLWString color_text =
					mParagraphText.substr(paragraph_offset + drawn,
										  current_color_length);
				// Append segment to line.
				line.emplace_back(color_text, current_color->mColor,
								  x_position);

				// Set up next screen position.
				x_position += font->getWidth(color_text.c_str());

				drawn += current_color_length;
				left_to_draw -= current_color_length;

				// Goto next paragraph color record.
				current_color++;

				if (current_color != end_segments)
				{
					current_color_length = current_color->mNumChars;
				}
			}

			if (left_to_draw > 0 && current_color != end_segments)
			{
				LLWString color_text =
					mParagraphText.substr(paragraph_offset + drawn,
										  left_to_draw);

				// Append segment to line.
				line.emplace_back(color_text, current_color->mColor,
								  x_position);

				current_color_length -= left_to_draw;
			}
			// Append line to paragraph line list.
			mLines.emplace_back(line);
		}
		else
		{
			break; // Nothing more to print
		}

		paragraph_offset += drawable + (found_newline ? 1 : 0);
	}
}

// Pass in the string and the default color for this block of text.
LLConsole::Paragraph::Paragraph(LLWString str, const LLColor4& color,
								F32 add_time)
:	mParagraphText(str),
	mAddTime(add_time),
	mMaxWidth(-1)
{
	// Generate one highlight color segment for this paragraph.
	mParagraphColorSegments.emplace_back(wstring_to_utf8str(str).size(),
										 color);
}
