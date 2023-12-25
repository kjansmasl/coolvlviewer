/**
 * @file llnet.h
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

#ifndef LL_NET_H
#define LL_NET_H

#include "llhost.h"

// Returns 0 on success, non-zero on error. Sets socket handler/descriptor,
// changes port_num if port requested is unavailable.
S32 start_net(S32& socket_out, int& port_num);

void end_net(S32& socket_out);

// Returns size of packet or 0 in case of error
S32 receive_packet(int sock_num, char* recv_buffer);

// Returns true on success.
bool send_packet(int sock_num, const char* send_buffer, int size,
				 U32 recipient, int port_num);

LLHost get_sender();
U32 get_sender_port();
U32 get_sender_ip();
LLHost get_receiving_interface();
U32 get_receiving_interface_ip();

#endif
