/**
 * @file llnet.cpp
 * @brief OS-specific implementation of cross-platform utility functions.
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

#include <stdexcept>

#if !LL_WINDOWS
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <fcntl.h>
# include <unistd.h>
# include <errno.h>
#endif

#include "llnet.h"

#include "indra_constants.h"

// Globals
#if LL_WINDOWS

SOCKADDR_IN stDstAddr;
SOCKADDR_IN stSrcAddr;
SOCKADDR_IN stLclAddr;
static WSADATA stWSAData;

#else

struct sockaddr_in stDstAddr;
struct sockaddr_in stSrcAddr;
struct sockaddr_in stLclAddr;

# if LL_DARWIN && !defined(_SOCKLEN_T)
#  define _SOCKLEN_T
typedef int socklen_t;
# endif

#endif

// Address to which datagram was sent:
static U32 gsnReceivingIFAddr = INVALID_HOST_IP_ADDRESS;

#if LL_DARWIN
// Mac OS X returns an error when trying to set these to 400000. Smaller values
// succeed.
const int	SEND_BUFFER_SIZE	= 200000;
const int	RECEIVE_BUFFER_SIZE	= 200000;
#else // LL_DARWIN
const int	SEND_BUFFER_SIZE	= 400000;
const int	RECEIVE_BUFFER_SIZE	= 400000;
#endif // LL_DARWIN

// Universal functions (cross-platform)

LLHost get_sender()
{
	return LLHost(stSrcAddr.sin_addr.s_addr, ntohs(stSrcAddr.sin_port));
}

U32 get_sender_ip()
{
	return stSrcAddr.sin_addr.s_addr;
}

U32 get_sender_port()
{
	return ntohs(stSrcAddr.sin_port);
}

LLHost get_receiving_interface()
{
	return LLHost(gsnReceivingIFAddr, INVALID_PORT);
}

U32 get_receiving_interface_ip()
{
	return gsnReceivingIFAddr;
}

#if LL_WINDOWS

///////////////////////////////////////////////////////////////////////////////
// Windows Versions
///////////////////////////////////////////////////////////////////////////////

S32 start_net(S32& socket_out, int& port_num)
{
	// Create socket, make non-blocking
    // Init WinSock
	int nret;

	// Initialize windows specific stuff
	if (WSAStartup(0x0202, &stWSAData))
	{
		S32 err = WSAGetLastError();
		llwarns << "Windows sockets initialization failed, with error "
				<< err << llendl;
		WSACleanup();
		return 1;
	}

	// Get a datagram socket
    int sock_num = (int)socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_num == INVALID_SOCKET)
	{
		S32 err = WSAGetLastError();
		llwarns << "socket() failedwith error " << err << llendl;
		WSACleanup();
		return 2;
	}

	// Name the socket (assign the local port number to receive on)
	stLclAddr.sin_family = AF_INET;
	stLclAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	stLclAddr.sin_port = htons(port_num);

	S32 attempt_port = port_num;
	LL_DEBUGS("AppInit") << "Attempting to connect on port " << attempt_port
						 << LL_ENDL;
	nret = bind(sock_num, (struct sockaddr*) &stLclAddr, sizeof(stLclAddr));

	if (nret == SOCKET_ERROR)
	{
		// If we got an address in use error...
		if (WSAGetLastError() == WSAEADDRINUSE)
		{
			// Try all ports from PORT_DISCOVERY_RANGE_MIN to
			// PORT_DISCOVERY_RANGE_MAX
			for (attempt_port = PORT_DISCOVERY_RANGE_MIN;
				 attempt_port <= PORT_DISCOVERY_RANGE_MAX; ++attempt_port)
			{
				stLclAddr.sin_port = htons(attempt_port);
				LL_DEBUGS("AppInit") << "Trying port " << attempt_port
									 << LL_ENDL;
				nret = bind(sock_num, (struct sockaddr*) &stLclAddr,
							sizeof(stLclAddr));

				if (!(nret == SOCKET_ERROR &&
					WSAGetLastError() == WSAEADDRINUSE))
				{
					break;
				}
			}

			if (nret == SOCKET_ERROR)
			{
				llwarns << "Network port " << port_num << " not available."
						<< llendl;
				WSACleanup();
				return 3;
			}
		}
		else	// Some other socket error
		{
			S32 err = WSAGetLastError();
			llwarns << "bind() to port " << port_num << " failed with error: "
					<< err << llendl;
			WSACleanup();
			return 4;
		}
	}

	sockaddr_in socket_address;
	S32 socket_address_size = sizeof(socket_address);
	getsockname(sock_num, (SOCKADDR*)&socket_address, &socket_address_size);
	attempt_port = ntohs(socket_address.sin_port);

	llinfos << "Connected on port " << attempt_port << llendl;
	port_num = attempt_port;

	// Set socket to be non-blocking
	unsigned long argp = 1;
	nret = ioctlsocket(sock_num, FIONBIO, &argp);
	if (nret == SOCKET_ERROR)
	{
		S32 err = WSAGetLastError();
		llwarns << "Failed to set socket non-blocking with error: " << err
				<< llendl;
	}

	// Set a large receive buffer
	int rec_size = RECEIVE_BUFFER_SIZE;
	int buff_size = 4;
	nret = setsockopt(sock_num, SOL_SOCKET, SO_RCVBUF, (char*)&rec_size,
					  buff_size);
	if (nret)
	{
		llinfos << "Cannot set receive buffer size !" << llendl;
	}

	int snd_size = SEND_BUFFER_SIZE;
	nret = setsockopt(sock_num, SOL_SOCKET, SO_SNDBUF, (char*)&snd_size,
					  buff_size);
	if (nret)
	{
		llinfos << "Cannot set send buffer size !" << llendl;
	}

	getsockopt(sock_num, SOL_SOCKET, SO_RCVBUF, (char*)&rec_size, &buff_size);
	getsockopt(sock_num, SOL_SOCKET, SO_SNDBUF, (char*)&snd_size, &buff_size);

	llinfos << "Receive buffer size: " << rec_size << " - Send buffer size: "
			<< snd_size << llendl;

	//  Setup a destination address
	stDstAddr.sin_family = AF_INET;
    stDstAddr.sin_addr.s_addr = INVALID_HOST_IP_ADDRESS;
    stDstAddr.sin_port = htons(port_num);
	socket_out = sock_num;

	return 0;
}

void end_net(S32& socket_out)
{
	if (socket_out >= 0)
	{
		shutdown(socket_out, SD_BOTH);
		closesocket(socket_out);
	}
	WSACleanup();
}

// Receives data asynchronously from the socket set by initNet(). Returns the
// number of bytes received into dataReceived, or zero if there is no data
// received.
S32 receive_packet(int sock_num, char* recv_buffer)
{
	int nret;
	int addr_size = sizeof(struct sockaddr_in);

	nret = recvfrom(sock_num, recv_buffer, NET_BUFFER_SIZE, 0,
					(struct sockaddr*)&stSrcAddr, &addr_size);
	if (nret == SOCKET_ERROR)
	{
		if (WSAEWOULDBLOCK == WSAGetLastError() ||
			WSAECONNRESET == WSAGetLastError())
		{
			return 0;
		}
		llinfos << "Failed with error: " << WSAGetLastError() << llendl;
	}

	return nret;
}

// Sends a packet to the address set in initNet. Returns true on success.
bool send_packet(int sock_num, const char* send_buffer, int size,
				 U32 recipient, int port_num)
{
	int nret = 0;
	U32 last_error = 0;

	stDstAddr.sin_addr.s_addr = recipient;
	stDstAddr.sin_port = htons(port_num);
	do
	{
		nret = sendto(sock_num, send_buffer, size, 0,
					  (struct sockaddr*)&stDstAddr, sizeof(stDstAddr));
		if (nret == SOCKET_ERROR)
		{
			last_error = WSAGetLastError();
			if (last_error != WSAEWOULDBLOCK)
			{
				// WSAECONNRESET - I think this is caused by an ICMP
				// "connection refused" message being sent back from a Linux
				// box... I'm not finding helpful documentation or web pages on
				// this. The question is whether the packet actually got sent
				// or not. Based on the structure of this code, I would assume
				// it is. JNC 2002.01.18
				if (WSAECONNRESET == WSAGetLastError())
				{
					return true;
				}
				llinfos << "sendto() failed to " << u32_to_ip_string(recipient)
						<< ":" << port_num << " - Error: " << last_error
						<< llendl;
			}
		}
	}
	while (nret == SOCKET_ERROR && last_error == WSAEWOULDBLOCK);

	return nret != SOCKET_ERROR;
}

#else

///////////////////////////////////////////////////////////////////////////////
// Linux Versions
///////////////////////////////////////////////////////////////////////////////

//  Create socket, make non-blocking
S32 start_net(S32& socket_out, int& port_num)
{
	int nret;

	//  Create socket
    int sock_num = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_num < 0)
	{
		llwarns << "socket() failed" << llendl;
		return 1;
	}

	if (port_num == NET_USE_OS_ASSIGNED_PORT)
	{
		// Although bind is not required it will tell us which port we were
		// assigned to.
		stLclAddr.sin_family      = AF_INET;
		stLclAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		stLclAddr.sin_port        = htons(0);
		llinfos << "attempting to connect on OS assigned port" << llendl;
		nret = bind(sock_num, (struct sockaddr*) &stLclAddr,
					sizeof(stLclAddr));
		if (nret < 0)
		{
			llwarns << "Failed to bind on an OS assigned port error: "
					<< nret << llendl;
		}
		else
		{
			sockaddr_in socket_info;
			socklen_t len = sizeof(sockaddr_in);
			int err = getsockname(sock_num, (sockaddr*)&socket_info, &len);
			llinfos << "Get socket returned: " << err << " length " << len
					<< llendl;
			port_num = ntohs(socket_info.sin_port);
			llinfos << "Assigned port: " << port_num << llendl;
		}
	}
	else
	{
	    // Name the socket (assign the local port number to receive on)
		stLclAddr.sin_family      = AF_INET;
		stLclAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		stLclAddr.sin_port        = htons(port_num);
		U32 attempt_port = port_num;
		llinfos << "Attempting to connect on port " << attempt_port << llendl;

		nret = bind(sock_num, (struct sockaddr*)&stLclAddr, sizeof(stLclAddr));
		if (nret < 0)
		{
			// If we got an address in use error...
			if (errno == EADDRINUSE)
			{
				// Try all ports from PORT_DISCOVERY_RANGE_MIN to
				// PORT_DISCOVERY_RANGE_MAX
				for (attempt_port = PORT_DISCOVERY_RANGE_MIN;
					attempt_port <= PORT_DISCOVERY_RANGE_MAX;
					attempt_port++)
				{
					stLclAddr.sin_port = htons(attempt_port);
					llinfos << "trying port " << attempt_port << llendl;
					nret = bind(sock_num, (struct sockaddr*)&stLclAddr,
								sizeof(stLclAddr));
					if (!((nret < 0) && (errno == EADDRINUSE)))
					{
						break;
					}
				}
				if (nret < 0)
				{
					llwarns << "Network port " << port_num << " not available."
							<< llendl;
					close(sock_num);
					return 3;
				}
			}
			// Some other socket error
			else
			{
				llwarns << "bind() to port " << port_num
						<< " failed with error: " << strerror(errno) << llendl;
				close(sock_num);
				return 4;
			}
		}
		llinfos << "Connected on port " << attempt_port << llendl;
		port_num = attempt_port;
	}
	// Set socket to be non-blocking
 	fcntl(sock_num, F_SETFL, O_NONBLOCK);
	// Set a large receive buffer
	int rec_size = RECEIVE_BUFFER_SIZE;
	socklen_t buff_size = 4;
	nret = setsockopt(sock_num, SOL_SOCKET, SO_RCVBUF, (char*)&rec_size,
					  buff_size);
	if (nret)
	{
		llinfos << "Cannot set receive size !" << llendl;
	}
	int snd_size = SEND_BUFFER_SIZE;
	nret = setsockopt(sock_num, SOL_SOCKET, SO_SNDBUF, (char*)&snd_size,
					  buff_size);
	if (nret)
	{
		llinfos << "Cannot set send size !" << llendl;
	}
	getsockopt(sock_num, SOL_SOCKET, SO_RCVBUF, (char*)&rec_size, &buff_size);
	getsockopt(sock_num, SOL_SOCKET, SO_SNDBUF, (char*)&snd_size, &buff_size);

	llinfos << "Receive buffer size: " << rec_size << " - Send buffer size: "
			<< snd_size << llendl;

#if LL_LINUX
	// Turn on recipient address tracking
	{
		int use_pktinfo = 1;
		if (setsockopt(sock_num, SOL_IP, IP_PKTINFO, &use_pktinfo,
					   sizeof(use_pktinfo)) == -1)
		{
			llwarns << "No IP_PKTINFO available" << llendl;
		}
		else
		{
			llinfos << "IP_PKKTINFO enabled" << llendl;
		}
	}
#endif

	//  Setup a destination address
	char achMCAddr[MAXADDRSTR] = "127.0.0.1";
	stDstAddr.sin_family = AF_INET;
	stDstAddr.sin_addr.s_addr = ip_string_to_u32(achMCAddr);
	stDstAddr.sin_port = htons(port_num);
	socket_out = sock_num;

	return 0;
}

void end_net(S32& socket_out)
{
	if (socket_out >= 0)
	{
		close(socket_out);
	}
}

#if LL_LINUX
static int recvfrom_destip(int sock_num, void* buf, int len,
						   struct sockaddr* from, socklen_t* fromlen,
						   U32* dstip)
{
	int size;
	struct iovec iov[1];
	char cmsg[CMSG_SPACE(sizeof(struct in_pktinfo))];
	struct cmsghdr* cmsgptr;
	struct msghdr msg = { 0 };

	iov[0].iov_base = buf;
	iov[0].iov_len = len;

	memset(&msg, 0, sizeof msg);
	msg.msg_name = from;
	msg.msg_namelen = *fromlen;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_control = &cmsg;
	msg.msg_controllen = sizeof(cmsg);

	size = recvmsg(sock_num, &msg, 0);

	if (size == -1)
	{
		return -1;
	}

	for (cmsgptr = CMSG_FIRSTHDR(&msg); cmsgptr != NULL;
		 cmsgptr = CMSG_NXTHDR(&msg, cmsgptr))
	{
		if (cmsgptr->cmsg_level == SOL_IP && cmsgptr->cmsg_type == IP_PKTINFO)
		{
			in_pktinfo* pktinfo = (in_pktinfo*)CMSG_DATA(cmsgptr);
			if (pktinfo)
			{
				// Two choices. routed and specified. ipi_addr is routed,
				// ipi_spec_dst is routed. We should stay with specified until
				// we go to multiple interfaces
				*dstip = pktinfo->ipi_spec_dst.s_addr;
			}
		}
	}

	return size;
}
#endif

// Receives data asynchronously from the socket set by initNet(). Returns the
// number of bytes received into dataReceived, or zero if there is no data
// received.
int receive_packet(int sock_num, char* recv_buffer)
{
	int nret;
	socklen_t addr_size = sizeof(struct sockaddr_in);

	gsnReceivingIFAddr = INVALID_HOST_IP_ADDRESS;

#if LL_LINUX
	nret = recvfrom_destip(sock_num, recv_buffer, NET_BUFFER_SIZE,
						   (struct sockaddr*)&stSrcAddr, &addr_size,
						   &gsnReceivingIFAddr);
#else
	int recv_flags = 0;
	nret = recvfrom(sock_num, recv_buffer, NET_BUFFER_SIZE, recv_flags,
					(struct sockaddr*)&stSrcAddr, &addr_size);
#endif

	if (nret == -1)
	{
		// To maintain consistency with the Windows implementation, return a
		// zero for size on error.
		return 0;
	}

	return nret;
}

bool send_packet(int sock_num, const char* send_buffer, int size,
				 U32 recipient, int port_num)
{
	stDstAddr.sin_addr.s_addr = recipient;
	stDstAddr.sin_port = htons(port_num);

	bool success = false;
	S32 send_attempts = 0;
	while (true)
	{
		if (++send_attempts > 3)
		{
			llinfos << "Bailing out of send after 3 failed attempts" << llendl;
			break;
		}

		success = sendto(sock_num, send_buffer, size, 0,
						 (struct sockaddr*)&stDstAddr, sizeof(stDstAddr)) >= 0;
		if (success)
		{
			break;
		}

		// send failed, check to see if we should resend
		if (errno == EAGAIN)
		{
			// Say nothing, just repeat send
			llinfos << "sendto() reported buffer full, resending (attempt "
					<< send_attempts << ") to "
					<< inet_ntoa(stDstAddr.sin_addr) << ":" << port_num
					<< llendl;
		}
		else if (errno == ECONNREFUSED)
		{
			// Response to ICMP connection refused message on earlier send
			llinfos << "sendto() reported connection refused, resending (attempt "
					<< send_attempts << ") to "
					<< inet_ntoa(stDstAddr.sin_addr) << ":" << port_num
					<< llendl;
		}
		else
		{
			// Some other error, abort !
			llinfos << "sendto() failed: " << errno << ", "
					<< strerror(errno) << ". Aborted sending to "
					<< inet_ntoa(stDstAddr.sin_addr) << ":" << port_num
					<< llendl;
			break;
		}
	}

	return success;
}

#endif
