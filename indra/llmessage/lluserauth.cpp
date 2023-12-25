/**
 * @file lluserauth.cpp
 * @brief LLUserAuth class implementation
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "lluserauth.h"

#include <sstream>
#include <iterator>

#include "curl/curl.h"
#include "xmlrpc-epi/xmlrpc.h"
// <xmlrpc-epi/queue.h> contains a harmful #define queue xmlrpc_queue. This
// breaks any use of std::queue. Ditch that #define: if any of our code wants
// to reference xmlrpc_queue, let it reference it directly.
#if defined(queue)
# undef queue
#endif

#include "llsdutil.h"
#include "llxmlrpctransaction.h"

// Do not define PLATFORM_STRING for unknown platforms: they need to get added
// to the login.cgi script, so we want this to cause an error if we get
// compiled for a different platform.
#if LL_WINDOWS
static const char* PLATFORM_STRING = "Win";
#elif LL_DARWIN
static const char* PLATFORM_STRING = "Mac";
#elif LL_LINUX
static const char* PLATFORM_STRING = "Lnx";
#else
# error("Unknown platform !")
#endif

LLUserAuth gUserAuth;

LLUserAuth::LLUserAuth()
:	mTransaction(NULL),
	mAuthResponse(E_NO_RESPONSE_YET),
	mUseMFA(false)
{
}

LLUserAuth::~LLUserAuth()
{
	reset();
}

void LLUserAuth::reset()
{
	delete mTransaction;
	mTransaction = NULL;
	mResponses.clear();
}

void LLUserAuth::init(const std::string& platform_version,
					  const std::string& os_string,
					  const std::string& version, const std::string& channel,
					  const std::string& serial_hash,
					  const std::string& mac_hash)
{
	mPlatformVersion = platform_version;
	mPlatformOSString = os_string;
	mViewerVersion = version;
	mViewerChannel = channel;
	mHashedSerial = serial_hash;
	mHashedMAC = mac_hash;
}

void LLUserAuth::setMFA(bool use_mfa, const std::string& mfa_hash,
						const std::string& mfa_token)
{
	mUseMFA = use_mfa;
	if (!use_mfa)
	{
		mMFAHash.clear();
		mMFAToken.clear();
	}
	// When replying to a MFA challenge (i.e. the token string is not empty),
	// we pass the token and an empty hash. Else, we use any last known good
	// MFA hash we got, with an empty token.
	// See: https://wiki.secondlife.com/wiki/User:Brad_Linden/Login_MFA
	else if (mfa_token.empty())
	{
		mMFAHash = mfa_hash;
		mMFAToken.clear();
	}
	else
	{
		mMFAHash.clear();
		mMFAToken = mfa_token;
	}
}

void LLUserAuth::authenticate(const std::string& auth_uri,
							  const std::string& method,
							  const std::string& firstname,
							  const std::string& lastname,
							  const std::string& passwd,
							  const std::string& start,
							  bool skip_optional,
							  bool accept_tos, bool accept_critical_message,
							  bool last_exec_froze,
							  const std::vector<const char*>& req_options)
{
	if (mHashedSerial.empty())
	{
		llerrs << "LLUserAuth was not properly initialized !" << llendl;
	}

	llinfos << "Authenticating: " << firstname << " " << lastname << llendl;

	// NOTE: passwd is already MD5 hashed by the time we get to it.
	std::string dpasswd("$1$");
	dpasswd.append(passwd);

	std::ostringstream option_str;
	option_str << "Options: ";
	std::ostream_iterator<const char*> appender(option_str, ", ");
	std::copy(req_options.begin(), req_options.end(), appender);
	option_str << "END";

	llinfos << option_str.str().c_str() << llendl;

	mAuthResponse = E_NO_RESPONSE_YET;
	//mDownloadTimer.reset();

	// Create the request
	XMLRPC_REQUEST request = XMLRPC_RequestNew();
	XMLRPC_RequestSetMethodName(request, method.c_str());
	XMLRPC_RequestSetRequestType(request, xmlrpc_request_call);

	// Stuff the parameters
	XMLRPC_VALUE params = XMLRPC_CreateVector(NULL, xmlrpc_vector_struct);
	XMLRPC_VectorAppendString(params, "first", firstname.c_str(), 0);
	XMLRPC_VectorAppendString(params, "last", lastname.c_str(), 0);
	XMLRPC_VectorAppendString(params, "passwd", dpasswd.c_str(), 0);
	XMLRPC_VectorAppendString(params, "start", start.c_str(), 0);
	// Includes channel name
	XMLRPC_VectorAppendString(params, "version", mViewerVersion.c_str(), 0);
	XMLRPC_VectorAppendString(params, "channel", mViewerChannel.c_str(), 0);
	XMLRPC_VectorAppendString(params, "platform", PLATFORM_STRING, 0);
	// Note: the viewer cannot any more be built for 32 bits platforms. HB
	XMLRPC_VectorAppendInt(params, "address_size", 64);
	XMLRPC_VectorAppendString(params, "platform_version",
							  mPlatformVersion.c_str(), 0);
	XMLRPC_VectorAppendString(params, "platform_string",
							  mPlatformOSString.c_str(), 0);
	XMLRPC_VectorAppendString(params, "mac", mHashedMAC.c_str(), 0);
	// A bit of pseudo-security through obscurity: id0 is volume_serial
	XMLRPC_VectorAppendString(params, "id0", mHashedSerial.c_str(), 0);
	if (mUseMFA)
	{
		XMLRPC_VectorAppendString(params, "mfa_hash", mMFAHash.c_str(), 0);
		XMLRPC_VectorAppendString(params, "token", mMFAToken.c_str(), 0);
	}
	if (skip_optional)
	{
		XMLRPC_VectorAppendString(params, "skipoptional", "true", 0);
	}
	if (accept_tos)
	{
		XMLRPC_VectorAppendString(params, "agree_to_tos", "true", 0);
	}
	if (accept_critical_message)
	{
		XMLRPC_VectorAppendString(params, "read_critical", "true", 0);
	}
	XMLRPC_VectorAppendInt(params, "last_exec_event", (int)last_exec_froze);

	// Append optional requests in an array
	XMLRPC_VALUE options = XMLRPC_CreateVector("options", xmlrpc_vector_array);
	for (std::vector<const char*>::const_iterator it = req_options.begin(),
												  end = req_options.end();
		 it < end; ++it)
	{
		XMLRPC_VectorAppendString(options, NULL, *it, 0);
	}
	XMLRPC_AddValueToVector(params, options);

	// Put the parameters on the request
	XMLRPC_RequestSetData(request, params);

	mTransaction = new LLXMLRPCTransaction(auth_uri, request);

	XMLRPC_RequestFree(request, 1);

	llinfos << "URI: " << auth_uri << llendl;
}

LLUserAuth::UserAuthcode LLUserAuth::authResponse()
{
	if (!mTransaction)
	{
		return mAuthResponse;
	}

	if (!mTransaction->process())	// All done ?
	{
		if (mTransaction->status(0) == LLXMLRPCTransaction::StatusDownloading)
		{
			mAuthResponse = E_DOWNLOADING;
		}
		return mAuthResponse;
	}

	S32 result;
	mTransaction->status(&result);
	mErrorMessage = mTransaction->statusMessage();

	// If curl was ok, parse the download area.
	switch (result)
	{
		case CURLE_OK:
			mResponses = parseResponse();
			LL_DEBUGS("UserAuth") << "Responses LLSD:\n"
								  << ll_pretty_print_sd(mResponses) << LL_ENDL;
			break;

		case CURLE_COULDNT_RESOLVE_HOST:
			mAuthResponse = E_COULDNT_RESOLVE_HOST;
			break;

#if CURLE_SSL_CACERT != CURLE_SSL_PEER_CERTIFICATE
		// Note: CURLE_SSL_CACERT and CURLE_SSL_CACERT may expand to the same
		// value in recent curl versions (seen with curl v7.68).
		case CURLE_SSL_PEER_CERTIFICATE:
			mAuthResponse = E_SSL_PEER_CERTIFICATE;
			break;
#endif

		case CURLE_SSL_CACERT:
			mAuthResponse = E_SSL_CACERT;
			break;

		case CURLE_SSL_CONNECT_ERROR:
			mAuthResponse = E_SSL_CONNECT_ERROR;
			break;

		default:
			mAuthResponse = E_UNHANDLED_ERROR;
	}

	llinfos << "Processed response: " << result << llendl;

	delete mTransaction;
	mTransaction = NULL;

	return mAuthResponse;
}

// The job of this method is to parse and extract every response and return
// everything as a LLSD.
LLSD LLUserAuth::parseResponse()
{
	XMLRPC_REQUEST response = mTransaction->response();
	if (!response)
	{
		mAuthResponse = E_UNHANDLED_ERROR;
		mErrorMessage = "No response";
		llwarns << "No response !" << llendl;
		return LLSD();
	}

	// Now, parse everything
	XMLRPC_VALUE param = XMLRPC_RequestGetData(response);
	if (!param)
	{
		mAuthResponse = E_UNHANDLED_ERROR;
		mErrorMessage = "Response contains no data";
		llwarns << "Response contains no data !" << llendl;
		return LLSD();
	}

	mAuthResponse = E_OK;

	// Now, parse everything
	return parseValues("", param);
}

LLSD LLUserAuth::parseValues(const std::string& key_pfx, XMLRPC_VALUE param)
{
	LLSD responses;

	mIndentation.push_back(' ');

	for (XMLRPC_VALUE current = XMLRPC_VectorRewind(param); current;
		 current = XMLRPC_VectorNext(param))
	{
		std::string key = XMLRPC_GetValueID(current);
		LL_DEBUGS("UserAuth") << mIndentation << "key: " << key << LL_ENDL;

		XMLRPC_VALUE_TYPE_EASY type = XMLRPC_GetValueTypeEasy(current);
		switch (type)
		{
			case xmlrpc_type_empty:
			{
				llinfos << "Empty result for key: " << key_pfx << llendl;
				responses.insert(key, LLSD());
				break;
			}

			case xmlrpc_type_base64:
			{
				S32 len = XMLRPC_GetValueStringLen(current);
				const char* buf = XMLRPC_GetValueBase64(current);
				if (len > 0 && buf)
				{
					LLSD::Binary val;
					val.resize(len);
					memcpy((void*)val.data(), (void*)buf, len);
					LL_DEBUGS("UserAuth") << mIndentation << " base64 val"
										  << LL_ENDL;
					responses.insert(key, val);
				}
				else
				{
					llwarns << "Malformed xmlrpc_type_base64 for key: "
						<< key_pfx << key << llendl;
					responses.insert(key, LLSD());
				}
				break;
			}

			case xmlrpc_type_boolean:
			{
				LLSD::Boolean val(XMLRPC_GetValueBoolean(current));
				LL_DEBUGS("UserAuth") << mIndentation << " boolean val = "
									  << val << LL_ENDL;
				responses.insert(key, val);
				break;
			}

			case xmlrpc_type_datetime:
			{
				std::string val(XMLRPC_GetValueDateTime_ISO8601(current));
				LL_DEBUGS("UserAuth") << mIndentation << " iso8601_date val = "
									  << val << LL_ENDL;
				responses.insert(key, LLSD::Date(val));
				break;
			}

			case xmlrpc_type_double:
			{
				LLSD::Real val(XMLRPC_GetValueDouble(current));
				LL_DEBUGS("UserAuth") << mIndentation << " double val = "
									  << val << LL_ENDL;
				responses.insert(key, val);
				break;
			}

			case xmlrpc_type_int:
			{
				LLSD::Integer val(XMLRPC_GetValueInt(current));
				LL_DEBUGS("UserAuth") << mIndentation << " int val = " << val
									  << LL_ENDL;
				responses.insert(key, val);
				break;
			}

			case xmlrpc_type_string:
			{
				LLSD::String val(XMLRPC_GetValueString(current));
				LL_DEBUGS("UserAuth") << mIndentation << " string val = "
									  << val << LL_ENDL;
				responses.insert(key, val);
				break;
			}

			case xmlrpc_type_array:
			case xmlrpc_type_mixed:
			{
				// We expect this to be an array of submaps. Walk the array,
				// recursively parsing each submap and collecting them.
				LLSD array;
				S32 i = 0;	// For descriptive purposes
				// Recursive call. For the lower-level key_pfx, if 'key' is
				// "foo", pass "foo[0]:", then "foo[1]:", etc. In the nested
				// call, a subkey "bar" will then be logged as "foo[0]:bar",
				// and so forth.
				std::string pfx;
				// Parse the scalar subkey/value pairs from this array entry
				// into a temp submap. Collect such submaps in 'array'.
				for (XMLRPC_VALUE row = XMLRPC_VectorRewind(current); row;
					 row = XMLRPC_VectorNext(current), ++i)
				{
					LL_DEBUGS("UserAuth") << mIndentation << "map #" << i
										  << LL_ENDL;
					pfx = key_pfx + key + llformat("[%d]:", i);
					array.append(parseValues(pfx, row));
				}
				// Having collected an 'array' of 'submap's, insert that whole
				// 'array' as the value of this 'key'.
				responses.insert(key, array);
				break;
			}

			case xmlrpc_type_struct:
			{
				LLSD submap = parseValues(key_pfx + key + ":", current);
				responses.insert(key, submap);
				break;
			}

			default: // Cannot handle this type (xmlrpc_type_none or other)
			{
				responses.insert(key, LLSD::String("???"));
				llwarns << "Unknown value type " << type << " for key: "
						<< key_pfx << key << llendl;
#if 0			// Do not care and keep logging in... HB
				mAuthResponse = E_UNHANDLED_ERROR;
				mErrorMessage = "Bad value type";
#endif
			}
		}
	}

	mIndentation.pop_back();

	return responses;
}

const LLSD& LLUserAuth::getResponse1stMap(const std::string& name) const
{
	if (mResponses.has(name) && mResponses[name].isArray() &&
		mResponses[name][0].isMap())
	{
		return mResponses[name][0];
	}

	static const LLSD empty;
	return empty;
}
