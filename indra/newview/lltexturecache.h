/**
 * @file lltexturecache.h
 * @brief Object for managing texture cachees.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
 * Copyright (c) 2011-2023, Henri Beauchamp.
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

#ifndef LL_LLTEXTURECACHE_H
#define LL_LLTEXTURECACHE_H

#include <map>
#include <memory>
#include <set>

#include "lldir.h"
#include "hbfastmap.h"
#include "llfile.h"
#include "llimage.h"
#include "llstring.h"
#include "llthreadpool.h"
#include "lluuid.h"

class LLTextureCache
{
	friend class LLTextureCacheWorker;
	friend class LLTextureCacheRemoteWorker;
	friend class LLTextureCacheLocalFileWorker;

protected:
	LOG_CLASS(LLTextureCache);

private:
	// Entries
	struct EntriesInfo
	{
		EntriesInfo()
		:	mVersion(0.f),
			mAddressSize(0),
			mEntries(0)
		{
		}

		F32 mVersion;
		U32 mAddressSize;
		U32 mEntries;
	};

	struct Entry
	{
        Entry()
		:	mBodySize(0),
			mImageSize(0),
			mTime(0)
		{
		}

		Entry(const LLUUID& id, S32 imagesize, S32 bodysize, U32 time)
		:	mID(id),
			mImageSize(imagesize),
			mBodySize(bodysize),
			mTime(time)
		{
		}

		LL_INLINE void init(const LLUUID& id, U32 time)
		{
			mID = id;
			mImageSize = mBodySize = 0;
			mTime = time;
		}

		LL_INLINE Entry& operator=(const Entry& entry)
		{
			mID = entry.mID;
			mImageSize = entry.mImageSize;
			mBodySize = entry.mBodySize;
			mTime = entry.mTime;
			return *this;
		}

		LLUUID	mID;		// 16 bytes
		S32		mImageSize;	// total size of image if known
		S32		mBodySize;	// size of body file in body cache
		U32		mTime;		// seconds since 1/1/1970
	};

public:
	class Responder : public LLThreadSafeRefCount
	{
	public:
		virtual ~Responder() = default;

		virtual void started() = 0;
		virtual void completed(bool success) = 0;

		virtual void setData(U8* data, S32 datasize, S32 imagesize,
							 S32 imageformat, bool imagelocal) = 0;
	};

	class ReadResponder : public Responder
	{
	public:
		LL_INLINE ReadResponder()
		:	mImageSize(0),
			mImageLocal(false)
		{
		}

		// Called from LLTextureCacheWorker::finishRead()
		void setData(U8* data, S32 datasize, S32 imagesize,
					 S32 imageformat, bool imagelocal) override;

		// Called from LLTextureFetchWorker::CacheReadResponder constructor
		LL_INLINE void setImage(LLImageFormatted* image)
		{
			mFormattedImage = image;
		}

	protected:
		LLPointer<LLImageFormatted>	mFormattedImage;
		S32							mImageSize;
		bool						mImageLocal;
	};

	class WriteResponder : public Responder
	{
		// Not used for write operations.
		LL_INLINE void setData(U8*, S32, S32, S32, bool) override
		{
		}
	};

	LLTextureCache();
	~LLTextureCache();

	void shutdown();

	S32 update();

	void purgeCache(ELLPath location);

	// Called from the main thread before initCache() is called.
	LL_INLINE void setReadOnly(bool read_only)	{ mReadOnly = read_only; }

	S64 initCache(ELLPath location, S64 maxsize);

	typedef U32 handle_t;

	// This is for reads from local files (typically, UI textures).
	bool readFromFile(const std::string& local_filename, const LLUUID& id,
					  S32 offset, S32 size, ReadResponder* responder);

	// This is for reads from the actual textures cache.
	bool readFromCache(const LLUUID& id, S32 offset, S32 size,
					   ReadResponder* responder);

	bool writeToCache(const LLUUID& id, U8* data, S32 datasize,
					  S32 imagesize, LLPointer<LLImageRaw> rawimage,
					  S32 discardlevel, WriteResponder* responder);

	bool removeFromCache(const LLUUID& id);

	// Debug
	LL_INLINE U32 getNumReads()					{ return mNumReads; }
	LL_INLINE U32 getNumWrites()				{ return mNumWrites; }
	LL_INLINE S64 getUsage()					{ return mTexturesSizeTotal; }
	LL_INLINE U32 getEntries()					{ return mHeaderEntriesInfo.mEntries; }

	bool isInCache(const LLUUID& id);
	bool isInLocal(const LLUUID& id);			// NOT thread-safe

	LL_INLINE static S64 getMaxUsage()			{ return sCacheMaxTexturesSize; }

protected:
	std::string getLocalFileName(const LLUUID& id);
	std::string getTextureFileName(const LLUUID& id);
	void setDirNames(ELLPath location);
	void readHeaderCache();
	void clearCorruptedCache();
	void purgeAllTextures(bool purge_directories);
	void purgeTextures(bool validate);
	void purgeTextureFilesTimeSliced(bool force = false);
	LLFile* openHeaderEntriesFile(bool readonly, S32 offset);
	void closeHeaderEntriesFile();
	void readEntriesHeader();
	void writeEntriesHeader();
	S32 openAndReadEntry(const LLUUID& id, Entry& entry, bool create);
	bool updateEntry(S32& idx, Entry& entry, S32 new_image_size,
					 S32 new_body_size);
	void updateEntryTimeStamp(S32 idx, Entry& entry);
	U32 openAndReadEntries(std::vector<Entry>& entries);
	void writeEntriesAndClose(const std::vector<Entry>& entries);
	void readEntryFromHeaderImmediately(S32& idx, Entry& entry);
	S32 readEntryFromHeaderImmediatelyShared(S32& idx, Entry& entry);
	void writeEntryToHeaderImmediately(S32& idx, Entry& entry,
									   bool write_header = false);
	void removeEntry(S32 idx, Entry& entry, std::string& filename,
					 bool remove_file = true);
	void removeCachedTexture(const LLUUID& id);
	S32 getHeaderCacheEntry(const LLUUID& id, Entry& entry);
	S32 setHeaderCacheEntry(const LLUUID& id, Entry& entry, S32 imagesize,
							S32 datasize);
	void writeUpdatedEntries();
	void updatedHeaderEntriesFile();

protected:
	typedef std::unique_ptr<LLThreadPool> thread_pool_ptr_t;
	thread_pool_ptr_t	mThreadPoolp;

	LLMutex				mLRUMutex;
	LLMutex				mHeaderMutex;
	LLAtomicU32			mNumReads;
	LLAtomicU32			mNumWrites;

	LLFile*				mHeaderFile;

	typedef fast_hmap<LLUUID, std::string> purge_map_t;
	purge_map_t			mFilesToDelete;
	LLTimer				mSlicedPurgeTimer;

	// Headers (each header entry includes the first mip)
	std::string			mHeaderEntriesFileName;
	std::string			mHeaderDataFileName;
	EntriesInfo			mHeaderEntriesInfo;
	// Indexes of the deleted entries; keep it as an ordered set so that the
	// list is auto-sorted !  HB
	std::set<S32>		mFreeList;
	uuid_list_t			mLRU;
	typedef fast_hmap<LLUUID, S32> id_map_t;
	id_map_t			mHeaderIDMap;

	// Bodies (textures minus headers)
	std::string			mTexturesDirName;
	typedef fast_hmap<LLUUID, S32> size_map_t;
	size_map_t			mTexturesSizeMap;
	S64					mTexturesSizeTotal;
	LLAtomicBool		mDoPurge;

	// Keep this as an ordered map !  HB
	typedef std::map<S32, Entry> idx_entry_map_t;
	idx_entry_map_t		mUpdatedEntryMap;

	bool				mReadOnly;

	static LLAtomicU32	sTotalHits;
	static LLAtomicU32	sTotalMisses;
	static LLAtomicU32	sTotalWrites;
	static LLAtomicU32	sTotalErrors;
	static U32			sCacheMaxEntries;
	static S64			sCacheMaxTexturesSize;
};

// Note: there is no good to define 1024 for TEXTURE_CACHE_ENTRY_SIZE while
// FIRST_PACKET_SIZE is 600 on sim side.
constexpr S32 TEXTURE_CACHE_ENTRY_SIZE = FIRST_PACKET_SIZE;

// Global, initialized in llappviewer.cpp and used in newview/. Moved here so
// that LLTextureCache consumers do not need to include llappviewer.h to  use
// it. HB
extern LLTextureCache* gTextureCachep;

#endif // LL_LLTEXTURECACHE_H
