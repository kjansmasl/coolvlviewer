/**
 * @file llmd5.h
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.	Terms of
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
 *
 * llMD5.CC - source code for the C++/object oriented translation and
 *					modification of MD5.
 *
 * Adapted to Linden Lab by Frank Filipanits, 6/25/2002
 * Fixed potential memory leak, James Cook, 6/27/2002
 *
 * Translation and modification (c) 1995 by Mordechai T. Abzug
 *
 * This translation/ modification is provided "as is," without express or
 * implied warranty of any kind.
 *
 * The translator/ modifier does not claim (1) that MD5 will do what you think
 * it does; (2) that this translation/ modification is accurate; or (3) that
 * this software is "merchantible."	(Language for this disclaimer partially
 * copied from the disclaimer below).
 *
 * Based on:
 *
 * MD5C.C - RSA Data Security, Inc., MD5 message-digest algorithm MDDRIVER.C -
 * test driver for MD2, MD4 and MD5
 *
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All rights
 * reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 * 
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 * 
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 * 
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

#ifndef LL_LLMD5_H
#define LL_LLMD5_H

#include "llerror.h"

// Use for the raw digest output
#define MD5RAW_BYTES 16

// Use for hex digest output
#define MD5HEX_STR_SIZE 33			// Message system fixed size with nul
#define MD5HEX_STR_BYTES 32			// Message system fixed size

class LLMD5
{
	friend std::ostream& operator<<(std::ostream&, LLMD5);

protected:
	LOG_CLASS(LLMD5);

public:
	LLMD5();

	void update(const unsigned char* input, U32 input_length);
	void update(std::istream& stream);
	void update(FILE* file);
	void update(const std::string& str);
	void finalize();

	// Constructors for special circumstances. All these constructors finalize
	// the MD5 context.
	LLMD5(const unsigned char* string);	// Digest string, finalize
	LLMD5(std::istream& stream);		// Digest stream, finalize
	LLMD5(FILE* file);					// Digest file, close, finalize
	LLMD5(const unsigned char* string, U32 number);

	// Methods to acquire finalized result:

	// Needs a 16 bytes array for binary data
	void raw_digest(unsigned char* array) const;
	// Needs a 33 bytes array for ASCII-hex string
	void hex_digest(char* string) const;

private:
	void init();	 // Called by all constructors

	// Does the real update work. Note that length is implied to be 64.
	void transform(const unsigned char* buffer);

	static void encode(unsigned char* dest, const U32* src, U32 length);
	static void decode(U32* dest, const unsigned char* src, U32 length);

private:
	U32				mState[4];
	U32				mCount[2];				// Number of *bits*, mod 2^64
	unsigned char	mBuffer[64];			// Input buffer
	unsigned char	mDigest[MD5RAW_BYTES];
	bool			mFinalized;
};

bool operator==(const LLMD5& a, const LLMD5& b);
bool operator!=(const LLMD5& a, const LLMD5& b);

#endif // LL_LLMD5_H
