/**
 * @file lluri.cpp
 * @author Phoenix
 * @date 2006-02-08
 * @brief Implementation of the LLURI class.
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

#include <iomanip>

#include "boost/tokenizer.hpp"

#include "lluri.h"

#include "llsd.h"

// Note: "-" removed from UNRESERVED_CHARS since recent CEF versions do not
// like it in path components... HB
#define UNRESERVED_CHARS \
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._~"
#define SUB_DELIMS "!$&'()*+,;="

///////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////

LL_NO_INLINE static std::string escapeHostAndPort(const std::string& s)
{
	static std::string allowed;
	if (allowed.empty())
	{
		allowed = UNRESERVED_CHARS SUB_DELIMS "-:";
		std::sort(allowed.begin(), allowed.end());
	}
	return LLURI::escape(s, allowed, true);
}

LL_NO_INLINE static std::string escapePathComponent(const std::string& s)
{
	static std::string allowed;
	if (allowed.empty())
	{
		// Note: "-" removed from allowed chars since recent CEF versions do
		// not like it... HB
		allowed = UNRESERVED_CHARS SUB_DELIMS ":@";
		std::sort(allowed.begin(), allowed.end());
	}
	return LLURI::escape(s, allowed, true);
}

LL_NO_INLINE static std::string escapeQueryVariable(const std::string& s)
{
	static std::string allowed;
	if (allowed.empty())
	{
		// sub_delims - "&;=" + ":@"
		allowed = UNRESERVED_CHARS "-:@!$'()*+,";
		std::sort(allowed.begin(), allowed.end());
	}
	return LLURI::escape(s, allowed, true);
}

LL_NO_INLINE static std::string escapeQueryValue(const std::string& s)
{
	static std::string allowed;
	if (allowed.empty())
	{
		// sub_delims - "&;" + ":@"
		allowed = UNRESERVED_CHARS "-:@!$'()*+,=";
		std::sort(allowed.begin(), allowed.end());
	}
	return LLURI::escape(s, allowed, true);
}

LL_NO_INLINE static std::string escapeUriQuery(const std::string& s)
{
	static std::string allowed;
	if (allowed.empty())
	{
		allowed = UNRESERVED_CHARS "-:@?&$;*+=%/";
		std::sort(allowed.begin(), allowed.end());
	}
	return LLURI::escape(s, allowed, true);
}

LL_NO_INLINE static std::string escapeUriData(const std::string& s)
{
	static std::string allowed;
	if (allowed.empty())
	{
		allowed = UNRESERVED_CHARS "-";
		std::sort(allowed.begin(), allowed.end());
	}
	return LLURI::escape(s, allowed, true);
}

LL_NO_INLINE static std::string escapeUriPath(const std::string& s)
{
	static std::string allowed;
	if (allowed.empty())
	{
		// Note: "-" removed from allowed chars since recent CEF versions do
		// not like it... HB
		allowed =
	 		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
			"$_.+!*'(),{}|\\^~[]`<>#%;/?:@&=";

		std::sort(allowed.begin(), allowed.end());
	}
	return LLURI::escape(s, allowed, true);
}

LL_NO_INLINE static bool is_default(const std::string& scheme, U16 port)
{
	if (scheme == "http")
	{
		return port == 80;
	}
	else if (scheme == "https")
	{
		return port == 443;
	}
	else if (scheme == "ftp")
	{
		return port == 21;
	}
	return false;
}

LL_NO_INLINE static void find_authority_parts(const std::string& authority,
											  std::string& user,
											  std::string& host,
											  std::string& port)
{
	size_t start_pos = authority.find('@');
	if (start_pos == std::string::npos)
	{
		user.clear();
		start_pos = 0;
	}
	else
	{
		user = authority.substr(0, start_pos++);
	}

	size_t end_pos = authority.find(':', start_pos);
	if (end_pos == std::string::npos)
	{
		host = authority.substr(start_pos);
		port.clear();
	}
	else
	{
		host = authority.substr(start_pos, end_pos - start_pos);
		port = authority.substr(end_pos + 1);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLURI class
///////////////////////////////////////////////////////////////////////////////

LLURI::LLURI(const std::string& escaped_str)
{
	size_t delim_pos;
	delim_pos = escaped_str.find(':');
	std::string temp;
	if (delim_pos == std::string::npos)
	{
		mScheme.clear();
		mEscapedOpaque = escaped_str;
	}
	else
	{
		mScheme = escaped_str.substr(0, delim_pos);
		mEscapedOpaque = escaped_str.substr(delim_pos + 1);
	}

	parseAuthorityAndPathUsingOpaque();

	delim_pos = mEscapedPath.find('?');
	if (delim_pos != std::string::npos)
	{
		mEscapedQuery = mEscapedPath.substr(delim_pos + 1);
		mEscapedPath = mEscapedPath.substr(0,delim_pos);
	}
}

LLURI::LLURI(const std::string& scheme, const std::string& user_name,
			 const std::string& password, const std::string& host_name,
			 U16 port, const std::string& escaped_path,
			 const std::string& escaped_query)
:	mScheme(scheme),
	mEscapedPath(escaped_path),
	mEscapedQuery(escaped_query)
{
	std::ostringstream auth;
	std::ostringstream opaque;

	opaque << "//";

	if (!user_name.empty())
	{
		auth << escape(user_name);
		if (!password.empty())
		{
			auth << ':' << escape(password);
		}
		auth << '@';
	}
	auth << host_name;
	if (!is_default(scheme, port))
	{
		auth << ':' << port;
	}
	mEscapedAuthority = auth.str();

	opaque << mEscapedAuthority << escaped_path << escaped_query;

	mEscapedOpaque = opaque.str();
}

//static
void LLURI::encodeCharacter(std::ostream& ostr, std::string::value_type val)
{
	ostr << "%" << std::uppercase << std::hex << std::setw(2)
		 << std::setfill('0')
	     // VWR-4010 Cannot cast to U32 because sign-extension on
	     // chars > 128 will result in FFFFFFC3 instead of F3.
		 << static_cast<S32>(static_cast<U8>(val))
		 // reset stream state
	     << std::nouppercase << std::dec << std::setfill(' ');
}

//static
std::string LLURI::escape(const std::string& str, const std::string& allowed,
						  bool is_allowed_sorted)
{
	// Note: This size determination feels like a good value to me. If someone
	// wants to come up with a more precise heuristic with some data to back up
	// the assertion that 'sort is good' then feel free to change this test a
	// bit.
	if (!is_allowed_sorted && (str.size() > 2 * allowed.size()))
	{
		// If it is not already sorted, or if the url is quite long, we want to
		// optimize this process.
		std::string sorted_allowed(allowed);
		std::sort(sorted_allowed.begin(), sorted_allowed.end());
		return escape(str, sorted_allowed, true);
	}

	std::ostringstream ostr;
	std::string::const_iterator it = str.begin();
	std::string::const_iterator end = str.end();
	std::string::value_type c;
	if (is_allowed_sorted)
	{
		for (std::string::const_iterator allowed_begin = allowed.begin(),
										 allowed_end = allowed.end();
			 it != end; ++it)
		{
			c = *it;
			if (std::binary_search(allowed_begin, allowed_end, c))
			{
				ostr << c;
			}
			else
			{
				encodeCharacter(ostr, c);
			}
		}
	}
	else
	{
		for ( ; it != end; ++it)
		{
			c = *it;
			if (allowed.find(c) == std::string::npos)
			{
				encodeCharacter(ostr, c);
			}
			else
			{
				ostr << c;
			}
		}
	}
	return ostr.str();
}

//static
std::string LLURI::escapePathAndData(const std::string& str)
{
	std::string result;

	if (str.compare(0, 5, "data:") == 0)
	{
		// This is not an URL, but data, data part needs to be properly
		// escaped; data part is separated by ',' from header. Minimal data URI
		// is: "data:,"
		size_t i = str.find(',');
		if (i != std::string::npos)
		{
			std::string header = str.substr(0, ++i);
			if (header.find("base64") != std::string::npos)
			{
				// base64 is URL-safe
				result = str;
			}
			else
			{
				std::string data = str.substr(i, str.length() - i);
				// Note: file can be partially pre-escaped, that is why
				// escaping ignores '%'. It somewhat limits user from
				// displaying strings like "%20" in text but that is how the
				// viewer worked for a while and user can double-encode it.
				// 'header' does not need escaping
				result = header + escapeUriData(data);
			}
		}
	}
	else
	{
		// Try processing it as path with query separator mark("?") character
		// and terminated by a number sign("#")
		size_t i = str.find('?');
		if (i == std::string::npos)
		{
			// Alternate separator
			i = str.find(';');
		}
		if (i != std::string::npos)
		{
			size_t path_size = i + 1;
			std::string query, fragment;
			size_t j = str.find('#');
			if (j != std::string::npos && j > i)
			{
				query = str.substr(path_size, j - path_size);
				fragment = str.substr(j);
			}
			else
			{
				query = str.substr(path_size);
			}
			result = escapeUriPath(str.substr(0, path_size)) +
					 escapeUriQuery(query) + escapeUriPath(fragment);
		}
	}

	if (result.empty())
	{
		// Not a known scheme or no data part, try just escaping as URI path
		result = escapeUriPath(str);
	}

	return result;
}

//static
std::string LLURI::unescape(const std::string& str)
{
	std::ostringstream ostr;
	for (std::string::const_iterator it = str.begin(), end = str.end();
		 it != end; ++it)
	{
		if (*it == '%')
		{
			if (++it == end) break;

			if (is_char_hex(*it))
			{
				U8 c = hex_as_nybble(*it++);
				c = c << 4;

				if (it == end) break;

				if (is_char_hex(*it))
				{
					c |= hex_as_nybble(*it);
					ostr.put((char)c);
				}
				else
				{
					ostr.put((char)c);
					ostr.put(*it);
				}
			}
			else
			{
				ostr.put('%');
				ostr.put(*it);
			}
		}
		else
		{
			ostr.put(*it);
		}
	}
	return ostr.str();
}

//static
std::string LLURI::escape(const std::string& str)
{
	static std::string allowed;
	if (allowed.empty())
	{
		allowed = UNRESERVED_CHARS "-";
		std::sort(allowed.begin(), allowed.end());
	}
	return escape(str, allowed, true);
}

void LLURI::parseAuthorityAndPathUsingOpaque()
{
	if (mScheme == "http" || mScheme == "https" || mScheme == "ftp" ||
		mScheme == "secondlife" || mScheme == "hop" ||
		mScheme == "x-grid-info" || mScheme == "x-grid-location-info")
	{
		if (mEscapedOpaque.substr(0, 2) != "//")
		{
			return;
		}

		size_t delim_pos, delim_pos2;
		delim_pos = mEscapedOpaque.find('/', 2);
		delim_pos2 = mEscapedOpaque.find('?', 2);
		// No path, no query
		if (delim_pos == std::string::npos && delim_pos2 == std::string::npos)
		{
			mEscapedAuthority = mEscapedOpaque.substr(2);
			mEscapedPath.clear();
		}
		// Path exist, no query
		else if (delim_pos2 == std::string::npos)
		{
			mEscapedAuthority = mEscapedOpaque.substr(2, delim_pos - 2);
			mEscapedPath = mEscapedOpaque.substr(delim_pos);
		}
		// No path, only query
		else if (delim_pos == std::string::npos || delim_pos2 < delim_pos)
		{
			mEscapedAuthority = mEscapedOpaque.substr(2, delim_pos2 - 2);
			// Query part will be broken out later
			mEscapedPath = mEscapedOpaque.substr(delim_pos2);
		}
		// Path and query
		else
		{
			mEscapedAuthority = mEscapedOpaque.substr(2, delim_pos - 2);
			// Query part will be broken out later
			mEscapedPath = mEscapedOpaque.substr(delim_pos);
		}
	}
	else if (mScheme == "about")
	{
		mEscapedPath = mEscapedOpaque;
	}
}

//static
LLURI LLURI::buildHTTP(const std::string& prefix, const LLSD& path)
{
	LLURI result;

	// *TODO: deal with '/' '?' '#' in host_port
	if (prefix.find("://") != prefix.npos)
	{
		// It is a prefix
		result = LLURI(prefix);
	}
	else
	{
		// It is just a host and optional port
		result.mScheme = "http";
		result.mEscapedAuthority = escapeHostAndPort(prefix);
	}

	if (path.isArray())
	{
		// Break out and escape each path component
		for (LLSD::array_const_iterator it = path.beginArray();
			 it != path.endArray();
			 ++it)
		{
			LL_DEBUGS("URI") << "PATH: inserting " << it->asString()
							 << LL_ENDL;
			result.mEscapedPath += "/" + escapePathComponent(it->asString());
		}
	}
	else if (path.isString())
	{
		result.mEscapedPath += "/" + escapePathComponent(path.asString());
	}
    else if (!path.isUndefined())
	{
	  llwarns << "Valid path arguments are array, string, or undef, you passed type "
			  << path.type() << llendl;
	}
	result.mEscapedOpaque = "//" + result.mEscapedAuthority +
							result.mEscapedPath;
	return result;
}

//static
LLURI LLURI::buildHTTP(const std::string& prefix, const LLSD& path,
					   const LLSD& query)
{
	LLURI uri = buildHTTP(prefix, path);
	// Break out and escape each query component
	uri.mEscapedQuery = mapToQueryString(query);
	uri.mEscapedOpaque += uri.mEscapedQuery ;
	uri.mEscapedQuery.erase(0, 1); // trim the leading '?'
	return uri;
}

//static
LLURI LLURI::buildHTTP(const std::string& host, const U32& port,
					   const LLSD& path)
{
	return LLURI::buildHTTP(llformat("%s:%u", host.c_str(), port), path);
}

//static
LLURI LLURI::buildHTTP(const std::string& host, const U32& port,
					   const LLSD& path, const LLSD& query)
{
	return LLURI::buildHTTP(llformat("%s:%u", host.c_str(), port),
							path, query);
}

std::string LLURI::asString() const
{
	return mScheme.empty() ? mEscapedOpaque : mScheme + ":" + mEscapedOpaque;
}

std::string LLURI::opaque() const
{
	return unescape(mEscapedOpaque);
}

std::string LLURI::authority() const
{
	return unescape(mEscapedAuthority);
}

std::string LLURI::hostName() const
{
	std::string user, host, port;
	find_authority_parts(mEscapedAuthority, user, host, port);
	return unescape(host);
}

std::string LLURI::userName() const
{
	std::string user, user_pass, host, port;
	find_authority_parts(mEscapedAuthority, user_pass, host, port);
	size_t pos = user_pass.find(':');
	if (pos != std::string::npos)
	{
		user = user_pass.substr(0, pos);
	}
	return unescape(user);
}

std::string LLURI::password() const
{
	std::string pass, user_pass, host, port;
	find_authority_parts(mEscapedAuthority, user_pass, host, port);
	size_t pos = user_pass.find(':');
	if (pos != std::string::npos)
	{
		pass = user_pass.substr(pos + 1);
	}
	return unescape(pass);
}

bool LLURI::defaultPort() const
{
	return is_default(mScheme, hostPort());
}

U16 LLURI::hostPort() const
{
	std::string user, host, port;
	find_authority_parts(mEscapedAuthority, user, host, port);
	if (port.empty())
	{
		if (mScheme == "http")
		{
			return 80;
		}
		else if (mScheme == "https")
		{
			return 443;
		}
		else if (mScheme == "ftp")
		{
			return 21;
		}
		return 0;
	}
	return atoi(port.c_str());
}

std::string LLURI::path() const
{
	return unescape(mEscapedPath);
}

LLSD LLURI::pathArray() const
{
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("/", "", boost::drop_empty_tokens);
	tokenizer tokens(mEscapedPath, sep);

	LLSD params;
	for (tokenizer::iterator it = tokens.begin(), end = tokens.end();
		 it != end; ++it)
	{
		params.append(*it);
	}
	return params;
}

std::string LLURI::query() const
{
	return unescape(mEscapedQuery);
}

LLSD LLURI::queryMap() const
{
	return queryMap(mEscapedQuery);
}

//static
LLSD LLURI::queryMap(std::string escaped_query_string)
{
	LL_DEBUGS("URI") << "LLURI::queryMap query params: "
					 << escaped_query_string << LL_ENDL;

	LLSD result = LLSD::emptyArray();
	while (!escaped_query_string.empty())
	{
		// Get tuple first
		std::string tuple;
		size_t tuple_begin = escaped_query_string.find('&');
		if (tuple_begin != std::string::npos)
		{
			tuple = escaped_query_string.substr(0, tuple_begin);
			escaped_query_string =
				escaped_query_string.substr(tuple_begin + 1);
		}
		else
		{
			tuple = escaped_query_string;
			escaped_query_string.clear();
		}
		if (tuple.empty()) continue;

		// Parse tuple
		size_t key_end = tuple.find('=');
		if (key_end != std::string::npos)
		{
			std::string key = unescape(tuple.substr(0, key_end));
			std::string value = unescape(tuple.substr(key_end + 1));
			LL_DEBUGS("URI") << "inserting key " << key << " value " << value
							 << LL_ENDL;
			result[key] = value;
		}
		else
		{
			LL_DEBUGS("URI") << "inserting key " << unescape(tuple)
							 << " value true" << LL_ENDL;
		    result[unescape(tuple)] = true;
		}
	}
	return result;
}

std::string LLURI::mapToQueryString(const LLSD& query_map)
{
	std::string query_string;
	if (query_map.isMap())
	{
		bool first_element = true;
		std::ostringstream ostr;
		for (LLSD::map_const_iterator iter = query_map.beginMap(),
									  end = query_map.endMap();
			 iter != end; ++iter)
		{
			if (first_element)
			{
				ostr << "?";
				first_element = false;
			}
			else
			{
				ostr << "&";
			}
			ostr << escapeQueryVariable(iter->first);
			if (iter->second.isDefined())
			{
				ostr << "=" <<  escapeQueryValue(iter->second.asString());
			}
		}
		query_string = ostr.str();
	}
	return query_string;
}

bool operator!=(const LLURI& first, const LLURI& second)
{
	return first.asString() != second.asString();
}
