/**
 * @file llsocks5.cpp
 * @brief Socks 5 implementation
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#include "llproxy.h"

#include "llapr.h"
#include "llcorehttpcommon.h"
#include "llhost.h"
#include "lltimer.h"		// For ms_sleep()

// Static class variable instances

// We want this to be static to avoid excessive indirection on every incoming
// packet just to do a simple bool test. The getter for this member is also
// static.
bool LLProxy::sUDPProxyEnabled = false;

// Some helpful TCP static functions.

// Do a TCP data handshake
static apr_status_t tcp_blocking_handshake(LLSocket::ptr_t handle,
										   char* dataout, apr_size_t outlen,
										   char* datain, apr_size_t maxinlen);

// Open a TCP channel to a given host
static LLSocket::ptr_t tcp_open_channel(LLHost host);
// Close an open TCP channel
static void tcp_close_channel(LLSocket::ptr_t* handle_ptr);

LLProxy::LLProxy()
:	mHTTPProxyEnabled(false),
	mProxyType(LLPROXY_SOCKS),
	mAuthMethodSelected(METHOD_NOAUTH)
{
}

LLProxy::~LLProxy()
{
	if (gAPRInitialized)
	{
		stopSOCKSProxy();
		disableHTTPProxy();
	}
}

/**
 * @brief Open the SOCKS 5 TCP control channel.
 *
 * Perform a SOCKS 5 authentication and UDP association with the proxy server.
 *
 * @param proxy The SOCKS 5 server to connect to.
 * @return SOCKS_OK if successful, otherwise a socks error code from llproxy.h.
 */
S32 LLProxy::proxyHandshake(LLHost proxy)
{
	S32 result;

	/* SOCKS 5 Auth request */
	socks_auth_request_t  socks_auth_request;
	socks_auth_response_t socks_auth_response;

	socks_auth_request.version		= SOCKS_VERSION;			// SOCKS version 5
	socks_auth_request.num_methods	= 1;						// Sending 1 method.
	socks_auth_request.methods		= getSelectedAuthMethod();	// Send only the selected method.

	result = tcp_blocking_handshake(mProxyControlChannel,
									static_cast<char*>(static_cast<void*>(&socks_auth_request)),
									sizeof(socks_auth_request),
									static_cast<char*>(static_cast<void*>(&socks_auth_response)),
									sizeof(socks_auth_response));
	if (result != APR_SUCCESS)
	{
		llwarns << "SOCKS authentication request failed, error on TCP control channel : "
				<< result << llendl;
		stopSOCKSProxy();
		return SOCKS_CONNECT_ERROR;
	}

	if (socks_auth_response.method == AUTH_NOT_ACCEPTABLE)
	{
		llwarns << "SOCKS 5 server refused all our authentication methods."
				<< llendl;
		stopSOCKSProxy();
		return SOCKS_NOT_ACCEPTABLE;
	}

	/* SOCKS 5 USERNAME/PASSWORD authentication */
	if (socks_auth_response.method == METHOD_PASSWORD)
	{
		// The server has requested a username/password combination
		std::string socks_username(getSocksUser());
		std::string socks_password(getSocksPwd());
		U32 request_size = socks_username.size() + socks_password.size() + 3;
		char* password_auth = new char[request_size];
		password_auth[0] = 0x01;
		password_auth[1] = (char)socks_username.size();
		memcpy(&password_auth[2], socks_username.c_str(), socks_username.size());
		password_auth[socks_username.size() + 2] = (char)socks_password.size();
		memcpy(&password_auth[socks_username.size() + 3],
			   socks_password.c_str(), socks_password.size());

		authmethod_password_reply_t password_reply;

		result = tcp_blocking_handshake(mProxyControlChannel,
										password_auth,
										request_size,
										static_cast<char*>(static_cast<void*>(&password_reply)),
										sizeof(password_reply));
		delete[] password_auth;

		if (result != APR_SUCCESS)
		{
			llwarns << "SOCKS authentication failed, error on TCP control channel : "
					<< result << llendl;
			stopSOCKSProxy();
			return SOCKS_CONNECT_ERROR;
		}

		if (password_reply.status != AUTH_SUCCESS)
		{
			llwarns << "SOCKS authentication failed" << llendl;
			stopSOCKSProxy();
			return SOCKS_AUTH_FAIL;
		}
	}

	/* SOCKS5 connect request */

	socks_command_request_t  connect_request;
	socks_command_response_t connect_reply;

	connect_request.version		= SOCKS_VERSION;         // SOCKS V5
	connect_request.command		= COMMAND_UDP_ASSOCIATE; // Associate UDP
	connect_request.reserved	= FIELD_RESERVED;
	connect_request.atype		= ADDRESS_IPV4;
	connect_request.address		= htonl(0); // 0.0.0.0
	connect_request.port		= htons(0); // 0
	// "If the client is not in possession of the information at the time of
	// the UDP ASSOCIATE, the client MUST use a port number and address of all
	// zeros. RFC 1928"

	result = tcp_blocking_handshake(mProxyControlChannel,
									static_cast<char*>(static_cast<void*>(&connect_request)),
									sizeof(connect_request),
									static_cast<char*>(static_cast<void*>(&connect_reply)),
									sizeof(connect_reply));
	if (result != APR_SUCCESS)
	{
		llwarns << "SOCKS connect request failed, error on TCP control channel : "
				<< result << llendl;
		stopSOCKSProxy();
		return SOCKS_CONNECT_ERROR;
	}

	if (connect_reply.reply != REPLY_REQUEST_GRANTED)
	{
		llwarns << "Connection to SOCKS 5 server failed, UDP forward request not granted"
				<< llendl;
		stopSOCKSProxy();
		return SOCKS_UDP_FWD_NOT_GRANTED;
	}

	// reply port is in network byte order
	mUDPProxy.setPort(ntohs(connect_reply.port));

	mUDPProxy.setAddress(proxy.getAddress());

	// The connection was successful. We now have the UDP port to send requests
	// that need forwarding to.
	llinfos << "SOCKS 5 UDP proxy connected on " << mUDPProxy << llendl;

	return SOCKS_OK;
}

/**
 * @brief Initiates a SOCKS 5 proxy session.
 *
 * Performs basic checks on host to verify that it is a valid address. Opens
 * the control channel and then negotiates the proxy connection with the
 * server. Closes any existing SOCKS connection before proceeding. Also
 * disables an HTTP proxy if it is using SOCKS as the proxy.
 *
 *
 * @param host Socks server to connect to.
 * @return SOCKS_OK if successful, otherwise a SOCKS error code defined in
 * llproxy.h.
 */
S32 LLProxy::startSOCKSProxy(LLHost host)
{
	if (host.isOk())
	{
		mTCPProxy = host;
	}
	else
	{
		return SOCKS_INVALID_HOST;
	}

	// Close any running SOCKS connection.
	stopSOCKSProxy();

	mProxyControlChannel = tcp_open_channel(mTCPProxy);
	if (!mProxyControlChannel)
	{
		return SOCKS_HOST_CONNECT_FAILED;
	}

	S32 status = proxyHandshake(mTCPProxy);

	if (status != SOCKS_OK)
	{
		// Shut down the proxy if any of the above steps failed.
		stopSOCKSProxy();
	}
	else
	{
		// Connection was successful.
		sUDPProxyEnabled = true;
	}

	return status;
}

/**
 * @brief Stop using the SOCKS 5 proxy.
 *
 * This will stop sending UDP packets through the SOCKS 5 proxy and will also
 * stop the HTTP proxy if it is configured to use SOCKS. The proxy control
 * channel will also be disconnected.
 */
void LLProxy::stopSOCKSProxy()
{
	sUDPProxyEnabled = false;

	// If the SOCKS proxy is requested to stop and we are using that for HTTP
	// as well then we must shut down any HTTP proxy operations. But it is
	// allowable if web proxy is being used to continue proxying HTTP.
	if (LLPROXY_SOCKS == getHTTPProxyType())
	{
		disableHTTPProxy();
	}

	if (mProxyControlChannel)
	{
		tcp_close_channel(&mProxyControlChannel);
	}
}

/**
 * @brief Set the proxy's SOCKS authentication method to none.
 */
void LLProxy::setAuthNone()
{
	mProxyMutex.lock();
	mAuthMethodSelected = METHOD_NOAUTH;
	mProxyMutex.unlock();
}

/**
 * @brief Set the proxy's SOCKS authentication method to password.
 *
 * Check whether the lengths of the supplied username
 * and password conform to the lengths allowed by the
 * SOCKS protocol.
 *
 * @param 	username The SOCKS username to send.
 * @param 	password The SOCKS password to send.
 * @return  Return true if applying the settings was successful. No changes are
 *          made if false.
 *
 */
bool LLProxy::setAuthPassword(const std::string& username,
							  const std::string& password)
{
	if (username.length() > SOCKSMAXUSERNAMELEN ||
		password.length() > SOCKSMAXPASSWORDLEN ||
		username.length() < SOCKSMINUSERNAMELEN ||
		password.length() < SOCKSMINPASSWORDLEN)
	{
		llwarns << "Invalid SOCKS 5 password or username length." << llendl;
		return false;
	}

	mProxyMutex.lock();
	mAuthMethodSelected = METHOD_PASSWORD;
	mSocksUsername = username;
	mSocksPassword = password;
	mProxyMutex.unlock();

	return true;
}

/**
 * @brief Enable the HTTP proxy for either SOCKS or HTTP.
 *
 * Check the supplied host to see if it is a valid IP and port.
 *
 * @param httpHost Proxy server to connect to.
 * @param type Is the host a SOCKS or HTTP proxy.
 * @return Return true if applying the setting was successful. No changes are
 *         made if false.
 */
bool LLProxy::enableHTTPProxy(LLHost http_host, LLHttpProxyType type)
{
	if (!http_host.isOk())
	{
		llwarns << "Invalid SOCKS 5 Server" << llendl;
		return false;
	}

	mProxyMutex.lock();
	mHTTPProxy = http_host;
	mProxyType = type;
	mHTTPProxyEnabled = true;
	mProxyMutex.unlock();

	return true;
}

/**
 * @brief Enable the HTTP proxy without changing the proxy settings.
 *
 * This should not be called unless the proxy has already been set up.
 *
 * @return Return true only if the current settings are valid and the proxy was
 *         enabled.
 */
bool LLProxy::enableHTTPProxy()
{
	mProxyMutex.lock();
	bool ok = mHTTPProxy.isOk();
	if (ok)
	{
		mHTTPProxyEnabled = true;
	}
	mProxyMutex.unlock();

	return ok;
}

/**
 * @brief Disable the HTTP proxy.
 */
void LLProxy::disableHTTPProxy()
{
	mProxyMutex.lock();
	mHTTPProxyEnabled = false;
	mProxyMutex.unlock();
}

/**
 * @brief Get the currently selected HTTP proxy type
 */
LLHttpProxyType LLProxy::getHTTPProxyType() const
{
	mProxyMutex.lock();
	LLHttpProxyType type = mProxyType;
	mProxyMutex.unlock();
	return type;
}

/**
 * @brief Get the SOCKS 5 password.
 */
std::string LLProxy::getSocksPwd() const
{
	mProxyMutex.lock();
	std::string password = mSocksPassword;
	mProxyMutex.unlock();
	return password;
}

/**
 * @brief Get the SOCKS 5 username.
 */
std::string LLProxy::getSocksUser() const
{
	mProxyMutex.lock();
	std::string username = mSocksUsername;
	mProxyMutex.unlock();
	return username;
}

/**
 * @brief Get the currently selected SOCKS 5 authentication method.
 *
 * @return Returns either none or password.
 */
LLSocks5AuthType LLProxy::getSelectedAuthMethod() const
{
	mProxyMutex.lock();
	LLSocks5AuthType type = mAuthMethodSelected;
	mProxyMutex.unlock();
	return type;
}

/**
 * @brief Stop the LLProxy and make certain that any APR pools and classes
 * are deleted before terminating APR.
 *
 * Deletes the LLProxy singleton, destroying the APR pool used by the control
 * channel as well as .
 */
//static
void LLProxy::cleanupClass()
{
	getInstance()->stopSOCKSProxy();
	deleteSingleton();
}

/**
 * @brief Apply proxy settings to a CuRL request if an HTTP proxy is enabled.
 *
 * This method has been designed to be safe to call from
 * any thread in the viewer.  This allows requests in the
 * texture fetch thread to be aware of the proxy settings.
 * When the HTTP proxy is enabled, the proxy mutex will
 * be locked every time this method is called.
 *
 * @param handle A pointer to a valid CURL request, before it has been
 *               performed.
 */
void LLProxy::applyProxySettings(CURL* handle)
{
	// Do a faster unlocked check to see if we are supposed to proxy.
	if (mHTTPProxyEnabled)
	{
		// We think we should proxy, lock the proxy mutex.
		mProxyMutex.lock();
		// Now test again to verify that the proxy wasn't disabled between the
		// first check and the lock.
		if (mHTTPProxyEnabled)
		{
			LLCore::LLHttp::check_curl_code(curl_easy_setopt(handle,
															 CURLOPT_PROXY,
															 mHTTPProxy.getIPString().c_str()),
											CURLOPT_PROXY);
			LLCore::LLHttp::check_curl_code(curl_easy_setopt(handle,
															 CURLOPT_PROXYPORT,
															 mHTTPProxy.getPort()),
											CURLOPT_PROXYPORT);

			if (mProxyType == LLPROXY_SOCKS)
			{
				LLCore::LLHttp::check_curl_code(curl_easy_setopt(handle,
																 CURLOPT_PROXYTYPE,
																 CURLPROXY_SOCKS5),
												CURLOPT_PROXYTYPE);
				if (mAuthMethodSelected == METHOD_PASSWORD)
				{
					std::string auth_string = mSocksUsername + ":" +
											  mSocksPassword;
					LLCore::LLHttp::check_curl_code(curl_easy_setopt(handle,
																	 CURLOPT_PROXYUSERPWD,
																	 auth_string.c_str()),
													CURLOPT_PROXYUSERPWD);
				}
			}
			else
			{
				LLCore::LLHttp::check_curl_code(curl_easy_setopt(handle,
																 CURLOPT_PROXYTYPE,
																 CURLPROXY_HTTP),
												CURLOPT_PROXYTYPE);
			}
		}
		mProxyMutex.unlock();
	}
}

/**
 * @brief Send one TCP packet and receive one in return.
 *
 * This operation is done synchronously with a 1000ms timeout. Therefore, it
 * should not be used when a blocking operation would impact the operation of
 * the viewer.
 *
 * @param handle_ptr 	Pointer to a connected LLSocket of type STREAM_TCP.
 * @param dataout		Data to send.
 * @param outlen		Length of dataout.
 * @param datain		Buffer for received data. Undefined if return value is
 *                      not APR_SUCCESS.
 * @param maxinlen		Maximum possible length of received data. Short reads
 *                      are allowed.
 * @return 				Indicates APR status code of exchange. APR_SUCCESS if
 *                      exchange was successful, -1 if invalid data length was
 *                      received.
 */
static apr_status_t tcp_blocking_handshake(LLSocket::ptr_t handle,
										   char* dataout, apr_size_t outlen,
										   char* datain, apr_size_t maxinlen)
{
	apr_socket_t* apr_socket = handle->getSocket();
	apr_status_t rv = APR_SUCCESS;

	apr_size_t expected_len = outlen;

	handle->setBlocking(1000);

  	rv = apr_socket_send(apr_socket, dataout, &outlen);
	if (rv != APR_SUCCESS)
	{
		char buf[MAX_STRING];
		llwarns << "Error sending data to proxy control channel, status: "
				<< rv << " - " << apr_strerror(rv, buf, MAX_STRING) << llendl;
		ll_apr_warn_status(rv);
	}
	else if (expected_len != outlen)
	{
		llwarns << "Incorrect data length sent. Expected: " << expected_len
				<< " Sent: " << outlen << llendl;
		rv = -1;
	}

	ms_sleep(1);

	if (rv == APR_SUCCESS)
	{
		expected_len = maxinlen;
		rv = apr_socket_recv(apr_socket, datain, &maxinlen);
		if (rv != APR_SUCCESS)
		{
			char buf[MAX_STRING];
			llwarns << "Error receiving data from proxy control channel, status: "
					<< rv << " - " << apr_strerror(rv, buf, MAX_STRING)
					<< llendl;
			ll_apr_warn_status(rv);
		}
		else if (expected_len < maxinlen)
		{
			llwarns << "Incorrect data length received. Expected: "
					<< expected_len << " Received: " << maxinlen << llendl;
			rv = -1;
		}
	}

	handle->setNonBlocking();

	return rv;
}

/**
 * @brief Open a LLSocket and do a blocking connect to the chosen host.
 *
 * Checks for a successful connection, and makes sure the connection is closed
 * if it fails.
 *
 * @param host		The host to open the connection to.
 * @return			The created socket. Will evaluate as NULL if the connection
 *                  is unsuccessful.
 */
static LLSocket::ptr_t tcp_open_channel(LLHost host)
{
	LLSocket::ptr_t socket = LLSocket::create(NULL, LLSocket::STREAM_TCP);
	bool connected = socket->blockingConnect(host);
	if (!connected)
	{
		tcp_close_channel(&socket);
	}

	return socket;
}

/**
 * @brief Close the socket.
 *
 * @param handle_ptr The handle of the socket being closed.
 *                   A pointer-to-pointer to avoid increasing the use count.
 */
static void tcp_close_channel(LLSocket::ptr_t* handle_ptr)
{
	LL_DEBUGS("Proxy") << "Resetting proxy LLSocket handle, use_count == "
					   << handle_ptr->use_count() << LL_ENDL;
	handle_ptr->reset();
}
