/*
 * @file llpngwrapper.h
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

#ifndef LL_LLPNGWRAPPER_H
#define LL_LLPNGWRAPPER_H

#include "png.h"
#include "llimage.h"

class LLPngWrapper
{
protected:
	LOG_CLASS(LLPngWrapper);

	void normalizeImage();
	void updateMetaData();

public:
	LLPngWrapper();
	~LLPngWrapper();

	struct ImageInfo
	{
		U16 mWidth;
		U16 mHeight;
		S8  mComponents;
	};

	bool isValidPng(U8* src);
	bool readPng(U8* src, S32 dataSize, LLImageRaw* rawImage,
				 ImageInfo* infop = NULL);
	bool writePng(const LLImageRaw* rawImage, U8* dst);
	LL_INLINE U32 getFinalSize()					{ return mFinalSize; }
	LL_INLINE const std::string& getErrorMessage()	{ return mErrorMessage; }

private:
	// Structure for writing/reading PNG data to/from memory as opposed to
	// using a file.
	struct PngDataInfo
	{
		U8* mData;
		U32 mOffset;
		S32 mDataSize;
	};

	// No-op since we are just writing to memory
	LL_INLINE static void writeFlush(png_structp png_ptr)
	{
	}

	static void errorHandler(png_structp png_ptr, png_const_charp msg);
	static void readDataCallback(png_structp png_ptr, png_bytep dest,
								 png_size_t length);
	static void writeDataCallback(png_structp png_ptr, png_bytep src,
								  png_size_t length);

	void releaseResources();

private:
	png_structp mReadPngPtr;
	png_infop mReadInfoPtr;
	png_structp mWritePngPtr;
	png_infop mWriteInfoPtr;

	U8** mRowPointers;

	png_uint_32 mWidth;
	png_uint_32 mHeight;
	S32 mBitDepth;
	S32 mColorType;
	S32 mChannels;
	S32 mInterlaceType;
	S32 mCompressionType;
	S32 mFilterMethod;

	U32 mFinalSize;

	F64 mGamma;

	std::string mErrorMessage;
};

#endif
