/**
 * @file llbase64.cpp
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * See below for the Apache license applicable to part of the code.
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

#include "linden_common.h"

#include "llbase64.h"

///////////////////////////////////////////////////////////////////////////////
// This is the APR-util libary code for apr_base64_*() functions, slightly
// ammended by me (HB). Since these functions are the only ones we have ever
// used in the viewer code, there is no point in using the APR-util library
// just for them. This part of the code is therefore under the Apache license:
//
// Licensed to the Apache Software Foundation (ASF) under one or more
// contributor license agreements.  See the NOTICE file distributed with
// this work for additional information regarding copyright ownership.
// The ASF licenses this file to You under the Apache License, Version 2.0
// (the "License"); you may not use this file except in compliance with
// the License.  You may obtain a copy of the License at
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

static const unsigned char pr2six[256] =
{
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
	52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
	64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
	64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
	41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
};

static const char basis_64[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

//static
size_t LLBase64::decodeLen(const char* input)
{
	const unsigned char* bin = (const unsigned char*)input;
	while (pr2six[*(bin++)] <= 63) ;

	size_t nprbytes = (bin - (const unsigned char*)input) - 1;
	return ((nprbytes + 3) / 4) * 3 + 1;
}

//static
size_t LLBase64::decode(unsigned char* output, const char* input)
{
	const unsigned char* bin = (const unsigned char*)input;
	while (pr2six[*(bin++)] <= 63) ;

	size_t nprbytes = (bin - (const unsigned char*)input) - 1;
	size_t nbytesdecoded = ((nprbytes + 3) / 4) * 3;

	unsigned char* bout = (unsigned char*)output;
	bin = (const unsigned char*)input;

	while (nprbytes > 4)
	{
		*(bout++) = (unsigned char)(pr2six[*bin] << 2 | pr2six[bin[1]] >> 4);
		*(bout++) = (unsigned char)(pr2six[bin[1]] << 4 | pr2six[bin[2]] >> 2);
		*(bout++) = (unsigned char)(pr2six[bin[2]] << 6 | pr2six[bin[3]]);
		bin += 4;
		nprbytes -= 4;
	}

	// Note: (nprbytes == 1) would be an error, so just ingore that case.
	if (nprbytes > 1)
	{
		*(bout++) = (unsigned char)(pr2six[*bin] << 2 | pr2six[bin[1]] >> 4);
	}
	if (nprbytes > 2)
	{
		*(bout++) = (unsigned char)(pr2six[bin[1]] << 4 | pr2six[bin[2]] >> 2);
	}
	if (nprbytes > 3)
	{
		*(bout++) = (unsigned char)(pr2six[bin[2]] << 6 | pr2six[bin[3]]);
	}

	nbytesdecoded -= (4 - nprbytes) & 3;
	return nbytesdecoded;
}

//static
size_t LLBase64::encode(char* output, const unsigned char* input, size_t len)
{
	size_t i;
	char* p = output;
	for (i = 0; i + 2 < len; i += 3)
	{
		*p++ = basis_64[(input[i] >> 2) & 0x3F];
		*p++ = basis_64[((input[i] & 0x3) << 4) |
			   ((int)(input[i + 1] & 0xF0) >> 4)];
		*p++ = basis_64[((input[i + 1] & 0xF) << 2) |
			   ((int)(input[i + 2] & 0xC0) >> 6)];
		*p++ = basis_64[input[i + 2] & 0x3F];
	}
	if (i < len)
	{
		*p++ = basis_64[(input[i] >> 2) & 0x3F];
		if (i == (len - 1))
		{
			*p++ = basis_64[((input[i] & 0x3) << 4)];
			*p++ = '=';
		}
		else
		{
			*p++ = basis_64[((input[i] & 0x3) << 4) |
				   ((int)(input[i + 1] & 0xF0) >> 4)];
			*p++ = basis_64[((input[i + 1] & 0xF) << 2)];
		}
		*p++ = '=';
	}
	*p++ = '\0';
	return p - output;
}

///////////////////////////////////////////////////////////////////////////////
// GPL-licensed part

//static
std::string LLBase64::encode(const char* input, size_t input_size)
{
	std::string output;
	if (input && input_size > 0)
	{
		// Estimated (maximum) length, trailing nul char included
		size_t b64_buffer_length = encodeLen(input_size);
		char* b64_buffer = new char[b64_buffer_length];
		encode(b64_buffer, (const unsigned char*)input, input_size);
		output.assign(b64_buffer); // Note: buffer is nul-terminated
		delete[] b64_buffer;
	}
	return output;
}

//static
std::string LLBase64::decode(const char* input)
{
	std::string output;
	if (input)
	{
		// Estimated (maximum) length
		size_t b64_buffer_length = decodeLen(input);
		unsigned char* b64_buffer = new unsigned char[b64_buffer_length];
		// Actual length
		b64_buffer_length = decode(b64_buffer, input);
		output.assign((char*)b64_buffer, b64_buffer_length);
		delete[] b64_buffer;
	}
	return output;
}
