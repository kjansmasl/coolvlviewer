/**
 * @file llsdserialize.h
 * @author Phoenix
 * @date 2006-02-26
 * @brief Declaration of parsers and formatters for LLSD
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

#ifndef LL_LLSDSERIALIZE_H
#define LL_LLSDSERIALIZE_H

#include <iosfwd>
#include <type_traits>

#include "llpointer.h"
#include "llrefcount.h"
#include "llsd.h"

// Set to 1 to enable the new deserialize code: it is currently plain BOGUS,
// so keep it disabled for now... HB
#define LL_USE_NEW_DESERIALIZE 0

// To express an index that might go negative (ssize_t being provided by SOME
// compilers, do not collide names).
typedef typename std::make_signed<size_t>::type llssize;

// Abstract base class for LLSD parsers.
class LLSDParser : public LLRefCount
{
protected:
	~LLSDParser() override = default;

public:
	// Anonymous enum to indicate parsing failure.
	enum
	{
		PARSE_FAILURE = -1
	};

	LLSDParser();

	// Call this method to parse a stream forr a structured data. It assumes
	// that the istream is a complete llsd object, for example an opened and
	// closed map with an arbitrary nesting of elements. Returns after reading
	// one data object, allowing continued reading from the stream by the
	// caller.
	// 'istr' is the input stream.
	// 'data' is the output for the parsed structured data.
	// 'max_bytes' is the maximum number of bytes that will be in the stream.
	// Pass in LLSDSerialize::SIZE_UNLIMITED (-1) to set no byte limit.
	// 'max_depth' is the maximum depth parser will check before exiting with
	// parse error, -1 = unlimited.
	// Returns the number of LLSD objects parsed into data or PARSE_FAILURE on
	// parse failure.
	S32 parse(std::istream& istr, LLSD& data, llssize max_bytes,
			  S32 max_depth = -1);

	// Like parse(), but uses a different call (istream.getline()) to read by
	// lines.  This API is better suited for XML, where the parse cannot tell
	// where the document actually ends.
	S32 parseLines(std::istream& istr, LLSD& data);

	// Resets the parser so parse() or parseLines() can be called again for
	// another <llsd> chunk.
	void reset()				{ doReset(); }


protected:
	// Pure virtual base for doing the parse.
	// This method parses the istream for a structured data. It assumes that
	// the istream is a complete llsd object, for example an opened and closed
	// map with an arbitrary nesting of elements. It will return after reading
	// one data object, allowing continued reading from the stream by the
	// caller. 'istr' is input stream, 'data[out]' is the newly parse
	// structured data, 'max_depth' is the max depth the parser will check
	// before exiting with parse error (-1 = unlimited). The method returns the
	// number of LLSD objects parsed into data or PARSE_FAILURE (-1) on parse
	// failure.
	virtual S32 doParse(std::istream& istr, LLSD& data,
						S32 max_depth = -1) const = 0;

	// Virtual default method for resetting the parser
	virtual void doReset() 		{}

	// The following istream helper methods exist to help correctly use the
	// mMaxBytesLeft without really thinking about it for most simple
	// operations. Use of the streamtools in llstreamtools.h will require
	// custom wrapping.

	// Gets a byte off the stream
	int get(std::istream& istr) const;

	// Gets several bytes off the stream into a buffer.
	// 'istr' is the istream to work with, 's' is the buffer to get into,
	// 'n' is the number of bytes in the buffer (nul terminator included); a
	// maximum of n - 1 bytes will be extracted. 'delim' is the delimiter to
	// get until found. Returns istr.
	std::istream& get(std::istream& istr, char* s, std::streamsize n,
					  char delim) const;

	// Gets several bytes off the stream into a streambuf
	std::istream& get(std::istream& istr, std::streambuf& sb, char dlim) const;

	// Ignores the next byte on the istream
	std::istream& ignore(std::istream& istr) const;

	// Puts the last character retrieved back on the stream
	std::istream& putback(std::istream& istr, char c) const;

	// Reads a block of 'n' characters into a buffer ('s')
	std::istream& read(std::istream& istr, char* s, std::streamsize n) const;

protected:
	// Accounte for 'bytes' read outside of the istream helpers. Conceptually
	// const since it only modifies mutable members.
	void account(llssize bytes) const;

protected:
	// The maximum number of bytes left to be parsed.
	mutable llssize	mMaxBytesLeft;

	// To set if byte counts should be checked during parsing.
	bool			mCheckLimits;

	// Set to use line-based reading to get text
	bool			mParseLines;
};

// Parser class which handles the original notation format for LLSD.
class LLSDNotationParser final : public LLSDParser
{
protected:
	LOG_CLASS(LLSDNotationParser);

public:
	LLSDNotationParser();

protected:
	~LLSDNotationParser() override = default;

	/**
	 * @brief Call this method to parse a stream for LLSD.
	 *
	 * This method parses the istream for a structured data. This
	 * method assumes that the istream is a complete llsd object --
	 * for example an opened and closed map with an arbitrary nesting
	 * of elements. This method will return after reading one data
	 * object, allowing continued reading from the stream by the
	 * caller.
	 * @param istr The input stream.
	 * @param data[out] The newly parse structured data. Undefined on failure.
	 * @param depth Max depth parser will check before exiting with
	 * parse error, -1 = unlimited.
	 * @return Returns the number of LLSD objects parsed into
	 * data. Returns PARSE_FAILURE (-1) on parse failure.
	 */
	S32 doParse(std::istream& istr, LLSD& data, S32 depth = -1) const override;

private:
	/**
	 * @brief Parse a map from the istream
	 *
	 * @param istr The input stream.
	 * @param map The map to add the parsed data.
	 * @param max_depth Allowed parsing depth.
	 * @return Returns The number of LLSD objects parsed into data.
	 */
	S32 parseMap(std::istream& istr, LLSD& map, S32 max_depth) const;

	/**
	 * @brief Parse an array from the istream.
	 *
	 * @param istr The input stream.
	 * @param array The array to append the parsed data.
	 * @param max_depth Allowed parsing depth.
	 * @return Returns The number of LLSD objects parsed into data.
	 */
	S32 parseArray(std::istream& istr, LLSD& array, S32 max_depth) const;

	/**
	 * @brief Parse a string from the istream and assign it to data.
	 *
	 * @param istr The input stream.
	 * @param data[out] The data to assign.
	 * @param max_depth Allowed parsing depth.
	 * @return Retuns true if a complete string was parsed.
	 */
	bool parseString(std::istream& istr, LLSD& data) const;

	/**
	 * @brief Parse binary data from the stream.
	 *
	 * @param istr The input stream.
	 * @param data[out] The data to assign.
	 * @param max_depth Allowed parsing depth.
	 * @return Retuns true if a complete blob was parsed.
	 */
	bool parseBinary(std::istream& istr, LLSD& data) const;
};

// Parser class which handles XML format LLSD.
class LLSDXMLParser final : public LLSDParser
{
protected:
	LOG_CLASS(LLSDXMLParser);

public:
	LLSDXMLParser(bool emit_errors = true);

protected:
	~LLSDXMLParser() override;

	/**
	 * @brief Call this method to parse a stream for LLSD.
	 *
	 * This method parses the istream for a structured data. This
	 * method assumes that the istream is a complete llsd object --
	 * for example an opened and closed map with an arbitrary nesting
	 * of elements. This method will return after reading one data
	 * object, allowing continued reading from the stream by the
	 * caller.
	 * @param istr The input stream.
	 * @param data[out] The newly parse structured data.
	 * @param depth Max depth parser will check before exiting with
	 * parse error, -1 = unlimited.
	 * @return Returns the number of LLSD objects parsed into
	 * data. Returns PARSE_FAILURE (-1) on parse failure.
	 */
	S32 doParse(std::istream& istr, LLSD& data, S32 depth = -1) const override;

	// Virtual default function for resetting the parser
	void doReset() override;

private:
	class Impl;
	Impl& impl;

	void parsePart(const char* buf, llssize len);
	friend class LLSDSerialize;
};

// Parser class which handles binary formatted LLSD.
class LLSDBinaryParser final : public LLSDParser
{
protected:
	LOG_CLASS(LLSDNotationParser);

public:
	LLSDBinaryParser();

protected:
	~LLSDBinaryParser() override = default;

	/**
	 * @brief Call this method to parse a stream for LLSD.
	 *
	 * This method parses the istream for a structured data. This
	 * method assumes that the istream is a complete llsd object --
	 * for example an opened and closed map with an arbitrary nesting
	 * of elements. This method will return after reading one data
	 * object, allowing continued reading from the stream by the
	 * caller.
	 * @param istr The input stream.
	 * @param data[out] The newly parse structured data.
	 * @param depth Max depth parser will check before exiting with
	 * parse error, -1 = unlimited.
	 * @return Returns the number of LLSD objects parsed into
	 * data. Returns -1 on parse failure.
	 */
	S32 doParse(std::istream& istr, LLSD& data, S32 depth = -1) const override;

private:
	/**
	 * @brief Parse a map from the istream
	 *
	 * @param istr The input stream.
	 * @param map The map to add the parsed data.
	 * @param max_depth Allowed parsing depth.
	 * @return Returns The number of LLSD objects parsed into data.
	 */
	S32 parseMap(std::istream& istr, LLSD& map, S32 max_depth) const;

	/**
	 * @brief Parse an array from the istream.
	 *
	 * @param istr The input stream.
	 * @param array The array to append the parsed data.
	 * @param max_depth Allowed parsing depth.
	 * @return Returns The number of LLSD objects parsed into data.
	 */
	S32 parseArray(std::istream& istr, LLSD& array, S32 max_depth) const;

	/**
	 * @brief Parse a string from the istream and assign it to data.
	 *
	 * @param istr The input stream.
	 * @param value[out] The string to assign.
	 * @return Retuns true if a complete string was parsed.
	 */
	bool parseString(std::istream& istr, std::string& value) const;
};

// Abstract base class for formatting LLSD.
class LLSDFormatter : public LLRefCount
{
protected:
	~LLSDFormatter() override = default;

public:
	/**
	 * Options for output
	 */
	typedef enum e_formatter_options_type
	{
		OPTIONS_NONE = 0,
		OPTIONS_PRETTY = 1,
		OPTIONS_PRETTY_BINARY = 2
	} EFormatterOptions;

	LLSDFormatter(bool bool_apha = false, const std::string& real_format = "",
				  EFormatterOptions options = OPTIONS_PRETTY_BINARY);

	/**
	 * @brief Set the boolean serialization format.
	 *
	 * @param alpha Serializes boolean as alpha if true.
	 */
	LL_INLINE void boolalpha(bool alpha)				{ mBoolAlpha = alpha; }

	/**
	 * @brief Set the real format
	 *
	 * By default, the formatter will use default double serialization
	 * which is frequently frustrating for many applications. You can
	 * set the precision on the stream independently, but that still
	 * might not work depending on the value.
	 * EXAMPLES:<br>
	 * %.2f<br>
	 * @param format A format string which follows the printf format
	 * rules. Specify an empty string to return to default formatting.
	 */
	LL_INLINE void realFormat(const std::string& fmt)	{ mRealFormat = fmt;}

	// Call this method to format an LLSD to a stream with options as set by
	// the constructor.
	S32 format(const LLSD& data, std::ostream& ostr) const;

	/**
	 * @brief Call this method to format an LLSD to a stream.
	 *
	 * @param data The data to write.
	 * @param ostr The destination stream for the data.
	 * @return Returns The number of LLSD objects fomatted out
	 */
	virtual S32 format(const LLSD& data, std::ostream& ostr,
					   EFormatterOptions options) const;

protected:
	// Implementation to format the data. This is called recursively.
	// 'data' is the data to write and 'ostr' the destination stream for the
	// data. Returns the number of LLSD objects formatted out.
	virtual S32 format_impl(const LLSD& data, std::ostream& ostr,
							EFormatterOptions options, U32 level) const = 0;

	/**
	 * @brief Helper method which appropriately obeys the real format.
	 *
	 * @param real The real value to format.
	 * @param ostr The destination stream for the data.
	 */
	void formatReal(LLSD::Real real, std::ostream& ostr) const;

protected:
	EFormatterOptions	mOptions;
	std::string			mRealFormat;
	bool				mBoolAlpha;
};

// Formatter class which outputs the original notation format for LLSD.
class LLSDNotationFormatter final : public LLSDFormatter
{
protected:
	~LLSDNotationFormatter() override = default;

public:
	LLSDNotationFormatter(bool bool_apha = false,
						  const std::string& real_format = "",
						  EFormatterOptions options = OPTIONS_PRETTY_BINARY);

	/**
	 * @brief Helper static method to return a notation escaped string
	 *
	 * This method will return the notation escaped string, but not
	 * the surrounding serialization identifiers such as a double or
	 * single quote. It will be up to the caller to embed those as
	 * appropriate.
	 * @param in The raw, unescaped string.
	 * @return Returns an escaped string appropriate for serialization.
	 */
	static std::string escapeString(const std::string& in);

protected:
	/**
	 * @brief Implementation to format the data. This is called recursively.
	 *
	 * @param data The data to write.
	 * @param ostr The destination stream for the data.
	 * @return Returns The number of LLSD objects fomatted out
	 */
	S32 format_impl(const LLSD& data, std::ostream& ostr,
					EFormatterOptions options, U32 level) const override;
};

// Formatter class which outputs the LLSD as XML.
class LLSDXMLFormatter final : public LLSDFormatter
{
protected:
	~LLSDXMLFormatter() override = default;

public:
	LLSDXMLFormatter(bool bool_apha = false,
					 const std::string& real_format = "",
					 EFormatterOptions options = OPTIONS_PRETTY_BINARY);

	/**
	 * @brief Helper static method to return an xml escaped string
	 *
	 * @param in A valid UTF-8 string.
	 * @return Returns an escaped string appropriate for serialization.
	 */
	static std::string escapeString(const std::string& in);

	/**
	 * @brief Call this method to format an LLSD to a stream.
	 *
	 * @param data The data to write.
	 * @param ostr The destination stream for the data.
	 * @return Returns The number of LLSD objects fomatted out
	 */
	S32 format(const LLSD& data, std::ostream& ostr,
			   EFormatterOptions options) const override;

	// Also pull down base-class format() method that isn't overridden
	using LLSDFormatter::format;

protected:
	/**
	 * @brief Implementation to format the data. This is called recursively.
	 *
	 * @param data The data to write.
	 * @param ostr The destination stream for the data.
	 * @return Returns The number of LLSD objects fomatted out
	 */
	S32 format_impl(const LLSD& data, std::ostream& ostr,
					EFormatterOptions options,
					U32 level) const override;
};

/**
 * @class LLSDBinaryFormatter
 * @brief Formatter which outputs the LLSD as a binary notation format.
 *
 * The binary format is a compact and efficient representation of
 * structured data useful for when transmitting over a small data pipe
 * or when transmission frequency is very high.<br>
 *
 * The normal boolalpha and real format commands are ignored.<br>
 *
 * All integers are transmitted in network byte order. The format is:<br>
 * Undefined: '!'<br>
 * Boolean: character '1' for true character '0' for false<br>
 * Integer: 'i' + 4 bytes network byte order<br>
 * Real: 'r' + 8 bytes IEEE double<br>
 * UUID: 'u' + 16 byte unsigned integer<br>
 * String: 's' + 4 byte integer size + string<br>
 * Date: 'd' + 8 byte IEEE double for seconds since epoch<br>
 * URI: 'l' + 4 byte integer size + string uri<br>
 * Binary: 'b' + 4 byte integer size + binary data<br>
 * Array: '[' + 4 byte integer size  + all values + ']'<br>
 * Map: '{' + 4 byte integer size  every(key + value) + '}'<br>
 *  map keys are serialized as 'k' + 4 byte integer size + string
 */
class LLSDBinaryFormatter final : public LLSDFormatter
{
protected:
	~LLSDBinaryFormatter() override = default;

public:
	LLSDBinaryFormatter(bool bool_apha = false,
						const std::string& real_format = "",
						EFormatterOptions options = OPTIONS_PRETTY_BINARY);

protected:
	/**
	 * @brief Implementation to format the data. This is called recursively.
	 *
	 * @param data The data to write.
	 * @param ostr The destination stream for the data.
	 * @return Returns The number of LLSD objects fomatted out
	 */
	S32 format_impl(const LLSD& data, std::ostream& ostr,
					EFormatterOptions options,
					U32 level) const override;
	/**
	 * @brief Helper method to serialize strings
	 *
	 * This method serializes a network byte order size and the raw
	 * string contents.
	 * @param string The string to write.
	 * @param ostr The destination stream for the data.
	 */
	void formatString(const std::string& string, std::ostream& ostr) const;
};

/**
 * @class LLSDNotationStreamFormatter
 * @brief Formatter which is specialized for use on streams which
 * outputs the original notation format for LLSD.
 *
 * This class is useful for doing inline stream operations. For example:
 *
 * <code>
 *  LLSD sd;<br>
 *  sd["foo"] = "bar";<br>
 *  std::stringstream params;<br>
 *	params << "[{'version':i1}," << LLSDOStreamer<LLSDNotationFormatter>(sd)
 *    << "]";
 *  </code>
 *
 * *NOTE - formerly this class inherited from its template parameter Formatter,
 * but all instantiations passed in LLRefCount subclasses.  This conflicted with
 * the auto allocation intended for this class template (demonstrated in the
 * example above).  -brad
 */
template <class Formatter>
class LLSDOStreamer
{
public:
	LLSDOStreamer(const LLSD& data,
				  LLSDFormatter::EFormatterOptions options =
					LLSDFormatter::OPTIONS_PRETTY_BINARY)
	:	mSD(data),
		mOptions(options)
	{
	}

	// Use this inline during construction during a stream operation.
	// 'str' is the destination stream for serialized output.
	// 'formatter' is the formatter which will output its LLSD.
	// Returns the stream passed in after streaming mSD.
	friend std::ostream& operator<<(std::ostream& str,
									const LLSDOStreamer<Formatter>& formatter)
	{
		LLPointer<Formatter> f = new Formatter;
		f->format(formatter.mSD, str, formatter.mOptions);
		return str;
	}

protected:
	LLSD								mSD;
	LLSDFormatter::EFormatterOptions	mOptions;
};

typedef LLSDOStreamer<LLSDNotationFormatter>	LLSDNotationStreamer;
typedef LLSDOStreamer<LLSDXMLFormatter>			LLSDXMLStreamer;

// Serializer / deserializer class for the various LLSD formats
class LLSDSerialize
{
protected:
	LOG_CLASS(LLSDSerialize);

public:
	enum ELLSD_Serialize
	{
		LLSD_BINARY, LLSD_XML, LLSD_NOTATION
	};

	// Anonymous enumeration for useful max_bytes constants.
	enum
	{
		// Setting an unlimited size is discouraged and should only be
		// used when reading cin or another stream source which does
		// not provide access to size.
		SIZE_UNLIMITED = -1,
	};

	// Generic input/output methods

	static void serialize(const LLSD& sd, std::ostream& str, ELLSD_Serialize,
						  LLSDFormatter::EFormatterOptions options =
							LLSDFormatter::OPTIONS_PRETTY_BINARY);

	// Examines a stream, and parse 1 sd object out based on contents.
	// Returns true if the stream appears to contain valid data
	// 'sd' is the output for the data found on the stream
	// 'str' is the incoming stream
	// 'max_bytes' is the maximum number of bytes to parse
	static bool deserialize(LLSD& sd, std::istream& str, llssize max_bytes);

	// Notation methods

	static S32 toNotation(const LLSD& sd, std::ostream& str)
	{
		LLPointer<LLSDNotationFormatter> f = new LLSDNotationFormatter;
		return f->format(sd, str, LLSDFormatter::OPTIONS_NONE);
	}

	static S32 toPrettyNotation(const LLSD& sd, std::ostream& str)
	{
		LLPointer<LLSDNotationFormatter> f = new LLSDNotationFormatter;
		return f->format(sd, str, LLSDFormatter::OPTIONS_PRETTY);
	}

	static S32 toPrettyBinaryNotation(const LLSD& sd, std::ostream& str)
	{
		LLPointer<LLSDNotationFormatter> f = new LLSDNotationFormatter;
		const LLSDFormatter::EFormatterOptions options =
			LLSDFormatter::EFormatterOptions(LLSDFormatter::OPTIONS_PRETTY |
											 LLSDFormatter::OPTIONS_PRETTY_BINARY);
		return f->format(sd, str, options);
	}

	static S32 fromNotation(LLSD& sd, std::istream& str, llssize max_bytes)
	{
		LLPointer<LLSDNotationParser> p = new LLSDNotationParser;
		return p->parse(str, sd, max_bytes);
	}

	static LLSD fromNotation(std::istream& str, llssize max_bytes)
	{
		LLPointer<LLSDNotationParser> p = new LLSDNotationParser;
		LLSD sd;
		(void)p->parse(str, sd, max_bytes);
		return sd;
	}

	// XML methods

	static S32 toXML(const LLSD& sd, std::ostream& str)
	{
		LLPointer<LLSDXMLFormatter> f = new LLSDXMLFormatter;
		return f->format(sd, str, LLSDFormatter::OPTIONS_NONE);
	}

	static S32 toPrettyXML(const LLSD& sd, std::ostream& str)
	{
		LLPointer<LLSDXMLFormatter> f = new LLSDXMLFormatter;
		return f->format(sd, str, LLSDFormatter::OPTIONS_PRETTY);
	}

	static S32 fromXMLEmbedded(LLSD& sd, std::istream& str,
							   bool emit_errors = true)
	{
		// No need for max_bytes since xml formatting is not subvertable by
		// bad sizes.
		LLPointer<LLSDXMLParser> p = new LLSDXMLParser(emit_errors);
		return p->parse(str, sd, LLSDSerialize::SIZE_UNLIMITED);
	}

	// Line oriented parser, 30% faster than fromXML(), but can
	// only be used when you know you have the complete XML
	// document available in the stream.
	static S32 fromXMLDocument(LLSD& sd, std::istream& str,
							   bool emit_errors = true)
	{
		LLPointer<LLSDXMLParser> p = new LLSDXMLParser(emit_errors);
		return p->parseLines(str, sd);
	}

	static S32 fromXML(LLSD& sd, std::istream& str, bool emit_errors = true)
	{
#if 1
		return fromXMLEmbedded(sd, str, emit_errors);
#else
		return fromXMLDocument(sd, str, emit_errors);
#endif
	}

	// Binary methods

	static S32 toBinary(const LLSD& sd, std::ostream& str)
	{
		LLPointer<LLSDBinaryFormatter> f = new LLSDBinaryFormatter;
		return f->format(sd, str, LLSDFormatter::OPTIONS_NONE);
	}

	static S32 fromBinary(LLSD& sd, std::istream& str, llssize max_bytes,
						  S32 max_depth = -1)
	{
		LLPointer<LLSDBinaryParser> p = new LLSDBinaryParser;
		return p->parse(str, sd, max_bytes, max_depth);
	}

	static LLSD fromBinary(std::istream& str, llssize max_bytes,
						   S32 max_depth = -1)
	{
		LLPointer<LLSDBinaryParser> p = new LLSDBinaryParser;
		LLSD sd;
		(void)p->parse(str, sd, max_bytes, max_depth);
		return sd;
	}
};

// Dirty little zip functions
std::string zip_llsd(LLSD& data);
bool unzip_llsd(LLSD& data, const U8* in, S32 size);
bool unzip_llsd(LLSD& data, std::istream& is, S32 size);
U8* unzip_llsdNavMesh(bool& valid, size_t& outsize, const U8* in, S32 size);

#endif // LL_LLSDSERIALIZE_H
