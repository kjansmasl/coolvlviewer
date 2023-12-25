/**
 * @file lluserauth.h
 * @brief LLUserAuth class header file
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

#ifndef LLUSERAUTH_H
#define LLUSERAUTH_H

#include <map>
#include <string>
#include <vector>

// Forward decl of types from xmlrpc.h
typedef struct _xmlrpc_value* XMLRPC_VALUE;
class LLXMLRPCTransaction;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// This class encapsulates the authentication and initialization from the login
// server. Construct an instance of this object, and call the authenticate()
// method, and call authResponse() until it returns a non-negative value. If
// that method returns E_OK, you can start asking for responses via the
// getResponse*() methods.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLUserAuth
{
protected:
	LOG_CLASS(LLUserAuth);

public:
	LLUserAuth();
	~LLUserAuth();

	// These codes map to the curl return codes...
	typedef enum {
		E_NO_RESPONSE_YET = -2,
		E_DOWNLOADING = -1,
		E_OK = 0,
		E_COULDNT_RESOLVE_HOST,
		E_SSL_PEER_CERTIFICATE,
		E_SSL_CACERT,
		E_SSL_CONNECT_ERROR,
		E_UNHANDLED_ERROR,
		E_LAST						// Never use this !
	} UserAuthcode;

	// Clears out internal data cache.
	void reset();

	// Used in llappviewer.cpp to transmit all the constant data to us.
	void init(const std::string& platform_ver, const std::string& os_string,
			  const std::string& viewer_version, const std::string& channel,
			  const std::string& serial_hash, const std::string& mac_hash);

	void setMFA(bool use_mfa, const std::string& mfa_hash,
				const std::string& mfa_token);

	void authenticate(const std::string& auth_uri,
					  const std::string& auth_method,
					  const std::string& firstname,
					  const std::string& lastname,
					  const std::string& password,
					  const std::string& start,
					  bool skip_optional_update, bool accept_tos,
					  bool accept_critical_message,
					  bool last_exec_froze,
					  const std::vector<const char*>& requested_options);

	UserAuthcode authResponse();

	LL_INLINE const std::string& errorMessage() const
	{
		return mErrorMessage;
	}

	LL_INLINE const LLSD& getResponse() const		{ return mResponses; }

	// Method to get a direct reponse from the login API by name.
	LL_INLINE const LLSD& getResponse(const std::string& name) const
	{
		return mResponses[name];
	}

	LL_INLINE std::string getResponseStr(const std::string& name) const
	{
		return mResponses.has(name) ? mResponses[name].asString() : "";
	}

	// Returns the mResponses[name][0] LLSD map when it exists
	const LLSD& getResponse1stMap(const std::string& name) const;

private:
	LLSD parseResponse();
	LLSD parseValues(const std::string& key_prefx, XMLRPC_VALUE param);

private:
	LLXMLRPCTransaction*	mTransaction;

	std::string				mPlatformVersion;
	std::string				mPlatformOSString;

	std::string				mViewerVersion;
	std::string				mViewerChannel;

	std::string				mHashedSerial;
	std::string				mHashedMAC;

	std::string				mMFAHash;
	std::string				mMFAToken;

	std::string				mErrorMessage;
	std::string				mIndentation;

	LLSD					mResponses;

	UserAuthcode			mAuthResponse;

	bool					mUseMFA;
};

extern LLUserAuth gUserAuth;

#endif // LLUSERAUTH_H
