/**
 * @file llstreamtools.cpp
 * @brief some helper functions for parsing legacy simstate and asset files.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llstreamtools.h"

// Skips whitespace, newlines, and carriage returns
static bool skip_emptyspace(std::istream& input_stream)
{
	int c = input_stream.peek();
	while (input_stream.good() &&
		   ('\t' == c || ' ' == c || '\n' == c || '\r' == c))
	{
		input_stream.get();
		c = input_stream.peek();
	}
	return input_stream.good();
}

// Skips emptyspace and lines that start with a #
bool skip_comments_and_emptyspace(std::istream& input_stream)
{
	while (skip_emptyspace(input_stream))
	{
		int c = input_stream.peek();
		if ('#' == c )
		{
			while ('\n' != c && input_stream.good())
			{
				c = input_stream.get();
			}
		}
		else
		{
			break;
		}
	}
	return input_stream.good();
}

static bool skip_line(std::istream& input_stream)
{
	int c;
	do
	{
		c = input_stream.get();
	}
	while ('\n' != c && input_stream.good());

	return input_stream.good();
}

bool skip_to_end_of_next_keyword(const char* keyword,
								 std::istream& input_stream)
{
	size_t key_length = strlen(keyword);
	if (!key_length)
	{
		return false;
	}
	while (input_stream.good())
	{
		skip_emptyspace(input_stream);
		int c = input_stream.get();
		if (keyword[0] != c)
		{
			skip_line(input_stream);
		}
		else
		{
			size_t key_index = 1;
			while (key_index < key_length && keyword[key_index - 1] == c &&
				   input_stream.good())
			{
				++key_index;
				c = input_stream.get();
			}

			if (key_index == key_length && keyword[key_index - 1] == c)
			{
				c = input_stream.peek();
				if (' ' == c || '\t' == c || '\r' == c || '\n' == c)
				{
					return true;
				}
				else
				{
					skip_line(input_stream);
				}
			}
			else
			{
				skip_line(input_stream);
			}
		}
	}
	return false;
}

// Gets everything up to and including the next newline up to the next n
// characters. Adds a newline on the end if bail before actual line ending
bool get_line(std::string& output_string, std::istream& input_stream, int n)
{
	output_string.clear();
	int char_count = 0;
	int c = input_stream.get();
	while (input_stream.good() && char_count < n)
	{
		++char_count;
		output_string += c;
		if (c == '\n')
		{
			break;
		}
		if (char_count >= n)
		{
			output_string.append("\n");
			break;
		}
		c = input_stream.get();
	}
	return input_stream.good();
}

// The 'keyword' is defined as the first word on a line. The 'value' is
// everything after the keyword on the same line starting at the first non-
// whitespace and ending right before the newline
void get_keyword_and_value(std::string& keyword, std::string& value,
						   const std::string& line)
{
	char c;
	size_t line_index = 0;
	size_t line_size = line.size();
	keyword.clear();
	value.clear();

	// Skip initial white spaces
	while (line_index < line_size)
	{
		c = line[line_index];
		if (!LLStringOps::isSpace(c))
		{
			break;
		}
		++line_index;
	}

	// Get the keyword
	while (line_index < line_size)
	{
		c = line[line_index];
		if (LLStringOps::isSpace(c) || c == '\r' || c == '\n')
		{
			break;
		}
		keyword += c;
		++line_index;
	}
	if (keyword.empty())
	{
		return;	// No keyword, no value !
	}

	// Get the value
	c = line[line_index];
	if (c != '\r' && c != '\n')
	{
		// Discard initial white spaces
		while (line_index < line_size)
		{
			c = line[line_index];
			if (c == ' ' || c == '\t')
			{
				break;
			}
			++line_index;
		}

		while (line_index < line_size)
		{
			c = line[line_index];
			if (c == '\r' || c == '\n')
			{
				break;
			}
			value += c;
			++line_index;
		}
	}
}

std::streamsize fullread(std::istream& istr, char* buf,
						 std::streamsize requested)
{
	std::streamsize got;
	std::streamsize total = 0;

	istr.read(buf, requested);
	got = istr.gcount();
	total += got;
	while (got && total < requested)
	{
		if (istr.fail())
		{
			// If bad is true, not much we can do; it implies loss of stream
			// integrity. Bail in that case, and otherwise clear and attempt to
			// continue.
			if (istr.bad())
			{
				return total;
			}
			istr.clear();
		}
		istr.read(buf + total, requested - total);
		got = istr.gcount();
		total += got;
	}
	return total;
}

std::istream& operator>>(std::istream& str, const char* tocheck)
{
	char c = '\0';
	const char* p = tocheck;
	while (*p && !str.bad())
	{
		str.get(c);
		if (c != *p)
		{
			str.setstate(std::ios::failbit);
			break;
		}
		++p;
	}
	return str;
}

int cat_streambuf::underflow()
{
	if (gptr() == egptr())
	{
		// Here because our buffer is empty
		std::streamsize sz = 0;
		// Until we have run out of mInputs, try reading the first of them into
		// mBuffer. If that fetches some characters, break the loop.
		while (!mInputs.empty() &&
			   !(sz = mInputs.front()->sgetn(mBuffer.data(), mBuffer.size())))
		{
			// We tried to read mInputs.front() but got zero characters.
			// Discard the first streambuf and try the next one.
			mInputs.pop_front();
		}
		// Either we ran out of mInputs or we succeeded in reading some
		// characters, that is, sz != 0. Tell base class what we have.
		setg(mBuffer.data(), mBuffer.data(), mBuffer.data() + sz);
	}
	// If we fell out of the above loop with mBuffer still empty, return
	// eof(), otherwise return the next character.
	return gptr() == egptr() ? std::char_traits<char>::eof()
							 : std::char_traits<char>::to_int_type(*gptr());
}
