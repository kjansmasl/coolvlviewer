/**
 * @file llimagetga.cpp
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab. Terms of
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

#include "llimagetga.h"

#include "lldir.h"
#include "llmath.h"
#include "llpointer.h"

// For expanding 5-bit pixel values to 8-bit with best rounding
// static
const U8 LLImageTGA::s5to8bits[32] = {
	0,   8,  16,  25,  33,  41,  49,  58,
   66,  74,  82,  90,  99, 107, 115, 123,
  132, 140, 148, 156, 165, 173, 181, 189,
  197, 206, 214, 222, 230, 239, 247, 255
};

LL_INLINE void LLImageTGA::decodeTruecolorPixel15(U8* dst, const U8* src)
{
    // We expand 5 bit data to 8 bit sample width.
    // The format of the 16-bit (LSB first) input word is
    // xRRRRRGGGGGBBBBB
	U32 t = U32(src[0]) + (U32(src[1]) << 8);
    dst[2] = s5to8bits[t & 0x1F];  // blue
    t >>= 5;
    dst[1] = s5to8bits[t & 0x1F];  // green
    t >>= 5;
    dst[0] = s5to8bits[t & 0x1F];  // red
}

LLImageTGA::LLImageTGA()
:	LLImageFormatted(IMG_CODEC_TGA),
	mColorMap(NULL),
	mColorMapStart(0),
	mColorMapLength(0),
	mColorMapBytesPerEntry(0),
	mIs15Bit(false),
	mAttributeBits(0),
	mColorMapDepth(0),
	mColorMapIndexHi(0),
	mColorMapIndexLo(0),
	mColorMapLengthHi(0),
	mColorMapLengthLo(0),
	mColorMapType(0),
	mDataOffset(0),
	mHeightHi(0),
	mHeightLo(0),
	mIDLength(0),
	mImageType(0),
	mInterleave(0),
	mOriginRightBit(0),
	mOriginTopBit(0),
	mPixelSize(0),
	mWidthHi(0),
	mWidthLo(0),
	mXOffsetHi(0),
	mXOffsetLo(0),
	mYOffsetHi(0),
	mYOffsetLo(0)
{
}

LLImageTGA::LLImageTGA(const std::string& file_name)
:	LLImageFormatted(IMG_CODEC_TGA),
	mColorMap(NULL),
	mColorMapStart(0),
	mColorMapLength(0),
	mColorMapBytesPerEntry(0),
	mIs15Bit(false)
{
	loadFile(file_name);
}

LLImageTGA::~LLImageTGA()
{
	delete [] mColorMap;
}

bool LLImageTGA::updateData()
{
	resetLastError();

	// Check to make sure that this instance has been initialized with data
	if (!getData() || (0 == getDataSize()))
	{
		setLastError("LLImageTGA uninitialized");
		return false;
	}

	// Pull image information from the header...
	U8 flags, junk[256];

	/**************************************************************************
	**	For more information about the original Truevision TGA(tm) file format,
	**	or for additional information about the new extensions to the
	**	Truevision TGA file, refer to the "Truevision TGA File Format
	**	Specification Version 2.0" available from Truevision or your
	**	Truevision dealer.
	**
	**  FILE STRUCTURE FOR THE ORIGINAL TRUEVISION TGA FILE
	**	  FIELD 1 :	NUMBER OF CHARACTERS IN ID FIELD (1 BYTES)
	**	  FIELD 2 :	COLOR MAP TYPE (1 BYTES)
	**	  FIELD 3 :	IMAGE TYPE CODE (1 BYTES)
	**					= 0	NO IMAGE DATA INCLUDED
	**					= 1	UNCOMPRESSED, COLOR-MAPPED IMAGE
	**					= 2	UNCOMPRESSED, TRUE-COLOR IMAGE
	**					= 3	UNCOMPRESSED, BLACK AND WHITE IMAGE
	**					= 9	RUN-LENGTH ENCODED COLOR-MAPPED IMAGE
	**					= 10 RUN-LENGTH ENCODED TRUE-COLOR IMAGE
	**					= 11 RUN-LENGTH ENCODED BLACK AND WHITE IMAGE
	**	  FIELD 4 :	COLOR MAP SPECIFICATION	(5 BYTES)
	**				4.1 : COLOR MAP ORIGIN (2 BYTES)
	**				4.2 : COLOR MAP LENGTH (2 BYTES)
	**				4.3 : COLOR MAP ENTRY SIZE (2 BYTES)
	**	  FIELD 5 :	IMAGE SPECIFICATION (10 BYTES)
	**				5.1 : X-ORIGIN OF IMAGE (2 BYTES)
	**				5.2 : Y-ORIGIN OF IMAGE (2 BYTES)
	**				5.3 : WIDTH OF IMAGE (2 BYTES)
	**				5.4 : HEIGHT OF IMAGE (2 BYTES)
	**				5.5 : IMAGE PIXEL SIZE (1 BYTE)
	**				5.6 : IMAGE DESCRIPTOR BYTE (1 BYTE)
	**	  FIELD 6 :	IMAGE ID FIELD (LENGTH SPECIFIED BY FIELD 1)
	**	  FIELD 7 :	COLOR MAP DATA (BIT WIDTH SPECIFIED BY FIELD 4.3 AND
	**				NUMBER OF COLOR MAP ENTRIES SPECIFIED IN FIELD 4.2)
	**	  FIELD 8 :	IMAGE DATA FIELD (WIDTH AND HEIGHT SPECIFIED IN
	**				FIELD 5.3 AND 5.4)
	****************************************************************************/

	mDataOffset = 0;
	const U8* datap = getData();
	mIDLength = *(datap + mDataOffset++);
	mColorMapType = *(datap + mDataOffset++);
	mImageType = *(datap + mDataOffset++);
	mColorMapIndexLo = *(datap + mDataOffset++);
	mColorMapIndexHi = *(datap + mDataOffset++);
	mColorMapLengthLo = *(datap + mDataOffset++);
	mColorMapLengthHi = *(datap + mDataOffset++);
	mColorMapDepth = *(datap + mDataOffset++);
	mXOffsetLo = *(datap + mDataOffset++);
	mXOffsetHi = *(datap + mDataOffset++);
	mYOffsetLo = *(datap + mDataOffset++);
	mYOffsetHi = *(datap + mDataOffset++);
	mWidthLo = *(datap + mDataOffset++);
	mWidthHi = *(datap + mDataOffset++);
	mHeightLo = *(datap + mDataOffset++);
	mHeightHi = *(datap + mDataOffset++);
	mPixelSize = *(datap + mDataOffset++);
	flags = *(datap + mDataOffset++);
	mAttributeBits = flags & 0xf;
	mOriginRightBit = (flags & 0x10) >> 4;
	mOriginTopBit = (flags & 0x20) >> 5;
	mInterleave = (flags & 0xc0) >> 6;

	switch (mImageType)
	{
		case 2: 	// Truecolor uncompressed
		case 9:		// Colormapped, RLE
		case 10:	// Truecolor, RLE
			break;

		case 0:		// No image data included in file
			setLastError("Unable to load file. TGA file contains no image data.");
			return false;

		case 1:		// Colormapped uncompressed
			if (mPixelSize != 8)
			{
				setLastError("Unable to load file. Colormapped images must have 8 bits per pixel.");
				return false;
			}
			break;

		case 3:		// Monochrome uncompressed
			if (mPixelSize != 8)
			{
				setLastError("Unable to load file. Monochrome images must have 8 bits per pixel.");
				return false;
			}
			break;

		case 11:	// Monochrome, RLE
			if (mPixelSize != 8)
			{
				setLastError("Unable to load file. Monochrome images must have 8 bits per pixel.");
				return false;
			}
			break;

		default:
			setLastError("Unable to load file. Unrecoginzed TGA image type.");
			return false;
	}

	// Discard the ID field, if any
	if (mIDLength)
	{
		memcpy(junk, getData()+mDataOffset, mIDLength);
		mDataOffset += mIDLength;
	}

	// Check to see if there's a colormap since even rgb files can have them
	S32 color_map_bytes = 0;
	if (mColorMapType == 1 && mColorMapDepth > 0)
	{
		mColorMapStart = (S32(mColorMapIndexHi) << 8) + mColorMapIndexLo;
		mColorMapLength = (S32(mColorMapLengthHi) << 8) + mColorMapLengthLo;

		if (mColorMapDepth > 24)
		{
			mColorMapBytesPerEntry = 4;
		}
		else if (mColorMapDepth > 16)
		{
			mColorMapBytesPerEntry = 3;
		}
		else if (mColorMapDepth > 8)
		{
			mColorMapBytesPerEntry = 2;
		}
		else
		{
			mColorMapBytesPerEntry = 1;
		}
		color_map_bytes = mColorMapLength * mColorMapBytesPerEntry;

		// Note: although it is legal for TGA files to have color maps and not
		// use them (some programs actually do this and use the color map for
		// other ends), we will only allocate memory for one if _we_ intend to
		// use it.
		if (mImageType == 1 || mImageType == 9)
		{
			mColorMap = new (std::nothrow) U8[color_map_bytes];
			if (!mColorMap)
			{
				LLMemory::allocationFailed(color_map_bytes);
				setLastError("LLImageTGA::out of memory");
				return false;
			}
			memcpy(mColorMap, getData() + mDataOffset, color_map_bytes);
		}

		mDataOffset += color_map_bytes;
	}

	// Heights are read as bytes to prevent endian problems
	S32 height = (S32(mHeightHi) << 8) + mHeightLo;
	S32 width = (S32(mWidthHi) << 8) + mWidthLo;

	// make sure that it's a pixel format that we understand
	S32 bits_per_pixel;
	if (mColorMap)
	{
		bits_per_pixel = mColorMapDepth;
	}
	else
	{
		bits_per_pixel = mPixelSize;
	}

	S32 components;
	switch (bits_per_pixel)
	{
		case 24:
			components = 3;
			break;

		case 32:
			components = 4;
#if 0		// Do not enforce this. ACDSee does not bother to set the attributes
			// bits correctly. Arrgh!
			if (mAttributeBits != 8)
			{
				setLastError("Unable to load file. 32 bit TGA image does not have 8 bits of alpha.");
				return false;
			}
#endif
			mAttributeBits = 8;
			break;

		case 15:
		case 16:
			components = 3;
			// 16th bit is used for Targa hardware interupts and is ignored
			mIs15Bit = true;
			break;

		case 8:
			components = 1;
			break;

		default:
			setLastError("Unable to load file. Unknown pixel size.");
			return false;
	}

	setSize(width, height, components);

	return true;
}

bool LLImageTGA::decode(LLImageRaw* raw_image)
{
	if (!raw_image)
	{
		llwarns << "Attempted to decode a NULL raw image buffer address"
				<< llendl;
		llassert(false);
		return false;
	}

	// Check to make sure that this instance has been initialized with data
	if (!getData() || getDataSize() == 0)
	{
		setLastError("Trying to decode an image with no data !");
		return false;
	}

	// Copy everything after the header.

	if (!raw_image->resize(getWidth(), getHeight(), getComponents()))
	{
		setLastError("LLImageTGA failed to resize image");
		return false;
	}

	if (getComponents() != 1 && getComponents() != 3 && getComponents() != 4)
	{
		setLastError("TGA images with a number of components other than 1, 3, and 4 are not supported.");
		return false;
	}

	if (raw_image->isBufferInvalid())
	{
		setLastError("LLImageTGA: out of memory");
		return false;
	}

	if (mOriginRightBit)
	{
		setLastError("TGA images with origin on right side are not supported.");
		return false;
	}

	bool flipped = mOriginTopBit != 0;
	bool rle_compressed = (mImageType & 0x08) != 0;

	if (mColorMap)
	{
		return decodeColorMap(raw_image, rle_compressed, flipped);
	}
	else
	{
		return decodeTruecolor(raw_image, rle_compressed, flipped);
	}
}

bool LLImageTGA::decodeTruecolor(LLImageRaw* raw_image, bool rle, bool flipped)
{
	if (!raw_image)
	{
		llwarns << "Attempted to decode a NULL raw image" << llendl;
		llassert(false);
		return false;
	}

	bool success = false;
	bool alpha_opaque = false;
	if (rle)
	{
		switch (getComponents())
		{
			case 1:
				success = decodeTruecolorRle8(raw_image);
				break;

			case 3:
				if (mIs15Bit)
				{
					success = decodeTruecolorRle15(raw_image);
				}
				else
				{
					success = decodeTruecolorRle24(raw_image);
				}
				break;

			case 4:
				success = decodeTruecolorRle32(raw_image, alpha_opaque);
				if (alpha_opaque)
				{
					// Alpha was entirely opaque; convert to 24 bits image.
					LLPointer<LLImageRaw> compacted_image =
						new LLImageRaw(raw_image->getWidth(),
									   raw_image->getHeight(), 3);
					if (compacted_image->isBufferInvalid())
					{
						success = false;
						break;
					}
					compacted_image->copy(raw_image);
					raw_image->resize(raw_image->getWidth(),
									  raw_image->getHeight(), 3);
					raw_image->copy(compacted_image);
				}
		}
	}
	else
	{
		bool alpha_opaque;
		success = decodeTruecolorNonRle(raw_image, alpha_opaque);
		if (success && alpha_opaque && raw_image->getComponents() == 4)
		{
			// Alpha was entirely opaque; convert to 24 bits image.
			LLPointer<LLImageRaw> compacted_image =
				new LLImageRaw(raw_image->getWidth(), raw_image->getHeight(),
							   3);
			if (compacted_image->isBufferInvalid())
			{
				success = false;
			}
			else
			{
				compacted_image->copy(raw_image);
				raw_image->resize(raw_image->getWidth(),
								  raw_image->getHeight(), 3);
				raw_image->copy(compacted_image);
			}
		}
	}

	if (success && flipped)
	{
		// This works because the Targa definition requires that RLE blocks
		// never encode pixels from more than one scanline (on the other hand,
		// it is not as fast as writing separate flipped versions as we did
		// with TruecolorNonRle).
		raw_image->verticalFlip();
	}

	return success;
}

bool LLImageTGA::decodeTruecolorNonRle(LLImageRaw* raw_image,
									   bool& alpha_opaque)
{
	if (!raw_image)
	{
		llwarns << "Attempted to decode a NULL raw image" << llendl;
		llassert(false);
		return false;
	}

	alpha_opaque = true;

	// Origin is the bottom left
	U8* dst = raw_image->getData();
	U8* src = getData() + mDataOffset;
	S32 pixels = getWidth() * getHeight();

	if (pixels * (mIs15Bit ? 2 : getComponents()) >
			getDataSize() - (S32)mDataOffset)
	{
		// here we have situation when data size in src less than actually
		// needed
		return false;
	}

	if (getComponents() == 4)
	{
		while (pixels--)
		{
			// Our data is stored in RGBA. TGA stores them as BGRA (little
			// endian ARGB)
			dst[0] = src[2]; // Red
			dst[1] = src[1]; // Green
			dst[2] = src[0]; // Blue
			dst[3] = src[3]; // Alpha
			if (dst[3] != 255)
			{
				alpha_opaque = false;
			}
			dst += 4;
			src += 4;
		}
	}
	else if (getComponents() == 3)
	{
		if (mIs15Bit)
		{
			while (pixels--)
			{
				decodeTruecolorPixel15(dst, src);
				dst += 3;
				src += 2;
			}
		}
		else
		{
			while (pixels--)
			{
				dst[0] = src[2]; // Red
				dst[1] = src[1]; // Green
				dst[2] = src[0]; // Blue
				dst += 3;
				src += 3;
			}
		}
	}
	else if (getComponents() == 1)
	{
		memcpy(dst, src, pixels);
	}

	return true;
}

void LLImageTGA::decodeColorMapPixel8(U8* dst, const U8* src)
{
	S32 index = llclamp(*src - mColorMapStart, 0, mColorMapLength - 1);
	dst[0] = mColorMap[index];
}

void LLImageTGA::decodeColorMapPixel15(U8* dst, const U8* src)
{
	S32 index = llclamp(*src - mColorMapStart, 0, mColorMapLength - 1);
	decodeTruecolorPixel15(dst, mColorMap + 2 * index);
}

void LLImageTGA::decodeColorMapPixel24(U8* dst, const U8* src)
{
	S32 index = 3 * llclamp(*src - mColorMapStart, 0, mColorMapLength - 1);
	dst[0] = mColorMap[index + 2];	// Red
	dst[1] = mColorMap[index + 1];	// Green
	dst[2] = mColorMap[index];		// Blue
}

void LLImageTGA::decodeColorMapPixel32(U8* dst, const U8* src)
{
	S32 index = 4 * llclamp(*src - mColorMapStart, 0, mColorMapLength - 1);
	dst[0] = mColorMap[index + 2];	// Red
	dst[1] = mColorMap[index + 1];	// Green
	dst[2] = mColorMap[index];		// Blue
	dst[3] = mColorMap[index + 3];	// Alpha
}

bool LLImageTGA::decodeColorMap(LLImageRaw* raw_image, bool rle, bool flipped)
{
	if (!raw_image)
	{
		llwarns << "Attempted to decode a NULL raw image" << llendl;
		llassert(false);
		return false;
	}

	// If flipped, origin is the top left. Need to reverse the order of the rows.
	// Otherwise the origin is the bottom left.

	if (8 != mPixelSize)
	{
		return false;
	}

	U8* src = getData() + mDataOffset;
	U8* dst = raw_image->getData();	// start from the top

	void (LLImageTGA::*pixel_decoder)(U8*, const U8*);

	switch (mColorMapBytesPerEntry)
	{
		case 1:	pixel_decoder = &LLImageTGA::decodeColorMapPixel8;  break;
		case 2:	pixel_decoder = &LLImageTGA::decodeColorMapPixel15; break;
		case 3:	pixel_decoder = &LLImageTGA::decodeColorMapPixel24; break;
		case 4:	pixel_decoder = &LLImageTGA::decodeColorMapPixel32; break;
		default: llassert(0); return false;
	}

	if (rle)
	{
		U8* last_dst = dst + getComponents() * (getHeight() * getWidth() - 1);
		while (dst <= last_dst)
		{
			// Read RLE block header
			U8 block_header_byte = *src++;

			U8 block_pixel_count = (block_header_byte & 0x7F) + 1;
			if (block_header_byte & 0x80)
			{
				// Encoded (duplicate-pixel) block
				do
				{
					(this->*pixel_decoder)(dst, src);
					dst += getComponents();
					--block_pixel_count;
				}
				while (block_pixel_count > 0);
				++src;
			}
			else
			{
				// Unencoded block
				do
				{
					(this->*pixel_decoder)(dst, src);
					dst += getComponents();
					++src;
					--block_pixel_count;
				}
				while (block_pixel_count > 0);
			}
		}

		raw_image->verticalFlip();
	}
	else
	{
		S32 src_row_bytes = getWidth();
		S32 dst_row_bytes = getWidth() * getComponents();

		if (flipped)
		{
			U8* src_last_row_start = src + (getHeight() - 1) * src_row_bytes;
			src = src_last_row_start;		// start from the bottom
			src_row_bytes *= -1;
		}

		S32 i;
		S32 j;

		for (S32 row = 0; row < getHeight(); ++row)
		{
			for (i = 0, j = 0; j < getWidth(); i += getComponents(), ++j)
			{
				(this->*pixel_decoder)(dst + i, src + j);
			}

			dst += dst_row_bytes;
			src += src_row_bytes;
		}
	}

	return true;
}

bool LLImageTGA::encode(const LLImageRaw* raw_image)
{
	if (!raw_image)
	{
		llwarns << "Attempted to encode a NULL raw image" << llendl;
		llassert(false);
		return false;
	}

	deleteData();

	setSize(raw_image->getWidth(), raw_image->getHeight(), raw_image->getComponents());

	// Data from header
	mIDLength = 0;		// Length of identifier string
	mColorMapType = 0;	// 0 = No Map

	// Supported: 2 = Uncompressed true color, 3 = uncompressed monochrome without colormap
	switch (getComponents())
	{
	case 1:
		mImageType = 3;
		break;
	case 2:	 // Interpret as intensity plus alpha
	case 3:
	case 4:
		mImageType = 2;
		break;
	default:
		return false;
	}

	// Color map stuff (unsupported)
	mColorMapIndexLo = 0;		// First color map entry (low order byte)
	mColorMapIndexHi = 0;		// First color map entry (high order byte)
	mColorMapLengthLo = 0;		// Color map length (low order byte)
	mColorMapLengthHi = 0;		// Color map length (high order byte)
	mColorMapDepth = 0;	// Size of color map entry (15, 16, 24, or 32 bits)

	// Image offset relative to origin.
	mXOffsetLo = 0;		// X offset from origin (low order byte)
	mXOffsetHi = 0;		// X offset from origin (hi order byte)
	mYOffsetLo = 0;		// Y offset from origin (low order byte)
	mYOffsetHi = 0;		// Y offset from origin (hi order byte)

	// Height and width
	mWidthLo = U8(getWidth() & 0xFF);			// Width (low order byte)
	mWidthHi = U8((getWidth() >> 8) & 0xFF);	// Width (hi order byte)
	mHeightLo = U8(getHeight() & 0xFF);			// Height (low order byte)
	mHeightHi = U8((getHeight() >> 8) & 0xFF);	// Height (hi order byte)

	S32 bytes_per_pixel;
	switch (getComponents())
	{
	case 1:
		bytes_per_pixel = 1;
		break;
	case 3:
		bytes_per_pixel = 3;
		break;
	case 2:	 // Interpret as intensity plus alpha. Store as RGBA.
	case 4:
		bytes_per_pixel = 4;
		break;
	default:
		return false;
	}
	mPixelSize = U8(bytes_per_pixel * 8);		// 8, 16, 24, 32 bits per pixel

	mAttributeBits = (4 == bytes_per_pixel) ? 8 : 0;	// 4 bits: number of attribute bits (alpha) per pixel
	mOriginRightBit = 0;	// 1 bit: origin, 0 = left, 1 = right
	mOriginTopBit = 0;	// 1 bit: origin, 0 = bottom, 1 = top
	mInterleave = 0;	// 2 bits: interleaved flag, 0 = none, 1 = interleaved 2, 2 = interleaved 4

	constexpr S32 TGA_HEADER_SIZE = 18;
	constexpr S32 COLOR_MAP_SIZE = 0;
	mDataOffset = TGA_HEADER_SIZE + mIDLength + COLOR_MAP_SIZE; // Offset from start of data to the actual header.

	S32 pixels = getWidth() * getHeight();
	S32 datasize = mDataOffset + bytes_per_pixel * pixels;
	U8* dst = allocateData(datasize);
	if (!dst) return false;

	// Write header
	*(dst++) = mIDLength;
	*(dst++) = mColorMapType;
	*(dst++) = mImageType;
	*(dst++) = mColorMapIndexLo;
	*(dst++) = mColorMapIndexHi;
	*(dst++) = mColorMapLengthLo;
	*(dst++) = mColorMapLengthHi;
	*(dst++) = mColorMapDepth;
	*(dst++) = mXOffsetLo;
	*(dst++) = mXOffsetHi;
	*(dst++) = mYOffsetLo;
	*(dst++) = mYOffsetHi;
	*(dst++) = mWidthLo;
	*(dst++) = mWidthHi;
	*(dst++) = mHeightLo;
	*(dst++) = mHeightHi;
	*(dst++) = mPixelSize;
	*(dst++) = ((mInterleave & 3) << 5) | ((mOriginTopBit & 1) << 4) |
			   ((mOriginRightBit & 1) << 3) | ((mAttributeBits & 0xF) << 0);

	// Write pixels
	const U8* src = raw_image->getData();
	llassert(dst == getData() + mDataOffset);
	S32 i = 0;
	S32 j = 0;
	switch (getComponents())
	{
	case 1:
		memcpy(dst, src, bytes_per_pixel * pixels);
		break;

	case 2:
		while (pixels--)
		{
			dst[i] = src[j];			// intensity
			dst[i + 1] = src[j];		// intensity
			dst[i + 2] = src[j];		// intensity
			dst[i + 3] = src[j + 1];	// alpha
			i += 4;
			j += 2;
		}
		break;

	case 3:
		while (pixels--)
		{
			dst[i] = src[i + 2];	// blue
			dst[i + 1] = src[i + 1];	// green
			dst[i + 2] = src[i];	// red
			i += 3;
		}
		break;

	case 4:
		while (pixels--)
		{
			dst[i] = src[i + 2];		// blue
			dst[i + 1] = src[i + 1];	// green
			dst[i + 2] = src[i];		// red
			dst[i + 3] = src[i + 3];	// alpha
			i += 4;
		}
		break;
	}

	return true;
}

bool LLImageTGA::decodeTruecolorRle32(LLImageRaw* raw_image,
									  bool& alpha_opaque)
{
	llassert(getComponents() == 4);

	if (!raw_image)
	{
		llwarns << "Attempted to decode a NULL raw image" << llendl;
		llassert(false);
		return false;
	}

	alpha_opaque = true;

	U8* dst = raw_image->getData();
	U32* dst_pixels = (U32*)dst;

	U8* src = getData() + mDataOffset;
	U8* last_src = src + getDataSize();

	U32 rgba;
	U8* rgba_byte_p = (U8*) &rgba;

	U32* last_dst_pixel = dst_pixels + getHeight() * getWidth() - 1;
	while (dst_pixels <= last_dst_pixel)
	{
		// Read RLE block header

		if (src >= last_src)
		{
			return false;
		}

		U8 block_header_byte = *src++;

		U32 block_pixel_count = (block_header_byte & 0x7F) + 1;
		if (block_header_byte & 0x80)
		{
			// Encoded (duplicate-pixel) block

			if (src + 3 >= last_src)
			{
				return false;
			}

			rgba_byte_p[0] = src[2];
			rgba_byte_p[1] = src[1];
			rgba_byte_p[2] = src[0];
			rgba_byte_p[3] = src[3];
			if (rgba_byte_p[3] != 255)
			{
				alpha_opaque = false;
			}

			src += 4;
			U32 value = rgba;
			do
			{
				*dst_pixels = value;
				++dst_pixels;
				--block_pixel_count;
			}
			while (block_pixel_count > 0);
		}
		else
		{
			// Unencoded block
			do
			{
				if (src + 3 >= last_src)
				{
					return false;
				}

				((U8*)dst_pixels)[0] = src[2];
				((U8*)dst_pixels)[1] = src[1];
				((U8*)dst_pixels)[2] = src[0];
				((U8*)dst_pixels)[3] = src[3];
				if (src[3] != 255)
				{
					alpha_opaque = false;
				}
				src += 4;
				++dst_pixels;
				--block_pixel_count;
			}
			while (block_pixel_count > 0);
		}
	}

	return true;
}

bool LLImageTGA::decodeTruecolorRle15(LLImageRaw* raw_image)
{
	llassert(getComponents() == 3);
	llassert(mIs15Bit);

	if (!raw_image)
	{
		llwarns << "Attempted to decode a NULL raw image" << llendl;
		llassert(false);
		return false;
	}

	U8* dst = raw_image->getData();
	U8* src = getData() + mDataOffset;

	U8* last_src = src + getDataSize();
	U8* last_dst = dst + getComponents() * (getHeight() * getWidth() - 1);

	while (dst <= last_dst)
	{
		// Read RLE block header

		if (src >= last_src)
		{
			return false;
		}

		U8 block_header_byte = *src++;

		U8 block_pixel_count = (block_header_byte & 0x7F) + 1;
		if (block_header_byte & 0x80)
		{
			// Encoded (duplicate-pixel) block
			do
			{
				if (src + 2 >= last_src)
				{
					return false;
				}

				decodeTruecolorPixel15(dst, src);   // slow
				dst += 3;
				--block_pixel_count;
			}
			while (block_pixel_count > 0);
			src += 2;
		}
		else
		{
			// Unencoded block
			do
			{
				if (src + 2 >= last_src)
				{
					return false;
				}

				decodeTruecolorPixel15(dst, src);
				dst += 3;
				src += 2;
				--block_pixel_count;
			}
			while (block_pixel_count > 0);
		}
	}

	return true;
}

bool LLImageTGA::decodeTruecolorRle24(LLImageRaw* raw_image)
{
	llassert(getComponents() == 3);

	if (!raw_image)
	{
		llwarns << "Attempted to decode a NULL raw image" << llendl;
		llassert(false);
		return false;
	}

	U8* dst = raw_image->getData();
	U8* src = getData() + mDataOffset;

	U8* last_src = src + getDataSize();
	U8* last_dst = dst + getComponents() * (getHeight() * getWidth() - 1);

	while (dst <= last_dst)
	{
		// Read RLE block header

		if (src >= last_src)
		{
			return false;
		}

		U8 block_header_byte = *src++;

		U8 block_pixel_count = (block_header_byte & 0x7F) + 1;
		if (block_header_byte & 0x80)
		{
			// Encoded (duplicate-pixel) block
			do
			{
				if (src + 2 >= last_src)
					return false;
				dst[0] = src[2];
				dst[1] = src[1];
				dst[2] = src[0];
				dst += 3;
				--block_pixel_count;
			}
			while (block_pixel_count > 0);
			src += 3;
		}
		else
		{
			// Unencoded block
			do
			{
				if (src + 2 >= last_src)
				{
					return false;
				}

				dst[0] = src[2];
				dst[1] = src[1];
				dst[2] = src[0];
				dst += 3;
				src += 3;
				--block_pixel_count;
			}
			while (block_pixel_count > 0);
		}
	}

	return true;
}

bool LLImageTGA::decodeTruecolorRle8(LLImageRaw* raw_image)
{
	llassert(getComponents() == 1);

	if (!raw_image)
	{
		llwarns << "Attempted to decode a NULL raw image" << llendl;
		llassert(false);
		return false;
	}

	U8* dst = raw_image->getData();
	U8* src = getData() + mDataOffset;

	U8* last_src = src + getDataSize();
	U8* last_dst = dst + getHeight() * getWidth() - 1;

	while (dst <= last_dst)
	{
		// Read RLE block header

		if (src >= last_src)
		{
			return false;
		}

		U8 block_header_byte = *src++;

		U8 block_pixel_count = (block_header_byte & 0x7F) + 1;
		if (block_header_byte & 0x80)
		{
			if (src >= last_src)
				return false;

			// Encoded (duplicate-pixel) block
			memset(dst, *src, block_pixel_count);
			dst += block_pixel_count;
			++src;
		}
		else
		{
			// Unencoded block
			do
			{
				if (src >= last_src)
				{
					return false;
				}

				*dst++ = *src++;
				--block_pixel_count;
			}
			while (block_pixel_count > 0);
		}
	}

	return true;
}

// Decoded and process the image for use in avatar gradient masks. Processing
// happens during the decode for speed.
bool LLImageTGA::decodeAndProcess(LLImageRaw* raw_image, F32 domain,
								  F32 weight)
{
	if (!raw_image)
	{
		llwarns << "Attempted to decode a NULL raw image" << llendl;
		llassert(false);
		return false;
	}

	// "Domain" isn't really the right word. It refers to the width of the ramp
	// portion of the function that relates input and output pixel values.
	// A domain of 0 gives a step function.
	//
	//   |                      /----------------
	//  O|                     / |
	//  u|                    /  |
	//  t|                   /   |
	//	p|------------------/    |
	//  u|                  |    |
	//  t|<---------------->|<-->|
	//	 |  "offset"         "domain"
	//   |
	// --+---Input--------------------------------
	//   |

	if (!getData() || getDataSize() == 0)
	{
		setLastError("Trying to decode an image with no data!");
		return false;
	}

	// Only works for unflipped monochrome RLE images
	if (getComponents() != 1 || mImageType != 11 || mOriginTopBit ||
		mOriginRightBit)
	{
		llwarns << "Trying to alpha-gradient process an image that's not a standard RLE, one component image"
				<< llendl;
		llassert(false);
		return false;
	}

	if (!raw_image->resize(getWidth(), getHeight(), getComponents()))
	{
		setLastError("LLImageTGA failed to resize image");
		return false;
	}

	U8* dst = raw_image->getData();
	U8* src = getData() + mDataOffset;
	U8* last_dst = dst + getHeight() * getWidth() - 1;

	if (domain > 0)
	{
		// Process using a look-up table (lut)
		constexpr S32 LUT_LEN = 256;
		U8 lut[LUT_LEN];
		S32 i;

		F32 scale = 1.f / domain;
		F32 offset = (1.f - domain) * llclampf(1.f - weight);
		F32 bias = -scale * offset;

		for (i = 0; i < LUT_LEN; ++i)
		{
			lut[i] = (U8)llclampb(255.f * (i/255.f * scale + bias));
		}

		while (dst <= last_dst)
		{
			// Read RLE block header
			U8 block_header_byte = *src++;

			U8 block_pixel_count = (block_header_byte & 0x7F) + 1;
			if (block_header_byte & 0x80)
			{
				// Encoded (duplicate-pixel) block
				memset(dst, lut[*src], block_pixel_count);
				dst += block_pixel_count;
				++src;
			}
			else
			{
				// Unencoded block
				do
				{
					*dst++ = lut[*src++];
					--block_pixel_count;
				}
				while (block_pixel_count > 0);
			}
		}
	}
	else
	{
		// Process using a simple comparison agains a threshold
		const U8 threshold = (U8)(0xFF * llclampf(1.f - weight));

		while (dst <= last_dst)
		{
			// Read RLE block header
			U8 block_header_byte = *src;
			src++;

			U8 block_pixel_count = (block_header_byte & 0x7F) + 1;
			if (block_header_byte & 0x80)
			{
				// Encoded (duplicate-pixel) block
				memset(dst, ((*src >= threshold) ? 0xFF : 0), block_pixel_count);
				dst += block_pixel_count;
				++src;
			}
			else
			{
				// Unencoded block
				do
				{
					*dst++ = (*src++ >= threshold ? 0xFF : 0);
					--block_pixel_count;
				}
				while (block_pixel_count > 0);
			}
		}
	}

	return true;
}

// Reads a .tga file and creates an LLImageTGA with its data.
bool LLImageTGA::loadFile(const std::string& path)
{
	S32 len = path.size();
	if (len < 5)
	{
		return false;
	}

	std::string extension = gDirUtilp->getExtension(path);
	if ("tga" != extension)
	{
		return false;
	}

	LLFILE* file = LLFile::open(path, "rb");
	if (!file)
	{
		llwarns << "Couldn't open file " << path << llendl;
		return false;
	}

	S32 file_size = 0;
	if (!fseek(file, 0, SEEK_END))
	{
		file_size = ftell(file);
		fseek(file, 0, SEEK_SET);
	}

	U8* buffer = allocateData(file_size);
	if (!buffer)
	{
		LLFile::close(file);
		llwarns << "Couldn't allocate memory to load file " << path << llendl;
		return false;
	}

	S32 bytes_read = fread(buffer, 1, file_size, file);
	LLFile::close(file);

	if (bytes_read != file_size)
	{
		deleteData();
		llwarns << "Couldn't read file " << path << llendl;
		return false;
	}

	if (!updateData())
	{
		llwarns << "Couldn't decode file " << path << llendl;
		deleteData();
		return false;
	}

	return true;
}
