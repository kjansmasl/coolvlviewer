/**
 * @file llsdserialize.cpp
 * @author Phoenix
 * @date 2006-03-05
 * @brief Implementation of LLSD parsers and formatters
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include <deque>
#include <iostream>
#include <memory>
#if !LL_WINDOWS
# include <netinet/in.h>	// For htonl() and ntohl()
#endif

#include "boost/iostreams/device/array.hpp"
#include "boost/iostreams/stream.hpp"
#include "expat.h"
#include "zlib.h"			// For Davep's dirty little zip functions

#include "llsdserialize.h"

#include "llbase64.h"
#include "llmemory.h"
#include "llmemorystream.h"
#include "llpointer.h"
#include "llsd.h"
#include "llstreamtools.h"	// For fullread()
#include "llstring.h"
#include "lluri.h"

// File constants
constexpr S32 UNZIP_LLSD_MAX_DEPTH = 96;
constexpr int MAX_HDR_LEN = 20;
static const char LEGACY_NON_HEADER[] = "<llsd>";
static const std::string LLSD_BINARY_HEADER("LLSD/Binary");
static const std::string LLSD_XML_HEADER("LLSD/XML");
static const std::string LLSD_NOTATION_HEADER("llsd/notation");

// Used to deflate a gzipped asset (currently used for navmeshes)
#define WINDOW_BITS 15
#define ENABLE_ZLIB_GZIP 32

// Helper templates

#if LL_USE_NEW_DESERIALIZE
template<class Formatter>
LL_INLINE void format_using(const LLSD& data, std::ostream& ostr,
							LLSDFormatter::EFormatterOptions options =
								LLSDFormatter::OPTIONS_PRETTY_BINARY)
{
	LLPointer<Formatter> f = new Formatter;
	f->format(data, ostr, options);
}

template<class Parser>
LL_INLINE S32 parse_using(std::istream& istr, LLSD& data, size_t max_bytes,
						  S32 max_depth = -1)
{
	LLPointer<Parser> p = new Parser;
	return p->parse(istr, data, max_bytes, max_depth);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// LLSDSerialize class
///////////////////////////////////////////////////////////////////////////////

//static
void LLSDSerialize::serialize(const LLSD& sd, std::ostream& str,
							  ELLSD_Serialize type,
							  LLSDFormatter::EFormatterOptions options)
{
	LLPointer<LLSDFormatter> f = NULL;

	switch (type)
	{
		case LLSD_BINARY:
			str << "<? " << LLSD_BINARY_HEADER << " ?>\n";
			f = new LLSDBinaryFormatter;
			break;

		case LLSD_XML:
			str << "<? " << LLSD_XML_HEADER << " ?>\n";
			f = new LLSDXMLFormatter;
			break;

		case LLSD_NOTATION:
			str << "<? " << LLSD_NOTATION_HEADER << " ?>\n";
			f = new LLSDNotationFormatter;
			break;

		default:
			llwarns << "serialize request for unknown ELLSD_Serialize"
					<< llendl;
	}

	if (f.notNull())
	{
		f->format(sd, str, options);
	}
}

//static
bool LLSDSerialize::deserialize(LLSD& sd, std::istream& str, llssize max_bytes)
{
	char hdr_buf[MAX_HDR_LEN + 1] = "";
	bool fail_if_not_legacy = false;

#if LL_USE_NEW_DESERIALIZE
	// Get the first line before anything. Do not read more than max_bytes:
	// this get() overload reads no more than (count-1) bytes into the
	// specified buffer. In the usual case when max_bytes exceed
	// ssizeof(hdr_buf), get() will read no more than sizeof(hdr_buf)-2.
	llssize max_hdr_read = MAX_HDR_LEN;
	if (max_bytes != LLSDSerialize::SIZE_UNLIMITED)
	{
		max_hdr_read = llmin(max_bytes + 1, max_hdr_read);
	}
	str.get(hdr_buf, max_hdr_read, '\n');
	S32 inbuf = (S32)str.gcount();
	// Reference: https://en.cppreference.com/w/cpp/io/basic_istream/get
	// When the get() above sees the specified delimiter '\n', it stops there
	// without pulling it from the stream. If it turns out that the stream does
	// NOT contain a header, and the content includes meaningful '\n', it is
	// important to pull that into hdr_buf too.
	if (inbuf < (S32)max_bytes && str.get(hdr_buf[inbuf]))
	{
		// Got the delimiting '\n'
		++inbuf;
		// None of the following requires that hdr_buf contain a final '\0'
		// byte. We could store one if needed, since even the incremented
		// inbuf would not exceed sizeof(hdr_buf)-1, but there is no need.
	}
	std::string header(hdr_buf, (size_t)inbuf);
	if (str.fail())
	{
		str.clear();
		fail_if_not_legacy = true;
	}

	if (!strnicmp(LEGACY_NON_HEADER, hdr_buf, strlen(LEGACY_NON_HEADER)))
	{
		// Create a LLSD XML parser, and parse the first chunk read above.
		LLSDXMLParser x;
		// Parse the first part that was already read
		x.parsePart(hdr_buf, inbuf);
		// Parse the rest of it
		S32 parsed = x.parse(str, sd, max_bytes - inbuf);
		// Formally we should probably check (parsed != PARSE_FAILURE &&
		// parsed > 0), but since PARSE_FAILURE is -1, this suffices.
		return parsed > 0;
	}

	if (fail_if_not_legacy)
	{
		llwarns << "Deserialize LLSD parse failure" << llendl;
		return false;
	}

	// Remove the newline chars
	std::string::size_type lastchar = header.find_last_not_of("\r\n");
	if (lastchar != std::string::npos)
	{
		// It is important that find_last_not_of() returns size_type, which is
		// why lastchar explicitly declares the type above. erase(size_type)
		// erases from that offset to the end of the string, whereas
		// erase(iterator) erases only a single character.
		header.erase(lastchar + 1);
	}

	// Trim off the <? ... ?> header syntax
	std::string::size_type start = header.find_first_not_of("<? ");
	if (start != std::string::npos)
	{
		std::string::size_type end = header.find_first_of(" ?", start);
		if (end != std::string::npos)
		{
			header = header.substr(start, end - start);
			std::ws(str);
		}
	}

	// Create  and invoke the parser as appropriate

	if (LLStringUtil::compareInsensitive(header, LLSD_BINARY_HEADER) == 0)
	{
		return parse_using<LLSDBinaryParser>(str, sd, max_bytes - inbuf) > 0;
	}
	if (LLStringUtil::compareInsensitive(header, LLSD_XML_HEADER) == 0)
	{
		return parse_using<LLSDXMLParser>(str, sd, max_bytes - inbuf) > 0;
	}
	if (LLStringUtil::compareInsensitive(header, LLSD_NOTATION_HEADER) == 0)
	{
		return parse_using<LLSDNotationParser>(str, sd, max_bytes - inbuf) > 0;
	}

	LLPointer<LLSDParser> p;
	if (inbuf && hdr_buf[0] == '<')
	{
		p = new LLSDXMLParser;
		LL_DEBUGS("Deserialize") << "No header found, assuming XML stream"
								 << LL_ENDL;
	}
	else
	{
		p = new LLSDNotationParser;
		LL_DEBUGS("Deserialize") << "No header found, assuming notation stream"
								 << LL_ENDL;
	}

	// Since we have already read 'inbuf' bytes into 'hdr_buf', prepend that
	// data to whatever remains in 'str'.
	LLMemoryStreamBuf already(reinterpret_cast<const U8*>(hdr_buf), inbuf);
	cat_streambuf prebuff(&already, str.rdbuf());
	std::istream prepend(&prebuff);
	return p->parse(prepend, sd, max_bytes) > 0;
#else
	LLPointer<LLSDParser> p = NULL;

	// Get the first line before anything.
	str.get(hdr_buf, MAX_HDR_LEN, '\n');
	if (str.fail())
	{
		str.clear();
		fail_if_not_legacy = true;
	}

	bool legacy_no_header = false;
	std::string header;
	S32 inbuf = 0;
	if (!strnicmp(LEGACY_NON_HEADER, hdr_buf, strlen(LEGACY_NON_HEADER)))
	{
		legacy_no_header = true;
		inbuf = str.gcount();
	}
	else
	{
		if (fail_if_not_legacy)
		{
			goto fail;
		}

		// Remove the newline chars
		for (S32 i = 0; i < MAX_HDR_LEN; i++)
		{
			if (hdr_buf[i] == 0 || hdr_buf[i] == '\r' || hdr_buf[i] == '\n')
			{
				hdr_buf[i] = 0;
				break;
			}
		}
		header = hdr_buf;

		std::string::size_type start = std::string::npos;
		std::string::size_type end = std::string::npos;
		start = header.find_first_not_of("<? ");
		if (start != std::string::npos)
		{
			end = header.find_first_of(" ?", start);
		}
		if (start == std::string::npos || end == std::string::npos)
		{
			goto fail;
		}

		header = header.substr(start, end - start);
		std::ws(str);
	}

	// Create the parser as appropriate
	if (legacy_no_header)
	{
		// Create a LLSD XML parser, and parse the first chunk read above
		LLSDXMLParser* x = new LLSDXMLParser();
		// Parse the first part that was already read
		x->parsePart(hdr_buf, inbuf);
		// Parse the rest of it
		x->parseLines(str, sd);
		delete x;
		return true;
	}

	if (header == LLSD_BINARY_HEADER)
	{
		p = new LLSDBinaryParser;
	}
	else if (header == LLSD_XML_HEADER)
	{
		p = new LLSDXMLParser;
	}
	else if (header == LLSD_NOTATION_HEADER)
	{
		p = new LLSDNotationParser;
	}
	else
	{
		llwarns << "Deserialize request for unknown ELLSD_Serialize" << llendl;
	}

	if (p.notNull())
	{
		p->parse(str, sd, max_bytes);
		return true;
	}

fail:
	llwarns << "deserialize LLSD parse failure" << llendl;
	return false;
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Endian handlers
///////////////////////////////////////////////////////////////////////////////

#if LL_BIG_ENDIAN
U64 ll_htonll(U64 hostlonglong)	{ return hostlonglong; }
U64 ll_ntohll(U64 netlonglong)	{ return netlonglong; }
F64 ll_htond(F64 hostlonglong)	{ return hostlonglong; }
F64 ll_ntohd(F64 netlonglong)	{ return netlonglong; }
#else
// I read some comments one a indicating that doing an integer add here would
// be faster than a bitwise or. For now, the or has programmer clarity, since
// the intended outcome matches the operation.
U64 ll_htonll(U64 hostlonglong)
{
	return ((U64)(htonl((U32)((hostlonglong >> 32) & 0xFFFFFFFF))) |
			((U64)(htonl((U32)(hostlonglong & 0xFFFFFFFF))) << 32));
}

U64 ll_ntohll(U64 netlonglong)
{
	return ((U64)(ntohl((U32)((netlonglong >> 32) & 0xFFFFFFFF))) |
			((U64)(ntohl((U32)(netlonglong & 0xFFFFFFFF))) << 32));
}

union LLEndianSwapper
{
	F64 d;
	U64 i;
};

F64 ll_htond(F64 hostdouble)
{
	LLEndianSwapper tmp;
	tmp.d = hostdouble;
	tmp.i = ll_htonll(tmp.i);
	return tmp.d;
}

F64 ll_ntohd(F64 netdouble)
{
	LLEndianSwapper tmp;
	tmp.d = netdouble;
	tmp.i = ll_ntohll(tmp.i);
	return tmp.d;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// Local functions.
///////////////////////////////////////////////////////////////////////////////

// Parses a delimited string.
//	istr: the stream to read from.
//	value [out]: the string which was found.
//	delim: the delimiter to use.
// Returns number of bytes read off of the stream or PARSE_FAILURE (-1) on
// failure.
llssize deserialize_string_delim(std::istream& istr, std::string& value,
								 char delim)
{
	std::ostringstream write_buffer;
	bool found_escape = false;
	bool found_hex = false;
	bool found_digit = false;
	U8 byte = 0;
	llssize count = 0;

	while (true)
	{
		int next_byte = istr.get();
		++count;

		if (istr.fail())
		{
			// If our stream is empty, break out
			value = write_buffer.str();
			return LLSDParser::PARSE_FAILURE;
		}

		char next_char = (char)next_byte; // Now that we know it is not EOF

		if (found_escape)
		{
			// next character(s) is a special sequence.
			if (found_hex)
			{
				if (found_digit)
				{
					found_digit = false;
					found_hex = false;
					found_escape = false;
					byte = byte << 4;
					byte |= hex_as_nybble(next_char);
					write_buffer << byte;
					byte = 0;
				}
				else
				{
					// next character is the first nybble of
					found_digit = true;
					byte = hex_as_nybble(next_char);
				}
			}
			else if (next_char == 'x')
			{
				found_hex = true;
			}
			else
			{
				switch (next_char)
				{
					case 'a':
						write_buffer << '\a';
						break;
					case 'b':
						write_buffer << '\b';
						break;
					case 'f':
						write_buffer << '\f';
						break;
					case 'n':
						write_buffer << '\n';
						break;
					case 'r':
						write_buffer << '\r';
						break;
					case 't':
						write_buffer << '\t';
						break;
					case 'v':
						write_buffer << '\v';
						break;
					default:
						write_buffer << next_char;
						break;
				}
				found_escape = false;
			}
		}
		else if (next_char == '\\')
		{
			found_escape = true;
		}
		else if (next_char == delim)
		{
			break;
		}
		else
		{
			write_buffer << next_char;
		}
	}

	value = write_buffer.str();
	return count;
}

// Reads a raw string off the stream.
//	istr: the stream to read from, with the (len) parameter leading the stream.
//	value [out]: the string which was found.
//	max_bytes: the maximum possible length of the string. Passing in a negative
//			   value will skip this check.
// Returns number of bytes read off of the stream or PARSE_FAILURE (-1) on
// failure.
llssize deserialize_string_raw(std::istream& istr, std::string& value,
							   llssize max_bytes)
{
	llssize count = 0;
	constexpr S32 BUF_LEN = 20;
	char buf[BUF_LEN];
	istr.get(buf, BUF_LEN - 1, ')');
	count += istr.gcount();
	int c = istr.get();
	c = istr.get();
	count += 2;
	if ((c == '"' || c == '\'') && buf[0] == '(')
	{
		// We probably have a valid raw string. Determine the size, and read
		// it. *FIX: This is memory inefficient.
		auto len = strtol(buf + 1, NULL, 0);
		if (max_bytes > 0 && len > max_bytes)
		{
			return LLSDParser::PARSE_FAILURE;
		}
		std::vector<char> buf;
		if (len)
		{
			buf.resize(len);
			count += fullread(istr, (char*)&buf[0], len);
			value.assign(buf.begin(), buf.end());
		}
		c = istr.get();
		++count;
		if (c != '"' && c != '\'')
		{
			return LLSDParser::PARSE_FAILURE;
		}
	}
	else
	{
		return LLSDParser::PARSE_FAILURE;
	}
	return count;
}

// Figures out what kind of string it is (raw or delimited) and handoff.
//	istr: the stream to read from.
//	value [out]: the string which was found.
//	max_bytes: the maximum possible length of the string. Passing in a negative
//			   value will skip this check.
// Returns number of bytes read off of the stream or PARSE_FAILURE (-1) on
// failure.
llssize deserialize_string(std::istream& istr, std::string& value,
						   llssize max_bytes)
{
	int c = istr.get();
	if (istr.fail())
	{
		// No data in stream, bail out but mention the character we grabbed.
		return LLSDParser::PARSE_FAILURE;
	}

	llssize rv = LLSDParser::PARSE_FAILURE;
	switch (c)
	{
		case '\'':
		case '"':
			rv = deserialize_string_delim(istr, value, c);
			break;
		case 's':
			// Technically, less than max_bytes, but this is just meant to
			// catch egregious protocol errors. parse errors will be caught in
			// the case of incorrect counts.
			rv = deserialize_string_raw(istr, value, max_bytes);
			break;
		default:
			break;
	}
	if (rv == LLSDParser::PARSE_FAILURE)
	{
		return rv;
	}
	return rv + 1; // Account for the character grabbed at the top.
}

// Helper function for dealing with the different notation boolean format.
//	istr: the stream to read from, with the leading character stripped.
//	compare: the string to compare the boolean against
//	value: the value to assign to data if the parse succeeds.
// Returns number of bytes read off of the stream or PARSE_FAILURE (-1) on
// failure.
llssize deserialize_boolean(std::istream& istr, LLSD& data,
							const std::string& compare, bool value)
{
	// This method is a little goofy, because it gets the stream at the point
	// where the t or f has already been consumed. Basically, parse for a patch
	// to the string passed in starting at index 1. If it's a match:
	//  * assign data to value
	//  * return the number of bytes read
	// otherwise:
	//  * set data to LLSD::null
	//  * return LLSDParser::PARSE_FAILURE (-1)

	llssize bytes_read = 0;
	std::string::size_type ii = 0;
	char c = istr.peek();
	while (++ii < compare.size() && tolower(c) == (int)compare[ii] &&
		   istr.good())
	{
		istr.ignore();
		++bytes_read;
		c = istr.peek();
	}
	if (compare.size() != ii)
	{
		data.clear();
		return LLSDParser::PARSE_FAILURE;
	}
	data = value;
	return bytes_read;
}

static const char* NOTATION_STRING_CHARACTERS[256] =
{
	"\\x00",	// 0
	"\\x01",	// 1
	"\\x02",	// 2
	"\\x03",	// 3
	"\\x04",	// 4
	"\\x05",	// 5
	"\\x06",	// 6
	"\\a",		// 7
	"\\b",		// 8
	"\\t",		// 9
	"\\n",		// 10
	"\\v",		// 11
	"\\f",		// 12
	"\\r",		// 13
	"\\x0e",	// 14
	"\\x0f",	// 15
	"\\x10",	// 16
	"\\x11",	// 17
	"\\x12",	// 18
	"\\x13",	// 19
	"\\x14",	// 20
	"\\x15",	// 21
	"\\x16",	// 22
	"\\x17",	// 23
	"\\x18",	// 24
	"\\x19",	// 25
	"\\x1a",	// 26
	"\\x1b",	// 27
	"\\x1c",	// 28
	"\\x1d",	// 29
	"\\x1e",	// 30
	"\\x1f",	// 31
	" ",		// 32
	"!",		// 33
	"\"",		// 34
	"#",		// 35
	"$",		// 36
	"%",		// 37
	"&",		// 38
	"\\'",		// 39
	"(",		// 40
	")",		// 41
	"*",		// 42
	"+",		// 43
	",",		// 44
	"-",		// 45
	".",		// 46
	"/",		// 47
	"0",		// 48
	"1",		// 49
	"2",		// 50
	"3",		// 51
	"4",		// 52
	"5",		// 53
	"6",		// 54
	"7",		// 55
	"8",		// 56
	"9",		// 57
	":",		// 58
	";",		// 59
	"<",		// 60
	"=",		// 61
	">",		// 62
	"?",		// 63
	"@",		// 64
	"A",		// 65
	"B",		// 66
	"C",		// 67
	"D",		// 68
	"E",		// 69
	"F",		// 70
	"G",		// 71
	"H",		// 72
	"I",		// 73
	"J",		// 74
	"K",		// 75
	"L",		// 76
	"M",		// 77
	"N",		// 78
	"O",		// 79
	"P",		// 80
	"Q",		// 81
	"R",		// 82
	"S",		// 83
	"T",		// 84
	"U",		// 85
	"V",		// 86
	"W",		// 87
	"X",		// 88
	"Y",		// 89
	"Z",		// 90
	"[",		// 91
	"\\\\",		// 92
	"]",		// 93
	"^",		// 94
	"_",		// 95
	"`",		// 96
	"a",		// 97
	"b",		// 98
	"c",		// 99
	"d",		// 100
	"e",		// 101
	"f",		// 102
	"g",		// 103
	"h",		// 104
	"i",		// 105
	"j",		// 106
	"k",		// 107
	"l",		// 108
	"m",		// 109
	"n",		// 110
	"o",		// 111
	"p",		// 112
	"q",		// 113
	"r",		// 114
	"s",		// 115
	"t",		// 116
	"u",		// 117
	"v",		// 118
	"w",		// 119
	"x",		// 120
	"y",		// 121
	"z",		// 122
	"{",		// 123
	"|",		// 124
	"}",		// 125
	"~",		// 126
	"\\x7f",	// 127
	"\\x80",	// 128
	"\\x81",	// 129
	"\\x82",	// 130
	"\\x83",	// 131
	"\\x84",	// 132
	"\\x85",	// 133
	"\\x86",	// 134
	"\\x87",	// 135
	"\\x88",	// 136
	"\\x89",	// 137
	"\\x8a",	// 138
	"\\x8b",	// 139
	"\\x8c",	// 140
	"\\x8d",	// 141
	"\\x8e",	// 142
	"\\x8f",	// 143
	"\\x90",	// 144
	"\\x91",	// 145
	"\\x92",	// 146
	"\\x93",	// 147
	"\\x94",	// 148
	"\\x95",	// 149
	"\\x96",	// 150
	"\\x97",	// 151
	"\\x98",	// 152
	"\\x99",	// 153
	"\\x9a",	// 154
	"\\x9b",	// 155
	"\\x9c",	// 156
	"\\x9d",	// 157
	"\\x9e",	// 158
	"\\x9f",	// 159
	"\\xa0",	// 160
	"\\xa1",	// 161
	"\\xa2",	// 162
	"\\xa3",	// 163
	"\\xa4",	// 164
	"\\xa5",	// 165
	"\\xa6",	// 166
	"\\xa7",	// 167
	"\\xa8",	// 168
	"\\xa9",	// 169
	"\\xaa",	// 170
	"\\xab",	// 171
	"\\xac",	// 172
	"\\xad",	// 173
	"\\xae",	// 174
	"\\xaf",	// 175
	"\\xb0",	// 176
	"\\xb1",	// 177
	"\\xb2",	// 178
	"\\xb3",	// 179
	"\\xb4",	// 180
	"\\xb5",	// 181
	"\\xb6",	// 182
	"\\xb7",	// 183
	"\\xb8",	// 184
	"\\xb9",	// 185
	"\\xba",	// 186
	"\\xbb",	// 187
	"\\xbc",	// 188
	"\\xbd",	// 189
	"\\xbe",	// 190
	"\\xbf",	// 191
	"\\xc0",	// 192
	"\\xc1",	// 193
	"\\xc2",	// 194
	"\\xc3",	// 195
	"\\xc4",	// 196
	"\\xc5",	// 197
	"\\xc6",	// 198
	"\\xc7",	// 199
	"\\xc8",	// 200
	"\\xc9",	// 201
	"\\xca",	// 202
	"\\xcb",	// 203
	"\\xcc",	// 204
	"\\xcd",	// 205
	"\\xce",	// 206
	"\\xcf",	// 207
	"\\xd0",	// 208
	"\\xd1",	// 209
	"\\xd2",	// 210
	"\\xd3",	// 211
	"\\xd4",	// 212
	"\\xd5",	// 213
	"\\xd6",	// 214
	"\\xd7",	// 215
	"\\xd8",	// 216
	"\\xd9",	// 217
	"\\xda",	// 218
	"\\xdb",	// 219
	"\\xdc",	// 220
	"\\xdd",	// 221
	"\\xde",	// 222
	"\\xdf",	// 223
	"\\xe0",	// 224
	"\\xe1",	// 225
	"\\xe2",	// 226
	"\\xe3",	// 227
	"\\xe4",	// 228
	"\\xe5",	// 229
	"\\xe6",	// 230
	"\\xe7",	// 231
	"\\xe8",	// 232
	"\\xe9",	// 233
	"\\xea",	// 234
	"\\xeb",	// 235
	"\\xec",	// 236
	"\\xed",	// 237
	"\\xee",	// 238
	"\\xef",	// 239
	"\\xf0",	// 240
	"\\xf1",	// 241
	"\\xf2",	// 242
	"\\xf3",	// 243
	"\\xf4",	// 244
	"\\xf5",	// 245
	"\\xf6",	// 246
	"\\xf7",	// 247
	"\\xf8",	// 248
	"\\xf9",	// 249
	"\\xfa",	// 250
	"\\xfb",	// 251
	"\\xfc",	// 252
	"\\xfd",	// 253
	"\\xfe",	// 254
	"\\xff"		// 255
};

// Does notation escaping of a string to an ostream.
//	value: the string to escape and serialize
//	str: the stream to serialize to.
void serialize_string(const std::string& value, std::ostream& str)
{
	std::string::const_iterator it = value.begin();
	std::string::const_iterator end = value.end();
	U8 c;
	for ( ; it != end; ++it)
	{
		c = (U8)(*it);
		str << NOTATION_STRING_CHARACTERS[c];
	}
}

std::ostream& operator<<(std::ostream& s, const LLSD& llsd)
{
	s << LLSDNotationStreamer(llsd);
	return s;
}

/**
 * Local constants.
 */
static const std::string NOTATION_TRUE_SERIAL("true");
static const std::string NOTATION_FALSE_SERIAL("false");

constexpr char BINARY_TRUE_SERIAL = '1';
constexpr char BINARY_FALSE_SERIAL = '0';

/**
 * LLSDParser
 */
LLSDParser::LLSDParser()
:	mCheckLimits(true), mMaxBytesLeft(0), mParseLines(false)
{
}


S32 LLSDParser::parse(std::istream& istr, LLSD& data, llssize max_bytes,
					  S32 max_depth)
{
	mCheckLimits = LLSDSerialize::SIZE_UNLIMITED != max_bytes;
	mMaxBytesLeft = max_bytes;
	return doParse(istr, data, max_depth);
}

// Parse using routine to get() lines, faster than parse()
S32 LLSDParser::parseLines(std::istream& istr, LLSD& data)
{
	mCheckLimits = false;
	mParseLines = true;
	return doParse(istr, data);
}

int LLSDParser::get(std::istream& istr) const
{
	if (mCheckLimits)
	{
		--mMaxBytesLeft;
	}
	return istr.get();
}

std::istream& LLSDParser::get(std::istream& istr, char* s, std::streamsize n,
							  char delim) const
{
	istr.get(s, n, delim);
	if (mCheckLimits)
	{
		mMaxBytesLeft -= istr.gcount();
	}
	return istr;
}

std::istream& LLSDParser::get(std::istream& istr, std::streambuf& sb,
							  char delim) const
{
	istr.get(sb, delim);
	if (mCheckLimits)
	{
		mMaxBytesLeft -= istr.gcount();
	}
	return istr;
}

std::istream& LLSDParser::ignore(std::istream& istr) const
{
	istr.ignore();
	if (mCheckLimits)
	{
		--mMaxBytesLeft;
	}
	return istr;
}

std::istream& LLSDParser::putback(std::istream& istr, char c) const
{
	istr.putback(c);
	if (mCheckLimits)
	{
		++mMaxBytesLeft;
	}
	return istr;
}

std::istream& LLSDParser::read(std::istream& istr, char* s,
							   std::streamsize n) const
{
	istr.read(s, n);
	if (mCheckLimits)
	{
		mMaxBytesLeft -= istr.gcount();
	}
	return istr;
}

void LLSDParser::account(llssize bytes) const
{
	if (mCheckLimits)
	{
		mMaxBytesLeft -= bytes;
	}
}

/**
 * LLSDNotationParser
 */
LLSDNotationParser::LLSDNotationParser()
{
}

//virtual
S32 LLSDNotationParser::doParse(std::istream& istr, LLSD& data,
								S32 max_depth) const
{
	// map: { string:object, string:object }
	// array: [ object, object, object ]
	// undef: !
	// boolean: true | false | 1 | 0 | T | F | t | f | TRUE | FALSE
	// integer: i####
	// real: r####
	// uuid: u####
	// string: "g'day" | 'have a "nice" day' | s(size)"raw data"
	// uri: l"escaped"
	// date: d"YYYY-MM-DDTHH:MM:SS.FFZ"
	// binary: b##"ff3120ab1" | b(size)"raw data"

	if (max_depth == 0)
	{
		return PARSE_FAILURE;
	}

	char c = istr.peek();
	while (isspace(c))
	{
		// pop the whitespace.
		c = get(istr);
		c = istr.peek();
		continue;
	}
	if (!istr.good())
	{
		return 0;
	}
	S32 parse_count = 1;
	switch (c)
	{
	case '{':
	{
		S32 child_count = parseMap(istr, data, max_depth - 1);
		if (child_count == PARSE_FAILURE || data.isUndefined())
		{
			parse_count = PARSE_FAILURE;
		}
		else
		{
			parse_count += child_count;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading map." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case '[':
	{
		S32 child_count = parseArray(istr, data, max_depth - 1);
		if (child_count == PARSE_FAILURE || data.isUndefined())
		{
			parse_count = PARSE_FAILURE;
		}
		else
		{
			parse_count += child_count;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading array." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case '!':
		c = get(istr);
		data.clear();
		break;

	case '0':
		c = get(istr);
		data = false;
		break;

	case 'F':
	case 'f':
		ignore(istr);
		c = istr.peek();
		if (isalpha(c))
		{
			llssize cnt = deserialize_boolean(istr, data,
											  NOTATION_FALSE_SERIAL, false);
			if (cnt == PARSE_FAILURE)
			{
				parse_count = cnt;
			}
			else
			{
				account(cnt);
			}
		}
		else
		{
			data = false;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading boolean." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;

	case '1':
		c = get(istr);
		data = true;
		break;

	case 'T':
	case 't':
		ignore(istr);
		c = istr.peek();
		if (isalpha(c))
		{
			llssize cnt = deserialize_boolean(istr, data, NOTATION_TRUE_SERIAL,
											  true);
			if (cnt == PARSE_FAILURE)
			{
				parse_count = cnt;
			}
			else
			{
				account(cnt);
			}
		}
		else
		{
			data = true;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading boolean." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;

	case 'i':
	{
		c = get(istr);
		S32 integer = 0;
		istr >> integer;
		data = integer;
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading integer." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case 'r':
	{
		c = get(istr);
		F64 real = 0.0;
		istr >> real;
		data = real;
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading real." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case 'u':
	{
		c = get(istr);
		LLUUID id;
		istr >> id;
		data = id;
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading uuid." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case '\"':
	case '\'':
	case 's':
		if (!parseString(istr, data))
		{
			parse_count = PARSE_FAILURE;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading string." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;

	case 'l':
	{
		c = get(istr); // pop the 'l'
		c = get(istr); // pop the delimiter
		std::string str;
		llssize cnt = deserialize_string_delim(istr, str, c);
		if (cnt == PARSE_FAILURE)
		{
			parse_count = PARSE_FAILURE;
		}
		else
		{
			data = LLURI(str);
			account(cnt);
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading link." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case 'd':
	{
		c = get(istr); // pop the 'd'
		c = get(istr); // pop the delimiter
		std::string str;
		llssize cnt = deserialize_string_delim(istr, str, c);
		if (cnt == PARSE_FAILURE)
		{
			parse_count = PARSE_FAILURE;
		}
		else
		{
			data = LLDate(str);
			account(cnt);
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading date." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case 'b':
		if (!parseBinary(istr, data))
		{
			parse_count = PARSE_FAILURE;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading data." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;

	default:
		parse_count = PARSE_FAILURE;
		llinfos << "Unrecognized character while parsing: int(" << int(c)
				<< ")" << llendl;
		break;
	}
	if (PARSE_FAILURE == parse_count)
	{
		data.clear();
	}
	return parse_count;
}

S32 LLSDNotationParser::parseMap(std::istream& istr, LLSD& map,
								 S32 max_depth) const
{
	// map: { string:object, string:object }
	map = LLSD::emptyMap();
	S32 parse_count = 0;
	char c = get(istr);
	if (c == '{')
	{
		// eat commas, white
		bool found_name = false;
		std::string name;
		c = get(istr);
		while (c != '}' && istr.good())
		{
			if (!found_name)
			{
				if (c == '\"' || c == '\'' || c == 's')
				{
					putback(istr, c);
					found_name = true;
					llssize count = deserialize_string(istr, name,
													   mMaxBytesLeft);
					if (count == PARSE_FAILURE)
					{
						return PARSE_FAILURE;
					}
					account(count);
				}
				c = get(istr);
			}
			else
			{
				if (isspace(c) || c == ':')
				{
					c = get(istr);
					continue;
				}
				putback(istr, c);
				LLSD child;
				S32 count = doParse(istr, child, max_depth);
				if (count > 0)
				{
					// There must be a value for every key, thus child_count
					// must be greater than 0.
					parse_count += count;
					map.insert(name, child);
				}
				else
				{
					return PARSE_FAILURE;
				}
				found_name = false;
				c = get(istr);
			}
		}
		if (c != '}')
		{
			map.clear();
			return PARSE_FAILURE;
		}
	}
	return parse_count;
}

S32 LLSDNotationParser::parseArray(std::istream& istr, LLSD& array,
								   S32 max_depth) const
{
	// array: [ object, object, object ]
	array = LLSD::emptyArray();
	S32 parse_count = 0;
	char c = get(istr);
	if (c == '[')
	{
		// eat commas, white
		c = get(istr);
		while (c != ']' && istr.good())
		{
			LLSD child;
			if (isspace(c) || c == ',')
			{
				c = get(istr);
				continue;
			}
			putback(istr, c);
			S32 count = doParse(istr, child, max_depth);
			if (count == PARSE_FAILURE)
			{
				return PARSE_FAILURE;
			}
			else
			{
				parse_count += count;
				array.append(child);
			}
			c = get(istr);
		}
		if (c != ']')
		{
			return PARSE_FAILURE;
		}
	}
	return parse_count;
}

bool LLSDNotationParser::parseString(std::istream& istr, LLSD& data) const
{
	std::string value;
	llssize count = deserialize_string(istr, value, mMaxBytesLeft);
	if (count == PARSE_FAILURE)
	{
		return false;
	}
	account(count);
	data = value;
	return true;
}

bool LLSDNotationParser::parseBinary(std::istream& istr, LLSD& data) const
{
	// binary: b##"ff3120ab1"
	// or: b(len)"..."

	// I want to manually control those values here to make sure the parser
	// doesn't break when someone changes a constant somewhere else.
	constexpr U32 BINARY_BUFFER_SIZE = 256;
	constexpr U32 STREAM_GET_COUNT = 255;

	// need to read the base out.
	char buf[BINARY_BUFFER_SIZE];
	get(istr, buf, STREAM_GET_COUNT, '"');
	char c = get(istr);
	if (c != '"') return false;
	if (strncmp("b(", buf, 2) == 0)
	{
		// We probably have a valid raw binary stream. determine
		// the size, and read it.
		auto len = strtol(buf + 2, NULL, 0);
		if (mCheckLimits && (len > mMaxBytesLeft)) return false;
		LLSD::Binary value;
		if (len)
		{
			value.resize(len);
			account(fullread(istr, (char *)&value[0], len));
		}
		c = get(istr); // strip off the trailing double-quote
		data = value;
	}
	else if (strncmp("b64", buf, 3) == 0)
	{
		std::stringstream coded_stream;
		get(istr, *(coded_stream.rdbuf()), '\"');
		c = get(istr);
		std::string encoded(coded_stream.str());
		S32 len = LLBase64::decodeLen(encoded.c_str());
		LLSD::Binary value;
		if (len)
		{
			value.resize(len);
			len = LLBase64::decode(value.data(), encoded.c_str());
			value.resize(len);
		}
		data = value;
	}
	else if (strncmp("b16", buf, 3) == 0)
	{
		// Yay, base 16. We pop the next character which is either a double
		// quote or base 16 data. If it's a double quote, we're done parsing.
		// If it's not, put the data back, and read the stream until the next
		// double quote.
		char* read;
		U8 byte;
		U8 byte_buffer[BINARY_BUFFER_SIZE];
		U8* write;
		LLSD::Binary value;
		c = get(istr);
		while (c != '"')
		{
			putback(istr, c);
			read = buf;
			write = byte_buffer;
			get(istr, buf, STREAM_GET_COUNT, '"');
			c = get(istr);
			while (*read != '\0')
			{
				byte = hex_as_nybble(*read++);
				byte = byte << 4;
				byte |= hex_as_nybble(*read++);
				*write++ = byte;
			}
			// Copy the data out of the byte buffer
			value.insert(value.end(), byte_buffer, write);
		}
		data = value;
	}
	else
	{
		return false;
	}
	return true;
}

/**
 * LLSDBinaryParser
 */
LLSDBinaryParser::LLSDBinaryParser()
{
}

//virtual
S32 LLSDBinaryParser::doParse(std::istream& istr, LLSD& data,
							  S32 max_depth) const
{
	/**
	 * Undefined: '!'<br>
	 * Boolean:	'1' for true '0' for false<br>
	 * Integer:	'i' + 4 bytes network byte order<br>
	 * Real:	'r' + 8 bytes IEEE double<br>
	 * UUID:	'u' + 16 byte unsigned integer<br>
	 * String:	's' + 4 byte integer size + string<br>
	 *			strings also secretly support the notation format
	 * Date:	'd' + 8 byte IEEE double for seconds since epoch<br>
	 * URI:		'l' + 4 byte integer size + string uri<br>
	 * Binary:	'b' + 4 byte integer size + binary data<br>
	 * Array:	'[' + 4 byte integer size  + all values + ']'<br>
	 * Map:		'{' + 4 byte integer size  every(key + value) + '}'<br>
	 *			map keys are serialized as s + 4 byte integer size + string or
	 *			in the notation format.
	 */

	char c;
	c = get(istr);
	if (!istr.good())
	{
		return 0;
	}

	if (max_depth == 0)
	{
		return PARSE_FAILURE;
	}

	S32 parse_count = 1;

	switch (c)
	{
	case '{':
	{
		S32 child_count = parseMap(istr, data, max_depth - 1);
		if ((child_count == PARSE_FAILURE) || data.isUndefined())
		{
			parse_count = PARSE_FAILURE;
		}
		else
		{
			parse_count += child_count;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading binary map." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case '[':
	{
		S32 child_count = parseArray(istr, data, max_depth - 1);
		if ((child_count == PARSE_FAILURE) || data.isUndefined())
		{
			parse_count = PARSE_FAILURE;
		}
		else
		{
			parse_count += child_count;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading binary array." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case '!':
		data.clear();
		break;

	case '0':
		data = false;
		break;

	case '1':
		data = true;
		break;

	case 'i':
	{
		U32 value_nbo = 0;
		read(istr, (char*)&value_nbo, sizeof(U32));
		data = (S32)ntohl(value_nbo);
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading binary integer." << llendl;
		}
		break;
	}

	case 'r':
	{
		F64 real_nbo = 0.0;
		read(istr, (char*)&real_nbo, sizeof(F64));
		data = ll_ntohd(real_nbo);
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading binary real." << llendl;
		}
		break;
	}

	case 'u':
	{
		LLUUID id;
		read(istr, (char*)(&id.mData), UUID_BYTES);
		data = id;
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading binary uuid." << llendl;
		}
		break;
	}

	case '\'':
	case '"':
	{
		std::string value;
		llssize cnt = deserialize_string_delim(istr, value, c);
		if (cnt == PARSE_FAILURE)
		{
			parse_count = PARSE_FAILURE;
		}
		else
		{
			data = value;
			account(cnt);
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading binary (notation-style) string."
					<< llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case 's':
	{
		std::string value;
		if (parseString(istr, value))
		{
			data = value;
		}
		else
		{
			parse_count = PARSE_FAILURE;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading binary string." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case 'l':
	{
		std::string value;
		if (parseString(istr, value))
		{
			data = LLURI(value);
		}
		else
		{
			parse_count = PARSE_FAILURE;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading binary link." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case 'd':
	{
		F64 real = 0.0;
		read(istr, (char*)&real, sizeof(F64));
		data = LLDate(real);
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading binary date." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	case 'b':
	{
		// We probably have a valid raw binary stream. Determine the size and
		// read it.
		U32 size_nbo = 0;
		read(istr, (char*)&size_nbo, sizeof(U32));
		S32 size = (S32)ntohl(size_nbo);
		if (size < 0 || (mCheckLimits && size > mMaxBytesLeft))
		{
			parse_count = PARSE_FAILURE;
		}
		else
		{
			LLSD::Binary value;
			if (size > 0)
			{
				value.resize(size);
				account(fullread(istr, (char*)&value[0], size));
			}
			data = value;
		}
		if (istr.fail())
		{
			llinfos << "STREAM FAILURE reading binary." << llendl;
			parse_count = PARSE_FAILURE;
		}
		break;
	}

	default:
		parse_count = PARSE_FAILURE;
		llinfos << "Unrecognized character while parsing: int(" << int(c)
				<< ")" << llendl;
		break;
	}
	if (PARSE_FAILURE == parse_count)
	{
		data.clear();
	}
	return parse_count;
}

S32 LLSDBinaryParser::parseMap(std::istream& istr, LLSD& map,
							   S32 max_depth) const
{
	map = LLSD::emptyMap();
	U32 value_nbo = 0;
	read(istr, (char*)&value_nbo, sizeof(U32));
	S32 size = (S32)ntohl(value_nbo);
	if (size < 0)
	{
		return PARSE_FAILURE;
	}
	S32 parse_count = 0;
	S32 count = 0;
	char c = get(istr);
	while (c != '}' && (count < size) && istr.good())
	{
		std::string name;
		switch (c)
		{
			case 'k':
				if (!parseString(istr, name))
				{
					return PARSE_FAILURE;
				}
				break;
			case '\'':
			case '"':
			{
				llssize cnt = deserialize_string_delim(istr, name, c);
				if (cnt == PARSE_FAILURE)
				{
					return PARSE_FAILURE;
				}
				account(cnt);
				break;
			}
		}
		LLSD child;
		S32 child_count = doParse(istr, child, max_depth);
		if (child_count > 0)
		{
			// There must be a value for every key, thus child_count must be
			// greater than 0.
			parse_count += child_count;
			map.insert(name, child);
		}
		else
		{
			return PARSE_FAILURE;
		}
		++count;
		c = get(istr);
	}
	if (c != '}' || count < size)
	{
		// Make sure it is correctly terminated and we parsed as many as were
		// said to be there.
		return PARSE_FAILURE;
	}
	return parse_count;
}

S32 LLSDBinaryParser::parseArray(std::istream& istr, LLSD& array,
								 S32 max_depth) const
{
	array = LLSD::emptyArray();
	U32 value_nbo = 0;
	read(istr, (char*)&value_nbo, sizeof(U32));
	S32 size = (S32)ntohl(value_nbo);
	if (size < 0)
	{
		return PARSE_FAILURE;
	}

	// *FIX: This would be a good place to reserve some space in the array...

	S32 parse_count = 0;
	S32 count = 0;
	char c = istr.peek();
	while (c != ']' && count < size && istr.good())
	{
		LLSD child;
		S32 child_count = doParse(istr, child, max_depth);
		if (PARSE_FAILURE == child_count)
		{
			return PARSE_FAILURE;
		}
		if (child_count)
		{
			parse_count += child_count;
			array.append(child);
		}
		++count;
		c = istr.peek();
	}
	c = get(istr);
	if (c != ']' || count < size)
	{
		// Make sure it is correctly terminated and we parsed as many as were
		// said to be there.
		return PARSE_FAILURE;
	}
	return parse_count;
}

bool LLSDBinaryParser::parseString(std::istream& istr,
								   std::string& value) const
{
	// *FIX: This is memory inefficient.
	U32 value_nbo = 0;
	read(istr, (char*)&value_nbo, sizeof(U32));
	S32 size = (S32)ntohl(value_nbo);
	if (size < 0 || (mCheckLimits && size > mMaxBytesLeft))
	{
		return false;
	}
	if (size)
	{
		std::vector<char> buf;
		buf.resize(size);
		account(fullread(istr, &buf[0], size));
		value.assign(buf.begin(), buf.end());
	}
	return true;
}

/**
 * LLSDFormatter
 */
LLSDFormatter::LLSDFormatter(bool bool_apha, const std::string& real_format,
							 EFormatterOptions options)
:	mBoolAlpha(bool_apha),
	mRealFormat(real_format),
	mOptions(options)
{
}

S32 LLSDFormatter::format(const LLSD& data, std::ostream& ostr) const
{
	 // Pass options captured by constructor
	return format(data, ostr, mOptions);
}

//virtual
S32 LLSDFormatter::format(const LLSD& data, std::ostream& ostr,
						  EFormatterOptions options) const
{
	return format_impl(data, ostr, options, 0);
}

void LLSDFormatter::formatReal(LLSD::Real real, std::ostream& ostr) const
{
	std::string buffer = llformat(mRealFormat.c_str(), real);
	ostr << buffer;
}

/**
 * LLSDNotationFormatter
 */
LLSDNotationFormatter::LLSDNotationFormatter(bool bool_apha,
											 const std::string& real_format,
											 EFormatterOptions options)
:	LLSDFormatter(bool_apha, real_format, options)
{
}

//static
std::string LLSDNotationFormatter::escapeString(const std::string& in)
{
	std::ostringstream ostr;
	serialize_string(in, ostr);
	return ostr.str();
}

S32 LLSDNotationFormatter::format_impl(const LLSD& data, std::ostream& ostr,
									   EFormatterOptions options,
									   U32 level) const
{
	S32 format_count = 1;
	std::string pre;
	std::string post;

	if (options & LLSDFormatter::OPTIONS_PRETTY)
	{
		for (U32 i = 0; i < level; ++i)
		{
			pre += "    ";
		}
		post = "\n";
	}

	switch (data.type())
	{
		case LLSD::TypeMap:
		{
			if (level) ostr << post << pre;
			ostr << "{";
			std::string inner_pre;
			if (options & LLSDFormatter::OPTIONS_PRETTY)
			{
				inner_pre = pre + "    ";
			}

			bool need_comma = false;
			LLSD::map_const_iterator iter = data.beginMap();
			LLSD::map_const_iterator end = data.endMap();
			for ( ; iter != end; ++iter)
			{
				if (need_comma) ostr << ",";
				need_comma = true;
				ostr << post << inner_pre << '\'';
				serialize_string(iter->first, ostr);
				ostr << "':";
				format_count += format_impl(iter->second, ostr,
											options, level + 2);
			}
			ostr << post << pre << "}";
			break;
		}

		case LLSD::TypeArray:
		{
			ostr << post << pre << "[";
			bool need_comma = false;
			LLSD::array_const_iterator iter = data.beginArray();
			LLSD::array_const_iterator end = data.endArray();
			for ( ; iter != end; ++iter)
			{
				if (need_comma) ostr << ",";
				need_comma = true;
				format_count += format_impl(*iter, ostr, options, level + 1);
			}
			ostr << "]";
			break;
		}

		case LLSD::TypeUndefined:
			ostr << "!";
			break;

		case LLSD::TypeBoolean:
#if (LL_WINDOWS || __GNUC__ > 2)
			if (mBoolAlpha || (ostr.flags() & std::ios::boolalpha))
#else
			if (mBoolAlpha || (ostr.flags() & 0x0100))
#endif
			{
				ostr << (data.asBoolean() ? NOTATION_TRUE_SERIAL
										  : NOTATION_FALSE_SERIAL);
			}
			else
			{
				ostr << (data.asBoolean() ? 1 : 0);
			}
			break;

		case LLSD::TypeInteger:
			ostr << "i" << data.asInteger();
			break;

		case LLSD::TypeReal:
			ostr << "r";
			if (mRealFormat.empty())
			{
				ostr << data.asReal();
			}
			else
			{
				formatReal(data.asReal(), ostr);
			}
			break;

		case LLSD::TypeUUID:
			ostr << "u" << data.asUUID();
			break;

		case LLSD::TypeString:
			ostr << '\'';
			serialize_string(data.asStringRef(), ostr);
			ostr << '\'';
			break;

		case LLSD::TypeDate:
			ostr << "d\"" << data.asDate() << "\"";
			break;

		case LLSD::TypeURI:
			ostr << "l\"";
			serialize_string(data.asString(), ostr);
			ostr << "\"";
			break;

		case LLSD::TypeBinary:
		{
			const LLSD::Binary& buffer = data.asBinary();
			size_t count = buffer.size();
			if (options & LLSDFormatter::OPTIONS_PRETTY_BINARY)
			{
				ostr << "b16\"";
				if (count)
				{
					std::ios_base::fmtflags old_flags = ostr.flags();
					ostr.setf(std::ios::hex, std::ios::basefield);
					ostr << std::uppercase;
					auto oldfill(ostr.fill('0'));
					auto oldwidth(ostr.width());
					for (size_t i = 0; i < count; ++i)
					{
						// Have to re-state setw() before every conversion
						ostr << std::setw(2) << (S32)buffer[i];
					}
					ostr.width(oldwidth);
					ostr.fill(oldfill);
					ostr.flags(old_flags);
				}
			}
			else
			{
				ostr << "b(" << count << ")\"";
				if (count)
				{
					ostr.write((const char*)&buffer[0], count);
				}
			}
			ostr << "\"";
			break;
		}

		default:
			// *NOTE: This should never happen.
			ostr << "!";
			break;
	}
	return format_count;
}

/**
 * LLSDBinaryFormatter
 */
LLSDBinaryFormatter::LLSDBinaryFormatter(bool bool_apha,
										 const std::string& real_format,
										 EFormatterOptions options)
:	LLSDFormatter(bool_apha, real_format, options)
{
}

//virtual
S32 LLSDBinaryFormatter::format_impl(const LLSD& data, std::ostream& ostr,
									 EFormatterOptions options,
									 U32 level) const
{
	S32 format_count = 1;
	switch (data.type())
	{
		case LLSD::TypeMap:
		{
			ostr.put('{');
			U32 size_nbo = htonl(data.size());
			ostr.write((const char*)(&size_nbo), sizeof(U32));
			LLSD::map_const_iterator iter = data.beginMap();
			LLSD::map_const_iterator end = data.endMap();
			for ( ; iter != end; ++iter)
			{
				ostr.put('k');
				formatString(iter->first, ostr);
				format_count += format_impl(iter->second, ostr, options,
											level + 1);
			}
			ostr.put('}');
			break;
		}

		case LLSD::TypeArray:
		{
			ostr.put('[');
			U32 size_nbo = htonl(data.size());
			ostr.write((const char*)(&size_nbo), sizeof(U32));
			LLSD::array_const_iterator iter = data.beginArray();
			LLSD::array_const_iterator end = data.endArray();
			for ( ; iter != end; ++iter)
			{
				format_count += format_impl(*iter, ostr, options, level + 1);
			}
			ostr.put(']');
			break;
		}

		case LLSD::TypeUndefined:
			ostr.put('!');
			break;

		case LLSD::TypeBoolean:
			if (data.asBoolean())
			{
				ostr.put(BINARY_TRUE_SERIAL);
			}
			else
			{
				ostr.put(BINARY_FALSE_SERIAL);
			}
			break;

		case LLSD::TypeInteger:
		{
			ostr.put('i');
			U32 value_nbo = htonl(data.asInteger());
			ostr.write((const char*)(&value_nbo), sizeof(U32));
			break;
		}

		case LLSD::TypeReal:
		{
			ostr.put('r');
			F64 value_nbo = ll_htond(data.asReal());
			ostr.write((const char*)(&value_nbo), sizeof(F64));
			break;
		}

		case LLSD::TypeUUID:
		{
			ostr.put('u');
			LLUUID value = data.asUUID();
			ostr.write((const char*)(&(value.mData)), UUID_BYTES);
			break;
		}

		case LLSD::TypeString:
			ostr.put('s');
			formatString(data.asStringRef(), ostr);
			break;

		case LLSD::TypeDate:
		{
			ostr.put('d');
			F64 value = data.asReal();
			ostr.write((const char*)(&value), sizeof(F64));
			break;
		}

		case LLSD::TypeURI:
			ostr.put('l');
			formatString(data.asString(), ostr);
			break;

		case LLSD::TypeBinary:
		{
			ostr.put('b');
			const LLSD::Binary& buffer = data.asBinary();
			U32 size_nbo = htonl(buffer.size());
			ostr.write((const char*)(&size_nbo), sizeof(U32));
			if (buffer.size())
			{
				ostr.write((const char*)&buffer[0], buffer.size());
			}
			break;
		}

		default:
			// *NOTE: This should never happen.
			ostr.put('!');
			break;
	}
	return format_count;
}

void LLSDBinaryFormatter::formatString(const std::string& string,
									   std::ostream& ostr) const
{
	U32 size_nbo = htonl(string.size());
	ostr.write((const char*)(&size_nbo), sizeof(U32));
	ostr.write(string.c_str(), string.size());
}

// Dirty little zippers: yell at Davep if these are horrid

constexpr U32 CHUNK_SIZE = 512 * 1024;

// Note: we cannot use too large a thread local storage, or we would cause
// crashes (seen under Linux with: static thread_local U8 tZipBuffer[65536]).
// So we instead use a thread_local unique_ptr pointing to a runtime allocated
// storage (thus allocated on the heap, but only once per thread); when the
// unique_ptr is destroyed at thread exit, it also destroys the storage.
static thread_local std::unique_ptr<U8[]> tZipperBufferp;
static U8* get_zipper_buffer()
{
	if (!tZipperBufferp)
	{
		tZipperBufferp =
			std::unique_ptr<U8[]>(new(std::nothrow) U8[CHUNK_SIZE]);
	}
	return tZipperBufferp.get();
}

// Returns a string containing gzipped bytes of binary serialized LLSD VERY
// inefficient (creates several copies of LLSD block in memory).
std::string zip_llsd(LLSD& data)
{
	std::stringstream llsd_strm;

	LLSDSerialize::toBinary(data, llsd_strm);

	z_stream strm;
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;

	S32 ret = deflateInit(&strm, Z_BEST_COMPRESSION);
	if (ret != Z_OK)
	{
		llwarns << "Failed to compress LLSD block." << llendl;
		return std::string();
	}

	std::string source = llsd_strm.str();

	strm.avail_in = (unsigned int)source.size();
	strm.next_in = (unsigned char*)source.data();
	U8* output = NULL;

	llssize cur_size = 0;

	U32 have = 0;

	U8* out = get_zipper_buffer();
	do
	{
		strm.avail_out = CHUNK_SIZE;
		strm.next_out = out;

		ret = deflate(&strm, Z_FINISH);
		if (ret == Z_OK || ret == Z_STREAM_END)
		{
			// Copy result into output
			if (strm.avail_out >= CHUNK_SIZE)
			{
				free(output);
				llwarns << "Failed to compress LLSD block." << llendl;
				return std::string();
			}

			have = CHUNK_SIZE - strm.avail_out;
			output = (U8*)realloc(output, cur_size + have);
			if (!output)
			{
				LLMemory::allocationFailed(cur_size + have);
				llwarns << "Out of memory while decompressing LLSD." << llendl;
				deflateEnd(&strm);
				return std::string();
			}
			memcpy(output + cur_size, out, have);
			cur_size += have;
		}
		else
		{
			free(output);
			llwarns << "Failed to compress LLSD block." << llendl;
			return std::string();
		}
	}
	while (ret == Z_OK);

	std::string::size_type size = cur_size;

	std::string result((char*) output, size);
	deflateEnd(&strm);
	free(output);

#if 0 // Verify results work with unzip_llsd
	std::istringstream test(result);
	LLSD test_sd;
	if (!unzip_llsd(test_sd, test, result.size()))
	{
		llerrs << "Invalid compression result !" << llendl;
	}
#endif

	return result;
}

// Decompress a block of LLSD from provided istream. Not very efficient:
// creates a copy of decompressed LLSD block in memory and deserializes
// from that copy using LLSDSerialize.
bool unzip_llsd(LLSD& data, std::istream& is, S32 size)
{
	U8* in = new(std::nothrow) U8[size];
	if (!in)
	{
		LLMemory::allocationFailed(size);
		return false;
	}

	is.read((char*)in, size);

	bool ret = unzip_llsd(data, in, size);
	delete[] in;
	return ret;
}

bool unzip_llsd(LLSD& data, const U8* in, S32 size)
{
	U8* result = NULL;
	llssize cur_size = 0;
	z_stream strm;

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = (unsigned int)size;
	strm.next_in = (unsigned char*)in;

	S32 ret = inflateInit(&strm);

	U8* out = get_zipper_buffer();
	do
	{
		strm.avail_out = CHUNK_SIZE;
		strm.next_out = out;
		ret = inflate(&strm, Z_NO_FLUSH);
		if (ret == Z_STREAM_ERROR)
		{
			inflateEnd(&strm);
			if (result)
			{
				free(result);
			}
			LL_DEBUGS("UnzipLLSD") << "Z_STREAM_ERROR" << LL_ENDL;
			return false;
		}

		switch (ret)
		{
			case Z_NEED_DICT:
				LL_DEBUGS("UnzipLLSD") << "Z_NEED_DICT" << LL_ENDL;
				ret = Z_DATA_ERROR;
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				inflateEnd(&strm);
				if (result)
				{
					free(result);
				}
				LL_DEBUGS("UnzipLLSD") << (ret == Z_DATA_ERROR ? "Z_DATA_ERROR"
															   : "Z_MEM_ERROR")
									   << LL_ENDL;
				return false;
			default:
				break;
		}

		U32 have = CHUNK_SIZE - strm.avail_out;

		U8* tmp = (U8*)realloc(result, cur_size + have);
		if (!tmp)
		{
			LLMemory::allocationFailed(cur_size + have);
			if (result)
			{
				free(result);
			}
			inflateEnd(&strm);
			return false;
		}
		result = tmp;

		memcpy(result + cur_size, out, have);
		cur_size += have;
	}
	while (ret == Z_OK);

	inflateEnd(&strm);

	if (ret != Z_STREAM_END)
	{
		LL_DEBUGS("UnzipLLSD") << "Error #" << ret << LL_ENDL;
		if (result)
		{
			free(result);
		}
		return false;
	}

	// 'result' now points to the decompressed LLSD block
	static char deprecated_header[] = "<? LLSD/Binary ?>";
	static size_t deprecated_header_size = strlen(deprecated_header);
	char* datap = (char*)result;
	if (!strncmp(datap, deprecated_header, deprecated_header_size))
	{
		datap += deprecated_header_size;
		cur_size -= deprecated_header_size;
	}
	boost::iostreams::stream<boost::iostreams::array_source> is(datap,
																cur_size);
	if (!LLSDSerialize::fromBinary(data, is, cur_size, UNZIP_LLSD_MAX_DEPTH))
	{
		llwarns << "Failed to unzip LLSD block" << llendl;
		free(result);
		return false;
	}

	free(result);

	return true;
}

// This unzip function will only work with a gzip header and trailer - while
// the contents of the actual compressed data is the same for either format
// (gzip vs zlib), the headers and trailers are different for the formats.
U8* unzip_llsdNavMesh(bool& valid, size_t& outsize, const U8* in, S32 size)
{
	if (size <= 0)
	{
		llwarns << "No data to unzip" << llendl;
		return NULL;
	}

	U8* result = NULL;
	llssize cur_size = 0;
	z_stream strm;

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = (unsigned int)size;
	strm.next_in = (unsigned char*)in;

	U8* out = get_zipper_buffer();
	valid = true;
	S32 ret = inflateInit2(&strm, WINDOW_BITS | ENABLE_ZLIB_GZIP);
	do
	{
		strm.avail_out = CHUNK_SIZE;
		strm.next_out = out;
		ret = inflate(&strm, Z_NO_FLUSH);
		switch (ret)
		{
			case Z_STREAM_ERROR:
				LL_DEBUGS("UnzipLLSD") << "Z_STREAM_ERROR" << LL_ENDL;
				valid = false;
			case Z_NEED_DICT:
				LL_DEBUGS("UnzipLLSD") << "Z_NEED_DICT" << LL_ENDL;
				valid = false;
			case Z_DATA_ERROR:
				LL_DEBUGS("UnzipLLSD") << "Z_DATA_ERROR" << LL_ENDL;
				valid = false;
			case Z_MEM_ERROR:
				if (valid)
				{
					LL_DEBUGS("UnzipLLSD") << "Z_MEM_ERROR" << LL_ENDL;
					valid = false;
				}
				inflateEnd(&strm);
				if (result)
				{
					free(result);
				}
				return NULL;

			default:
				break;
		}

		U32 have = CHUNK_SIZE - strm.avail_out;
		U8* tmp = (U8*)realloc(result, cur_size + have);
		if (!tmp)
		{
			LLMemory::allocationFailed(cur_size + have);
			if (result)
			{
				free(result);
			}
			inflateEnd(&strm);
			valid = false;
			return NULL;
		}
		result = tmp;

		memcpy(result + cur_size, out, have);
		cur_size += have;
	}
	while (ret == Z_OK);

	inflateEnd(&strm);

	if (ret != Z_STREAM_END)
	{
		if (result)
		{
			free(result);
		}
		valid = false;
		LL_DEBUGS("UnzipLLSD") << "Error #" << ret << LL_ENDL;
		return NULL;
	}

	// Result now points to the decompressed LLSD block
	outsize = cur_size;
	valid = true;

	return result;
}

///////////////////////////////////////////////////////////////////////////////
// LLSDXMLFormatter and LLSDXMLParser classes (used to be in a separate
// llsdserialize_xml.cpp file, despite the fact they implement classes declared
// in llsdserialize.h... Go figure ! HB)
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// LLSDXMLFormatter class
///////////////////////////////////////////////////////////////////////////////

LLSDXMLFormatter::LLSDXMLFormatter(bool bool_apha,
								   const std::string& real_format,
								   EFormatterOptions options)
:	LLSDFormatter(bool_apha, real_format, options)
{
}

//virtual
S32 LLSDXMLFormatter::format(const LLSD& data, std::ostream& ostr,
							 EFormatterOptions options) const
{
	std::streamsize old_precision = ostr.precision(25);

	std::string post;
	if (options & LLSDFormatter::OPTIONS_PRETTY)
	{
		post = "\n";
	}
	ostr << "<llsd>" << post;
	S32 rv = format_impl(data, ostr, options, 1);
	ostr << "</llsd>\n";

	ostr.precision(old_precision);
	return rv;
}

S32 LLSDXMLFormatter::format_impl(const LLSD& data, std::ostream& ostr,
								  EFormatterOptions options, U32 level) const
{
	S32 format_count = 1;
	std::string pre;
	std::string post;

	if (options & LLSDFormatter::OPTIONS_PRETTY)
	{
		for (U32 i = 0; i < level; ++i)
		{
			pre += "    ";
		}
		post = "\n";
	}

	switch (data.type())
	{
	case LLSD::TypeMap:
		if (0 == data.size())
		{
			ostr << pre << "<map />" << post;
		}
		else
		{
			ostr << pre << "<map>" << post;
			LLSD::map_const_iterator iter = data.beginMap();
			LLSD::map_const_iterator end = data.endMap();
			for (; iter != end; ++iter)
			{
				ostr << pre << "<key>" << escapeString((*iter).first) << "</key>" << post;
				format_count += format_impl((*iter).second, ostr, options, level + 1);
			}
			ostr << pre <<  "</map>" << post;
		}
		break;

	case LLSD::TypeArray:
		if (0 == data.size())
		{
			ostr << pre << "<array />" << post;
		}
		else
		{
			ostr << pre << "<array>" << post;
			LLSD::array_const_iterator iter = data.beginArray();
			LLSD::array_const_iterator end = data.endArray();
			for(; iter != end; ++iter)
			{
				format_count += format_impl(*iter, ostr, options, level + 1);
			}
			ostr << pre << "</array>" << post;
		}
		break;

	case LLSD::TypeUndefined:
		ostr << pre << "<undef />" << post;
		break;

	case LLSD::TypeBoolean:
		ostr << pre << "<boolean>";
		if (mBoolAlpha || (ostr.flags() & std::ios::boolalpha))
		{
			ostr << (data.asBoolean() ? "true" : "false");
		}
		else
		{
			ostr << (data.asBoolean() ? 1 : 0);
		}
		ostr << "</boolean>" << post;
		break;

	case LLSD::TypeInteger:
		ostr << pre << "<integer>" << data.asInteger() << "</integer>" << post;
		break;

	case LLSD::TypeReal:
		ostr << pre << "<real>";
		if (mRealFormat.empty())
		{
			ostr << data.asReal();
		}
		else
		{
			formatReal(data.asReal(), ostr);
		}
		ostr << "</real>" << post;
		break;

	case LLSD::TypeUUID:
		if (data.asUUID().isNull())
		{
			ostr << pre << "<uuid />" << post;
		}
		else
		{
			ostr << pre << "<uuid>" << data.asUUID() << "</uuid>" << post;
		}
		break;

	case LLSD::TypeString:
		if (data.asStringRef().empty())
		{
			ostr << pre << "<string />" << post;
		}
		else
		{
			ostr << pre << "<string>" << escapeString(data.asStringRef())
				 << "</string>" << post;
		}
		break;

	case LLSD::TypeDate:
		ostr << pre << "<date>" << data.asDate() << "</date>" << post;
		break;

	case LLSD::TypeURI:
		ostr << pre << "<uri>" << escapeString(data.asString()) << "</uri>"
			 << post;
		break;

	case LLSD::TypeBinary:
	{
		const LLSD::Binary& buffer = data.asBinary();
		if (buffer.empty())
		{
			ostr << pre << "<binary />" << post;
		}
		else
		{
			ostr << pre << "<binary encoding=\"base64\">";
			size_t buffsz = buffer.size();
			size_t b64_buffer_len = LLBase64::encodeLen(buffsz);
			char* b64_buffer = new char[b64_buffer_len];
			b64_buffer_len = LLBase64::encode(b64_buffer, buffer.data(),
											  buffsz);
			ostr.write(b64_buffer, b64_buffer_len - 1);
			delete[] b64_buffer;
			ostr << "</binary>" << post;
		}
		break;
	}

	default:
		// *NOTE: This should never happen.
		ostr << pre << "<undef />" << post;
		break;
	}
	return format_count;
}

//static
std::string LLSDXMLFormatter::escapeString(const std::string& in)
{
	std::ostringstream out;
	for (std::string::const_iterator it = in.begin(),
									 end = in.end();
		 it != end; ++it)
	{
		switch (*it)
		{
			case '<':
				out << "&lt;";
				break;

			case '>':
				out << "&gt;";
				break;

			case '&':
				out << "&amp;";
				break;

			case '\'':
				out << "&apos;";
				break;

			case '"':
				out << "&quot;";
				break;

			case '\t':
			case '\n':
			case '\r':
				out << *it;
				break;

			default:
				if (*it >= 0 && *it < 20)
				{
					// Do not output control codes
					out << "?";
				}
				else
				{
					out << *it;
				}
		}
	}
	return out.str();
}

class LLSDXMLParser::Impl
{
protected:
	LOG_CLASS(LLSDXMLParser);

public:
	Impl(bool emit_errors);
	~Impl();

	S32 parse(std::istream& input, LLSD& data);
	S32 parseLines(std::istream& input, LLSD& data);

	void parsePart(const char* buf, llssize len);

	void reset();

private:
	void startElementHandler(const XML_Char* name, const XML_Char** attributes);
	void endElementHandler(const XML_Char* name);
	void characterDataHandler(const XML_Char* data, int length);

	static void sStartElementHandler(void* userData, const XML_Char* name,
									 const XML_Char** attributes);
	static void sEndElementHandler(void* userData, const XML_Char* name);
	static void sCharacterDataHandler(void* userData, const XML_Char* data,
									  int length);

	void startSkipping();

	enum Element {
		ELEMENT_LLSD,
		ELEMENT_UNDEF,
		ELEMENT_BOOL,
		ELEMENT_INTEGER,
		ELEMENT_REAL,
		ELEMENT_STRING,
		ELEMENT_UUID,
		ELEMENT_DATE,
		ELEMENT_URI,
		ELEMENT_BINARY,
		ELEMENT_MAP,
		ELEMENT_ARRAY,
		ELEMENT_KEY,
		ELEMENT_UNKNOWN
	};
	static Element readElement(const XML_Char* name);

	static const XML_Char* findAttribute(const XML_Char* name,
										 const XML_Char** pairs);

private:
	XML_Parser		mParser;

	LLSD			mResult;
	S32				mParseCount;

	bool			mInLLSDElement;		// true if we are on LLSD
	bool			mGracefullStop;		// true if we found the </llsd
	bool			mEmitErrors;

	typedef std::deque<LLSD*> LLSDRefStack;
	LLSDRefStack	mStack;

	int				mDepth;
	bool			mSkipping;
	int				mSkipThrough;

	std::string		mCurrentKey;		// Current XML <tag>
	std::string		mCurrentContent;	// String data between <tag> and </tag>
};

LLSDXMLParser::Impl::Impl(bool emit_errors)
:	mEmitErrors(emit_errors)
{
	mParser = XML_ParserCreate(NULL);
	reset();
}

LLSDXMLParser::Impl::~Impl()
{
	XML_ParserFree(mParser);
}

LL_INLINE bool is_eol(char c)
{
	return (c == '\n' || c == '\r');
}

void clear_eol(std::istream& input)
{
	char c = input.peek();
	while (input.good() && is_eol(c))
	{
		input.get(c);
		c = input.peek();
	}
}

static unsigned get_till_eol(std::istream& input, char* buf, unsigned bufsize)
{
	unsigned count = 0;
	while (count < bufsize && input.good())
	{
		char c = input.get();
		buf[count++] = c;
		if (is_eol(c))
		{
			break;
		}
	}
	return count;
}

S32 LLSDXMLParser::Impl::parse(std::istream& input, LLSD& data)
{
	XML_Status status;

	constexpr int BUFFER_SIZE = 1024;
	void* buffer = NULL;
	int count = 0;
	while (input.good() && !input.eof())
	{
		buffer = XML_GetBuffer(mParser, BUFFER_SIZE);

		// If we happened to end our last buffer right at the end of the LLSD,
		// but the stream is still going we will get a null buffer here.
		// Check for mGracefullStop.
		if (!buffer)
		{
			break;
		}
		count = get_till_eol(input, (char*)buffer, BUFFER_SIZE);
		if (!count)
		{
			break;
		}
		status = XML_ParseBuffer(mParser, count, false);

		if (status == XML_STATUS_ERROR)
		{
			break;
		}
	}

	// *FIX.: this code is buggy; if the stream was empty or not good, there is
	// no buffer to parse, both the call to XML_ParseBuffer and the buffer
	// manipulations are illegal futhermore, it is not clear that the expat
	// buffer semantics are preserved

	status = XML_ParseBuffer(mParser, 0, true);
	if (status == XML_STATUS_ERROR && !mGracefullStop)
	{
		if (buffer)
		{
			((char*)buffer)[count ? count - 1 : 0] = '\0';
		}
		if (mEmitErrors)
		{
			llwarns << "XML_STATUS_ERROR parsing: " << (char*)buffer << llendl;
		}
		data = LLSD();
		return LLSDParser::PARSE_FAILURE;
	}

	clear_eol(input);
	data = mResult;
	return mParseCount;
}

S32 LLSDXMLParser::Impl::parseLines(std::istream& input, LLSD& data)
{
	XML_Status status = XML_STATUS_OK;

	data = LLSD();

	constexpr int BUFFER_SIZE = 1024;

	// Must get rid of any leading \n, otherwise the stream gets into an
	// error/eof state
	clear_eol(input);

	while (!mGracefullStop && input.good() && !input.eof())
	{
		void* buffer = XML_GetBuffer(mParser, BUFFER_SIZE);
		// If we happened to end our last buffer right at the end of the LLSD,
		// but the stream is still going we will get a null buffer here.
		// Check for mGracefullStop.
		// I do not think this is actually true - zero 2008-05-09
		if (!buffer)
		{
			break;
		}

		// Get one line
		input.getline((char*)buffer, BUFFER_SIZE);
		std::streamsize num_read = input.gcount();
		if (num_read > 0)
		{
			if (!input.good())
			{
				// Clear state that is set when we run out of buffer
				input.clear();
			}

			// Re-insert with the \n that was absorbed by getline()
			char * text = (char *) buffer;
			if (text[num_read - 1] == 0)
			{
				text[num_read - 1] = '\n';
			}
		}

		status = XML_ParseBuffer(mParser, num_read, false);
		if (status == XML_STATUS_ERROR)
		{
			break;
		}
	}

	if (status != XML_STATUS_ERROR && !mGracefullStop)
	{
		// Parse last bit
		status = XML_ParseBuffer(mParser, 0, true);
	}

	if (status == XML_STATUS_ERROR && !mGracefullStop)
	{
		if (mEmitErrors)
		{
			llwarns << "XML_STATUS_ERROR" << llendl;
		}
		return LLSDParser::PARSE_FAILURE;
	}

	clear_eol(input);
	data = mResult;
	return mParseCount;
}

void LLSDXMLParser::Impl::reset()
{
	mResult.clear();
	mParseCount = 0;

	mInLLSDElement = false;
	mDepth = 0;

	mGracefullStop = false;

	mStack.clear();

	mSkipping = false;

	mCurrentKey.clear();

	XML_ParserReset(mParser, "utf-8");
	XML_SetUserData(mParser, this);
	XML_SetElementHandler(mParser, sStartElementHandler, sEndElementHandler);
	XML_SetCharacterDataHandler(mParser, sCharacterDataHandler);
}

void LLSDXMLParser::Impl::startSkipping()
{
	mSkipping = true;
	mSkipThrough = mDepth;
}

const XML_Char* LLSDXMLParser::Impl::findAttribute(const XML_Char* name,
												   const XML_Char** pairs)
{
	while (pairs && *pairs)
	{
		if (strcmp(name, *pairs) == 0)
		{
			return *(pairs + 1);
		}
		pairs += 2;
	}
	return NULL;
}

void LLSDXMLParser::Impl::parsePart(const char* buf, llssize len)
{
	if (buf && len > 0)
	{
		XML_Status status = XML_Parse(mParser, buf, len, false);
		if (status == XML_STATUS_ERROR)
		{
			llinfos << "Unexpected XML parsing error at start" << llendl;
		}
	}
}

void LLSDXMLParser::Impl::startElementHandler(const XML_Char* name,
											  const XML_Char** attributes)
{
	++mDepth;
	if (mSkipping)
	{
		return;
	}

	Element element = readElement(name);

	mCurrentContent.clear();

	switch (element)
	{
		case ELEMENT_LLSD:
			if (mInLLSDElement)
			{
				return startSkipping();
			}
			mInLLSDElement = true;
			return;

		case ELEMENT_KEY:
			if (mStack.empty() || !mStack.back()->isMap())
			{
				return startSkipping();
			}
			return;

		case ELEMENT_BINARY:
		{
			const XML_Char* encoding = findAttribute("encoding", attributes);
			if (encoding && strcmp("base64", encoding) != 0)
			{
				return startSkipping();
			}
			break;
		}

		default:
			// All the rest are values, fall through
			break;
	}

	if (!mInLLSDElement)
	{
		return startSkipping();
	}

	if (mStack.empty())
	{
		mStack.push_back(&mResult);
	}
	else if (mStack.back()->isMap())
	{
		if (mCurrentKey.empty())
		{
			return startSkipping();
		}

		LLSD& map = *mStack.back();
		LLSD& newElement = map[mCurrentKey];
		mStack.push_back(&newElement);

		mCurrentKey.clear();
	}
	else if (mStack.back()->isArray())
	{
		LLSD& array = *mStack.back();
		array.append(LLSD());
		LLSD& newElement = array[array.size() - 1];
		mStack.push_back(&newElement);
	}
	else
	{
		// Improperly nested value in a non-structure
		return startSkipping();
	}

	++mParseCount;
	switch (element)
	{
		case ELEMENT_MAP:
			*mStack.back() = LLSD::emptyMap();
			break;

		case ELEMENT_ARRAY:
			*mStack.back() = LLSD::emptyArray();
			break;

		default:
			// All the other values will be set in the end element handler
			break;
	}
}

void LLSDXMLParser::Impl::endElementHandler(const XML_Char* name)
{
	--mDepth;
	if (mSkipping)
	{
		if (mDepth < mSkipThrough)
		{
			mSkipping = false;
		}
		return;
	}

	Element element = readElement(name);

	switch (element)
	{
		case ELEMENT_LLSD:
			if (mInLLSDElement)
			{
				mInLLSDElement = false;
				mGracefullStop = true;
				XML_StopParser(mParser, false);
			}
			return;

		case ELEMENT_KEY:
			mCurrentKey = mCurrentContent;
			return;

		default:
			// All rest are values, fall through
			;
	}

	if (!mInLLSDElement)
	{
		return;
	}

	LLSD& value = *mStack.back();
	mStack.pop_back();

	switch (element)
	{
		case ELEMENT_UNDEF:
			value.clear();
			break;

		case ELEMENT_BOOL:
			value = mCurrentContent == "true" || mCurrentContent == "1";
			break;

		case ELEMENT_INTEGER:
		{
			S32 i;
			if (sscanf(mCurrentContent.c_str(), "%d", &i) == 1)
			{
				// See if sscanf works - it is faster
				value = i;
			}
			else
			{
				value = LLSD(mCurrentContent).asInteger();
			}
			break;
		}

		case ELEMENT_REAL:
		{
			// sscanf() is sensitive to the locale and will fail decoding
			// the decimal point for locales where the decimal separator
			// is a comma... So, better not using sscanf() for this purpose.
			// See http://jira.secondlife.com/browse/EXP-700
#if 0
			F64 r;
			if (sscanf(mCurrentContent.c_str(), "%lf", &r) == 1)
			{
				// See if sscanf works - it is faster
				value = r;
			}
			else
			{
				value = LLSD(mCurrentContent).asReal();
			}
#else
			value = LLSD(mCurrentContent).asReal();
#endif
			break;
		}

		case ELEMENT_STRING:
			value = mCurrentContent;
			break;

		case ELEMENT_UUID:
			value = LLSD(mCurrentContent).asUUID();
			break;

		case ELEMENT_DATE:
			value = LLSD(mCurrentContent).asDate();
			break;

		case ELEMENT_URI:
			value = LLSD(mCurrentContent).asURI();
			break;

		case ELEMENT_BINARY:
		{
			// Fix for white spaces in base64, created by python and other
			// non-linden systems. HB: rewritten to avoid costly regex usage.
			std::string stripped;
			for (size_t i = 0, count = mCurrentContent.size(); i < count; ++i)
			{
				const char& c = mCurrentContent[i];
				if (!isspace(c))
				{
					stripped += c;
				}
			}
			size_t len = LLBase64::decodeLen(stripped.c_str());
			LLSD::Binary buff;
			buff.resize(len);
			len = LLBase64::decode(buff.data(), stripped.c_str());
			buff.resize(len);
			value = buff;
			break;
		}

		case ELEMENT_UNKNOWN:
			value.clear();
			break;

		default:
			// Other values, map and array, have already been set
			break;
	}

	mCurrentContent.clear();
}

void LLSDXMLParser::Impl::characterDataHandler(const XML_Char* data,
											   int length)
{
	mCurrentContent.append(data, length);
}

void LLSDXMLParser::Impl::sStartElementHandler(void* userData,
											   const XML_Char* name,
											   const XML_Char** attributes)
{
	((LLSDXMLParser::Impl*)userData)->startElementHandler(name, attributes);
}

void LLSDXMLParser::Impl::sEndElementHandler(void* userData,
											 const XML_Char* name)
{
	((LLSDXMLParser::Impl*)userData)->endElementHandler(name);
}

void LLSDXMLParser::Impl::sCharacterDataHandler(void* userData,
												const XML_Char* data,
												int length)
{
	((LLSDXMLParser::Impl*)userData)->characterDataHandler(data, length);
}

/*
	This code is time critical

	This is a sample of tag occurances of text in simstate file with ~8000 objects.
	A tag pair (<key>something</key>) counts is counted as two:

		key     - 2680178
		real    - 1818362
		integer -  906078
		array   -  295682
		map     -  191818
		uuid    -  177903
		binary  -  175748
		string  -   53482
		undef   -   40353
		boolean -   33874
		llsd    -   16332
		uri     -      38
		date    -       1
*/
LLSDXMLParser::Impl::Element LLSDXMLParser::Impl::readElement(const XML_Char* name)
{
	XML_Char c = *name++;
	switch (c)
	{
		case 'k':
			if (strcmp(name, "ey") == 0)
			{
				return ELEMENT_KEY;
			}
			break;

		case 'r':
			if (strcmp(name, "eal") == 0)
			{
				return ELEMENT_REAL;
			}
			break;

		case 'i':
			if (strcmp(name, "nteger") == 0)
			{
				return ELEMENT_INTEGER;
			}
			break;

		case 'a':
			if (strcmp(name, "rray") == 0)
			{
				return ELEMENT_ARRAY;
			}
			break;

		case 'm':
			if (strcmp(name, "ap") == 0)
			{
				return ELEMENT_MAP;
			}
			break;

		case 'u':
			if (strcmp(name, "uid") == 0)
			{
				return ELEMENT_UUID;
			}
			if (strcmp(name, "ndef") == 0)
			{
				return ELEMENT_UNDEF;
			}
			if (strcmp(name, "ri") == 0)
			{
				return ELEMENT_URI;
			}
			break;

		case 'b':
			if (strcmp(name, "inary") == 0)
			{
				return ELEMENT_BINARY;
			}
			if (strcmp(name, "oolean") == 0)
			{
				return ELEMENT_BOOL;
			}
			break;

		case 's':
			if (strcmp(name, "tring") == 0)
			{
				return ELEMENT_STRING;
			}
			break;

		case 'l':
			if (strcmp(name, "lsd") == 0)
			{
				return ELEMENT_LLSD;
			}
			break;

		case 'd':
			if (strcmp(name, "ate") == 0)
			{
				return ELEMENT_DATE;
			}
			break;
	}
	return ELEMENT_UNKNOWN;
}

///////////////////////////////////////////////////////////////////////////////
// LLSDXMLParser class
///////////////////////////////////////////////////////////////////////////////

LLSDXMLParser::LLSDXMLParser(bool emit_errors)
:	impl(*new Impl(emit_errors))
{
}

LLSDXMLParser::~LLSDXMLParser()
{
	delete &impl;
}

void LLSDXMLParser::parsePart(const char* buf, llssize len)
{
	impl.parsePart(buf, len);
}

//virtual
S32 LLSDXMLParser::doParse(std::istream& input, LLSD& data, S32) const
{
	if (mParseLines)
	{
		// Use line-based reading (faster code)
		return impl.parseLines(input, data);
	}

	return impl.parse(input, data);
}

//virtual
void LLSDXMLParser::doReset()
{
	impl.reset();
}
