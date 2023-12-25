/**
 * @file llimagej2c.h
 * @brief Image implementation for jpeg2000.
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

#ifndef LL_LLIMAGEJ2C_H
#define LL_LLIMAGEJ2C_H

#include "llerror.h"
#include "llimage.h"
#include "llassettype.h"

class LLImageJ2C final : public LLImageFormatted
{
protected:
	LOG_CLASS(LLImageJ2C);

	~LLImageJ2C() override = default;

public:
	LLImageJ2C();

	// Base class overrides

	std::string getExtension() override				{ return "j2c"; }

	bool updateData() override;

	LL_INLINE bool decode(LLImageRaw* raw_imagep) override
	{
		return decodeChannels(raw_imagep, 0, 4);
	}

	bool decodeChannels(LLImageRaw* raw_imagep, S32 first_channel,
						S32 max_channel_count) override;

	LL_INLINE bool encode(const LLImageRaw* raw_imagep) override
	{
		return raw_imagep && encode(raw_imagep, NULL);
	}

	LL_INLINE S32 calcHeaderSize() override
	{
		// *HACK: just needs to be >= actual header size...
		return FIRST_PACKET_SIZE;
	}

	S32 calcDataSize(S32 discard_level = 0) override;
	S32 calcDiscardLevelBytes(S32 bytes) override;
	LL_INLINE S8 getRawDiscardLevel() override		{ return mRawDiscardLevel; }

	// Override these so that we do not try to set a global variable from a DLL
	LL_INLINE void resetLastError() override		{ mLastError.clear(); }
	void setLastError(const std::string& message,
					  const std::string& filename = std::string()) override;

	// Encode with comment text
	bool encode(const LLImageRaw* raw_imagep, const char* comment_text);

	bool validate(U8* data, U32 file_size);
	bool loadAndValidate(const std::string& filename);

	// Encode accessors
	// Use non-lossy ?
	LL_INLINE void setReversible(bool b)			{ mReversible = b; }
	LL_INLINE void setRate(F32 rate)				{ mRate = rate; }
	LL_INLINE void setMaxBytes(S32 max_bytes)		{ mMaxBytes = max_bytes; }
	LL_INLINE S32 getMaxBytes() const				{ return mMaxBytes; }

	static S32 calcDataSizeJ2C(S32 w, S32 h, S32 comp, S32 discard_level,
							   F32 rate = 0.f);

	static std::string getEngineInfo();

protected:
	void updateRawDiscardLevel();

	// Finds out the image size and number of channels. Returns true if image
	// size and number of channels was determined, false otherwise.
	bool getMetadata();
	// Fast header-based scan method.
	bool getMetadataFast(S32& width, S32& height, S32& comps);

	bool decodeImpl(LLImageRaw& raw_image, S32 first_channel,
					S32 max_channel_count);
	bool encodeImpl(const LLImageRaw& raw_image, const char* comment_text);

	static void eventMgrCallback(const char* msg, void*);
	static void initEventManager();

protected:
	std::string	mLastError;
	F32			mRate;
	S32			mMaxBytes;			// Maximum number of bytes of data to use.
	S8			mRawDiscardLevel;
	bool		mReversible;
};

#endif
