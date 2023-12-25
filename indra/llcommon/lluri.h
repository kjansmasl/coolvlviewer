/**
 * @file lluri.h
 * @author Phoenix
 * @date 2006-02-05
 * @brief Declaration of the URI class.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab. Terms of
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

#ifndef LL_LLURI_H
#define LL_LLURI_H

#include <string>

#include "llpreprocessor.h"

class LLSD;

// Note: LLURI instances are immutable.
// See: http://www.ietf.org/rfc/rfc3986.txt

class LLURI
{
protected:
	LOG_CLASS(LLURI);

public:
	LLURI() = default;
	LLURI(const std::string& escaped_str);
	LLURI(const std::string& scheme, const std::string& user_name,
		  const std::string& password, const std::string& host_name, U16 port,
		  const std::string& escaped_path, const std::string& escaped_query);

	static LLURI buildHTTP(const std::string& prefix, const LLSD& path);

	// Prefix is either a full URL prefix of the form
	// "http://example.com:8080", or it can be simply a host and optional port
	// like "example.com" or "example.com:8080", in these cases, the "http://"
	// will be added
	static LLURI buildHTTP(const std::string& prefix, const LLSD& path,
						   const LLSD& query);

	static LLURI buildHTTP(const std::string& host,	const U32& port,
						   const LLSD& path);
	static LLURI buildHTTP(const std::string& host, const U32& port,
						   const LLSD& path, const LLSD& query);

	// Returns the whole URI, escaped as needed
	std::string asString() const;

	// These functions return parts of the decoded URI. The returned strings
	// are un-escaped as needed.

	// For all schemes. E.g. "http"; note the lack of a colon
	LL_INLINE const std::string& scheme() const
	{
		return mScheme;
	}

	std::string opaque() const;		// Everything after the colon

	// For schemes that follow path like syntax (http, https, ftp)
	std::string authority() const;	// Ex.: "host.com:80"
	std::string hostName() const;	// Ex.: "host.com"
	std::string userName() const;
	std::string password() const;
	U16 hostPort() const;			// Ex.: 80, will include implicit port
	bool defaultPort() const;		// true if port is default for scheme
	LL_INLINE const std::string& escapedPath() const	{ return mEscapedPath; }
	std::string path() const;		// Ex.: "/abc/def", includes leading slash
	LLSD pathArray() const;			// above decoded into an array of strings
	std::string query() const;		// Ex.: "x=34", section after "?"
	LL_INLINE const std::string& escapedQuery() const	{ return mEscapedQuery; }
	LLSD queryMap() const;			// above decoded into a map
	static LLSD queryMap(std::string escaped_query_string);

	// Given a name value map, returns a serialized query string (such as
	// '?n1=v1&n2=v2&...'). 'query_map' is  a map of name value. Every value
	// must be representable as a string.
	static std::string mapToQueryString(const LLSD& query_map);

	// Returns the rfc 1738 escaped URI or an empty string.
	//	ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
	//	0123456789
	//	-._~
	// See http://www.ietf.org/rfc/rfc1738.txt
	static std::string escape(const std::string& str);

	// Escapes symbols into stream
	static void encodeCharacter(std::ostream& ostr,
								std::string::value_type val);

	// Escapes a string with a specified set of allowed characters, URL-
	// encoding all the characters that are not in the allowed string.
	// 'is_allowed_sorted' is an optimization hint if allowed array is sorted.
	static std::string escape(const std::string& str,
							  const std::string& allowed,
							  bool is_allowed_sorted = false);


	// Break string into data part and path or sheme and escape path (if
	// present) and data. The data part is not allowed to have path related
	// symbols.
	static std::string escapePathAndData(const std::string& str);

	// Unescapes an escaped URI string.
	static std::string unescape(const std::string& str);

private:
	 // Only "http", "https", "ftp", and "secondlife" schemes are parsed;
	 // secondlife scheme parses authority as "" and includes it as part of
	 // the path, i.e.:
	// secondlife://app/login has mAuthority = "" and mPath = "/app/login"
	void parseAuthorityAndPathUsingOpaque();

private:
	std::string mScheme;
	std::string mEscapedOpaque;
	std::string mEscapedAuthority;
	std::string mEscapedPath;
	std::string mEscapedQuery;
};

// This operator required for tut
bool operator!=(const LLURI& first, const LLURI& second);

#endif // LL_LLURI_H
