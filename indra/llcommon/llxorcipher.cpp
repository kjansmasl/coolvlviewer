/**
 * @file llxorcipher.cpp
 * @brief Implementation of LLXORCipher
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

#include "llxorcipher.h"

LLXORCipher::LLXORCipher(const U8* pad, U32 pad_len)
:	mPad(NULL),
	mHead(NULL),
	mPadLen(0)
{
	init(pad, pad_len);
}

LLXORCipher::LLXORCipher(const std::string& pad)
:	mPad(NULL),
	mHead(NULL),
	mPadLen(0)
{
	init((const U8*)pad.data(), (U32)pad.size());
}

LLXORCipher::LLXORCipher(const LLXORCipher& cipher)
:	mPad(NULL),
	mHead(NULL),
	mPadLen(0)
{
	init(cipher.mPad, cipher.mPadLen);
}

LLXORCipher::~LLXORCipher()
{
	if (mPad)
	{
		delete[] mPad;
		mPad = NULL;
		mPadLen = 0;
	}
}

void LLXORCipher::init(const U8* pad, U32 pad_len)
{
	if (mPad)
	{
		delete[] mPad;
		mPad = NULL;
		mPadLen = 0;
	}
	if (pad && pad_len)
	{
		mPadLen = pad_len;
		mPad = new U8[mPadLen];
		if (mPad != NULL)
		{
			memcpy(mPad, pad, mPadLen);
		}
	}
	mHead = mPad;
}

LLXORCipher& LLXORCipher::operator=(const LLXORCipher& cipher)
{
	if (this == &cipher) return *this;
	init(cipher.mPad, cipher.mPadLen);
	return *this;
}

U32 LLXORCipher::encrypt(const U8* src, U32 src_len, U8* dst)
{
	if (!src || !src_len || !dst || !mPad)
	{
		return 0;
	}

	U8* pad_end = mPad + mPadLen;
	U32 count = src_len;
	while (count--)
	{
		*dst++ = *src++ ^ *mHead++;
		if (mHead >= pad_end)
		{
			mHead = mPad;
		}
	}
	return src_len;
}

U32 LLXORCipher::encrypt(const std::string& src, U8* dst)
{
	if (src.empty() || !dst || !mPad)
	{
		return 0;
	}

	U8* pad_end = mPad + mPadLen;
	U32 count = src.size();
	for (U32 i = 0; i < count; ++i)
	{
		*dst++ = U8(src[i]) ^ *mHead++;
		if (mHead >= pad_end)
		{
			mHead = mPad;
		}
	}
	return count;
}
