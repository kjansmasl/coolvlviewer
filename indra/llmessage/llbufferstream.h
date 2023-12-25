/**
 * @file llbufferstream.h
 * @author Phoenix
 * @date 2005-10-10
 * @brief Classes to treat an LLBufferArray as a c++ iostream.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLBUFFERSTREAM_H
#define LL_LLBUFFERSTREAM_H

#include <iosfwd>
#include <iostream>

#include "llbuffer.h"

/**
 * @class LLBufferStreamBuf
 * @brief This implements the buffer wrapper for an istream
 *
 * The buffer array passed in is not owned by the stream buf object.
 */
class LLBufferStreamBuf final : public std::streambuf
{
public:
	LLBufferStreamBuf(const LLChannelDescriptors& channels,
					  LLBufferArray* buffer);
	virtual ~LLBufferStreamBuf();

protected:
	typedef std::streambuf::pos_type pos_type;
	typedef std::streambuf::off_type off_type;

	// streambuf vrtual implementations

	// Called when we hit the end of input. Returns the character at the
	// current position or EOF.
	int underflow() override;

	// Called when we hit the end of output
	// - c: the character to store at the current put position
	// Returns EOF if the function failed. Any other value on success.
	int overflow(int c) override;

	// Synchronizes the buffer.Returns 0 on success or -1 on failure.
	int sync() override;

	// Seeks to an offset position in a stream.
	// - off: offset value relative to way paramter
	// - way: the seek direction. One of ios::beg, ios::cur, and ios::end.
	// - which: which pointer to modify. One of ios::in, ios::out, or both
    //   masked together.
	// Returns the new position or an invalid position on failure.
	pos_type seekoff(off_type off, std::ios::seekdir way,
					 std::ios::openmode which) override;

protected:
	// This channels we are working on.
	LLChannelDescriptors mChannels;

	// The buffer we work on
	LLBufferArray* mBuffer;
};

/**
 * @class LLBufferStream
 * @brief This implements an istream based wrapper around an LLBufferArray.
 *
 * This class does not own the buffer array, and does not hold a shared pointer
 * to it. Since the class itself is fairly ligthweight, just make one on the
 * stack when needed and let it fall out of scope.
 */
class LLBufferStream final : public std::iostream
{
public:
	LLBufferStream(const LLChannelDescriptors& channels,
				   LLBufferArray* buffer);

protected:
	LLBufferStreamBuf mStreamBuf;
};

#endif // LL_LLBUFFERSTREAM_H
