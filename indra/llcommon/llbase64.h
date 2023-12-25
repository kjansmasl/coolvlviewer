/**
 * @file llbase64.h
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * See llbase64.cpp for the Apache license applicable to part of the code.
 *
 * Copyright (c) 2004 Apache Foundation (for parts borrowed from APR-util),
 *           (c) 2007-2009, Linden Research, Inc.
 *           (c) 2009-2023, Henri Beauchamp.
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

#ifndef LL_LLBASE64_H
#define LL_LLBASE64_H

#include <string>

#include "llpreprocessor.h"

// Purely static class
class LLBase64
{
	LLBase64() = delete;
	~LLBase64() = delete;

public:
	static std::string encode(const char* input, size_t input_size);

	LL_INLINE static std::string encode(const std::string& input)
	{
		return encode(input.c_str(), input.size());
	}

	static std::string decode(const char* input);

	LL_INLINE static std::string decode(const std::string& input)
	{
		return decode(input.c_str());
	}

	// Low-level API, derived from APR-util's code (see llbase64.cpp for the
	// Apache license applicable to this code). Only used in lldserialize.cpp
	// (keep it that way, please and use instead the above methods for any new
	// code needing base64 coding, since the underlying code may change in the
	// future for a better/faster implementation). HB

	// Returns an estimation of the required maximum buffer size for encoding.
	LL_INLINE static size_t encodeLen(size_t len)
	{
		return ((len + 2) / 3 * 4) + 1;
	}

	// Returns an estimation of the required maximum buffer size for decoding.
	static size_t decodeLen(const char* input);

	static size_t decode(unsigned char* output, const char* input);
	static size_t encode(char* output, const unsigned char* input, size_t len);
};

#endif	// LL_LLBASE64_H
