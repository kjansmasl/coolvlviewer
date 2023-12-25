/**
 * @file llimagetga.h
 * @brief Image implementation to compresses and decompressed TGA files.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLIMAGETGA_H
#define LL_LLIMAGETGA_H

#include "llimage.h"

// This class compresses and decompressed TGA (targa) files

class LLImageTGA final : public LLImageFormatted
{
protected:
	LOG_CLASS(LLImageTGA);

	~LLImageTGA() override;

public:
	LLImageTGA();
	LLImageTGA(const std::string& file_name);

	std::string getExtension() override			{ return "tga"; }
	bool updateData() override;
	bool decode(LLImageRaw* raw_image) override;
	bool encode(const LLImageRaw* raw_image) override;

	bool decodeAndProcess(LLImageRaw* raw_image, F32 domain, F32 weight);

private:
	bool decodeTruecolor(LLImageRaw* raw_image, bool rle, bool flipped);

	bool decodeTruecolorRle8(LLImageRaw* raw_image);
	bool decodeTruecolorRle15(LLImageRaw* raw_image);
	bool decodeTruecolorRle24(LLImageRaw* raw_image);
	bool decodeTruecolorRle32(LLImageRaw* raw_image, bool& alpha_opaque);

	void decodeTruecolorPixel15(U8* dst, const U8* src);

	bool decodeTruecolorNonRle(LLImageRaw* raw_image, bool& alpha_opaque);

	bool decodeColorMap(LLImageRaw* raw_image, bool rle, bool flipped);

	void decodeColorMapPixel8(U8* dst, const U8* src);
	void decodeColorMapPixel15(U8* dst, const U8* src);
	void decodeColorMapPixel24(U8* dst, const U8* src);
	void decodeColorMapPixel32(U8* dst, const U8* src);

	bool loadFile(const std::string& file_name);

private:
	U32 mDataOffset; // Offset from start of data to the actual header.

	// Data from header
	U8 mIDLength;		// Length of identifier string
	U8 mColorMapType;	// 0 = No Map
	// Supported: 2 = Uncompressed true color, 3 = uncompressed monochrome
	// without colormap:
	U8 mImageType;
	U8 mColorMapIndexLo;	// First color map entry (low order byte)
	U8 mColorMapIndexHi;	// First color map entry (high order byte)
	U8 mColorMapLengthLo;	// Color map length (low order byte)
	U8 mColorMapLengthHi;	// Color map length (high order byte)
	U8 mColorMapDepth;	// Size of color map entry (15, 16, 24, or 32 bits)
	U8 mXOffsetLo;		// X offset of image (low order byte)
	U8 mXOffsetHi;		// X offset of image (hi order byte)
	U8 mYOffsetLo;		// Y offset of image (low order byte)
	U8 mYOffsetHi;		// Y offset of image (hi order byte)
	U8 mWidthLo;		// Width (low order byte)
	U8 mWidthHi;		// Width (hi order byte)
	U8 mHeightLo;		// Height (low order byte)
	U8 mHeightHi;		// Height (hi order byte)
	U8 mPixelSize;		// 8, 16, 24, 32 bits per pixel
	U8 mAttributeBits;	// 4 bits: number of attributes per pixel
	U8 mOriginRightBit;	// 1 bit: origin, 0 = left, 1 = right
	U8 mOriginTopBit;	// 1 bit: origin, 0 = bottom, 1 = top
	// 2 bits: interleaved flag, 0 = none, 1 = interleaved 2, 2 = interleaved 4
	U8 mInterleave;

	U8*		mColorMap;
	S32		mColorMapStart;
	S32		mColorMapLength;
	S32		mColorMapBytesPerEntry;

	bool	mIs15Bit;

	static const U8 s5to8bits[32];
};

#endif
