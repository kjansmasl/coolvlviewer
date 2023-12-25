/**
 * @file lldatapacker.cpp
 * @brief Data packer implementation.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "lldatapacker.h"

#include "llcolor4.h"
#include "llcolor4u.h"
#include "llmessage.h"
#include "llvector2.h"
#include "llvector3.h"
#include "llvector4.h"

// *NOTE: there are functions below which use sscanf and rely on this
// particular value of DP_BUFSIZE. Search for '511' (DP_BUFSIZE - 1) to find
// them if you change this number.
constexpr S32 DP_BUFSIZE = 512;

static char DUMMY_BUFFER[128];

LLDataPacker::LLDataPacker()
:	mPassFlags(0),
	mWriteEnabled(false)
{
}

//virtual
void LLDataPacker::reset()
{
	llerrs << "Using unimplemented datapacker reset !" << llendl;
}

//virtual
void LLDataPacker::dumpBufferToLog()
{
	llerrs << "Not implemented for this type !" << llendl;
}

bool LLDataPacker::packFixed(F32 value, const char* name, bool is_signed,
							 U32 int_bits, U32 frac_bits)
{
	bool success = true;

	S32 unsigned_bits = int_bits + frac_bits;
	S32 total_bits = unsigned_bits;

	if (is_signed)
	{
		++total_bits;
	}

	S32 min_val;
	U32 max_val;
	if (is_signed)
	{
		min_val = 1 << int_bits;
		min_val *= -1;
	}
	else
	{
		min_val = 0;
	}
	max_val = 1 << int_bits;

	// Clamp to be within range
	F32 fixed_val = llclamp(value, (F32)min_val, (F32)max_val);
	if (is_signed)
	{
		fixed_val += max_val;
	}
	fixed_val *= 1 << frac_bits;

	if (total_bits <= 8)
	{
		success = packU8((U8)fixed_val, name);
	}
	else if (total_bits <= 16)
	{
		success = packU16((U16)fixed_val, name);
	}
	else if (total_bits <= 31)
	{
		success = packU32((U32)fixed_val, name);
	}
	else
	{
		llerrs << "Using fixed-point packing of " << total_bits
			   << " bits, why ?!" << llendl;
	}
	return success;
}

bool LLDataPacker::unpackFixed(F32& value, const char* name, bool is_signed,
							   U32 int_bits, U32 frac_bits)
{
	bool ok = false;
	S32 unsigned_bits = int_bits + frac_bits;
	S32 total_bits = unsigned_bits;

	if (is_signed)
	{
		++total_bits;
	}

	U32 max_val;
	max_val = 1 << int_bits;

	F32 fixed_val;
	if (total_bits <= 8)
	{
		U8 fixed_8;
		ok = unpackU8(fixed_8, name);
		fixed_val = (F32)fixed_8;
	}
	else if (total_bits <= 16)
	{
		U16 fixed_16;
		ok = unpackU16(fixed_16, name);
		fixed_val = (F32)fixed_16;
	}
	else if (total_bits <= 31)
	{
		U32 fixed_32;
		ok = unpackU32(fixed_32, name);
		fixed_val = (F32)fixed_32;
	}
	else
	{
		fixed_val = 0;
		llerrs << "Bad bit count: " << total_bits << llendl;
	}

	fixed_val /= (F32)(1 << frac_bits);
	if (is_signed)
	{
		fixed_val -= max_val;
	}
	value = fixed_val;

	return ok;
}

//---------------------------------------------------------------------------
// LLDataPackerBinaryBuffer implementation
//---------------------------------------------------------------------------

void LLDataPackerBinaryBuffer::warnBadLength(S32 data_size, const char* name)
{
	llwarns << "Buffer overflow in BinaryBuffer length verify, field name '"
			<< name << "' !  Current pos: "	<< (S32)(mCurBufferp - mBufferp)
			<< " - Buffer size: " << mBufferSize << " - Data size: "
			<< data_size << llendl;
}

bool LLDataPackerBinaryBuffer::packString(const std::string& value,
										  const char* name)
{
	S32 length = value.length() + 1;
	if (!verifyLength(length, name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, value.c_str(), MVT_VARIABLE, length);
	}
	mCurBufferp += length;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackString(std::string& value,
											const char* name)
{
	S32 length = (S32)strlen((char*)mCurBufferp) + 1;
	if (!verifyLength(length, name))
	{
		return false;
	}

	// We already assume NULL termination calling strlen()
	value = std::string((char*)mCurBufferp);

	mCurBufferp += length;

	return true;
}

bool LLDataPackerBinaryBuffer::packBinaryData(const U8* value, S32 size,
											  const char* name)
{
	if (!verifyLength(size + 4, name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, &size, MVT_S32, 4);
	}
	mCurBufferp += 4;
	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, value, MVT_VARIABLE, size);
	}
	mCurBufferp += size;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackBinaryData(U8* value, S32& size,
												const char* name)
{
	if (!verifyLength(4, name))
	{
		llwarns << "Invalid data, aborting !" << llendl;
		return false;
	}

	htonmemcpy(&size, mCurBufferp, MVT_S32, 4);
	if (size < 0)
	{
		llwarns << "Invalid size, aborting !" << llendl;
		return false;
	}
	mCurBufferp += 4;

	if (!verifyLength(size, name))
	{
		llwarns << "Invalid data, aborting !" << llendl;
		return false;
	}

	if (!value)
	{
		llwarns << "NULL 'value', aborting !" << llendl;
		return false;
	}

	htonmemcpy(value, mCurBufferp, MVT_VARIABLE, size);
	mCurBufferp += size;

	return true;
}

bool LLDataPackerBinaryBuffer::packBinaryDataFixed(const U8* value, S32 size,
												   const char* name)
{
	if (!verifyLength(size, name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, value, MVT_VARIABLE, size);
	}
	mCurBufferp += size;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackBinaryDataFixed(U8* value, S32 size,
													 const char* name)
{
	if (!verifyLength(size, name))
	{
		return false;
	}

	htonmemcpy(value, mCurBufferp, MVT_VARIABLE, size);
	mCurBufferp += size;

	return true;
}

bool LLDataPackerBinaryBuffer::packU8(U8 value, const char* name)
{
	if (!verifyLength(sizeof(U8), name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		*mCurBufferp = value;
	}
	++mCurBufferp;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackU8(U8 &value, const char* name)
{
	if (!verifyLength(sizeof(U8), name))
	{
		return false;
	}

	value = *mCurBufferp;
	++mCurBufferp;

	return true;
}

bool LLDataPackerBinaryBuffer::packU16(U16 value, const char* name)
{
	if (!verifyLength(sizeof(U16), name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, &value, MVT_U16, 2);
	}
	mCurBufferp += 2;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackU16(U16& value, const char* name)
{
	if (!verifyLength(sizeof(U16), name))
	{
		return false;
	}

	htonmemcpy(&value, mCurBufferp, MVT_U16, 2);
	mCurBufferp += 2;

	return true;
}

bool LLDataPackerBinaryBuffer::packU32(U32 value, const char* name)
{
	if (!verifyLength(sizeof(U32), name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, &value, MVT_U32, 4);
	}
	mCurBufferp += 4;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackU32(U32& value, const char* name)
{
	if (!verifyLength(sizeof(U32), name))
	{
		return false;
	}

	htonmemcpy(&value, mCurBufferp, MVT_U32, 4);
	mCurBufferp += 4;

	return true;
}

bool LLDataPackerBinaryBuffer::packS32(S32 value, const char* name)
{
	if (!verifyLength(sizeof(S32), name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, &value, MVT_S32, 4);
	}
	mCurBufferp += 4;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackS32(S32& value, const char* name)
{
	if (!verifyLength(sizeof(S32), name))
	{
		return false;
	}

	htonmemcpy(&value, mCurBufferp, MVT_S32, 4);
	mCurBufferp += 4;

	return true;
}

bool LLDataPackerBinaryBuffer::packF32(F32 value, const char* name)
{
	if (!verifyLength(sizeof(F32), name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, &value, MVT_F32, 4);
	}
	mCurBufferp += 4;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackF32(F32& value, const char* name)
{
	if (!verifyLength(sizeof(F32), name))
	{
		return false;
	}

	htonmemcpy(&value, mCurBufferp, MVT_F32, 4);
	mCurBufferp += 4;

	return true;
}

bool LLDataPackerBinaryBuffer::packColor4(const LLColor4& value,
										  const char* name)
{
	if (!verifyLength(16, name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, value.mV, MVT_LLVector4, 16);
	}
	mCurBufferp += 16;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackColor4(LLColor4& value, const char* name)
{
	if (!verifyLength(16, name))
	{
		return false;
	}

	htonmemcpy(value.mV, mCurBufferp, MVT_LLVector4, 16);
	mCurBufferp += 16;

	return true;
}

bool LLDataPackerBinaryBuffer::packColor4U(const LLColor4U& value,
										   const char* name)
{
	if (!verifyLength(4, name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, value.mV, MVT_VARIABLE, 4);
	}
	mCurBufferp += 4;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackColor4U(LLColor4U& value,
											 const char* name)
{
	if (!verifyLength(4, name))
	{
		return false;
	}

	htonmemcpy(value.mV, mCurBufferp, MVT_VARIABLE, 4);
	mCurBufferp += 4;

	return true;
}

bool LLDataPackerBinaryBuffer::packVector2(const LLVector2& value,
										   const char* name)
{
	if (!verifyLength(8, name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, &value.mV[0], MVT_F32, 4);
		htonmemcpy(mCurBufferp + 4, &value.mV[1], MVT_F32, 4);
	}
	mCurBufferp += 8;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackVector2(LLVector2& value,
											 const char* name)
{
	if (!verifyLength(8, name))
	{
		return false;
	}

	htonmemcpy(&value.mV[0], mCurBufferp, MVT_F32, 4);
	htonmemcpy(&value.mV[1], mCurBufferp + 4, MVT_F32, 4);
	mCurBufferp += 8;

	return true;
}

bool LLDataPackerBinaryBuffer::packVector3(const LLVector3& value,
										   const char* name)
{
	if (!verifyLength(12, name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, value.mV, MVT_LLVector3, 12);
	}
	mCurBufferp += 12;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackVector3(LLVector3& value,
											 const char* name)
{
	if (!verifyLength(12, name))
	{
		return false;
	}

	htonmemcpy(value.mV, mCurBufferp, MVT_LLVector3, 12);
	mCurBufferp += 12;

	return true;
}

bool LLDataPackerBinaryBuffer::packVector4(const LLVector4& value,
										   const char* name)
{
	if (!verifyLength(16, name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, value.mV, MVT_LLVector4, 16);
	}
	mCurBufferp += 16;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackVector4(LLVector4& value,
											 const char* name)
{
	if (!verifyLength(16, name))
	{
		return false;
	}

	htonmemcpy(value.mV, mCurBufferp, MVT_LLVector4, 16);
	mCurBufferp += 16;

	return true;
}

bool LLDataPackerBinaryBuffer::packUUID(const LLUUID& value, const char* name)
{
	if (!verifyLength(16, name))
	{
		return false;
	}

	if (mWriteEnabled)
	{
		htonmemcpy(mCurBufferp, value.mData, MVT_LLUUID, 16);
	}
	mCurBufferp += 16;

	return true;
}

bool LLDataPackerBinaryBuffer::unpackUUID(LLUUID& value, const char* name)
{
	if (!verifyLength(16, name))
	{
		return false;
	}

	htonmemcpy(value.mData, mCurBufferp, MVT_LLUUID, 16);
	mCurBufferp += 16;

	return true;
}

const LLDataPackerBinaryBuffer& LLDataPackerBinaryBuffer::operator=(const LLDataPackerBinaryBuffer& a)
{
	if (a.getBufferSize() > getBufferSize())
	{
		// We have got problems, ack !
		llerrs << "Trying to do an assignment with not enough room in the target."
			   << llendl;
	}
	memcpy(mBufferp, a.mBufferp, a.getBufferSize());
	return *this;
}

void LLDataPackerBinaryBuffer::dumpBufferToLog()
{
	llwarns << "Binary Buffer Dump, size: " << mBufferSize << llendl;
	char line_buffer[256];
	S32 cur_line_pos = 0;
	S32 cur_line = 0;
	for (S32 i = 0; i < mBufferSize; ++i)
	{
		snprintf(line_buffer + cur_line_pos * 3,
				 sizeof(line_buffer) - cur_line_pos * 3,
				 "%02x ", mBufferp[i]);
		if (++cur_line_pos >= 16)
		{
			cur_line_pos = 0;
			llwarns << "Offset:" << std::hex << cur_line * 16 << std::dec
					<< " Data:" << line_buffer << llendl;
			++cur_line;
		}
	}
	if (cur_line_pos)
	{
		llwarns << "Offset:" << std::hex << cur_line * 16 << std::dec
				<< " Data:" << line_buffer << llendl;
	}
}

//---------------------------------------------------------------------------
// LLDataPackerAsciiBuffer implementation
//---------------------------------------------------------------------------
bool LLDataPackerAsciiBuffer::packString(const std::string& value,
										 const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%s\n", value.c_str());
	}
	else
	{
		numCopied = value.length() + 1;
	}

	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
		// *NOTE: I believe we need to mark a failure bit at this point.
	    numCopied = getBufferSize() - getCurrentSize();
		llwarns << "String truncated: " << value << llendl;
	}
	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackString(std::string& value,
										   const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE)) // NULL terminated
	{
		value = valuestr;
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packBinaryData(const U8* value, S32 size,
											 const char* name)
{
	writeIndentedName(name);

	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%010d ", size);

		// snprintf returns number of bytes that would have been written had
		// the output not being truncated. In that case, it will return
		// >= passed in size value. So a check needs to be added to detect
		// truncation, and if there is any, only account for the actual number
		// of bytes written... and not what could have been written.
		if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
		{
			numCopied = getBufferSize() - getCurrentSize();
			llwarns << "Number truncated at size: " << size << llendl;
		}
		mCurBufferp += numCopied;

		bool buffer_full = false;
		for (S32 i = 0; i < size && !buffer_full; ++i)
		{
			numCopied = snprintf(mCurBufferp,
								 getBufferSize() - getCurrentSize(), "%02x ",
								 value[i]);
			if (numCopied < 0 ||
				numCopied > getBufferSize() - getCurrentSize())
			{
				numCopied = getBufferSize() - getCurrentSize();
				llwarns << "Data truncated" << llendl;
				buffer_full = true;
			}
			mCurBufferp += numCopied;
		}

		if (!buffer_full)
		{
			numCopied = snprintf(mCurBufferp,
								 getBufferSize() - getCurrentSize(),
								 "\n");
			if (numCopied < 0 ||
				numCopied > getBufferSize() - getCurrentSize())
			{
				numCopied = getBufferSize() - getCurrentSize();
				llwarns << "Newline truncated" << llendl;
			}
			mCurBufferp += numCopied;
		}
	}
	else
	{
		// why +10 ?? XXXCHECK
		numCopied = 10 + 1; // size plus newline
		numCopied += size;
		if (numCopied > getBufferSize() - getCurrentSize())
		{
			numCopied = getBufferSize() - getCurrentSize();
		}
		mCurBufferp += numCopied;
	}

	return true;
}

bool LLDataPackerAsciiBuffer::unpackBinaryData(U8* value, S32& size,
											   const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		char* cur_pos = &valuestr[0];
		sscanf(valuestr,"%010d", &size);
		cur_pos += 11;
		for (S32 i = 0; i < size; ++i)
		{
			S32 val;
			sscanf(cur_pos,"%02x", &val);
			value[i] = val;
			cur_pos += 3;
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packBinaryDataFixed(const U8* value, S32 size,
												  const char* name)
{
	writeIndentedName(name);
	if (mWriteEnabled)
	{
		S32 numCopied = 0;
		bool buffer_full = false;
		for (S32 i = 0; i < size && !buffer_full; ++i)
		{
			numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
								 "%02x ", value[i]);
			if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
			{
			    numCopied = getBufferSize() - getCurrentSize();
				llwarns << "Data truncated" << llendl;
			    buffer_full = true;
			}
			mCurBufferp += numCopied;

		}
		if (!buffer_full)
		{
			numCopied = snprintf(mCurBufferp,getBufferSize() - getCurrentSize(),
								 "\n");
			if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
			{
				numCopied = getBufferSize() - getCurrentSize();
				llwarns << "Newline truncated" << llendl;
			}

			mCurBufferp += numCopied;
		}
	}
	else
	{
		S32 numCopied = 2 * size + 1; //hex bytes plus newline
		if (numCopied > getBufferSize() - getCurrentSize())
		{
			numCopied = getBufferSize() - getCurrentSize();
		}
		mCurBufferp += numCopied;
	}
	return true;
}

bool LLDataPackerAsciiBuffer::unpackBinaryDataFixed(U8* value, S32 size,
													const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		char* cur_pos = &valuestr[0];
		for (S32 i = 0; i < size; ++i)
		{
			S32 val;
			sscanf(cur_pos,"%02x", &val);
			value[i] = val;
			cur_pos += 3;
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packU8(U8 value, const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%d\n", value);
	}
	else
	{
		// just do the write to a temp buffer to get the length
		numCopied = snprintf(DUMMY_BUFFER, sizeof(DUMMY_BUFFER), "%d\n",
							 value);
	}

	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
		numCopied = getBufferSize() - getCurrentSize();
		llwarns << "U8truncated: " << value << llendl;
	}

	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackU8(U8 &value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		S32 in_val;
		sscanf(valuestr,"%d", &in_val);
		value = in_val;
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packU16(U16 value, const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%d\n", value);
	}
	else
	{
		numCopied = snprintf(DUMMY_BUFFER, sizeof(DUMMY_BUFFER), "%d\n",
							 value);
	}

	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
		numCopied = getBufferSize() - getCurrentSize();
		llwarns << "U16 truncated: " << value << llendl;
	}

	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackU16(U16& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		S32 in_val;
		sscanf(valuestr,"%d", &in_val);
		value = in_val;
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packU32(U32 value, const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%u\n", value);
	}
	else
	{
		numCopied = snprintf(DUMMY_BUFFER, sizeof(DUMMY_BUFFER), "%u\n",
							 value);
	}
	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
		numCopied = getBufferSize() - getCurrentSize();
		llwarns << "U32 truncated: " << value << llendl;
	}

	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackU32(U32& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%u", &value);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packS32(S32 value, const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%d\n", value);
	}
	else
	{
		numCopied = snprintf(DUMMY_BUFFER, sizeof(DUMMY_BUFFER), "%d\n",
							 value);
	}
	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
		numCopied = getBufferSize() - getCurrentSize();
		llwarns << "S32 truncated: " << value << llendl;
	}

	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackS32(S32& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%d", &value);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packF32(F32 value, const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%f\n", value);
	}
	else
	{
		numCopied = snprintf(DUMMY_BUFFER, sizeof(DUMMY_BUFFER), "%f\n",
							 value);
	}
	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
		numCopied = getBufferSize() - getCurrentSize();
		llwarns << "F32 truncated: " << value << llendl;
	}

	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackF32(F32& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%f", &value);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packColor4(const LLColor4& value,
										 const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%f %f %f %f\n", value.mV[0], value.mV[1],
							 value.mV[2], value.mV[3]);
	}
	else
	{
		numCopied = snprintf(DUMMY_BUFFER, sizeof(DUMMY_BUFFER),
							 "%f %f %f %f\n", value.mV[0], value.mV[1],
							 value.mV[2], value.mV[3]);
	}
	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
		numCopied = getBufferSize() - getCurrentSize();
		llwarns << "Color4: truncated: " << value << llendl;
	}

	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackColor4(LLColor4& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%f %f %f %f", &value.mV[0], &value.mV[1],
			   &value.mV[2], &value.mV[3]);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packColor4U(const LLColor4U& value,
										  const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%d %d %d %d\n", value.mV[0], value.mV[1],
							 value.mV[2], value.mV[3]);
	}
	else
	{
		numCopied = snprintf(DUMMY_BUFFER, sizeof(DUMMY_BUFFER),
							 "%d %d %d %d\n", value.mV[0], value.mV[1],
							 value.mV[2], value.mV[3]);
	}
	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
		numCopied = getBufferSize() - getCurrentSize();
		llwarns << "Color4U truncated: " << value << llendl;
	}

	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackColor4U(LLColor4U& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		S32 r, g, b, a;
		sscanf(valuestr,"%d %d %d %d", &r, &g, &b, &a);
		value.mV[0] = r;
		value.mV[1] = g;
		value.mV[2] = b;
		value.mV[3] = a;
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packVector2(const LLVector2& value,
										  const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%f %f\n", value.mV[0], value.mV[1]);
	}
	else
	{
		numCopied = snprintf(DUMMY_BUFFER,sizeof(DUMMY_BUFFER),"%f %f\n",
							 value.mV[0], value.mV[1]);
	}
	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
		numCopied = getBufferSize() - getCurrentSize();
		llwarns << "Vector2 truncated: " << value << llendl;
	}

	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackVector2(LLVector2& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%f %f", &value.mV[0], &value.mV[1]);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packVector3(const LLVector3& value,
										  const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%f %f %f\n", value.mV[0], value.mV[1],
							 value.mV[2]);
	}
	else
	{
		numCopied = snprintf(DUMMY_BUFFER, sizeof(DUMMY_BUFFER), "%f %f %f\n",
							 value.mV[0], value.mV[1], value.mV[2]);
	}
	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
	    numCopied = getBufferSize() - getCurrentSize();
		llwarns << "Vector3 truncated: " << value << llendl;
	}

	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackVector3(LLVector3& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%f %f %f", &value.mV[0], &value.mV[1], &value.mV[2]);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packVector4(const LLVector4& value,
										  const char* name)
{
	writeIndentedName(name);
	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%f %f %f %f\n", value.mV[0], value.mV[1],
							 value.mV[2], value.mV[3]);
	}
	else
	{
		numCopied = snprintf(DUMMY_BUFFER, sizeof(DUMMY_BUFFER),
							 "%f %f %f %f\n", value.mV[0], value.mV[1],
							 value.mV[2], value.mV[3]);
	}
	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
	    numCopied = getBufferSize() - getCurrentSize();
		llwarns << "Vector4 truncated: " << value << llendl;
	}

	mCurBufferp += numCopied;

	return true;
}

bool LLDataPackerAsciiBuffer::unpackVector4(LLVector4& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%f %f %f %f", &value.mV[0], &value.mV[1],
			   &value.mV[2], &value.mV[3]);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiBuffer::packUUID(const LLUUID& value, const char* name)
{
	bool success = true;
	writeIndentedName(name);

	S32 numCopied = 0;
	if (mWriteEnabled)
	{
		std::string tmp_str;
		value.toString(tmp_str);
		numCopied = snprintf(mCurBufferp, getBufferSize() - getCurrentSize(),
							 "%s\n", tmp_str.c_str());
	}
	else
	{
		numCopied = 64 + 1; // UUID + newline
	}
	// snprintf returns number of bytes that would have been written had the
	// output not being truncated. In that case, it will return either -1 or
	// value >= passed in size value . So a check needs to be added to detect
	// truncation, and if there is any, only account for the actual number of
	// bytes written..and not what could have been written.
	if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
	{
	    numCopied = getBufferSize() - getCurrentSize();
		llwarns << "UUID truncated: " << value << llendl;
		success = false;
	}
	mCurBufferp += numCopied;

	return success;
}

bool LLDataPackerAsciiBuffer::unpackUUID(LLUUID& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		char tmp_str[64];
		sscanf(valuestr, "%63s", tmp_str);
		value.set(tmp_str);
		return true;
	}
	else
	{
		return false;
	}
}

void LLDataPackerAsciiBuffer::dump()
{
	llinfos << "Buffer: " << mBufferp << llendl;
}

void LLDataPackerAsciiBuffer::writeIndentedName(const char* name)
{
	if (mIncludeNames)
	{
		S32 numCopied = 0;
		if (mWriteEnabled)
		{
			numCopied = snprintf(mCurBufferp,
								 getBufferSize() - getCurrentSize(),
								 "%s\t", name);
		}
		else
		{
			// name + tab
			numCopied = (S32)strlen(name) + 1;
		}

		// snprintf returns number of bytes that would have been written had
		// the output not being truncated. In that case, it will return either
		// -1 or value >= passed in size value . So a check needs to be added
		// to detect truncation, and if there is any, only account for the
		// actual number of bytes written..and not what could have been
		// written.
		if (numCopied < 0 || numCopied > getBufferSize() - getCurrentSize())
		{
			numCopied = getBufferSize() - getCurrentSize();
			llwarns << "Name truncated: " << name << llendl;
		}

		mCurBufferp += numCopied;
	}
}

bool LLDataPackerAsciiBuffer::getValueStr(const char* name, char* out_value,
										  S32 value_len)
{
	char buffer[DP_BUFSIZE];
	char keyword[DP_BUFSIZE];
	char value[DP_BUFSIZE];

	buffer[0] = '\0';
	keyword[0] = '\0';
	value[0] = '\0';

	if (mIncludeNames)
	{
		// Read both the name and the value, and validate the name.
		sscanf(mCurBufferp, "%511[^\n]", buffer);
		// Skip the \n
		mCurBufferp += (S32)strlen(buffer) + 1;

		sscanf(buffer, "%511s %511[^\n]", keyword, value);

		if (strcmp(keyword, name))
		{
			llwarns << "Data packer expecting keyword of type " << name
					<< ", got " << keyword << " instead !" << llendl;
			return false;
		}
	}
	else
	{
		// Just the value exists
		sscanf(mCurBufferp, "%511[^\n]", value);
		// Skip the \n
		mCurBufferp += (S32)strlen(value) + 1;
	}

	S32 in_value_len = (S32)strlen(value) + 1;
	S32 min_len = llmin(in_value_len, value_len);
	memcpy(out_value, value, min_len);
	out_value[min_len-1] = 0;

	return true;
}

// helper function used by LLDataPackerAsciiFile to convert F32 into a string.
// This is to avoid << operator writing F32 value into a stream since it does
// not seem to preserve the float value
std::string convertF32ToString(F32 val)
{
	std::string str;
	char buf[20];
	snprintf(buf, 20, "%f", val);
	str = buf;
	return str;
}

//---------------------------------------------------------------------------
// LLDataPackerAsciiFile implementation
//---------------------------------------------------------------------------
bool LLDataPackerAsciiFile::packString(const std::string& value,
									   const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%s\n", value.c_str());
	}
	else if (mOutputStream)
	{
		*mOutputStream << value << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackString(std::string& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		value = valuestr;
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packBinaryData(const U8* value, S32 size,
										   const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP, "%010d ", size);

		for (S32 i = 0; i < size; ++i)
		{
			fprintf(mFP, "%02x ", value[i]);
		}
		fprintf(mFP, "\n");
	}
	else if (mOutputStream)
	{
		char buffer[32];
		snprintf(buffer,sizeof(buffer), "%010d ", size);
		*mOutputStream << buffer;

		for (S32 i = 0; i < size; ++i)
		{
			snprintf(buffer, sizeof(buffer), "%02x ", value[i]);
			*mOutputStream << buffer;
		}
		*mOutputStream << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackBinaryData(U8* value, S32& size,
											 const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		char* cur_pos = &valuestr[0];
		sscanf(valuestr,"%010d", &size);
		cur_pos += 11;
		for (S32 i = 0; i < size; ++i)
		{
			S32 val;
			sscanf(cur_pos,"%02x", &val);
			value[i] = val;
			cur_pos += 3;
		}
		return true;
	}
	else
	{
		return false;
	}

}

bool LLDataPackerAsciiFile::packBinaryDataFixed(const U8* value, S32 size,
												const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		for (S32 i = 0; i < size; ++i)
		{
			fprintf(mFP, "%02x ", value[i]);
		}
		fprintf(mFP, "\n");
	}
	else if (mOutputStream)
	{
		char buffer[32];
		for (S32 i = 0; i < size; ++i)
		{
			snprintf(buffer, sizeof(buffer), "%02x ", value[i]);
			*mOutputStream << buffer;
		}
		*mOutputStream << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackBinaryDataFixed(U8* value, S32 size,
												  const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		char *cur_pos = &valuestr[0];
		for (S32 i = 0; i < size; ++i)
		{
			S32 val;
			sscanf(cur_pos,"%02x", &val);
			value[i] = val;
			cur_pos += 3;
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packU8(U8 value, const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%d\n", value);
	}
	else if (mOutputStream)
	{
		// We have to cast this to an integer because streams serialize bytes
		// as bytes - not as text.
		*mOutputStream << (S32)value << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackU8(U8 &value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		S32 in_val;
		sscanf(valuestr,"%d", &in_val);
		value = in_val;
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packU16(U16 value, const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%d\n", value);
	}
	else if (mOutputStream)
	{
		*mOutputStream <<"" << value << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackU16(U16& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		S32 in_val;
		sscanf(valuestr,"%d", &in_val);
		value = in_val;
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packU32(U32 value, const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%u\n", value);
	}
	else if (mOutputStream)
	{
		*mOutputStream <<"" << value << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackU32(U32& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%u", &value);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packS32(S32 value, const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%d\n", value);
	}
	else if (mOutputStream)
	{
		*mOutputStream <<"" << value << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackS32(S32& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%d", &value);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packF32(F32 value, const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%f\n", value);
	}
	else if (mOutputStream)
	{
		*mOutputStream <<"" << convertF32ToString(value) << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackF32(F32& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%f", &value);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packColor4(const LLColor4& value, const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%f %f %f %f\n", value.mV[0], value.mV[1], value.mV[2],
				value.mV[3]);
	}
	else if (mOutputStream)
	{
		*mOutputStream << convertF32ToString(value.mV[0]) << " "
					   << convertF32ToString(value.mV[1]) << " "
					   << convertF32ToString(value.mV[2]) << " "
					   << convertF32ToString(value.mV[3]) << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackColor4(LLColor4& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%f %f %f %f", &value.mV[0], &value.mV[1],
			   &value.mV[2], &value.mV[3]);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packColor4U(const LLColor4U& value,
										const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%d %d %d %d\n", value.mV[0], value.mV[1], value.mV[2],
				value.mV[3]);
	}
	else if (mOutputStream)
	{
		*mOutputStream << (S32)(value.mV[0]) << " "
					   << (S32)(value.mV[1]) << " "
					   << (S32)(value.mV[2]) << " "
					   << (S32)(value.mV[3]) << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackColor4U(LLColor4U& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		S32 r, g, b, a;
		sscanf(valuestr,"%d %d %d %d", &r, &g, &b, &a);
		value.mV[0] = r;
		value.mV[1] = g;
		value.mV[2] = b;
		value.mV[3] = a;
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packVector2(const LLVector2& value,
										const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%f %f\n", value.mV[0], value.mV[1]);
	}
	else if (mOutputStream)
	{
		*mOutputStream << convertF32ToString(value.mV[0]) << " "
					   << convertF32ToString(value.mV[1]) << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackVector2(LLVector2& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%f %f", &value.mV[0], &value.mV[1]);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packVector3(const LLVector3& value,
										const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%f %f %f\n", value.mV[0], value.mV[1], value.mV[2]);
	}
	else if (mOutputStream)
	{
		*mOutputStream << convertF32ToString(value.mV[0]) << " "
					   << convertF32ToString(value.mV[1]) << " "
					   << convertF32ToString(value.mV[2]) << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackVector3(LLVector3& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%f %f %f", &value.mV[0], &value.mV[1], &value.mV[2]);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packVector4(const LLVector4& value,
										const char* name)
{
	writeIndentedName(name);
	if (mFP)
	{
		fprintf(mFP,"%f %f %f %f\n", value.mV[0], value.mV[1], value.mV[2],
				value.mV[3]);
	}
	else if (mOutputStream)
	{
		*mOutputStream << convertF32ToString(value.mV[0]) << " "
					   << convertF32ToString(value.mV[1]) << " "
					   << convertF32ToString(value.mV[2]) << " "
					   << convertF32ToString(value.mV[3]) << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackVector4(LLVector4& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		sscanf(valuestr,"%f %f %f %f", &value.mV[0], &value.mV[1],
			   &value.mV[2], &value.mV[3]);
		return true;
	}
	else
	{
		return false;
	}
}

bool LLDataPackerAsciiFile::packUUID(const LLUUID& value, const char* name)
{
	writeIndentedName(name);
	std::string tmp_str;
	value.toString(tmp_str);
	if (mFP)
	{
		fprintf(mFP,"%s\n", tmp_str.c_str());
	}
	else if (mOutputStream)
	{
		*mOutputStream <<"" << tmp_str << "\n";
	}
	return true;
}

bool LLDataPackerAsciiFile::unpackUUID(LLUUID& value, const char* name)
{
	char valuestr[DP_BUFSIZE];
	if (getValueStr(name, valuestr, DP_BUFSIZE))
	{
		char tmp_str[64];
		sscanf(valuestr,"%63s",tmp_str);
		value.set(tmp_str);
		return true;
	}
	else
	{
		return false;
	}
}

void LLDataPackerAsciiFile::writeIndentedName(const char* name)
{
	std::string indent_buf;
	indent_buf.reserve(mIndent + 1);

	S32 i;
	for (i = 0; i < mIndent; ++i)
	{
		indent_buf[i] = '\t';
	}
	indent_buf[i] = 0;
	if (mFP)
	{
		fprintf(mFP,"%s%s\t",indent_buf.c_str(), name);
	}
	else if (mOutputStream)
	{
		*mOutputStream << indent_buf << name << "\t";
	}
}

bool LLDataPackerAsciiFile::getValueStr(const char* name, char* out_value,
										S32 value_len)
{
	bool success = false;
	char buffer[DP_BUFSIZE];
	char keyword[DP_BUFSIZE];
	char value[DP_BUFSIZE];

	buffer[0] = '\0';
	keyword[0] = '\0';
	value[0] = '\0';

	if (mFP)
	{
		fpos_t last_pos;
		if (fgetpos(mFP, &last_pos) != 0) // 0==success for fgetpos
		{
			llwarns << "Data packer failed to fgetpos" << llendl;
			return false;
		}

		if (fgets(buffer, DP_BUFSIZE, mFP) == NULL)
		{
			buffer[0] = '\0';
		}

		sscanf(buffer, "%511s %511[^\n]", keyword, value);

		if (!keyword[0])
		{
			llwarns << "Data packer could not get the keyword !" << llendl;
			fsetpos(mFP, &last_pos);
			return false;
		}
		if (strcmp(keyword, name))
		{
			llwarns << "Data packer expecting keyword of type " << name
					<< ", got " << keyword << " instead !" << llendl;
			fsetpos(mFP, &last_pos);
			return false;
		}

		S32 in_value_len = (S32)strlen(value) + 1;
		S32 min_len = llmin(in_value_len, value_len);
		memcpy(out_value, value, min_len);
		out_value[min_len - 1] = 0;
		success = true;
	}
	else if (mInputStream)
	{
		mInputStream->getline(buffer, DP_BUFSIZE);

		sscanf(buffer, "%511s %511[^\n]", keyword, value);
		if (!keyword[0])
		{
			llwarns << "Data packer could not get the keyword !" << llendl;
			return false;
		}
		if (strcmp(keyword, name))
		{
			llwarns << "Data packer expecting keyword of type " << name
					<< ", got " << keyword << " instead !" << llendl;
			return false;
		}

		S32 in_value_len = (S32)strlen(value) + 1;
		S32 min_len = llmin(in_value_len, value_len);
		memcpy(out_value, value, min_len);
		out_value[min_len - 1] = 0;
		success = true;
	}

	return success;
}
