/**
 * @file llxorcipher.h
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

#ifndef LLXORCIPHER_H
#define LLXORCIPHER_H

#include "llpreprocessor.h"

#include "stdtypes.h"

class LLXORCipher
{
public:
	LLXORCipher(const U8* pad, U32 pad_len);
	LLXORCipher(const std::string& pad);
	LLXORCipher(const LLXORCipher& cipher);

	virtual ~LLXORCipher();

	LLXORCipher& operator=(const LLXORCipher& cipher);

	// Cipher methods
	U32 encrypt(const U8* src, U32 src_len, U8* dst);
	U32 encrypt(const std::string& src, U8* dst);

	LL_INLINE U32 decrypt(const U8* src, U32 src_len, U8* dst)
	{
		// Since XOR is a symetric cipher, just call the encrypt() method.
		return encrypt(src, src_len, dst);
	}

	LL_INLINE U32 decrypt(const std::string& src, U8* dst)
	{
		// Since XOR is a symetric cipher, just call the encrypt() method.
		return encrypt(src, dst);
	}

	// Special syntactic-sugar since xor can be performed in place.
	// *BUG: THIS MEANS THAT THE COMPILER GETS FOOLED ABOUT THE CONSTNESS OF
	// THE INPUT BUFFER: DO MAKE SURE TO COPY THE CONST INPUT STRING INTO THE
	// DESTINATION BEFORE HAND, OR YOUR CONST INPUT SOURCE WILL GET CORRUPTED !
	// *TODO: change to fix the above bug.
	LL_INLINE U32 encrypt(U8* buf, U32 len)
	{
		return encrypt((const U8*)buf, len, buf);
	}

	LL_INLINE U32 decrypt(U8* buf, U32 len)
	{
		return encrypt((const U8*)buf, len, buf);
	}

	LL_INLINE U32 encrypt(std::string& src)
	{
		return encrypt(src, (U8*)src.data());
	}

	LL_INLINE U32 decrypt(std::string& src)
	{
		return encrypt(src, (U8*)src.data());
	}

protected:
	void init(const U8* pad, U32 pad_len);

protected:
	U8* mPad;
	U8* mHead;
	U32 mPadLen;
};

#endif
