/**
 * @file llaudiodecodemgr.cpp
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include <iterator> // for VS2010
#include <deque>

#include "llaudiodecodemgr.h"

#include "llapp.h"
#include "llassetstorage.h"
#include "llaudioengine.h"
#include "lldir.h"
#include "llendianswizzle.h"
#include "llfilesystem.h"
#include "llmemory.h"
#include "llrefcount.h"
#include "llstring.h"
#include "llworkqueue.h"

#include "llvorbisencode.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

extern LLAudioEngine* gAudiop;

LLAudioDecodeMgr* gAudioDecodeMgrp = NULL;

U32 LLAudioDecodeMgr::sMaxDecodes = 0;

constexpr S32 WAV_HEADER_SIZE = 44;

//////////////////////////////////////////////////////////////////////////////

class LLVorbisDecodeState : public LLThreadSafeRefCount
{
protected:
	LOG_CLASS(LLVorbisDecodeState);

	virtual ~LLVorbisDecodeState();

public:
	typedef LLPointer<LLVorbisDecodeState> ptr_t;

	LLVorbisDecodeState(const LLUUID& id, const std::string& out_filename);

	bool initDecode();
	bool decodeSection(); // Return true if done.
	void finishDecode();

	LL_INLINE bool isValid() const				{ return mValid; }
	LL_INLINE bool isDone() const				{ return mDone; }
	LL_INLINE bool isWritten() const			{ return mWritten; }
	LL_INLINE const LLUUID& getUUID() const		{ return mUUID; }

private:
	void flushBadFile();

protected:
	LLFileSystem*			mInFilep;

	std::string				mOutFilename;

	std::vector<U8>			mWAVBuffer;

	LLUUID					mUUID;

	OggVorbis_File			mVF;

	S32						mCurrentSection;

	bool					mValid;
	bool					mDone;
	bool					mWritten;
};

static size_t cache_read(void* ptr, size_t size, size_t nmemb, void* userdata)
{
	if (size <= 0 || nmemb <= 0) return 0;

	LLFileSystem* file = (LLFileSystem*)userdata;

	if (file->read((U8*)ptr, (S32)(size * nmemb)))
	{
		S32 read = file->getLastBytesRead();
		return read / size;
	}
	return 0;
}

static S32 cache_seek(void* userdata, ogg_int64_t offset, S32 whence)
{
	// Cache has 31 bits files
	if (offset > S32_MAX)
	{
		return -1;
	}

	LLFileSystem* file = (LLFileSystem*)userdata;

	S32 origin = 0;
	switch (whence)
	{
		case SEEK_SET:
			break;

		case SEEK_END:
			origin = file->getSize();
			break;

		case SEEK_CUR:
			origin = -1;
			break;

		default:
			llerrs << "Invalid whence argument" << llendl;
	}

	return file->seek((S32)offset, origin) ? 0 : -1;
}

static long cache_tell(void* userdata)
{
	return ((LLFileSystem*)userdata)->tell();
}

LLVorbisDecodeState::LLVorbisDecodeState(const LLUUID& id,
										 const std::string& out_filename)
:	mUUID(id),
	mOutFilename(out_filename),
	mInFilep(NULL),
	mCurrentSection(0),
	mDone(false),
	mValid(false),
	mWritten(false)
{
	// No default value for mVF, is it an OGG structure ?
}

LLVorbisDecodeState::~LLVorbisDecodeState()
{
	if (mInFilep)
	{
		delete mInFilep;
		// The fact that mInFilep is not NULL is a sign that mVF got
		// initialized via ov_open_callbacks(), so we must clear it. HB
		ov_clear(&mVF);
	}
}

void LLVorbisDecodeState::flushBadFile()
{
	if (mInFilep)
	{
		llwarns << "Removing bad (invalid vorbis data) cache file for asset: "
				<< mUUID << llendl;
		mInFilep->remove();
	}
}

bool LLVorbisDecodeState::initDecode()
{
	ov_callbacks cache_callbacks;
	cache_callbacks.read_func = cache_read;
	cache_callbacks.seek_func = cache_seek;
	// We manage mInFilep life ourselves !  HB
	cache_callbacks.close_func = NULL;
	cache_callbacks.tell_func = cache_tell;

	mInFilep = new LLFileSystem(mUUID);
	if (!mInFilep)
	{
		llwarns << "Unable to open cache file for reading asset: " << mUUID
				<< llendl;
		return false;
	}
	if (!mInFilep->getSize())
	{
		llwarns << "Empty cache file for asset: " << mUUID << llendl;
		flushBadFile();
		// This ensures we will not attempt to clear mVF in the destructor,
		// since it was not allocated via ov_open_callbacks() below. HB
		delete mInFilep;
		mInFilep = NULL;
		return false;
	}

	S32 r = ov_open_callbacks(mInFilep, &mVF, NULL, 0, cache_callbacks);
	if (r < 0)
	{
		llwarns << "Error " << r
				<< " while opening Vorbis data streal for asset " << mUUID
				<< ". This does not appear to be an Ogg bitstream." << llendl;
		flushBadFile();
		return false;
	}

	S32 sample_count = ov_pcm_total(&mVF, -1);
	size_t size_guess = (size_t)sample_count;
	vorbis_info* vi = ov_info(&mVF, -1);
	size_guess *= vi->channels;
	size_guess *= 2;
	size_guess += 2048;

	bool abort_decode = false;

	if (vi->channels < 1 || vi->channels > LLVORBIS_CLIP_MAX_CHANNELS)
	{
		abort_decode = true;
		llwarns << "Bad channel count: " << vi->channels << llendl;
	}

	if ((size_t)sample_count > LLVORBIS_CLIP_REJECT_SAMPLES)
	{
		abort_decode = true;
		llwarns << "Illegal sample count: " << sample_count << llendl;
	}

	if (size_guess > LLVORBIS_CLIP_REJECT_SIZE)
	{
		abort_decode = true;
		llwarns << "Illegal sample size: " << size_guess << llendl;
	}

	if (abort_decode)
	{
		llwarns << "Cancelling initDecode. Bad asset: " << mUUID
				<< "Bad asset encoded by: "
				<< ov_comment(&mVF, -1)->vendor << llendl;
		flushBadFile();
		return false;
	}

	try
	{
		mWAVBuffer.reserve(size_guess);
		mWAVBuffer.resize(WAV_HEADER_SIZE);
	}
	catch (const std::bad_alloc&)
	{
		llwarns << "Failure to allocate buffer for asset: " << mUUID << llendl;
		LLMemory::allocationFailed(size_guess);
		return false;
	}

	// Write the .wav format header

	// "RIFF"
	mWAVBuffer[0] = 0x52;
	mWAVBuffer[1] = 0x49;
	mWAVBuffer[2] = 0x46;
	mWAVBuffer[3] = 0x46;

	// Length = datalen + 36 (to be filled in later)
	mWAVBuffer[4] = 0x00;
	mWAVBuffer[5] = 0x00;
	mWAVBuffer[6] = 0x00;
	mWAVBuffer[7] = 0x00;

	// "WAVE"
	mWAVBuffer[8] = 0x57;
	mWAVBuffer[9] = 0x41;
	mWAVBuffer[10] = 0x56;
	mWAVBuffer[11] = 0x45;

	// "fmt "
	mWAVBuffer[12] = 0x66;
	mWAVBuffer[13] = 0x6D;
	mWAVBuffer[14] = 0x74;
	mWAVBuffer[15] = 0x20;

	// Chunk size = 16
	mWAVBuffer[16] = 0x10;
	mWAVBuffer[17] = 0x00;
	mWAVBuffer[18] = 0x00;
	mWAVBuffer[19] = 0x00;

	// Format (1 = PCM)
	mWAVBuffer[20] = 0x01;
	mWAVBuffer[21] = 0x00;

	// Number of channels
	mWAVBuffer[22] = 0x01;
	mWAVBuffer[23] = 0x00;

	// Samples per second
	mWAVBuffer[24] = 0x44;
	mWAVBuffer[25] = 0xAC;
	mWAVBuffer[26] = 0x00;
	mWAVBuffer[27] = 0x00;

	// Average bytes per second
	mWAVBuffer[28] = 0x88;
	mWAVBuffer[29] = 0x58;
	mWAVBuffer[30] = 0x01;
	mWAVBuffer[31] = 0x00;

	// Bytes to output at a single time
	mWAVBuffer[32] = 0x02;
	mWAVBuffer[33] = 0x00;

	// 16 bits per sample
	mWAVBuffer[34] = 0x10;
	mWAVBuffer[35] = 0x00;

	// "data"
	mWAVBuffer[36] = 0x64;
	mWAVBuffer[37] = 0x61;
	mWAVBuffer[38] = 0x74;
	mWAVBuffer[39] = 0x61;

	// These are the length of the data chunk, to be filled in later
	mWAVBuffer[40] = 0x00;
	mWAVBuffer[41] = 0x00;
	mWAVBuffer[42] = 0x00;
	mWAVBuffer[43] = 0x00;

#if 0
	char** ptr = ov_comment(&mVF, -1)->user_comments;
	vorbis_info* vi = ov_info(&vf, -1);
	while (*ptr)
	{
		fprintf(stderr,"%s\n", *ptr++);
	}
	fprintf(stderr, "\nBitstream is %d channel, %ldHz\n", vi->channels,
			vi->rate);
	fprintf(stderr, "\nDecoded length: %ld samples\n",
			(long)ov_pcm_total(&vf, -1));
	fprintf(stderr, "Encoded by: %s\n\n", ov_comment(&vf, -1)->vendor);
#endif

	return true;
}

bool LLVorbisDecodeState::decodeSection()
{
	if (!mInFilep)
	{
		llwarns << "No cache file to decode for " << mUUID << llendl;
		return true;
	}
	if (mDone)
	{
 		LL_DEBUGS("Audio") << "Already done with decode for " << mUUID
						   << LL_ENDL;
		return true;
	}

	char pcmout[4096];
	long ret = ov_read(&mVF, pcmout, sizeof(pcmout), 0, 2, 1,
					   &mCurrentSection);
	if (ret == 0)
	{
		// EOF
		mDone = true;
		mValid = true;
		// We are done, return true.
		return true;
	}

	if (ret < 0)
	{
		// Error in the stream. Not a problem, just reporting it in case we
		// (the app) cares. In this case, we do not.
		llwarns << "Bad vorbis decode for" << mUUID << llendl;
		mValid = false;
		mDone = true;
		flushBadFile();
		// We are done, return true.
		return true;
	}

	// We do not bother dealing with sample rate changes, etc, but you will
	// have to
	std::copy(pcmout, pcmout + ret, std::back_inserter(mWAVBuffer));

	// We are not yet done, return false.
	return false;
}

void LLVorbisDecodeState::finishDecode()
{
	// Write "data" chunk length, in little-endian format
	S32 data_length = mWAVBuffer.size() - WAV_HEADER_SIZE;
	mWAVBuffer[40] = (data_length) & 0x000000FF;
	mWAVBuffer[41] = (data_length >> 8) & 0x000000FF;
	mWAVBuffer[42] = (data_length >> 16) & 0x000000FF;
	mWAVBuffer[43] = (data_length >> 24) & 0x000000FF;

	// Write overall "RIFF" length, in little-endian format
	data_length += 36;
	mWAVBuffer[4] = (data_length) & 0x000000FF;
	mWAVBuffer[5] = (data_length >> 8) & 0x000000FF;
	mWAVBuffer[6] = (data_length >> 16) & 0x000000FF;
	mWAVBuffer[7] = (data_length >> 24) & 0x000000FF;

	// Vorbis encode/decode messes up loop point transitions (pop)do a
	// cheap-and-cheesy crossfade
	S16* samplep;
	char pcmout[4096];

	S32 fade_length = llmin(128, (data_length - 36) / 8);
	if ((S32)mWAVBuffer.size() >= WAV_HEADER_SIZE + 2 * fade_length)
	{
		memcpy(pcmout, &mWAVBuffer[WAV_HEADER_SIZE], 2 * fade_length);
	}
	llendianswizzle(&pcmout, 2, fade_length);

	samplep = (S16*)pcmout;
	for (S32 i = 0 ; i < fade_length; ++i)
	{
		*samplep = llfloor(F32(*samplep) * (F32)i / (F32)fade_length);
		++samplep;
	}

	llendianswizzle(&pcmout, 2, fade_length);
	if (WAV_HEADER_SIZE + 2 * fade_length < (S32)mWAVBuffer.size())
	{
		memcpy(&mWAVBuffer[WAV_HEADER_SIZE], pcmout, 2 * fade_length);
	}
	S32 near_end = mWAVBuffer.size() - 2 * fade_length;
	if ((S32)mWAVBuffer.size() >= near_end + 2 * fade_length)
	{
		memcpy(pcmout, &mWAVBuffer[near_end], (2 * fade_length));
	}
	llendianswizzle(&pcmout, 2, fade_length);
	samplep = (S16*)pcmout;
	for (S32 i = fade_length - 1; i >= 0; --i)
	{
		*samplep = llfloor(F32(*samplep) * (F32)i / (F32)fade_length);
		++samplep;
	}

	llendianswizzle(&pcmout, 2, fade_length);
	if (near_end + 2 * fade_length < (S32)mWAVBuffer.size())
	{
		memcpy(&mWAVBuffer[near_end], pcmout, 2 * fade_length);
	}

	if (data_length == 36)
	{
		llwarns_once << "Bad Vorbis decode for " << mUUID << ", aborting."
					 << llendl;
		mValid = false;
		flushBadFile();
		return; // We have finished
	}

	LL_DEBUGS("Audio") << "Starting file write for " << mUUID << LL_ENDL;
	LLFile oufile(mOutFilename, "wb");
	S64 file_size = mWAVBuffer.size();
	if (oufile.write(mWAVBuffer.data(), file_size) == file_size)
	{
		mDone = true;
		LL_DEBUGS("Audio") << "Decoded file written for " << mUUID
						   << LL_ENDL;
	}
	else
	{
		llwarns << "Unable to write file for " << mUUID << llendl;
		mValid = false;
	}
	mWritten = true;
}

//////////////////////////////////////////////////////////////////////////////

class LLAudioDecodeMgr::Impl
{
	friend class LLAudioDecodeMgr;

protected:
	LOG_CLASS(LLAudioDecodeMgr::Impl);

public:
	Impl() = default;

	// Called from the main thread only.
	LL_INLINE void processQueue()
	{
		checkDecodesFinished();
		startMoreDecodes();
	}

protected:
	void enqueueFinishAudio(const LLUUID& id,
							LLVorbisDecodeState::ptr_t& state);

private:
	// Starts as many decodes from the queue as permitted.
	void startMoreDecodes();
	// Checks if any audio from in-progress decodes are ready to play. If so,
	// mark them ready for playback (or errored, in case of error).
	void checkDecodesFinished();
	// Returns the in-progress decode state, which may be an empty LLPointer if
	// there was an error and there is no more work to be done.
	LLVorbisDecodeState::ptr_t beginDecode(const LLUUID& id);
	// Returns true if finished.
	bool tryFinishAudio(const LLUUID& id, LLVorbisDecodeState::ptr_t state);

protected:
	std::deque<LLUUID>	mDecodeQueue;
	typedef fast_hmap<LLUUID, LLVorbisDecodeState::ptr_t> decodes_map_t;
	decodes_map_t		mDecodes;
	uuid_list_t			mBadAssetList;
};

// Called from the main thread only.
void LLAudioDecodeMgr::Impl::startMoreDecodes()
{
	if (!sMaxDecodes)
	{
		// At this point the "General" queue has not not yet been initialized.
		// Bail out to avoid storing an empty static weak pointer below ! HB
		LL_DEBUGS("Audio") << "General queue not yet ready. Aborting."
						   << LL_ENDL;
		return;
	}

	if (LLApp::isExiting())
	{
		return;
	}

	static LLWorkQueue::weak_t main_queue =
		LLWorkQueue::getNamedInstance("mainloop");
	static LLWorkQueue::weak_t general_queue =
		LLWorkQueue::getNamedInstance("General");

	auto mainq = main_queue.lock();
	if (!mainq)
	{
		LL_DEBUGS("Audio") << "Main queue is gone !  Aborting." << LL_ENDL;
		return;	// Queue is gone !
	}

	while (gAudioDecodeMgrp && !LLApp::isExiting() && !mDecodeQueue.empty())
	{
		const LLUUID id = mDecodeQueue.front();
		mDecodeQueue.pop_front();
		if (mDecodes.count(id) || (gAudiop && gAudiop->hasDecodedFile(id)))
		{
			// Do not decode the same file twice
			LL_DEBUGS("Audio") << id
							   << " is already decoded or queued for decoding."
							   << LL_ENDL;
			continue;
		}

		// Kick off a decode
		mDecodes.emplace(id, LLVorbisDecodeState::ptr_t(NULL));
		if (mainq->isClosed() ||
			!mainq->postTo(general_queue,
						   // Work done on general queue
						   [id, this]()
						   {
								 LLVorbisDecodeState::ptr_t state =
									beginDecode(id);
								 return state;
						   },
						   // Callback to main thread
						   [id, this](LLVorbisDecodeState::ptr_t state) mutable
						   {
								// Let's ensure 'this' is still valid... HB
								if (gAudioDecodeMgrp)
								{
									enqueueFinishAudio(id, state);
								}
						   }))
		{
			LL_DEBUGS("Audio") << "Failed to post decode for " << id
							   << LL_ENDL;
			break;
		}
		LL_DEBUGS("Audio") << "Posted decode to \"General\" queue for " << id
						   << LL_ENDL;
		if (mDecodes.size() > sMaxDecodes)
		{
			LL_DEBUGS("Audio") << "Decodes queue is full ("
							   << mDecodes.size() << "/" << sMaxDecodes
							   << ")" << LL_ENDL;
			break;
		}
	}
}

// Called from worker thread only.
LLVorbisDecodeState::ptr_t LLAudioDecodeMgr::Impl::beginDecode(const LLUUID& id)
{
	LL_DEBUGS("Audio") << "Decoding " << id << " from audio queue." << LL_ENDL;
	std::string d_path = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														id.asString()) +
						 ".dsf";
	LLVorbisDecodeState::ptr_t state = new LLVorbisDecodeState(id, d_path);
	if (!state->initDecode())
	{
		mBadAssetList.emplace(state->getUUID());
		return NULL;
	}

	// Decode in a loop until we are done
	while (!state->decodeSection()) ;

	if (!state->isDone() || !state->isValid())
	{
		// Decode stopped early, or something bad happened to the file during
		// decoding.
		mBadAssetList.emplace(state->getUUID());
		return NULL;
	}

	// Write of the decoded audio to the disk cache.
	state->finishDecode();

	return state;
}

// Called from the main thread only, with the result from the worker thread.
void LLAudioDecodeMgr::Impl::enqueueFinishAudio(const LLUUID& id,
												LLVorbisDecodeState::ptr_t& state)
{
	if (tryFinishAudio(id, state))
	{
		// Done early or aborted.
		llassert(mDecodes.count(id));
		mDecodes.erase(id);
		LL_DEBUGS("Audio") << "Finished decode of " << id
						   << " - Decodes queue size = " << mDecodes.size()
						   << LL_ENDL;
		return;
	}
	// Not done; enqueue it.
	mDecodes.emplace(id, state);
	LL_DEBUGS("Audio") << "Enqueued decode for " << id
					   << " - Decodes queue size = " << mDecodes.size()
					   << LL_ENDL;
}

// Called from the main thread only.
bool LLAudioDecodeMgr::Impl::tryFinishAudio(const LLUUID& id,
											LLVorbisDecodeState::ptr_t state)
{
	if (!gAudiop || !(state && state->isWritten()))
	{
		return false;
	}

	LLAudioData* adp = gAudiop->getAudioData(id);
	if (!adp)
	{
		llwarns << "Missing audio data for " << id << llendl;
		return true;
	}

	bool valid = state && state->isValid();
	// Mark current decode finished regardless of success or failure
	adp->setHasCompletedDecode(true);
	// Flip flags for decoded data
	adp->setHasDecodeFailed(!valid);
	adp->setHasDecodedData(valid);
	// When finished decoding, there will also be a decoded wav file cached on
	// disk with the .dsf extension
	if (valid)
	{
		LL_DEBUGS("Audio") << "Valid decoded data for " << id << LL_ENDL;
		adp->setHasWAVLoadFailed(false);
	}
	return true;
}

// Called from the main thread only.
void LLAudioDecodeMgr::Impl::checkDecodesFinished()
{
	decodes_map_t::iterator it = mDecodes.begin();
	while (it != mDecodes.end())
	{
		const LLUUID& id = it->first;
		const LLVorbisDecodeState::ptr_t& state = it->second;
		if (tryFinishAudio(id, state))
		{
			it = mDecodes.erase(it);
			LL_DEBUGS("Audio") << "Finished decode of " << id
							   << " - Decodes queue size = " << mDecodes.size()
							   << LL_ENDL;
		}
		else
		{
			++it;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

LLAudioDecodeMgr::LLAudioDecodeMgr()
:	mImpl(new Impl)
{
}

LLAudioDecodeMgr::~LLAudioDecodeMgr()
{
	delete mImpl;
}

void LLAudioDecodeMgr::processQueue()
{
	mImpl->processQueue();
}

bool LLAudioDecodeMgr::addDecodeRequest(const LLUUID& id)
{
	if (mImpl->mBadAssetList.count(id))
	{
		// Do not try to decode identified corrupted assets.
		return false;
	}

	if (gAudiop && gAudiop->hasDecodedFile(id))
	{
		// Already have a decoded version, we do not need to decode it.
		return true;
	}

	if (gAssetStoragep &&
		gAssetStoragep->hasLocalAsset(id, LLAssetType::AT_SOUND))
	{
		// Just put it on the decode queue if not already there.
		std::deque<LLUUID>::iterator end = mImpl->mDecodeQueue.end();
		if (std::find(mImpl->mDecodeQueue.begin(), end, id) == end)
		{
			mImpl->mDecodeQueue.emplace_back(id);
		}
		return true;
	}

	return false;
}
