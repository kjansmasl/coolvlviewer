/**
 * @file llimagejpeg.h
 * @brief This class compresses and decompresses JPEG files
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

#ifndef LL_LLIMAGEJPEG_H
#define LL_LLIMAGEJPEG_H

#include "llwin32headerslean.h"

#include "llimage.h"

extern "C" {
#include "jpeglib.h"
#include "jerror.h"
}

class LLImageJPEG final : public LLImageFormatted
{
protected:
	LOG_CLASS(LLImageJPEG);

	~LLImageJPEG() override;

public:
	LLImageJPEG(S32 quality = 75);

	LL_INLINE std::string getExtension() override	{ return "jpg"; }
	bool updateData() override;
	bool decode(LLImageRaw* raw_image) override;
	bool encode(const LLImageRaw* raw_image) override;

	// On a scale from 1 to 100
	LL_INLINE void setEncodeQuality(S32 q)			{ mEncodeQuality = q; }
	LL_INLINE S32 getEncodeQuality()				{ return mEncodeQuality; }

private:
	// Callbacks registered with jpeglib
	static void encodeInitDestination(j_compress_ptr cinfo);
	static boolean encodeEmptyOutputBuffer(j_compress_ptr cinfo);
	static void encodeTermDestination(j_compress_ptr cinfo);

	static void decodeInitSource(j_decompress_ptr cinfo);
	static boolean decodeFillInputBuffer(j_decompress_ptr cinfo);
	static void decodeSkipInputData(j_decompress_ptr cinfo, long num_bytes);
	static void decodeTermSource(j_decompress_ptr cinfo);

	static void errorExit(j_common_ptr cinfo);
	static void errorEmitMessage(j_common_ptr cinfo, int msg_level);
	static void errorOutputMessage(j_common_ptr cinfo);

private:
	U8* mOutputBuffer;		// Temporary buffer used during encoding
	S32 mOutputBufferSize;	// Bytes in mOuputBuffer
	S32 mEncodeQuality;		// On a scale from 1 to 100
};

#endif  // LL_LLIMAGEJPEG_H
