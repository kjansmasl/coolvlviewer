/**
 * @file llstring.h
 * @brief String utility functions and std::string class.
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

#ifndef LL_LLSTRING_H
#define LL_LLSTRING_H

#include <algorithm>
#include <cstdio>
#include <locale>
#include <iomanip>
#include <string>

#include "boost/scoped_ptr.hpp"

#include "llsd.h"

#if LL_LINUX
# include <wctype.h>
# include <wchar.h>
#endif

#include <string.h>

constexpr char LL_UNKNOWN_CHAR = '?';

class LLStringOps
{
public:
	LL_INLINE static char toUpper(char elem)		{ return toupper((unsigned char)elem); }
	LL_INLINE static llwchar toUpper(llwchar elem)	{ return towupper(elem); }

	LL_INLINE static char toLower(char elem)		{ return tolower((unsigned char)elem); }
	LL_INLINE static llwchar toLower(llwchar elem)	{ return towlower(elem); }

	LL_INLINE static bool isSpace(char elem)		{ return isspace((unsigned char)elem) != 0; }
	LL_INLINE static bool isSpace(llwchar elem)		{ return iswspace(elem) != 0; }

	LL_INLINE static bool isUpper(char elem)		{ return isupper((unsigned char)elem) != 0; }
	LL_INLINE static bool isUpper(llwchar elem)		{ return iswupper(elem) != 0; }

	LL_INLINE static bool isLower(char elem)		{ return islower((unsigned char)elem) != 0; }
	LL_INLINE static bool isLower(llwchar elem)		{ return iswlower(elem) != 0; }

	LL_INLINE static bool isDigit(char a)			{ return isdigit((unsigned char)a) != 0; }
	LL_INLINE static bool isDigit(llwchar a)		{ return iswdigit(a) != 0; }

	LL_INLINE static bool isPunct(char a)			{ return ispunct((unsigned char)a) != 0; }
	LL_INLINE static bool isPunct(llwchar a)		{ return iswpunct(a) != 0; }

	LL_INLINE static bool isAlpha(char a)			{ return isalpha((unsigned char)a) != 0; }
	LL_INLINE static bool isAlpha(llwchar a)		{ return iswalpha(a) != 0; }

	LL_INLINE static bool isAlnum(char a)			{ return isalnum((unsigned char)a) != 0; }
	LL_INLINE static bool isAlnum(llwchar a)		{ return iswalnum(a) != 0; }

	LL_INLINE static S32 collate(const char* a, const char* b)
	{
		return strcoll(a, b);
	}

	static S32 collate(const llwchar* a, const llwchar* b);

	static bool isHexString(const std::string& str);

	static void setupDatetimeInfo(bool pacific_daylight_time);

	static void setupWeekDaysNames(const std::string& data);
	static void setupWeekDaysShortNames(const std::string& data);
	static void setupMonthNames(const std::string& data);
	static void setupMonthShortNames(const std::string& data);
	static void setupDayFormat(const std::string& data);

	LL_INLINE static long getPacificTimeOffset()	{ return sPacificTimeOffset;}
	LL_INLINE static long getLocalTimeOffset()		{ return sLocalTimeOffset;}
	// Returns true when the Pacific time zone (aka server time zone) is
	// currently in daylight savings time.
	LL_INLINE static bool getPacificDaylightTime()	{ return sPacificDaylightTime;}

	static std::string getDatetimeCode(std::string key);

public:
	static std::vector<std::string> sWeekDayList;
	static std::vector<std::string> sWeekDayShortList;
	static std::vector<std::string> sMonthList;
	static std::vector<std::string> sMonthShortList;
	static std::string sDayFormat;

	static std::string sAM;
	static std::string sPM;

private:
	static long sPacificTimeOffset;
	static long sLocalTimeOffset;
	static bool sPacificDaylightTime;

	static std::map<std::string, std::string> datetimeToCodes;
};

// Return a string constructed from in without crashing if the pointer is NULL.

LL_INLINE std::string ll_safe_string(const char* in)
{
	if (in)
	{
		return std::string(in);
	}
	return std::string();
}

LL_INLINE std::string ll_safe_string(const char* in, S32 maxlen)
{
	if (in && maxlen > 0)
	{
		return std::string(in, maxlen);
	}
	return std::string();
}

// Allowing assignments from non-strings into format_map_t is apparently
// *really* error-prone, so subclass std::string with just basic c'tors.
class LLFormatMapString
{
public:
	LLFormatMapString()
	{
	}

	LLFormatMapString(const char* s)
	:	mString(ll_safe_string(s))
	{
	}

	LLFormatMapString(const std::string& s)
	:	mString(s)
	{
	}

	LL_INLINE operator std::string() const			{ return mString; }

	LL_INLINE bool operator<(const LLFormatMapString& rhs) const
	{
		return mString < rhs.mString;
	}

	LL_INLINE std::size_t length() const			{ return mString.length(); }

private:
	std::string mString;
};

template <class T>
class LLStringUtilBase
{
private:
	static std::string sLocale;

public:
	typedef std::basic_string<T> string_type;
	typedef typename string_type::size_type size_type;

public:
	/////////////////////////////////////////////////////////////////////////////////////////
	// Static Utility methods that operate on std::strings

	static const string_type null;

	typedef std::map<LLFormatMapString, LLFormatMapString> format_map_t;

	static void getTokens(const string_type& instr,
						  std::vector<string_type>& tokens,
						  const string_type& delims);
	// Like simple scan overload, but returns scanned vector
	static std::vector<string_type> getTokens(const string_type& instr,
											  const string_type& delims);
	// Adds support for keep_delims and quotes (either could be empty string)
	static void getTokens(const string_type& instr,
						  std::vector<string_type>& tokens,
						  const string_type& drop_delims,
						  const string_type& keep_delims,
						  const string_type& quotes = string_type());
	// Like keep_delims-and-quotes overload, but returns scanned vector
	static std::vector<string_type> getTokens(const string_type& instr,
											  const string_type& drop_delims,
											  const string_type& keep_delims,
											  const string_type& quotes =
												string_type());
	// Adds support for escapes (could be empty string)
	static void getTokens(const string_type& instr,
						  std::vector<string_type>& tokens,
						  const string_type& drop_delims,
						  const string_type& keep_delims,
						  const string_type& quotes,
						  const string_type& escapes);
	// Like escapes overload, but returns scanned vector
	static std::vector<string_type> getTokens(const string_type& instr,
											  const string_type& drop_delims,
											  const string_type& keep_delims,
											  const string_type& quotes,
											  const string_type& escapes);

	static void formatNumber(string_type& num_str, S32 decimals);
	static bool formatDatetime(string_type& replacement,
							   const string_type& token,
							   const string_type& param, S32 sec_from_epoch);
	static S32 format(string_type& s, const format_map_t& substitutions);
	static S32 format(string_type& s, const LLSD& substitutions);
	static bool simpleReplacement(string_type& replacement,
								  const string_type& token,
								  const format_map_t& substitutions);
	static bool simpleReplacement(string_type& replacement,
								  const string_type& token,
								  const LLSD& substitutions);
	static void setLocale(std::string in_locale);
	static std::string getLocale();

	LL_INLINE static bool contains(const string_type& string, T c,
								   size_type i = 0)
	{
		return string.find(c, i) != string_type::npos;
	}

	static void	trimHead(string_type& string);
	static void	trimTail(string_type& string);
	LL_INLINE static void trim(string_type& string)
	{
		trimHead(string);
		trimTail(string);
	}
	static void truncate(string_type& string, size_type count);

	static void	toUpper(string_type& string);
	static void	toLower(string_type& string);

	// True if this is the head of s.
	static bool	isHead(const string_type& string, const T* s);

	// Returns true if string starts with substr. If either string or substr
	// are empty, this method returns false.
	static bool startsWith(const string_type& str, const string_type& substr);

	// Returns true if string starts with substr. If either string or substr
	// are empty, this method returns false.
	static bool endsWith(const string_type& string, const string_type& substr);

	static void	addCRLF(string_type& string);
	static void	removeCRLF(string_type& string);

	static void	replaceTabsWithSpaces(string_type& string,
									  size_type spaces_per_tab);
	static void	replaceNonstandardASCII(string_type& string, T replacement);
	static void	replaceChar(string_type& string, T target, T replacement);
	static void replaceString(string_type& string, string_type target,
							  string_type replacement);

	static bool	containsNonprintable(const string_type& string);
	static void	stripNonprintable(string_type& string);

	// Unsafe way to make ascii characters. You should probably only call this
	// when interacting with the host operating system.
	// The 1 byte std::string does not work correctly.
	// The 2 and 4 byte std::string probably work, so LLWStringUtil::_makeASCII
	// should work.
	static void _makeASCII(string_type& string);

	// Conversion to other data types
	static bool	convertToBool(const string_type& string, bool& value);
	static bool	convertToU8(const string_type& string, U8& value);
	static bool	convertToS8(const string_type& string, S8& value);
	static bool	convertToS16(const string_type& string, S16& value);
	static bool	convertToU16(const string_type& string, U16& value);
	static bool	convertToU32(const string_type& string, U32& value);
	static bool	convertToS32(const string_type& string, S32& value);
	static bool	convertToF32(const string_type& string, F32& value);
	static bool	convertToF64(const string_type& string, F64& value);

	///////////////////////////////////////////////////////////////////////////
	// Utility methods for working with char*'s and strings

	// Like strcmp but also handles empty strings. Uses current locale.
	static S32 compareStrings(const T* lhs, const T* rhs);
	static S32 compareStrings(const string_type& lhs,
							  const string_type& rhs);

	// Case-insensitive version of above. Uses current locale on Win32, and
	// falls back to a non-locale aware comparison on Linux.
	static S32 compareInsensitive(const T* lhs, const T* rhs);
	static S32 compareInsensitive(const string_type& lhs,
								  const string_type& rhs);

	// Case-sensitive comparison with good handling of numbers. Does not use
	// current locale.
	// a.k.a. strdictcmp()
	static S32 compareDict(const string_type& a, const string_type& b);

	// Case *in*sensitive comparison with good handling of numbers. Does not
	// use current locale.
	// a.k.a. strdictcmp()
	static S32 compareDictInsensitive(const string_type& a,
									  const string_type& b);

	// Puts compareDict() in a form appropriate for LL container classes to use
	// for sorting.
	static bool precedesDict(const string_type& a, const string_type& b);

	// A replacement for strncpy.
	// If the dst buffer is dst_size bytes long or more, ensures that dst is
	// null terminated and holds up to dst_size-1 characters of src.
	static void copy(T* dst, const T* src, size_type dst_size);

	// Copies src into dst at a given offset.
	static void copyInto(string_type& dst, const string_type& src,
						 size_type offset);

	LL_INLINE static bool isPartOfWord(T c)
	{
		return (c == (T)'_') || LLStringOps::isAlnum(c);
	}

	LL_INLINE static bool isPartOfLexicalWord(T c)
	{
		return (c == (T)'\'') || LLStringOps::isAlpha(c) ||
				!(LLStringOps::isDigit(c) || LLStringOps::isSpace(c) ||
				LLStringOps::isPunct(c));
	}

private:
	static size_type getSubstitution(const string_type& instr,
									 size_type& start,
									 std::vector<string_type >& tokens);
};

template<class T> const std::basic_string<T> LLStringUtilBase<T>::null;
template<class T> std::string LLStringUtilBase<T>::sLocale;

typedef LLStringUtilBase<char> LLStringUtil;
typedef LLStringUtilBase<llwchar> LLWStringUtil;
typedef std::basic_string<llwchar> LLWString;

struct LLDictionaryLess
{
public:
	LL_INLINE bool operator()(const std::string& a, const std::string& b) const
	{
		return LLStringUtil::precedesDict(a, b);
	}
};

/**
 * Simple support functions
 */

/**
 * @brief chop off the trailing characters in a string.
 *
 * This function works on bytes rather than glyphs, so this will incorrectly
 * truncate non-single byte strings. Use utf8str_truncate() for utf8 strings.
 * @return a copy of in string minus the trailing count bytes.
 */
LL_INLINE std::string chop_tail_copy(const std::string& in,
									 std::string::size_type count)
{
	return std::string(in, 0, in.length() - count);
}

/**
 * @brief This translates a nybble stored as a hex value from 0-f back to a
 * nybble in the low order bits of the return byte.
 */
U8 hex_as_nybble(char hex);

bool is_char_hex(char hex);

/**
 * @brief read the contents of a file into a string.
 *
 * Since this function has no concept of character encoding, most anything you
 * do with this method ill-advised. Please avoid.
 * @param str [out] The string which will have.
 * @param filename The full name of the file to read.
 * @return Returns true on success. If false, str is unmodified.
 */
bool _read_file_into_string(std::string& str, const std::string& filename);
bool iswindividual(llwchar elem);

std::string capitalized(const std::string& str);

/**
 * Unicode support
 */

 //
 // We should never use UTF16 except when communicating with Win32 (or the macOS
 // clipboard, needing utf16str_to_wstring()) !
 //
typedef std::basic_string<U16> llutf16string;

#if LL_WINDOWS && defined(_NATIVE_WCHAR_T_DEFINED)
// wchar_t is a distinct native type, so llutf16string is also a distinct type
// and there IS a point to converting separately to/from llutf16string.

// Generic conversion aliases
template<typename TO, typename FROM, typename Enable = void>
class ll_convert_impl
{
public:
	// Do not even provide a generic implementation. We specialize for every
	// combination we do support.
	TO operator()(const FROM& in) const;
};

// Use a function template to get the nice ll_convert<TO>(from_value) API.
template<typename TO, typename FROM>
TO ll_convert(const FROM& in)
{
	return ll_convert_impl<TO, FROM>()(in);
}

// Degenerate case
template<typename T>
class ll_convert_impl<T, T>
{
public:
	LL_INLINE T operator()(const T& in) const	{ return in; }
};

// Specialize ll_convert_impl<TO, FROM> to return EXPR
# define LL_CONVERT_ALIAS(TO, FROM, EXPR)					\
template<>													\
class ll_convert_impl<TO, FROM>								\
{															\
public:														\
	TO operator()(const FROM& in) const { return EXPR; }	\
};

// LLWString is identical to std::wstring, so these aliases for std::wstring
// would collide with those for LLWString; converting between std::wstring and
// llutf16string means copying chars.
LL_CONVERT_ALIAS(llutf16string, std::wstring, llutf16string(in.begin(), in.end()));
LL_CONVERT_ALIAS(std::wstring, llutf16string, std::wstring(in.begin(), in.end()));

#else	// LL_WINDOWS && defined(_NATIVE_WCHAR_T_DEFINED)
// No such conversions needed under Linux, macOS, or Windows with /Zc:wchar_t-
// MSVC compilation option.
# define LL_CONVERT_ALIAS(TO, FROM, EXPR)
#endif	// LL_WINDOWS && defined(_NATIVE_WCHAR_T_DEFINED)


LLWString utf16str_to_wstring(const llutf16string& utf16str, S32 len);

LL_INLINE LLWString utf16str_to_wstring(const llutf16string& utf16str)
{
	return utf16str_to_wstring(utf16str, (S32)utf16str.length());
}

LL_CONVERT_ALIAS(LLWString, llutf16string, utf16str_to_wstring(in));

llutf16string wstring_to_utf16str(const LLWString& utf32str, S32 len);

LL_INLINE llutf16string wstring_to_utf16str(const LLWString& utf32str)
{
	return wstring_to_utf16str(utf32str, (S32)utf32str.length());
}

LL_CONVERT_ALIAS(llutf16string, LLWString, wstring_to_utf16str(in));

LLWString utf8str_to_wstring(const std::string& utf8str, S32 len);

LL_INLINE LLWString utf8str_to_wstring(const std::string& utf8str)
{
	return utf8str_to_wstring(utf8str, (S32)utf8str.length());
}

LL_CONVERT_ALIAS(LLWString, std::string, utf8str_to_wstring(in));

LL_INLINE llutf16string utf8str_to_utf16str(const std::string& utf8str)
{
	return wstring_to_utf16str(utf8str_to_wstring(utf8str));
}

LL_CONVERT_ALIAS(llutf16string, std::string, utf8str_to_utf16str(in));

std::ptrdiff_t wchar_to_utf8chars(llwchar inchar, char* outchars);

std::string wstring_to_utf8str(const LLWString& utf32str, S32 len);

LL_INLINE std::string wstring_to_utf8str(const LLWString& utf32str)
{
	return wstring_to_utf8str(utf32str, (S32)utf32str.length());
}

LL_CONVERT_ALIAS(std::string, LLWString, wstring_to_utf8str(in));

// Make the incoming string a utf8 string. Replaces any unknown glyph
// with the UNKNOWN_CHARACTER. Once any unknown glyph is found, the rest
// of the data may not be recovered.
LL_INLINE std::string rawstr_to_utf8(const std::string& raw)
{
	return wstring_to_utf8str(utf8str_to_wstring(raw));
}

LL_INLINE std::string utf16str_to_utf8str(const llutf16string& utf16str,
										  S32 len)
{
	return wstring_to_utf8str(utf16str_to_wstring(utf16str, len), len);
}

LL_INLINE std::string utf16str_to_utf8str(const llutf16string& utf16str)
{
	return wstring_to_utf8str(utf16str_to_wstring(utf16str));
}

LL_CONVERT_ALIAS(std::string, llutf16string, utf16str_to_utf8str(in));

// Length of this UTF32 string in bytes when transformed to UTF8
S32 wstring_utf8_length(const LLWString& wstr);

// Length in bytes of this wide char in a UTF8 string
S32 wchar_utf8_length(const llwchar wc);

std::string utf8str_tolower(const std::string& utf8str);

// Length in llwchar (UTF-32) of the first len units (16 bits) of the given
// UTF-16 string.
S32 utf16str_wstring_length(const llutf16string& utf16str, S32 len);

// Length in utf16string (UTF-16) of wlen wchars beginning at woffset.
S32 wstring_utf16_length(const LLWString& wstr, S32 woffset, S32 wlen);

// Length in wstring (i.e., llwchar count) of a part of a wstring specified by
// utf16 length (i.e., utf16 units.)
S32 wstring_length_from_utf16_length(const LLWString& wstr, S32 woffset,
									 S32 utf16_length, bool* unaligned = NULL);

/**
 * @brief Properly truncate a utf8 string to a maximum byte count.
 *
 * The returned string may be less than max_len if the truncation happens in
 * the middle of a glyph. If max_len is longer than the string passed in, the
 * return value == utf8str.
 * @param utf8str A valid utf8 string to truncate.
 * @param max_len The maximum number of bytes in the return value.
 * @return Returns a valid utf8 string with byte count <= max_len.
 */
std::string utf8str_truncate(const std::string& utf8str, S32 max_len);

std::string utf8str_trim(const std::string& utf8str);

S32 utf8str_compare_insensitive(const std::string& lhs,
								const std::string& rhs);

/**
 * @brief Replace all occurences of target_char with replace_char
 *
 * @param utf8str A utf8 string to process.
 * @param target_char The wchar to be replaced
 * @param replace_char The wchar which is written on replace
 */
std::string utf8str_substChar(const std::string& utf8str,
							  const llwchar target_char,
							  const llwchar replace_char);

std::string utf8str_makeASCII(const std::string& utf8str);

// Hack - used for evil notecards.
std::string mbcsstring_makeASCII(const std::string& str);

std::string utf8str_removeCRLF(const std::string& utf8str);

std::string iso8859_to_utf8(const std::string& iso8859str);
std::string utf8_to_iso8859(const std::string& utf8str);

#if LL_WINDOWS
// Windows string helpers

// Converts a wide string to std::string. This replaces the unsafe W2A macro
// from ATL.
std::string ll_convert_wide_to_string(const wchar_t* in,
									  unsigned int code_page);
// Defaults to CP_UTF8
std::string ll_convert_wide_to_string(const wchar_t* in);

// Converts a string to wide string.
std::wstring ll_convert_string_to_wide(const std::string& in,
									   unsigned int code_page);
std::wstring ll_convert_string_to_wide(const std::string& in);

// Defaults CP_UTF8
LL_CONVERT_ALIAS(std::wstring, std::string, ll_convert_string_to_wide(in));

LLWString ll_convert_wide_to_wstring(const std::wstring& in);
LL_CONVERT_ALIAS(LLWString, std::wstring, ll_convert_wide_to_wstring(in));

// Converts incoming string into utf8 string
std::string ll_convert_string_to_utf8_string(const std::string& in);

#endif // LL_WINDOWS

///////////////////////////////////////////////////////////////////////////////
// Formerly in u64.h - Utilities for conversions between U64 and string
///////////////////////////////////////////////////////////////////////////////

// Forgivingly parses a nul terminated character array. Returns the first U64
// value found in the string or 0 on failure.
U64 str_to_U64(const std::string& str);

// Given a U64 value, returns a printable representation. 'value' is the U64 to
// turn into a printable character array. Returns the result string.
std::string U64_to_str(U64 value);

// Given a U64 value, returns a printable representation.
// The client of this function is expected to provide an allocated buffer. The
// function then snprintf() into that buffer, so providing NULL has undefined
// behavior. Providing a buffer which is too small will truncate the printable
// value, so usually you want to declare the buffer:
//  char result[U64_BUF];
//  std::cout << "value: " << U64_to_str(value, result, U64_BUF);
//
// 'value' is the U64 to turn into a printable character array.
// 'result' is the buffer to use.
// 'result_size' is the size of the buffer allocated. Use U64_BUF.
// Returns the result pointer.
char* U64_to_str(U64 value, char* result, S32 result_size);

// Helper function to wrap strtoull() which is not available on windows.
U64 llstrtou64(const char* str, char** end, S32 base);

///////////////////////////////////////////////////////////////////////////////

/**
 * Many of the 'strip' and 'replace' methods of LLStringUtilBase need
 * specialization to work with the signed char type.
 * Sadly, it is not possible (AFAIK) to specialize a single method of
 * a template class.
 * That stuff should go here.
 */
namespace LLStringFn
{
	/**
	 * @brief Replace all non-printable characters with replacement in
	 * string.
	 * NOTE - this will zap non-ascii
	 *
	 * @param [in,out] string the to modify. out value is the string
	 * with zero non-printable characters.
	 * @param The replacement character. use LL_UNKNOWN_CHAR if unsure.
	 */
	void replace_nonprintable_in_ascii(std::basic_string<char>& string,
									   char replacement);

	/**
	 * @brief Replace all non-printable characters and pipe characters
	 * with replacement in a string.
	 * NOTE - this will zap non-ascii
	 *
	 * @param [in,out] the string to modify. out value is the string
	 * with zero non-printable characters and zero pipe characters.
	 * @param The replacement character. use LL_UNKNOWN_CHAR if unsure.
	 */
	void replace_nonprintable_and_pipe_in_ascii(std::basic_string<char>& str,
												char replacement);

	/**
	 * @brief Remove all characters that are not allowed in XML 1.0.
	 * Returns a copy of the string with those characters removed.
	 * Works with US ASCII and UTF-8 encoded strings.  JC
	 */
	std::string strip_invalid_xml(const std::string& input);

	/**
	 * @brief Replace all control characters (0 <= c < 0x20) with replacement in
	 * string.   This is safe for utf-8
	 *
	 * @param [in,out] string the to modify. out value is the string
	 * with zero non-printable characters.
	 * @param The replacement character. use LL_UNKNOWN_CHAR if unsure.
	 */
	void replace_ascii_controlchars(std::basic_string<char>& string,
									char replacement);
}

///////////////////////////////////////////////////////////////////////////////
// getTokens() templates
///////////////////////////////////////////////////////////////////////////////

//static
template <class T>
std::vector<typename LLStringUtilBase<T>::string_type>
LLStringUtilBase<T>::getTokens(const string_type& instr,
							   const string_type& delims)
{
	std::vector<string_type> tokens;
	getTokens(instr, tokens, delims);
	return tokens;
}

//static
template <class T>
std::vector<typename LLStringUtilBase<T>::string_type>
LLStringUtilBase<T>::getTokens(const string_type& instr,
							   const string_type& drop_delims,
							   const string_type& keep_delims,
							   const string_type& quotes)
{
	std::vector<string_type> tokens;
	getTokens(instr, tokens, drop_delims, keep_delims, quotes);
	return tokens;
}

//static
template <class T>
std::vector<typename LLStringUtilBase<T>::string_type>
LLStringUtilBase<T>::getTokens(const string_type& instr,
							   const string_type& drop_delims,
							   const string_type& keep_delims,
							   const string_type& quotes,
							   const string_type& escapes)
{
	std::vector<string_type> tokens;
	getTokens(instr, tokens, drop_delims, keep_delims, quotes, escapes);
	return tokens;
}

namespace LLStringUtilBaseImpl
{

// Input string scanner helper for getTokens(), or really any other character
// parsing routine that may have to deal with escape characters. This
// implementation defines the concept (also an interface, should you choose to
// implement the concept by subclassing) and provides trivial implementations
// for a string @em without escape processing.
template <class T>
class InString
{
public:
	typedef std::basic_string<T> string_type;
	typedef typename string_type::const_iterator const_iterator;

	LL_INLINE InString(const_iterator b, const_iterator e)
	:	mIter(b),
		mEnd(e)
	{
	}

	virtual ~InString() = default;

	LL_INLINE bool done() const					{ return mIter == mEnd; }
	// Is the current character (*mIter) escaped ?  This implementation can
	// answer trivially because it does not support escapes.
	LL_INLINE virtual bool escaped() const		{ return false; }
	// Obtains the current character and advances mIter.
	LL_INLINE virtual T next()					{ return *mIter++; }
	// Does the current character match specified character ?
	LL_INLINE virtual bool is(T ch) const		{ return !done() && *mIter == ch; }
	// Is the current character any one of the specified characters ?
	LL_INLINE virtual bool oneof(const string_type& delims) const
	{
		return !done() && LLStringUtilBase<T>::contains(delims, *mIter);
	}

	// Scans forward from 'from' until either 'delim' or end. This is primarily
	// useful for processing quoted sub-strings.
	// If it sees 'delim', appends everything from 'from' until (excluding)
	// 'delim' to 'into', advances mIter to skip 'delim', and returns true.
	// If it does not see 'delim', it does not alter 'into' or mIter and
	// returns false.
	// Note: the false case described above implements normal getTokens()
	// treatment of an unmatched open quote: it treats the quote character as
	// if escaped, that is, simply collects it as part of the current token.
	// Other plausible behaviors directly affect the way getTokens() deals with
	// an unmatched quote: e.g. throwing an exception to treat it as an error,
	// or assuming a close quote beyond end of string (in which case it returns
	// true).
	virtual bool collect_until(string_type& into, const_iterator from, T delim)
	{
		const_iterator found = std::find(from, mEnd, delim);
		// If we did not find delim, change nothing, just tell caller.
		if (found == mEnd)
		{
			return false;
		}
		// Found delim; append everything between from and found.
		into.append(from, found);
		// Advance past delim in input
		mIter = found + 1;
		return true;
	}

public:
	const_iterator mIter;
	const_iterator mEnd;
};

// InString subclass that handles escape characters
template <class T>
class InEscString : public InString<T>
{
public:
	typedef InString<T> super;
	typedef typename super::string_type string_type;
	typedef typename super::const_iterator const_iterator;
	using super::done;
	using super::mIter;
	using super::mEnd;

	LL_INLINE InEscString(const_iterator b, const_iterator e,
						  const string_type& escapes)
	:	super(b, e),
		mEscapes(escapes)
	{
		// Even though we have already initialized 'mIter' via our base-class
		// constructor, set it again to check for initial escape char.
		setiter(b);
	}

	// This implementation uses the answer cached by setiter().
	LL_INLINE bool escaped() const override		{ return mIsEsc; }

	T next() override
	{
		// If we are looking at the escape character of an escape sequence,
		// skip that character. This is the one time we can modify mIter
		// without using setiter: for this one case we DO NOT CARE if the
		// escaped character is itself an escape.
		if (mIsEsc)
		{
			++mIter;
		}
		// If we were looking at an escape character, this is the escaped
		// character; otherwise it is just the next character.
		T result(*mIter);
		// Advance mIter, checking for escape sequence.
		setiter(mIter + 1);
		return result;
	}

	LL_INLINE bool is(T ch) const override
	{
		// Like base-class is(), except that an escaped character matches
		// nothing.
		return !mIsEsc && !done() && *mIter == ch;
	}

	LL_INLINE bool oneof(const string_type& delims) const override
	{
		// Like base-class is(), except that an escaped character matches
		// nothing.
		return !mIsEsc && !done() &&
			   LLStringUtilBase<T>::contains(delims, *mIter);
	}

	bool collect_until(string_type& into, const_iterator from,
					   T delim) override
	{
		// Deal with escapes in the characters we collect; that is, an escaped
		// character must become just that character without the preceding
		// escape. Collect characters in a separate string rather than directly
		// appending to 'into' in case we do not find delim, in which case we
		// are supposed to leave 'into' unmodified.
		string_type collected;
		// For scanning purposes, we are going to work directly with 'mIter'.
		// Save its current value in case we fail to see delim.
		const_iterator save_iter(mIter);
		// Okay, set 'mIter', checking for escape.
		setiter(from);
		while (!done())
		{
			// If we see an unescaped delim, stop and report success.
			if (!mIsEsc && *mIter == delim)
			{
				// Append collected chars to 'into'.
				into.append(collected);
				// Do not forget to advance mIter past 'delim'.
				setiter(mIter + 1);
				return true;
			}
			// We are not at end, and either we're not looking at delim or it
			// is escaped. Collect this character and keep going.
			collected.push_back(next());
		}
		// Here we hit mEnd without ever seeing delim. Restore mIter and tell
		// caller.
		setiter(save_iter);
		return false;
	}

private:
	LL_INLINE void setiter(const_iterator i)
	{
		mIter = i;
		// Every time we change mIter, set mIsEsc to be able to repetitively
		// answer escaped() without having to rescan mEscapes. mIsEsc caches
		// contains(mEscapes, *mIter).
		// We are looking at an escaped char if we are not already at end (that
		// is, *mIter is even meaningful); if *mIter is in fact one of the
		// specified escape characters; and if there is one more character
		// following it. That is, if an escape character is the very last
		// character of the input string, it loses its special meaning.
		mIsEsc = !done() && LLStringUtilBase<T>::contains(mEscapes, *mIter) &&
				 mIter + 1 != mEnd;
	}

private:
	const string_type	mEscapes;
	bool				mIsEsc;
};

// getTokens() implementation based on InString concept
template <typename INSTRING, typename string_type>
void getTokens(INSTRING& instr, std::vector<string_type>& tokens,
			   const string_type& drop_delims, const string_type& keep_delims,
			   const string_type& quotes)
{
	// There are times when we want to match either drop_delims or keep_delims.
	// Concatenate them up front to speed things up.
	string_type all_delims = drop_delims + keep_delims;
	// No tokens yet
	tokens.clear();
	// Try for another token
	while (!instr.done())
	{
		// Scan past any drop_delims
		while (instr.oneof(drop_delims))
		{
			// Skip this drop_delim
			instr.next();
			// But if that was the end of the string, we are done
			if (instr.done())
			{
				return;
			}
		}
		// Found the start of another token: make a slot for it.
		tokens.push_back(string_type());
		if (instr.oneof(keep_delims))
		{
			// *iter is a keep_delim, a token of exactly 1 character. Append
			// that character to the new token and proceed.
			tokens.back().push_back(instr.next());
			continue;
		}
		// Here we have a non-delimiter token, which might consist of a mix of
		// quoted and unquoted parts. Use bash rules for quoting: you can embed
		// a quoted substring in the midst of an unquoted token (e.g.
		// ~/"sub dir"/myfile.txt); you can ram two quoted substrings together
		// to make a single token (e.g. 'He said, "'"Don't."'"'). We diverge
		// from bash in that bash considers an unmatched quote an error. Our
		// param signature does not allow for errors, so just pretend it is not
		// a quote and embed it.
		// At this level, keep scanning until we hit the next delimiter of
		// either type (drop_delims or keep_delims).
		while (!instr.oneof(all_delims))
		{
			// If we are looking at an open quote, search forward for a close
			// quote, collecting characters along the way.
			if (!instr.oneof(quotes) ||
				!instr.collect_until(tokens.back(), instr.mIter + 1,
									 *instr.mIter))
			{
				// Either *iter is not a quote, or there is no matching close
				// quote: in other words, just an ordinary char. Append it to
				// current token.
				tokens.back().push_back(instr.next());
			}
			// Having scanned that segment of this token, if we have reached
			// the end of the string, we are done.
			if (instr.done())
			{
				return;
			}
		}
	}
}

}	// namespace LLStringUtilBaseImpl

//static
template <class T>
void LLStringUtilBase<T>::getTokens(const string_type& string,
									std::vector<string_type>& tokens,
									const string_type& drop_delims,
									const string_type& keep_delims,
									const string_type& quotes)
{
	// Because this overload does not support escapes, use simple InString to
	// manage input range.
	LLStringUtilBaseImpl::InString<T> instring(string.begin(), string.end());
	LLStringUtilBaseImpl::getTokens(instring, tokens, drop_delims, keep_delims,
									quotes);
}

//static
template <class T>
void LLStringUtilBase<T>::getTokens(const string_type& string,
									std::vector<string_type>& tokens,
									const string_type& drop_delims,
									const string_type& keep_delims,
									const string_type& quotes,
									const string_type& escapes)
{
	// This overload must deal with escapes. Delegate that to InEscString
	// (unless there are no escapes).
	boost::scoped_ptr<LLStringUtilBaseImpl::InString<T> > instrp;
	if (escapes.empty())
	{
		instrp.reset(new LLStringUtilBaseImpl::InString<T>(string.begin(),
														   string.end()));
	}
	else
	{
		instrp.reset(new LLStringUtilBaseImpl::InEscString<T>(string.begin(),
															  string.end(),
															  escapes));
	}
	LLStringUtilBaseImpl::getTokens(*instrp, tokens, drop_delims, keep_delims,
									quotes);
}

///////////////////////////////////////////////////////////////////////////////

//static
template<class T>
S32 LLStringUtilBase<T>::compareStrings(const T* lhs, const T* rhs)
{
	S32 result;
	if (lhs == rhs)
	{
		result = 0;
	}
	else if (!lhs || !lhs[0])
	{
		result = ((!rhs || !rhs[0]) ? 0 : 1);
	}
	else if (!rhs || !rhs[0])
	{
		result = -1;
	}
	else
	{
		result = LLStringOps::collate(lhs, rhs);
	}
	return result;
}

//static
template<class T>
S32 LLStringUtilBase<T>::compareStrings(const std::basic_string<T>& lhs,
										const std::basic_string<T>& rhs)
{
	return LLStringOps::collate(lhs.c_str(), rhs.c_str());
}

//static
template<class T>
S32 LLStringUtilBase<T>::compareInsensitive(const T* lhs, const T* rhs)
{
	S32 result;
	if (lhs == rhs)
	{
		result = 0;
	}
	else if (!lhs || !lhs[0])
	{
		result = ((!rhs || !rhs[0]) ? 0 : 1);
	}
	else if (!rhs || !rhs[0])
	{
		result = -1;
	}
	else
	{
		std::basic_string<T> lhs_string(lhs);
		std::basic_string<T> rhs_string(rhs);
		LLStringUtilBase<T>::toUpper(lhs_string);
		LLStringUtilBase<T>::toUpper(rhs_string);
		result = LLStringOps::collate(lhs_string.c_str(), rhs_string.c_str());
	}
	return result;
}

//static
template<class T>
S32 LLStringUtilBase<T>::compareInsensitive(const std::basic_string<T>& lhs,
											const std::basic_string<T>& rhs)
{
	std::basic_string<T> lhs_string(lhs);
	std::basic_string<T> rhs_string(rhs);
	LLStringUtilBase<T>::toUpper(lhs_string);
	LLStringUtilBase<T>::toUpper(rhs_string);
	return LLStringOps::collate(lhs_string.c_str(), rhs_string.c_str());
}

// Case sensitive comparison with good handling of numbers. Does not use
// current locale. AKA strdictcmp()

//static
template<class T>
S32 LLStringUtilBase<T>::compareDict(const std::basic_string<T>& astr,
									 const std::basic_string<T>& bstr)
{
	const T* a = astr.c_str();
	const T* b = bstr.c_str();
	T ca, cb;
	S32 ai, bi, cnt = 0;
	S32 bias = 0;

	ca = *(a++);
	cb = *(b++);
	while (ca && cb)
	{
		if (bias == 0)
		{
			if (LLStringOps::isUpper(ca))
			{
				ca = LLStringOps::toLower(ca);
				--bias;
			}
			if (LLStringOps::isUpper(cb))
			{
				cb = LLStringOps::toLower(cb);
				++bias;
			}
		}
		else
		{
			if (LLStringOps::isUpper(ca))
			{
				ca = LLStringOps::toLower(ca);
			}
			if (LLStringOps::isUpper(cb))
			{
				cb = LLStringOps::toLower(cb);
			}
		}
		if (LLStringOps::isDigit(ca))
		{
			if (cnt-- > 0)
			{
				if (cb != ca) break;
			}
			else
			{
				if (!LLStringOps::isDigit(cb)) break;
				for (ai = 0; LLStringOps::isDigit(a[ai]); ++ai);
				for (bi = 0; LLStringOps::isDigit(b[bi]); ++bi);
				if (ai < bi)
				{
					ca = 0;
					break;
				}
				if (bi < ai)
				{
					cb = 0;
					break;
				}
				if (ca != cb)
				{
					break;
				}
				cnt = ai;
			}
		}
		else if (ca != cb)
		{
			break;
		}
		ca = *(a++);
		cb = *(b++);
	}
	if (ca == cb)
	{
		ca += bias;
	}
	return ca - cb;
}

//static
template<class T>
S32 LLStringUtilBase<T>::compareDictInsensitive(const std::basic_string<T>& astr,
												const std::basic_string<T>& bstr)
{
	const T* a = astr.c_str();
	const T* b = bstr.c_str();
	T ca, cb;
	S32 ai, bi, cnt = 0;

	ca = *(a++);
	cb = *(b++);
	while (ca && cb)
	{
		if (LLStringOps::isUpper(ca))
		{
			ca = LLStringOps::toLower(ca);
		}
		if (LLStringOps::isUpper(cb))
		{
			cb = LLStringOps::toLower(cb);
		}
		if (LLStringOps::isDigit(ca))
		{
			if (cnt-- > 0)
			{
				if (cb != ca) break;
			}
			else
			{
				if (!LLStringOps::isDigit(cb))
				{
					break;
				}
				for (ai = 0; LLStringOps::isDigit(a[ai]); ++ai);
				for (bi = 0; LLStringOps::isDigit(b[bi]); ++bi);
				if (ai < bi)
				{
					ca = 0;
					break;
				}
				if (bi < ai)
				{
					cb = 0;
					break;
				}
				if (ca != cb)
				{
					break;
				}
				cnt = ai;
			}
		}
		else if (ca!=cb)
		{
			break;
		}
		ca = *(a++);
		cb = *(b++);
	}
	return ca - cb;
}

// Puts compareDict() in a form appropriate for LL container classes to use for
// sorting.
//static
template<class T>
bool LLStringUtilBase<T>::precedesDict(const std::basic_string<T>& a,
									   const std::basic_string<T>& b)
{
	if (a.size() && b.size())
	{
		return LLStringUtilBase<T>::compareDict(a.c_str(), b.c_str()) < 0;
	}
	else
	{
		return !b.empty();
	}
}

//static
template<class T>
void LLStringUtilBase<T>::toUpper(std::basic_string<T>& string)
{
	if (!string.empty())
	{
		std::transform(string.begin(), string.end(), string.begin(),
					   (T(*)(T)) &LLStringOps::toUpper);
	}
}

//static
template<class T>
void LLStringUtilBase<T>::toLower(std::basic_string<T>& string)
{
	if (!string.empty())
	{
		std::transform(string.begin(), string.end(), string.begin(),
					   (T(*)(T)) &LLStringOps::toLower);
	}
}

//static
template<class T>
void LLStringUtilBase<T>::trimHead(std::basic_string<T>& string)
{
	if (!string.empty())
	{
		size_type i = 0;
		while (i < string.length() && LLStringOps::isSpace(string[i]))
		{
			++i;
		}
		string.erase(0, i);
	}
}

//static
template<class T>
void LLStringUtilBase<T>::trimTail(std::basic_string<T>& string)
{
	if (string.size())
	{
		size_type len = string.length();
		size_type i = len;
		while (i > 0 && LLStringOps::isSpace(string[i - 1]))
		{
			--i;
		}

		string.erase(i, len - i);
	}
}

// Replace line feeds with carriage return-line feed pairs.
//static
template<class T>
void LLStringUtilBase<T>::addCRLF(std::basic_string<T>& string)
{
	const T LF = 10;
	const T CR = 13;

	// Count the number of line feeds
	size_type count = 0;
	size_type len = string.size();
	size_type i;
	for (i = 0; i < len; ++i)
	{
		if (string[i] == LF)
		{
			++count;
		}
	}

	// Insert a carriage return before each line feed
	if (count)
	{
		size_type size = len + count;
		T* t = new T[size];
		size_type j = 0;
		for (i = 0; i < len; ++i)
		{
			if (string[i] == LF)
			{
				t[j++] = CR;
			}
			t[j++] = string[i];
		}

		string.assign(t, size);
		delete[] t;
	}
}

// Remove all carriage returns
//static
template<class T>
void LLStringUtilBase<T>::removeCRLF(std::basic_string<T>& string)
{
	const T CR = 13;

	size_type cr_count = 0;
	size_type len = string.size();
	size_type i;
	for (i = 0; i < len - cr_count; ++i)
	{
		if (string[i + cr_count] == CR)
		{
			++cr_count;
		}

		string[i] = string[i + cr_count];
	}
	string.erase(i, cr_count);
}

//static
template<class T>
void LLStringUtilBase<T>::replaceChar(std::basic_string<T>& string, T target,
									  T replacement)
{
	size_type found_pos = 0;
	while ((found_pos = string.find(target, found_pos)) != std::basic_string<T>::npos)
	{
		string[found_pos] = replacement;
		++found_pos; // avoid infinite defeat if target == replacement
	}
}

//static
template<class T>
void LLStringUtilBase<T>::replaceString(std::basic_string<T>& string,
										std::basic_string<T> target,
										std::basic_string<T> replacement)
{
	size_type found_pos = 0;
	while ((found_pos = string.find(target, found_pos)) != std::basic_string<T>::npos)
	{
		string.replace(found_pos, target.length(), replacement);
		// Avoid infinite defeat if replacement contains target
		found_pos += replacement.length();
	}
}

//static
template<class T>
void LLStringUtilBase<T>::replaceNonstandardASCII(std::basic_string<T>& string,
												  T replacement)
{
	constexpr char LF = '\n';
	constexpr S8 MIN = ' ';

	size_type len = string.size();
	for (size_type i = 0; i < len; ++i)
	{
		// No need to test MAX < mText[i] because we treat mText[i] as a
		// signed char which has a max value of 127.
		if (S8(string[i]) < MIN && string[i] != LF)
		{
			string[i] = replacement;
		}
	}
}

//static
template<class T>
void LLStringUtilBase<T>::replaceTabsWithSpaces(std::basic_string<T>& str,
												size_type spaces_per_tab)
{
	const T TAB = '\t';
	const T SPACE = ' ';

	std::basic_string<T> out_str;
	// Replace tabs with spaces
	for (size_type i = 0; i < str.length(); ++i)
	{
		if (str[i] == TAB)
		{
			for (size_type j = 0; j < spaces_per_tab; ++j)
			{
				out_str += SPACE;
			}
		}
		else
		{
			out_str += str[i];
		}
	}
	str = out_str;
}

//static
template<class T>
bool LLStringUtilBase<T>::containsNonprintable(const std::basic_string<T>& string)
{
	const char MIN = 32;
	bool rv = false;
	for (size_type i = 0, count = string.size(); i < count; ++i)
	{
		if (string[i] < MIN)
		{
			rv = true;
			break;
		}
	}
	return rv;
}

//static
template<class T>
void LLStringUtilBase<T>::stripNonprintable(std::basic_string<T>& string)
{
	const char MIN = 32;
	size_type j = 0;
	if (string.empty())
	{
		return;
	}
	size_t src_size = string.size();
	char* c_string = new char[src_size + 1];
	if (c_string == NULL)
	{
		return;
	}
	copy(c_string, string.c_str(), src_size + 1);
	char* write_head = &c_string[0];
	for (size_type i = 0; i < src_size; ++i)
	{
		char* read_head = &string[i];
		write_head = &c_string[j];
		if (!(*read_head < MIN))
		{
			*write_head = *read_head;
			++j;
		}
	}
	c_string[j]= '\0';
	string = c_string;
	delete []c_string;
}

template<class T>
void LLStringUtilBase<T>::_makeASCII(std::basic_string<T>& string)
{
	// Replace non-ASCII chars with LL_UNKNOWN_CHAR
	for (size_type i = 0, count = string.length(); i < count; ++i)
	{
		if (string[i] > 0x7f)
		{
			string[i] = LL_UNKNOWN_CHAR;
		}
	}
}

//static
template<class T>
void LLStringUtilBase<T>::copy(T* dst, const T* src, size_type dst_size)
{
	if (dst_size > 0)
	{
		size_type min_len = 0;
		if (src)
		{
			min_len = llmin(dst_size - 1, strlen(src));
			memcpy(dst, src, min_len * sizeof(T));
		}
		dst[min_len] = '\0';
	}
}

//static
template<class T>
void LLStringUtilBase<T>::copyInto(std::basic_string<T>& dst,
								   const std::basic_string<T>& src,
								   size_type offset)
{
	if (offset == dst.length())
	{
		// Special case: append to end of string and avoid expensive (when
		// strings are large) string manipulations
		dst += src;
	}
	else
	{
		std::basic_string<T> tail = dst.substr(offset);

		dst = dst.substr(0, offset);
		dst += src;
		dst += tail;
	};
}

// True if this is the head of s.
//static
template<class T>
bool LLStringUtilBase<T>::isHead(const std::basic_string<T>& string,
								 const T* s)
{
	if (string.empty())
	{
		// Early exit
		return false;
	}
	else
	{
		return strncmp(s, string.c_str(), string.size()) == 0;
	}
}

//static
template<class T>
LL_INLINE bool LLStringUtilBase<T>::startsWith(const std::basic_string<T>& str,
											   const std::basic_string<T>& substr)
{
	size_t str_len = str.length();
	if (!str_len) return false;

	size_t sub_len = substr.length();
	if (!sub_len) return false;

	return str_len >= sub_len && str.compare(0, sub_len, substr) == 0;
}

//static
template<class T>
LL_INLINE bool LLStringUtilBase<T>::endsWith(const std::basic_string<T>& str,
											 const std::basic_string<T>& substr)
{
	size_t str_len = str.length();
	if (!str_len) return false;

	size_t sub_len = substr.length();
	if (!sub_len) return false;

	return str_len >= sub_len &&
		   str.compare(str_len - sub_len, sub_len, substr) == 0;
}

template<class T>
bool LLStringUtilBase<T>::convertToBool(const std::basic_string<T>& string,
										bool& value)
{
	if (string.empty())
	{
		return false;
	}

	std::basic_string<T> temp(string);
	trim(temp);
	if (temp == "1" || temp == "T" || temp == "t" || temp == "TRUE" ||
		temp == "true" || temp == "True")
	{
		value = true;
		return true;
	}
	else if (temp == "0" || temp == "F" || temp == "f" || temp == "FALSE" ||
			 temp == "false" || temp == "False")
	{
		value = false;
		return true;
	}

	return false;
}

template<class T>
bool LLStringUtilBase<T>::convertToU8(const std::basic_string<T>& string,
									  U8& value)
{
	S32 value32 = 0;
	bool success = convertToS32(string, value32);
	if (success && U8_MIN <= value32 && value32 <= U8_MAX)
	{
		value = (U8)value32;
		return true;
	}
	return false;
}

template<class T>
bool LLStringUtilBase<T>::convertToS8(const std::basic_string<T>& string,
									  S8& value)
{
	S32 value32 = 0;
	bool success = convertToS32(string, value32);
	if (success && S8_MIN <= value32 && value32 <= S8_MAX)
	{
		value = (S8)value32;
		return true;
	}
	return false;
}

template<class T>
bool LLStringUtilBase<T>::convertToS16(const std::basic_string<T>& string,
									   S16& value)
{
	S32 value32 = 0;
	bool success = convertToS32(string, value32);
	if (success && S16_MIN <= value32 && value32 <= S16_MAX)
	{
		value = (S16)value32;
		return true;
	}
	return false;
}

template<class T>
bool LLStringUtilBase<T>::convertToU16(const std::basic_string<T>& string,
									   U16& value)
{
	S32 value32 = 0;
	bool success = convertToS32(string, value32);
	if (success && U16_MIN <= value32 && value32 <= U16_MAX)
	{
		value = (U16)value32;
		return true;
	}
	return false;
}

template<class T>
bool LLStringUtilBase<T>::convertToU32(const std::basic_string<T>& string,
									   U32& value)
{
	if (string.empty())
	{
		return false;
	}

	std::basic_string<T> temp(string);
	trim(temp);
	U32 v;
	std::basic_istringstream<T> i_stream((std::basic_string<T>)temp);
	if (i_stream >> v)
	{
		value = v;
		return true;
	}
	return false;
}

template<class T>
bool LLStringUtilBase<T>::convertToS32(const std::basic_string<T>& string,
									   S32& value)
{
	if (string.empty())
	{
		return false;
	}

	std::basic_string<T> temp(string);
	trim(temp);
	S32 v;
	std::basic_istringstream<T> i_stream((std::basic_string<T>)temp);
	if (i_stream >> v)
	{
#if 0	// *TODO: figure out overflow and underflow reporting here
		if (LONG_MAX == v || LONG_MIN == v)
		{
			// Underflow or overflow
			return false;
		}
#endif
		value = v;
		return true;
	}
	return false;
}

template<class T>
bool LLStringUtilBase<T>::convertToF32(const std::basic_string<T>& string,
									   F32& value)
{
	F64 value64 = 0.0;
	bool success = convertToF64(string, value64);
	if (success && -F32_MAX <= value64 && value64 <= F32_MAX)
	{
		value = (F32)value64;
		return true;
	}
	return false;
}

template<class T>
bool LLStringUtilBase<T>::convertToF64(const std::basic_string<T>& string,
									   F64& value)
{
	if (string.empty())
	{
		return false;
	}

	std::basic_string<T> temp(string);
	trim(temp);
	F64 v;
	std::basic_istringstream<T> i_stream((std::basic_string<T>)temp);
	if (i_stream >> v)
	{
#if 0	// *TODO: figure out overflow and underflow reporting here
		if (-HUGE_VAL == v || HUGE_VAL == v)
		{
			// Underflow or overflow
			return false;
		}
#endif
		value = v;
		return true;
	}
	return false;
}

template<class T>
void LLStringUtilBase<T>::truncate(std::basic_string<T>& string,
								   size_type count)
{
	size_type cur_size = string.size();
	string.resize(count < cur_size ? count : cur_size);
}

// Overload for use with boost::unordered_map and boost::unordered_set.
// Note: the hash does not need to be unique (it is only used to determine in
// which bucket the actual key will be stored), thus why we only care for a
// few characters and the length of the string: this is faster than boost's
// hash (which uses hash_combine() on each character of the string), but on the
// other hand, there will be more hash collisions if the strings are very
// similar (which is not the case for the maps this hash is used for). HB
LL_INLINE size_t hash_value(const std::string& str) noexcept
{
	const char* ptr = str.data();
	size_t len = str.length();
	U32 hash = len + 1;

	if (LL_LIKELY(len > 3))
	{
		// We use the four last characters of the string, which are more likely
		// to differ from one string to the other in our code and data...
		U32* ptr32 = (U32*)(ptr + len - 4);
		// Note: ptr[2] = first letter after "LL" in "LLStuff", which is
		// important, for example, with singletons names.
		return (size_t)(*ptr32 * hash + ptr[2]);
	}

	// This path is very unlikely to be taken, given our usage of strings as
	// keys in the viewer... Still faster than a loop, especially if the
	// compiler optimizes properly with a jump table.
	switch (len)
	{
		case 3:
			hash <<= 8;
			hash += ptr[2];
		case 2:
			hash <<= 8;
			hash += ptr[1];
		case 1:
			hash <<= 8;
			hash += *ptr;
		default:
			return (size_t)hash;
	}
}

// This used to be in separate llformat.h header file. Moved here for
// coherency. HB
//
// Use as follows:
// std::string result = llformat("Test:%d (%.2f %.2f)", idx, x, y);
//
// Note: uses an internal buffer limited to 1024, (but vsnprintf prevents any
// overrun).
std::string llformat(const char* fmt, ...);

#endif  // LL_STRING_H
