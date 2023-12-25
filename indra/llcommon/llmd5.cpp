/**
 * @file llmd5.cpp
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

#include "linden_common.h"

#include "llmd5.h"

// How many bytes to grab at a time when checking files
constexpr size_t BLOCK_LEN = 4096;

LLMD5::LLMD5()
{
	init();
}

LLMD5::LLMD5(FILE* file)
{
	init();				// Must be called be all constructors
	update(file);
	finalize();
}

LLMD5::LLMD5(std::istream& stream)
{
	init();				// Must called by all constructors
	update(stream);
	finalize();
}

// Digest a string of the format ("%s:%i" % (s, number))
LLMD5::LLMD5(const unsigned char* string, U32 number)
{
	init();
	update(string, (U32)strlen((const char*)string));

	const char* colon = ":";
	update((unsigned char*)colon, 1);

	char tbuf[MD5RAW_BYTES];
	snprintf(tbuf, sizeof(tbuf), "%i", number);
	update((unsigned char*)tbuf, (U32)strlen(tbuf));
	finalize();
}

// Digest a string
LLMD5::LLMD5(const unsigned char* s)
{
	init();
	update(s, (U32)strlen((const char*)s));
	finalize();
}

void LLMD5::init()
{
	mFinalized = false;	// We just started !

	// Nothing counted, so mCount=0
	mCount[0] = mCount[1] = 0;

	// Load magic initialization constants.
	mState[0] = 0x67452301;
	mState[1] = 0xefcdab89;
	mState[2] = 0x98badcfe;
	mState[3] = 0x10325476;
}

// MD5 block update operation. Continues an MD5 message-digest operation,
// processing another message block, and updating the context.
void LLMD5::update(const unsigned char* input, U32 input_length)
{
	if (mFinalized)
	{
		// So we cannot update !
		llwarns << "Cannot update a finalized digest !" << llendl;
		return;
	}

	// Compute number of bytes mod 64
	U32 buffer_index = (U32)((mCount[0] >> 3) & 0x3F);

	// Update number of bits
	if ((mCount[0] += ((U32)input_length << 3)) < ((U32)input_length << 3))
	{
		++mCount[1];
	}

	mCount[1] += (U32)input_length >> 29;

	U32 input_index = 0;	// So we can buffer the whole input

	U32 buffer_space = 64 - buffer_index;	// How much space is left in buffer
	// Transform as many times as possible.
	if (input_length >= buffer_space)	// Have we enough to fill the buffer ?
	{
		// Fill the rest of the buffer and transform
		memcpy((void*)(mBuffer + buffer_index), (void*)input, buffer_space);
		transform(mBuffer);

		// Now, transform each 64-byte piece of the input, bypassing the buffer
		if (!input || input_length == 0)
		{
			llwarns << "Invalid input" << llendl;
			return;
		}

		for (input_index = buffer_space; input_index + 63 < input_length;
			 input_index += 64)
		{
			transform(input + input_index);
		}

		buffer_index = 0;	// So we can buffer remaining
	}

	// And here we do the buffering:
	memcpy((void*)(mBuffer + buffer_index), (void*)(input + input_index),
		   input_length - input_index);
}

// MD5 update for files. Like above, except that it works on files (and uses
// above as a primitive).
void LLMD5::update(FILE* file)
{
	unsigned char buffer[BLOCK_LEN];
	U32 len;
	while ((len = fread((void*)buffer, 1, BLOCK_LEN, file)))
	{
		update(buffer, len);
	}
	fclose(file);
}

// MD5 update for istreams. Like update for files; see above.
void LLMD5::update(std::istream& stream)
{
	unsigned char buffer[BLOCK_LEN];
	U32 len;
	while (stream.good())
	{
		// Note that return value of read is unusable.
		stream.read((char*)buffer, BLOCK_LEN);
		len = stream.gcount();
		update(buffer, len);
	}
}

void LLMD5::update(const std::string& s)
{
	update((unsigned char*)s.c_str(), s.length());
}

// MD5 finalization. Ends an MD5 message-digest operation, writing the message
// digest and zeroizing the context.
void LLMD5::finalize()
{
	static unsigned char PADDING[64]= {
		0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};

	if (mFinalized)
	{
		llwarns << "Already finalized this digest !" << llendl;
		return;
	}

	// Save number of bits
	unsigned char bits[8];
	encode(bits, mCount, 8);

	// Pad out to 56 mod 64.
	U32 index = (U32)((mCount[0] >> 3) & 0x3f);
	U32 pad_len = index < 56 ? 56 - index : 120 - index;
	update(PADDING, pad_len);

	// Append length (before padding)
	update(bits, 8);

	// Store mState in digest
	encode(mDigest, mState, MD5RAW_BYTES);

	// Zeroize sensitive information
	memset((void*)mBuffer, 0, sizeof(mBuffer));

	mFinalized = true;
}

void LLMD5::raw_digest(unsigned char* s) const
{
	if (!mFinalized)
	{
		llwarns << "Cannot get digest if you have not finalized the digest !"
				<< llendl;
		s[0] = '\0';
		return;
	}

	memcpy((void*)s, (void*)mDigest, MD5RAW_BYTES);
}

void LLMD5::hex_digest(char* s) const
{
	if (!mFinalized)
	{
		llwarns << "Cannot get digest if you have not finalized the digest !"
				<< llendl;
		s[0] = '\0';
		return;
	}

	for (S32 i = 0; i < MD5RAW_BYTES; ++i)
	{
		sprintf(s + i * 2, "%02x", mDigest[i]);
	}

	s[32] = '\0';
}

std::ostream& operator<<(std::ostream& stream, LLMD5 context)
{
	char s[33];
	context.hex_digest(s);
	stream << s;
	return stream;
}

bool operator==(const LLMD5& a, const LLMD5& b)
{
	unsigned char a_guts[MD5RAW_BYTES];
	unsigned char b_guts[MD5RAW_BYTES];
	a.raw_digest(a_guts);
	b.raw_digest(b_guts);
	return memcmp(a_guts, b_guts, MD5RAW_BYTES) == 0;
}

bool operator!=(const LLMD5& a, const LLMD5& b)
{
	unsigned char a_guts[MD5RAW_BYTES];
	unsigned char b_guts[MD5RAW_BYTES];
	a.raw_digest(a_guts);
	b.raw_digest(b_guts);
	return memcmp(a_guts, b_guts, MD5RAW_BYTES) != 0;
}

// Constants for MD5Transform routine.
// Although we could use C++ style constants, defines are actually better,
// since they let us easily evade scope clashes.

#define s11 7
#define s12 12
#define s13 17
#define s14 22
#define s21 5
#define s22 9
#define s23 14
#define s24 20
#define s31 4
#define s32 11
#define s33 16
#define s34 23
#define s41 6
#define s42 10
#define s43 15
#define s44 21

// #defines are faster than inlines. Timing tests prove that this works ~40%
// faster on win with msvc++2k3 over using static inline.

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
Rotation is separate from addition to prevent recomputation.
 */
#define FF(a, b, c, d, x, s, ac) { \
 (a) += F ((b), (c), (d)) + (x) + (U32)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
	}
#define GG(a, b, c, d, x, s, ac) { \
 (a) += G ((b), (c), (d)) + (x) + (U32)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
	}
#define HH(a, b, c, d, x, s, ac) { \
 (a) += H ((b), (c), (d)) + (x) + (U32)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
	}
#define II(a, b, c, d, x, s, ac) { \
 (a) += I ((b), (c), (d)) + (x) + (U32)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
	}

// LLMD5 basic transformation. Transforms mState based on block.
void LLMD5::transform(const unsigned char block[64])
{
	U32 a = mState[0];
	U32 b = mState[1];
	U32 c = mState[2];
	U32 d = mState[3];

	U32 x[MD5RAW_BYTES];
	decode(x, block, 64);

	// Not just a user error, since the method is private
	llassert(!mFinalized);

	// Round 1
	FF(a, b, c, d, x[ 0], s11, 0xd76aa478); /* 1 */
	FF(d, a, b, c, x[ 1], s12, 0xe8c7b756); /* 2 */
	FF(c, d, a, b, x[ 2], s13, 0x242070db); /* 3 */
	FF(b, c, d, a, x[ 3], s14, 0xc1bdceee); /* 4 */
	FF(a, b, c, d, x[ 4], s11, 0xf57c0faf); /* 5 */
	FF(d, a, b, c, x[ 5], s12, 0x4787c62a); /* 6 */
	FF(c, d, a, b, x[ 6], s13, 0xa8304613); /* 7 */
	FF(b, c, d, a, x[ 7], s14, 0xfd469501); /* 8 */
	FF(a, b, c, d, x[ 8], s11, 0x698098d8); /* 9 */
	FF(d, a, b, c, x[ 9], s12, 0x8b44f7af); /* 10 */
	FF(c, d, a, b, x[10], s13, 0xffff5bb1); /* 11 */
	FF(b, c, d, a, x[11], s14, 0x895cd7be); /* 12 */
	FF(a, b, c, d, x[12], s11, 0x6b901122); /* 13 */
	FF(d, a, b, c, x[13], s12, 0xfd987193); /* 14 */
	FF(c, d, a, b, x[14], s13, 0xa679438e); /* 15 */
	FF(b, c, d, a, x[15], s14, 0x49b40821); /* 16 */

	// Round 2
	GG(a, b, c, d, x[ 1], s21, 0xf61e2562); /* 17 */
	GG(d, a, b, c, x[ 6], s22, 0xc040b340); /* 18 */
	GG(c, d, a, b, x[11], s23, 0x265e5a51); /* 19 */
	GG(b, c, d, a, x[ 0], s24, 0xe9b6c7aa); /* 20 */
	GG(a, b, c, d, x[ 5], s21, 0xd62f105d); /* 21 */
	GG(d, a, b, c, x[10], s22,	0x2441453); /* 22 */
	GG(c, d, a, b, x[15], s23, 0xd8a1e681); /* 23 */
	GG(b, c, d, a, x[ 4], s24, 0xe7d3fbc8); /* 24 */
	GG(a, b, c, d, x[ 9], s21, 0x21e1cde6); /* 25 */
	GG(d, a, b, c, x[14], s22, 0xc33707d6); /* 26 */
	GG(c, d, a, b, x[ 3], s23, 0xf4d50d87); /* 27 */
	GG(b, c, d, a, x[ 8], s24, 0x455a14ed); /* 28 */
	GG(a, b, c, d, x[13], s21, 0xa9e3e905); /* 29 */
	GG(d, a, b, c, x[ 2], s22, 0xfcefa3f8); /* 30 */
	GG(c, d, a, b, x[ 7], s23, 0x676f02d9); /* 31 */
	GG(b, c, d, a, x[12], s24, 0x8d2a4c8a); /* 32 */

	// Round 3
	HH(a, b, c, d, x[ 5], s31, 0xfffa3942); /* 33 */
	HH(d, a, b, c, x[ 8], s32, 0x8771f681); /* 34 */
	HH(c, d, a, b, x[11], s33, 0x6d9d6122); /* 35 */
	HH(b, c, d, a, x[14], s34, 0xfde5380c); /* 36 */
	HH(a, b, c, d, x[ 1], s31, 0xa4beea44); /* 37 */
	HH(d, a, b, c, x[ 4], s32, 0x4bdecfa9); /* 38 */
	HH(c, d, a, b, x[ 7], s33, 0xf6bb4b60); /* 39 */
	HH(b, c, d, a, x[10], s34, 0xbebfbc70); /* 40 */
	HH(a, b, c, d, x[13], s31, 0x289b7ec6); /* 41 */
	HH(d, a, b, c, x[ 0], s32, 0xeaa127fa); /* 42 */
	HH(c, d, a, b, x[ 3], s33, 0xd4ef3085); /* 43 */
	HH(b, c, d, a, x[ 6], s34,	0x4881d05); /* 44 */
	HH(a, b, c, d, x[ 9], s31, 0xd9d4d039); /* 45 */
	HH(d, a, b, c, x[12], s32, 0xe6db99e5); /* 46 */
	HH(c, d, a, b, x[15], s33, 0x1fa27cf8); /* 47 */
	HH(b, c, d, a, x[ 2], s34, 0xc4ac5665); /* 48 */

	// Round 4
	II(a, b, c, d, x[ 0], s41, 0xf4292244); /* 49 */
	II(d, a, b, c, x[ 7], s42, 0x432aff97); /* 50 */
	II(c, d, a, b, x[14], s43, 0xab9423a7); /* 51 */
	II(b, c, d, a, x[ 5], s44, 0xfc93a039); /* 52 */
	II(a, b, c, d, x[12], s41, 0x655b59c3); /* 53 */
	II(d, a, b, c, x[ 3], s42, 0x8f0ccc92); /* 54 */
	II(c, d, a, b, x[10], s43, 0xffeff47d); /* 55 */
	II(b, c, d, a, x[ 1], s44, 0x85845dd1); /* 56 */
	II(a, b, c, d, x[ 8], s41, 0x6fa87e4f); /* 57 */
	II(d, a, b, c, x[15], s42, 0xfe2ce6e0); /* 58 */
	II(c, d, a, b, x[ 6], s43, 0xa3014314); /* 59 */
	II(b, c, d, a, x[13], s44, 0x4e0811a1); /* 60 */
	II(a, b, c, d, x[ 4], s41, 0xf7537e82); /* 61 */
	II(d, a, b, c, x[11], s42, 0xbd3af235); /* 62 */
	II(c, d, a, b, x[ 2], s43, 0x2ad7d2bb); /* 63 */
	II(b, c, d, a, x[ 9], s44, 0xeb86d391); /* 64 */

	mState[0] += a;
	mState[1] += b;
	mState[2] += c;
	mState[3] += d;

	// Zeroize sensitive information.
	memset((void*)x, 0, sizeof(x));
}

// Encodes input (U32) into output (unsigned char). Assumes len is a multiple
// of 4.
void LLMD5::encode(unsigned char* output, const U32* input, U32 len)
{
	for (U32 i = 0, j = 0; j < len; ++i, j += 4)
	{
		output[j] = (unsigned char)(input[i] & 0xff);
		output[j + 1] = (unsigned char)((input[i] >> 8) & 0xff);
		output[j + 2] = (unsigned char)((input[i] >> 16) & 0xff);
		output[j + 3] = (unsigned char)((input[i] >> 24) & 0xff);
	}
}

// Decodes input (unsigned char) into output (U32). Assumes len is a multiple
// of 4.
void LLMD5::decode(U32* output, const unsigned char* input, U32 len)
{
	for (U32 i = 0, j = 0; j < len; ++i, j += 4)
	{
		output[i] = ((U32)input[j]) |
					(((U32)input[j + 1]) << 8) |
					(((U32)input[j + 2]) << 16) |
					(((U32)input[j + 3]) << 24);
	}
}
