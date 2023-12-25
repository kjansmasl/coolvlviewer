/**
 * @file llimagedecodethread.h
 * @brief Image decode thread class declaration.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLIMAGEDECODETHREAD_H
#define LL_LLIMAGEDECODETHREAD_H

#include <memory>

#include "llimage.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llthreadpool.h"

class LLImageDecodeThread
{
protected:
	LOG_CLASS(LLImageDecodeThread);

public:
	class Responder : public LLThreadSafeRefCount
	{
	protected:
		LOG_CLASS(LLImageDecodeThread::Responder);

		~Responder() override = default;

	public:
		virtual void completed(bool success, LLImageRaw* raw,
							   LLImageRaw* aux) = 0;
	};

	// 'pool_size' is the number of LLThreads that will be launched. When
	// omitted or equal to 0, this number is determined automatically
	// depending on the available threading concurrency.
	LLImageDecodeThread(U32 pool_size = 0);

	void shutdown();

	size_t getPending();

	bool decodeImage(const LLPointer<LLImageFormatted>& image, S32 discard,
					 bool needs_aux, const LLPointer<Responder>& responder);

private:
	class ImageRequest
	{
	protected:
		LOG_CLASS(LLImageDecodeThread::ImageRequest);

	public:
		ImageRequest(const LLPointer<LLImageFormatted>& image,
					 S32 discard, bool needs_aux,
					 const LLPointer<Responder>& responder);
		~ImageRequest();

		bool processRequest();
		void finishRequest(bool completed);

	private:
		// Input
		LLPointer<LLImageFormatted>					mFormattedImage;
		LLPointer<LLImageRaw>						mDecodedImageRaw;
		LLPointer<LLImageRaw>						mDecodedImageAux;
		LLPointer<LLImageDecodeThread::Responder>	mResponder;
		S32											mDiscardLevel;
		bool										mNeedsAux;
		// Output
		bool										mDecodedRaw;
		bool										mDecodedAux;
	};

private:
	std::unique_ptr<LLThreadPool>					mThreadPoolp;
};

// Global, initialized in llappviewer.cpp and used in newview/. Moved here so
// that LLImageDecodeThread consumers do not need to include llappviewer.h to
// use it. HB
extern LLImageDecodeThread* gImageDecodeThreadp;

#endif
