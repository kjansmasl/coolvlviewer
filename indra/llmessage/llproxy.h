/**
 * @file llsocks5.h
 * @brief Socks 5 implementation
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

#ifndef LL_PROXY_H
#define LL_PROXY_H

#include <string>

#include "curl/curl.h"

#include "llatomic.h"
#include "llhost.h"
#include "lliosocket.h"
#include "llsingleton.h"
#include "llthread.h"

// SOCKS error codes returned from the StartProxy method
#define SOCKS_OK 0
#define SOCKS_CONNECT_ERROR (-1)
#define SOCKS_NOT_PERMITTED (-2)
#define SOCKS_NOT_ACCEPTABLE (-3)
#define SOCKS_AUTH_FAIL (-4)
#define SOCKS_UDP_FWD_NOT_GRANTED (-5)
#define SOCKS_HOST_CONNECT_FAILED (-6)
#define SOCKS_INVALID_HOST (-7)

#ifndef MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN (255 + 1) /* socks5: 255, +1 for len. */
#endif

#define SOCKSMAXUSERNAMELEN 255
#define SOCKSMAXPASSWORDLEN 255

#define SOCKSMINUSERNAMELEN 1
#define SOCKSMINPASSWORDLEN 1

#define SOCKS_VERSION 0x05 // we are using SOCKS 5

#define SOCKS_HEADER_SIZE 10

// SOCKS 5 address/hostname types
#define ADDRESS_IPV4     0x01
#define ADDRESS_HOSTNAME 0x03
#define ADDRESS_IPV6     0x04

// Lets just use our own IPv4 struct rather than dragging in system specific
// headers
union ipv4_address_t
{
	U8	octets[4];
	U32	addr32;
};

// SOCKS 5 control channel commands
#define COMMAND_TCP_STREAM    0x01
#define COMMAND_TCP_BIND      0x02
#define COMMAND_UDP_ASSOCIATE 0x03

// SOCKS 5 command replies
#define REPLY_REQUEST_GRANTED     0x00
#define REPLY_GENERAL_FAIL        0x01
#define REPLY_RULESET_FAIL        0x02
#define REPLY_NETWORK_UNREACHABLE 0x03
#define REPLY_HOST_UNREACHABLE    0x04
#define REPLY_CONNECTION_REFUSED  0x05
#define REPLY_TTL_EXPIRED         0x06
#define REPLY_PROTOCOL_ERROR      0x07
#define REPLY_TYPE_NOT_SUPPORTED  0x08

#define FIELD_RESERVED 0x00

// The standard SOCKS 5 request packet
// Push current alignment to stack and set alignment to 1 byte boundary
// This enabled us to use structs directly to set up and receive network packets
// into the correct fields, without fear of boundary alignment causing issues
#pragma pack(push, 1)

// SOCKS 5 command packet
struct socks_command_request_t
{
	U8	version;
	U8	command;
	U8	reserved;
	U8	atype;
	U32	address;
	U16	port;
};

// Standard SOCKS 5 reply packet
struct socks_command_response_t
{
	U8	version;
	U8	reply;
	U8	reserved;
	U8	atype;
	U8	add_bytes[4];
	U16	port;
};

#define AUTH_NOT_ACCEPTABLE 0xFF // reply if preferred methods are not available
#define AUTH_SUCCESS        0x00 // reply if authentication successful

// SOCKS 5 authentication request, stating which methods the client supports
struct socks_auth_request_t
{
	U8 version;
	U8 num_methods;
	U8 methods; // We are only using a single method currently
};

// SOCKS 5 authentication response packet, stating server preferred method
struct socks_auth_response_t
{
	U8 version;
	U8 method;
};

// SOCKS 5 password reply packet
struct authmethod_password_reply_t
{
	U8 version;
	U8	status;
};

// SOCKS 5 UDP packet header
struct proxywrap_t
{
	U16	rsv;
	U8	frag;
	U8	atype;
	U32	addr;
	U16	port;
};

#pragma pack(pop)	// Restore original alignment from stack

// Currently selected HTTP proxy type
enum LLHttpProxyType
{
	LLPROXY_SOCKS = 0,
	LLPROXY_HTTP  = 1
};

// Auth types
enum LLSocks5AuthType
{
	METHOD_NOAUTH   = 0x00,	// Client supports no auth
	METHOD_GSSAPI   = 0x01,	// Client supports GSSAPI (Not currently supported)
	METHOD_PASSWORD = 0x02 	// Client supports username/password
};

// LLProxy is responsible for managing two interconnected tasks, connecting to
// a SOCKS 5 proxy for use by LLPacketRing to send UDP packets and managing
// proxy settings for HTTP requests.
//
// Threading:
// Because HTTP requests can be generated in threads outside the main thread,
// it is necessary for some of the information stored by this class to be
// available to other threads. The members that need to be read across threads
// are in a labeled section below. To protect those members, a mutex,
// mProxyMutex should be locked before reading or writing those members.
// Methods that can lock mProxyMutex are in a labeled section below. Those
// methods should not be called while the mutex is already locked.
//
// There is also a LLAtomic type flag (mHTTPProxyEnabled) that is used to track
// whether the HTTP proxy is currently enabled. This allows for faster unlocked
// checks to see if the proxy is enabled. This allows us to cut down on the
// performance hit when the proxy is disabled compared to before this class was
// introduced.
//
// UDP Proxying:
// UDP datagrams are proxied via a SOCKS 5 proxy with the UDP associate
// command. To initiate the proxy, a TCP socket connection is opened to the
// SOCKS 5 host, and after a handshake exchange, the server returns a port and
// address to send the UDP traffic that is to be proxied to. The LLProxy class
// tracks this address and port after the exchange and provides it to
// LLPacketRing when required to. All UDP proxy management occurs in the main
// thread.
//
// HTTP Proxying:
// This class allows all viewer HTTP packets to be sent through a proxy. The
// user can select whether to send HTTP packets through a standard "web" HTTP
// proxy, through a SOCKS 5 proxy, or to not proxy HTTP communication. This
// class does not manage the integrated web browser proxy, which is handled in
// llviewermedia.cpp.
//
// The implementation of HTTP proxying is handled by libcurl. LLProxy is
// responsible for managing the HTTP proxy options and provides a thread-safe
// method to apply those options to a curl request (via
// LLProxy::applyProxySettings()). This method is overloaded to accommodate the
// various abstraction libcurl layers that exist throughout the viewer (CURL).
//
// To ensure thread safety, all LLProxy members that relate to the HTTP proxy
// require the LLProxyMutex to be locked before accessing.

class LLProxy final : public LLSingleton<LLProxy>
{
	friend class LLSingleton<LLProxy>;

protected:
	LOG_CLASS(LLProxy);

public:
	// ########################################################################
	// METHODS THAT DO NOT LOCK mProxyMutex !
	// ########################################################################
	// Constructor, cannot have parameters due to LLSingleton parent class.
	// Call from main thread only.
	LLProxy();

	// Static check for enabled status for UDP packets. Call from main thread
	// only.
	LL_INLINE static bool isSOCKSProxyEnabled()		{ return sUDPProxyEnabled; }

	// Get the UDP proxy address and port. Call from main thread only.
	LL_INLINE LLHost getUDPProxy() const			{ return mUDPProxy; }

	// ########################################################################
	// METHODS THAT DO LOCK mProxyMutex !  DO NOT CALL WHILE mProxyMutex IS
	// LOCKED !
	// ########################################################################
	// Destructor, closes open connections. Do not call directly, use
	// cleanupClass().
	~LLProxy();

	// Delete LLProxy singleton. Allows the apr_socket used in the SOCKS 5
	// control channel to be destroyed before the call to apr_terminate. Call
	// from main thread only.
	static void cleanupClass();

	// Apply the current proxy settings to a curl request. Does not do anything
	// if mHTTPProxyEnabled is false. Safe to call from any thread.
	void applyProxySettings(CURL* handle);

	// Start a connection to the SOCKS 5 proxy. Call from main thread only.
	S32 startSOCKSProxy(LLHost host);

	// Disconnect and clean up any connection to the SOCKS 5 proxy. Call from
	// main thread only.
	void stopSOCKSProxy();

	// Use Password auth when connecting to the SOCKS proxy. Call from main
	// thread only.
	bool setAuthPassword(const std::string& username,
						 const std::string& password);

	// Disable authentication when connecting to the SOCKS proxy. Call from
	// main thread only.
	void setAuthNone();

	// Proxy HTTP packets via httpHost, which can be a SOCKS 5 or a HTTP proxy.
	// as specified in type. Call from main thread only.
	bool enableHTTPProxy(LLHost http_host, LLHttpProxyType type);
	bool enableHTTPProxy();

	// Stop proxying HTTP packets. Call from main thread only.
	void disableHTTPProxy();

private:
	// ########################################################################
	// METHODS THAT DO LOCK mProxyMutex !  DO NOT CALL WHILE mProxyMutex IS
	// LOCKED !
	// ########################################################################

	// Perform a SOCKS 5 authentication and UDP association with the proxy server.
	S32 proxyHandshake(LLHost proxy);

	// Get the currently selected auth method.
	LLSocks5AuthType getSelectedAuthMethod() const;

	// Get the currently selected HTTP proxy type
	LLHttpProxyType getHTTPProxyType() const;

	std::string getSocksPwd() const;
	std::string getSocksUser() const;

private:
	// Is the HTTP proxy enabled ? Safe to read in any thread, but do not write
	// directly. Instead use enableHTTPProxy() and disableHTTPProxy() instead.
	mutable LLAtomicBool	mHTTPProxyEnabled;

	// Mutex to protect shared members in non-main thread calls to
	// applyProxySettings().
	mutable LLMutex 		mProxyMutex;

	// ########################################################################
	// MEMBERS READ AND WRITTEN ONLY IN THE MAIN THREAD. DO NOT SHARE !
	// ########################################################################

	// Is the UDP proxy enabled ?
	static bool				sUDPProxyEnabled;

	// UDP proxy address and port
	LLHost					mUDPProxy;
	// TCP proxy control channel address and port
	LLHost					mTCPProxy;

	// socket handle to proxy TCP control channel
	LLSocket::ptr_t			mProxyControlChannel;

	// ########################################################################
	// MEMBERS WRITTEN IN MAIN THREAD AND READ IN ANY THREAD. ONLY READ OR
	// WRITE AFTER LOCKING mProxyMutex !
	// ########################################################################

	// HTTP proxy address and port
	LLHost					mHTTPProxy;

	// Currently selected HTTP proxy type. Can be web or socks.
	LLHttpProxyType			mProxyType;

	// SOCKS 5 selected authentication method.
	LLSocks5AuthType		mAuthMethodSelected;

	// SOCKS 5 username
	std::string				mSocksUsername;
	// SOCKS 5 password
	std::string				mSocksPassword;
};

#endif
