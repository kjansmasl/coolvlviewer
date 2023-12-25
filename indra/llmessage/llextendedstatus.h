/** 
 * @file llextendedstatus.h
 * @date August 2007
 * @brief Extended status codes for curl/resident asset storage & delivery
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_LLEXTENDEDSTATUS_H
#define LL_LLEXTENDEDSTATUS_H

enum class LLExtStat : U32
{
	// Status provider groups. Top bits indicate which status type it is
	// Zero is common status code (next section).
	CURL_RESULT	= 1UL << 30,		// Serviced by curl
	RES_RESULT	= 2UL << 30,		// Serviced by resident copy
	CACHE_RESULT	= 3UL << 30,	// Serviced by cache

	// Common status codes
	NONE				= 0x00000,	// No extra info here, sorry !
	NULL_UUID			= 0x10001,	// Null asset ID
	NO_UPSTREAM			= 0x10002,	// Attempt to upload without valid upstream
	REQUEST_DROPPED		= 0x10003,	// Request was dropped unserviced
	NONEXISTENT_FILE	= 0x10004,	// Tried to upload non existent file
	BLOCKED_FILE		= 0x10005,	// Tried to upload a file we cannot open

	// Curl status codes:
	// Mask off CURL_RESULT for original result and
	// See: include/curl/curl.h
	
	// Cache status codes:
	CACHE_CACHED		= CACHE_RESULT | 0x0001,
	CACHE_CORRUPT		= CACHE_RESULT | 0x0002,
};

#endif // LL_LLEXTENDEDSTATUS_H
