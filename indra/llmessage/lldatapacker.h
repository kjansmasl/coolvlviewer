/**
 * @file lldatapacker.h
 * @brief Data packer declaration for tightly storing binary data.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLDATAPACKER_H
#define LL_LLDATAPACKER_H

#include "llpreprocessor.h"

class LLColor4;
class LLColor4U;
class LLVector2;
class LLVector3;
class LLVector4;
class LLUUID;

class LLDataPacker
{
protected:
	LOG_CLASS(LLDataPacker);

public:
	~LLDataPacker() = default;

	// Not required to override, but error to call ?
	virtual void reset();
	virtual void dumpBufferToLog();

	virtual bool hasNext() const = 0;

	virtual bool packString(const std::string& value, const char* name) = 0;
	virtual bool unpackString(std::string& value, const char* name) = 0;

	virtual bool packBinaryData(const U8* value, S32 size,
								const char* name) = 0;
	virtual bool unpackBinaryData(U8* value, S32& size,
								  const char* name) = 0;

	// Constant size binary data packing
	virtual bool packBinaryDataFixed(const U8* value, S32 size,
									 const char* name) = 0;
	virtual bool unpackBinaryDataFixed(U8* value, S32 size,
									   const char* name) = 0;

	virtual bool packU8(U8 value, const char* name) = 0;
	virtual bool unpackU8(U8 &value, const char* name) = 0;

	virtual bool packU16(U16 value, const char* name) = 0;
	virtual bool unpackU16(U16& value, const char* name) = 0;

	virtual bool packU32(U32 value, const char* name) = 0;
	virtual bool unpackU32(U32& value, const char* name) = 0;

	virtual bool packS32(S32 value, const char* name) = 0;
	virtual bool unpackS32(S32& value, const char* name) = 0;

	virtual bool packF32(F32 value, const char* name) = 0;
	virtual bool unpackF32(F32& value, const char* name) = 0;

	// Packs a float into an integer, using the given size and picks the right
	// U* data type to pack into.
	bool packFixed(F32 value, const char* name, bool is_signed, U32 int_bits,
				   U32 frac_bits);
	bool unpackFixed(F32& value, const char* name, bool is_signed,
					 U32 int_bits, U32 frac_bits);

	virtual bool packColor4(const LLColor4& value, const char* name) = 0;
	virtual bool unpackColor4(LLColor4& value, const char* name) = 0;

	virtual bool packColor4U(const LLColor4U& value, const char* name) = 0;
	virtual bool unpackColor4U(LLColor4U& value, const char* name) = 0;

	virtual bool packVector2(const LLVector2& value, const char* name) = 0;
	virtual bool unpackVector2(LLVector2& value, const char* name) = 0;

	virtual bool packVector3(const LLVector3& value, const char* name) = 0;
	virtual bool unpackVector3(LLVector3& value, const char* name) = 0;

	virtual bool packVector4(const LLVector4& value, const char* name) = 0;
	virtual bool unpackVector4(LLVector4& value, const char* name) = 0;

	virtual bool packUUID(const LLUUID& value, const char* name) = 0;
	virtual bool unpackUUID(LLUUID& value, const char* name) = 0;

	LL_INLINE U32 getPassFlags() const			{ return mPassFlags; }
	LL_INLINE void setPassFlags(U32 flags)		{ mPassFlags = flags; }

protected:
	LLDataPacker();

protected:
	U32		mPassFlags;

	// Disable this to do things like determine filesize without actually
	// copying data
	bool	mWriteEnabled;
};

class LLDataPackerBinaryBuffer final : public LLDataPacker
{
protected:
	LOG_CLASS(LLDataPackerBinaryBuffer);

public:
	LLDataPackerBinaryBuffer(U8* bufferp, S32 size)
	:	LLDataPacker(),
		mBufferp(bufferp),
		mCurBufferp(bufferp),
		mBufferSize(size)
	{
		mWriteEnabled = true;
	}

	LLDataPackerBinaryBuffer()
	:	LLDataPacker(),
		mBufferp(NULL),
		mCurBufferp(NULL),
		mBufferSize(0)
	{
	}

	bool packString(const std::string& value, const char* name) override;
	bool unpackString(std::string& value, const char* name) override;

	bool packBinaryData(const U8* value, S32 size, const char* name) override;
	bool unpackBinaryData(U8* value, S32& size, const char* name) override;

	// Constant size binary data packing
	bool packBinaryDataFixed(const U8* value, S32 size,
							 const char* name) override;
	bool unpackBinaryDataFixed(U8* value, S32 size, const char* name) override;

	bool packU8(U8 value, const char* name) override;
	bool unpackU8(U8 &value, const char* name) override;

	bool packU16(U16 value, const char* name) override;
	bool unpackU16(U16& value, const char* name) override;

	bool packU32(U32 value, const char* name) override;
	bool unpackU32(U32& value, const char* name) override;

	bool packS32(S32 value, const char* name) override;
	bool unpackS32(S32& value, const char* name) override;

	bool packF32(F32 value, const char* name) override;
	bool unpackF32(F32& value, const char* name) override;

	bool packColor4(const LLColor4& value, const char* name) override;
	bool unpackColor4(LLColor4& value, const char* name) override;

	bool packColor4U(const LLColor4U& value, const char* name) override;
	bool unpackColor4U(LLColor4U& value, const char* name) override;

	bool packVector2(const LLVector2& value, const char* name) override;
	bool unpackVector2(LLVector2& value, const char* name) override;

	bool packVector3(const LLVector3& value, const char* name) override;
	bool unpackVector3(LLVector3& value, const char* name) override;

	bool packVector4(const LLVector4& value, const char* name) override;
	bool unpackVector4(LLVector4& value, const char* name) override;

	bool packUUID(const LLUUID& value, const char* name) override;
	bool unpackUUID(LLUUID& value, const char* name) override;

	LL_INLINE S32 getCurrentSize() const		{ return (S32)(mCurBufferp - mBufferp); }
	LL_INLINE S32 getBufferSize() const			{ return mBufferSize; }
	LL_INLINE const U8*	getBuffer() const		{ return mBufferp; }

	LL_INLINE void reset() override
	{
		mCurBufferp = mBufferp;
		mWriteEnabled = mCurBufferp != NULL;
	}

	LL_INLINE void shift(S32 offset)			{ reset(); mCurBufferp += offset; }

	LL_INLINE void freeBuffer()
	{
		if (mBufferp)
		{
			delete[] mBufferp;
		}
		mBufferp = mCurBufferp = NULL;
		mBufferSize = 0;
		mWriteEnabled = false;
	}

	LL_INLINE void assignBuffer(U8* bufferp, S32 size)
	{
		if (mBufferp && mBufferp != bufferp)
		{
			freeBuffer();
		}
		mBufferp = bufferp;
		mCurBufferp = bufferp;
		mBufferSize = size;
		mWriteEnabled = true;
	}

	const LLDataPackerBinaryBuffer&	operator=(const LLDataPackerBinaryBuffer &a);

	LL_INLINE bool hasNext() const override
	{
		return getCurrentSize() < getBufferSize();
	}

	void dumpBufferToLog() override;

protected:
	LL_INLINE bool verifyLength(S32 data_size, const char* name)
	{
		if (mWriteEnabled && mCurBufferp - mBufferp > mBufferSize - data_size)
		{
			warnBadLength(data_size, name);
			return false;
		}
		return true;
	}

	// Avoids inlining a llwarns while keeping verifyLength() inlined. HB
	LL_NO_INLINE void warnBadLength(S32 data_size, const char* name);

protected:
	U8* mBufferp;
	U8* mCurBufferp;
	S32 mBufferSize;
};

class LLDataPackerAsciiBuffer final : public LLDataPacker
{
protected:
	LOG_CLASS(LLDataPackerAsciiBuffer);

public:
	LLDataPackerAsciiBuffer(char* bufferp, S32 size)
	{
		mBufferp = bufferp;
		mCurBufferp = bufferp;
		mBufferSize = size;
		mPassFlags = 0;
		mIncludeNames = false;
		mWriteEnabled = true;
	}

	LLDataPackerAsciiBuffer()
	{
		mBufferp = NULL;
		mCurBufferp = NULL;
		mBufferSize = 0;
		mPassFlags = 0;
		mIncludeNames = false;
		mWriteEnabled = false;
	}

	bool packString(const std::string& value, const char* name) override;
	bool unpackString(std::string& value, const char* name) override;

	bool packBinaryData(const U8* value, S32 size, const char* name) override;
	bool unpackBinaryData(U8* value, S32& size, const char* name) override;

	// Constant size binary data packing
	bool packBinaryDataFixed(const U8* value, S32 size,
							 const char* name) override;
	bool unpackBinaryDataFixed(U8* value, S32 size, const char* name) override;

	bool packU8(U8 value, const char* name) override;
	bool unpackU8(U8 &value, const char* name) override;

	bool packU16(U16 value, const char* name) override;
	bool unpackU16(U16& value, const char* name) override;

	bool packU32(U32 value, const char* name) override;
	bool unpackU32(U32& value, const char* name) override;

	bool packS32(S32 value, const char* name) override;
	bool unpackS32(S32& value, const char* name) override;

	bool packF32(F32 value, const char* name) override;
	bool unpackF32(F32& value, const char* name) override;

	bool packColor4(const LLColor4& value, const char* name) override;
	bool unpackColor4(LLColor4& value, const char* name) override;

	bool packColor4U(const LLColor4U& value, const char* name) override;
	bool unpackColor4U(LLColor4U& value, const char* name) override;

	bool packVector2(const LLVector2& value, const char* name) override;
	bool unpackVector2(LLVector2& value, const char* name) override;

	bool packVector3(const LLVector3& value, const char* name) override;
	bool unpackVector3(LLVector3& value, const char* name) override;

	bool packVector4(const LLVector4& value, const char* name) override;
	bool unpackVector4(LLVector4& value, const char* name) override;

	bool packUUID(const LLUUID& value, const char* name) override;
	bool unpackUUID(LLUUID& value, const char* name) override;

	LL_INLINE void setIncludeNames(bool b)		{ mIncludeNames = b; }

	// Include the trailing NULL so it's always a valid string
	LL_INLINE S32 getCurrentSize() const		{ return (S32)(mCurBufferp - mBufferp) + 1; }

	LL_INLINE S32 getBufferSize() const			{ return mBufferSize; }

	LL_INLINE void reset() override
	{
		mCurBufferp = mBufferp;
		mWriteEnabled = mCurBufferp != NULL;
	}

	LL_INLINE bool hasNext() const override		{ return getCurrentSize() < getBufferSize(); }

	LL_INLINE void freeBuffer();
	LL_INLINE void assignBuffer(char* bufferp, S32 size);

	void dump();

protected:
	void writeIndentedName(const char* name);
	bool getValueStr(const char* name, char* out_value, S32 value_len);

protected:
	LL_INLINE bool verifyLength(S32 data_size, const char* name);

	char*	mBufferp;
	char*	mCurBufferp;
	S32		mBufferSize;
	bool	mIncludeNames;	// useful for debugging, print the name of each field
};

LL_INLINE void	LLDataPackerAsciiBuffer::freeBuffer()
{
	if (mBufferp)
	{
		delete[] mBufferp;
	}
	mBufferp = mCurBufferp = NULL;
	mBufferSize = 0;
	mWriteEnabled = false;
}

LL_INLINE void	LLDataPackerAsciiBuffer::assignBuffer(char* bufferp, S32 size)
{
	mBufferp = bufferp;
	mCurBufferp = bufferp;
	mBufferSize = size;
	mWriteEnabled = true;
}

LL_INLINE bool LLDataPackerAsciiBuffer::verifyLength(S32 data_size,
													 const char* name)
{
	if (mWriteEnabled && mCurBufferp - mBufferp > mBufferSize - data_size)
	{
		llwarns << "Buffer overflow in AsciiBuffer length verify, field name '"
				<< name << "' !  Current pos: "
				<< (S32)(mCurBufferp - mBufferp)
				<< " - Buffer size: " << mBufferSize << " - Data size: "
				<< data_size << llendl;
		return false;
	}

	return true;
}

class LLDataPackerAsciiFile final : public LLDataPacker
{
protected:
	LOG_CLASS(LLDataPackerAsciiFile);

public:
	LLDataPackerAsciiFile(LLFILE* fp, S32 indent = 2)
	: 	LLDataPacker(),
		mIndent(indent),
		mFP(fp),
		mOutputStream(NULL),
		mInputStream(NULL)
	{
	}

	LLDataPackerAsciiFile(std::ostream& output_stream, S32 indent = 2)
	: 	LLDataPacker(),
		mIndent(indent),
		mFP(NULL),
		mOutputStream(&output_stream),
		mInputStream(NULL)
	{
		mWriteEnabled = true;
	}

	LLDataPackerAsciiFile(std::istream& input_stream, S32 indent = 2)
	: 	LLDataPacker(),
		mIndent(indent),
		mFP(NULL),
		mOutputStream(NULL),
		mInputStream(&input_stream)
	{
	}

	bool packString(const std::string& value, const char* name) override;
	bool unpackString(std::string& value, const char* name) override;

	bool packBinaryData(const U8* value, S32 size, const char* name) override;
	bool unpackBinaryData(U8* value, S32& size, const char* name) override;

	bool packBinaryDataFixed(const U8* value, S32 size,
							 const char* name) override;
	bool unpackBinaryDataFixed(U8* value, S32 size, const char* name) override;

	bool packU8(U8 value, const char* name) override;
	bool unpackU8(U8 &value, const char* name) override;

	bool packU16(U16 value, const char* name) override;
	bool unpackU16(U16& value, const char* name) override;

	bool packU32(U32 value, const char* name) override;
	bool unpackU32(U32& value, const char* name) override;

	bool packS32(S32 value, const char* name) override;
	bool unpackS32(S32& value, const char* name) override;

	bool packF32(F32 value, const char* name) override;
	bool unpackF32(F32& value, const char* name) override;

	bool packColor4(const LLColor4& value, const char* name) override;
	bool unpackColor4(LLColor4& value, const char* name) override;

	bool packColor4U(const LLColor4U& value, const char* name) override;
	bool unpackColor4U(LLColor4U& value, const char* name) override;

	bool packVector2(const LLVector2& value, const char* name) override;
	bool unpackVector2(LLVector2& value, const char* name) override;

	bool packVector3(const LLVector3& value, const char* name) override;
	bool unpackVector3(LLVector3& value, const char* name) override;

	bool packVector4(const LLVector4& value, const char* name) override;
	bool unpackVector4(LLVector4& value, const char* name) override;

	bool packUUID(const LLUUID& value, const char* name) override;
	bool unpackUUID(LLUUID& value, const char* name) override;

protected:
	void writeIndentedName(const char* name);
	bool getValueStr(const char* name, char* out_value, S32 value_len);

	LL_INLINE bool hasNext() const override		{ return true; }

protected:
	S32				mIndent;
	LLFILE*			mFP;
	std::ostream*	mOutputStream;
	std::istream*	mInputStream;
};

#endif // LL_LLDATAPACKER
